#include <cstring>
#include <algorithm>

#include "tssd.h"
#include "typeinfo.h"
#include "buffer.h"
#include "flat.h"

namespace tssd {

TError merge_byte_slice_dump(VBytes input, int &len) {
    if (input.size() < 8) {
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

    if (input.size() < std::size_t(8 + arrayN)) {
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

TError
Fragment::Unmarshal(VBytes input, int &remain_pos)
{
    auto magic_pos = Manager::findMagic(input);
    if (magic_pos < 0) {
        return ERR_FORMAT_ERROR;
    }

    VBytes data(input.subspan(magic_pos));
    if (data.size() < 8) {
        remain_pos = magic_pos;
        return ERR_INSUFFICIENT_DATA;
    }

    if (data[7] != std::byte(TType::Tschema)) {
        remain_pos = magic_pos;
        return ERR_FORMAT_ERROR;
    }

    std::memcpy(header.magic, data.data(), 5);
    std::memcpy(header.version, data.data() + 5, 2);

    std::size_t cursor = 8; // 5-byte magic + 2-byte version + 1-byte Tschema
    Buffer schema_buf(data.subspan(cursor));
    const auto schema_size = schema_buf.size();
    if (auto ret = schema.Unmarshal(schema_buf)) {
        remain_pos = magic_pos;
        return ret;
    }
    cursor += schema_size - schema_buf.size();

    std::size_t posData = cursor + 8;
    int payload_len(0);
    if (auto ret = merge_byte_slice_dump(data.subspan(cursor), payload_len)) {
        remain_pos = magic_pos;
        return ret;
    }
    heads = data.subspan(0, posData);
    payload = data.subspan(posData, payload_len);

    const std::size_t need_check_end = posData + payload.size();
    int checksum_len(0);
    if (auto ret = merge_byte_slice_dump(data.subspan(need_check_end), checksum_len)) {
        remain_pos = magic_pos;
        return ret;
    }
    checksum = data.subspan(need_check_end+8, checksum_len);

    const std::size_t posChecksum = need_check_end + 8;

    VBytes need_check =  data.subspan(0, need_check_end);

    if (auto ret = Validate(need_check)) {
        remain_pos = magic_pos;
        return ret;
    }

    this->data.assign(data.begin(), data.begin() + posChecksum + checksum_len);

    remain_pos = this->data.size();
    return OK;
}

TError
Fragment::Validate(VBytes input) const
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

} // namespace
