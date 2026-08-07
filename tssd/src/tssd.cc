#include <cstring>
#include <algorithm>
#include <unistd.h>

#include "tssd.h"
#include "typeinfo.h"
#include "buffer.h"
#include "flat.h"

namespace tssd {


// set more if met ERR_INSUFFICIENT_DATA
TError merge_byte_slice_dump(VBytes input, std::size_t &len, std::size_t &more) {
    if (input.size() < TSSD_TARRAYM_HEAD_LENGTH) {
        more = TSSD_TARRAYM_HEAD_LENGTH - input.size();
        return ERR_INSUFFICIENT_DATA;
    }

    if (input[0] != std::byte(TType::Tarraym) || input[1] != std::byte(TType::Tuint8)) {
        return ERR_FORMAT_ERROR;
    }

    std::int32_t size4 = 0;
    std::memcpy(&size4, input.data() + 2, sizeof(size4));

    std::int16_t arrayN = 0;
    std::memcpy(&arrayN, input.data() + 6, sizeof(arrayN));

    if (size4 != std::int32_t(arrayN + TSSD_SIZEA_LENGTH)) {
        return ERR_FORMAT_ERROR;
    }

    if (input.size() < std::size_t(TSSD_TARRAYM_HEAD_LENGTH + arrayN)) {
        more = std::size_t(TSSD_TARRAYM_HEAD_LENGTH + arrayN) - input.size();
        return ERR_INSUFFICIENT_DATA;
    }

    len = arrayN;
    return OK;
}

TError
Schema::Marshal(Buffer &buf)
{
    return Manager::schemaTypeInfo->save((std::byte*)this, buf);
}

TError
Schema::Unmarshal(Buffer &buf)
{
    return Manager::schemaTypeInfo->dump(buf, (std::byte*)this);
}

// we need unmarshal fragment manualy
// @desc  internal api, we don't copy data from user space
// input: data should contains magic "TSSDV", length should > TSSD_FRAGMENT_MIN_HEADER_SIZE
//
// return
//
//      more:   need more data if we meet ErrorInSufficientData
//      remain: remain bytes after consume when unmarshal success
//      TError: ErrorInSufficientData means need more data to unmarshal
//              ErrorInvalidTSSDData is invalid data
TError
Fragment::Unmarshal(VBytes input, std::size_t &remain_pos, std::size_t &more)
{
    auto magic_pos = Manager::findMagic(input);
    if (magic_pos < 0) {
        return ERR_FORMAT_ERROR;
    }

    VBytes data(input.subspan(magic_pos));
    if (data.size() < TSSD_FRAGMENT_MIN_HEADER_SIZE) {
        more = TSSD_FRAGMENT_MIN_HEADER_SIZE - data.size();
        return ERR_INSUFFICIENT_DATA;
    }

    if (data[7] != std::byte(TType::Tschema)) {
        return ERR_FORMAT_ERROR;
    }

    std::memcpy(header.magic, data.data(), 5);
    std::memcpy(header.version, data.data() + 5, 2);

    std::size_t cursor = 8; // 5-byte magic + 2-byte version + 1-byte Tschema
    Buffer schema_buf(data.subspan(cursor));
    const auto schema_size = schema_buf.size();
    if (auto ret = schema.Unmarshal(schema_buf)) {
        if (ret==ERR_INSUFFICIENT_DATA)
            more = TSSD_FRAGMENT_MIN_HEADER_SIZE - schema_size;
        return ret;
    }
    cursor += schema_size - schema_buf.size();

    std::size_t payload_begin = cursor + TSSD_TARRAYM_HEAD_LENGTH;
    std::size_t payload_len(0);
    if (auto ret = merge_byte_slice_dump(data.subspan(cursor), payload_len, more)) {
        return ret;
    }

    const std::size_t need_check_end = payload_begin + payload_len;
    std::size_t checksum_len(0);
    if (auto ret = merge_byte_slice_dump(data.subspan(need_check_end), checksum_len, more)) {
        return ret;
    }

    if (auto ret = Validate(data.subspan(0, need_check_end), data.subspan(need_check_end+TSSD_TARRAYM_HEAD_LENGTH, checksum_len))) {
        return ret;
    }

    this->data.assign(data.begin(), data.begin() + need_check_end + TSSD_TARRAYM_HEAD_LENGTH + checksum_len);
    heads = std::span<std::byte>(&this->data[0], payload_begin);
    payload = std::span<std::byte>(&this->data[payload_begin], payload_len);
    checksum = std::span<std::byte>(&this->data[need_check_end], TSSD_TARRAYM_HEAD_LENGTH + checksum_len);

    remain_pos = magic_pos + this->data.size();
    return OK;
}

TError
Fragment::Validate(VBytes input, VBytes checksum) const
{
    if (!checksum.empty()) {
        auto expected = Manager::checksum(input.data(), static_cast<int>(input.size()));
        const std::string actual(reinterpret_cast<const char*>(checksum.data()), checksum.size());
        if (expected != actual) {
            return ERR_CHECKSUM_FAILURE;
        }
    }
    return OK;
}

TError Fragment::Read(int fd)
{
    std::size_t more = TSSD_FRAGMENT_MIN_HEADER_SIZE;
    Bytes bs(TSSD_BUFFER_MTU);
    size_t size = 0, remain_pos;
    while(1)
    {
        errno = 0;
        int n = read(fd, &bs[size], more);
        if (!n || errno) {
            return ERR_IO;
        }
        size += n;
        auto ret = this->Unmarshal(std::span<std::byte>(&bs[0], size), remain_pos, more);
        if (!ret) {
            return OK;
        }
        // other error
        if (ret!=ERR_INSUFFICIENT_DATA) {
            return ret;
        }
    }

}

} // namespace
