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
    remain_pos = more = 0;
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
Fragment::Validate(VBytes input, VBytes checksum)
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
        if (size+more > TSSD_BUFFER_MTU) {
            Bytes bss((size+more)*(std::size_t)2);
            bss.resize(size);
            std::memcpy(&bss[0], &bs[0], size);
            bs.swap(bss);
        }
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

void FBuffer::append(const std::byte *data, const std::size_t nsize)
{
    if (nsize <=0) return;
    auto size = buffer_->size();
    if (size + nsize > buffer_->capacity()) {
        auto bs = std::make_shared<Bytes>((size+nsize)*2);
        bs->assign(buffer_->begin(), buffer_->end());
        buffer_->swap(*bs);
    }
    buffer_->resize(size + nsize);
    std::memcpy(&(*buffer_)[size], data, nsize);
}

TError
FBuffer::DetectMagic(const Bytes &data, std::size_t &more, const std::size_t skip)
{
    if (magic_ >= 0) {
        append(data);
        return OK;
    }
    auto pre_size = buffer_->size();
    if (pre_size >= 5) {
        if ((magic_ = findMagic(*buffer_, skip)) >= 0 ) {
            append(data);
            return OK;
        }
        // if got "TSSDVTSSDV...", we met fmt err, need skip 5
        // when FMT_ERR,  size >= 8(Tschema), it is safe copy 4 to front
        for (std::size_t i=0; i<4; i++)
            (*buffer_)[i] = (*buffer_)[pre_size-4+i];
        buffer_->resize(4);
    }
    pre_size = buffer_->size();
    if (pre_size + data.size() < 8) {
        append(data);
        more = TSSD_FRAGMENT_MIN_HEADER_SIZE - buffer_->size();
        return ERR_INSUFFICIENT_DATA;
    }
    append(data, std::min(data.size(), (std::size_t)4));
    if ((magic_ = findMagic(*buffer_)) < 0 ) {
        auto pos = findMagic(data);
        if (pos < 0) {
            auto size = data.size();
            buffer_->resize(4);
            append(&data[size-4], 4);
            more = TSSD_FRAGMENT_MIN_HEADER_SIZE - buffer_->size();
            return ERR_INSUFFICIENT_DATA;
        }
        buffer_->resize(0);
        append(&data[pos], data.size() - pos);
        magic_ = 0;
        return OK;
    }
    if (magic_ > 0) {
        for (auto i=0; i<4-magic_; ++i)
            (*buffer_)[i] = (*buffer_)[magic_+i];
    }
    buffer_->resize(4-magic_);
    append(data);
    magic_ = findMagic(*buffer_);
    return OK;
}

TError FBuffer::DumpMergeArrayHeader(const std::size_t pos, int &len, std::size_t &more)
{
    if (pos + TSSD_TARRAYM_HEAD_LENGTH > this->size()) {
        more = TSSD_TARRAYM_HEAD_LENGTH - this->size() - pos;
        return ERR_INSUFFICIENT_DATA;
    }

    if ((*buffer_)[pos] != std::byte(TType::Tarraym) || (*buffer_)[pos+1] != std::byte(TType::Tuint8)) {
        return ERR_FORMAT_ERROR;
    }

    std::int32_t size4 = 0;
    std::memcpy(&size4, &(*buffer_)[pos+2], sizeof(size4));

    std::int16_t arrayN = 0;
    std::memcpy(&arrayN, &(*buffer_)[pos+6], sizeof(arrayN));

    if (size4 != std::int32_t(arrayN + TSSD_SIZEA_LENGTH)) {
        return ERR_FORMAT_ERROR;
    }

    len = arrayN;
    return OK;
}

TError FBuffer::ParseHeads(std::size_t &more)
{
    if (heads_len_>=0) return OK;
    if (this->size() < TSSD_FRAGMENT_MIN_HEADER_SIZE) {
        more =  TSSD_FRAGMENT_MIN_HEADER_SIZE - this->size();
        return ERR_INSUFFICIENT_DATA;
    }

    if ((*buffer_)[magic_+7] != std::byte(TType::Tschema)) {
        // find next magic
        return ERR_FORMAT_ERROR;
    }
    std::memcpy(header.magic, &(*this)[0], 5);
    std::memcpy(header.version, &(*this)[5], 2);

    std::size_t cursor = magic_ + 8; // 5-byte magic + 2-byte version + 1-byte Tschema
    Buffer schema_buf(VBytes(&(*buffer_)[cursor], buffer_->size() - cursor));
    const auto schema_size = schema_buf.size();
    if (auto ret = schema.Unmarshal(schema_buf)) {
        if (ret == ERR_INSUFFICIENT_DATA) {
            more = TSSD_FRAGMENT_MIN_HEADER_SIZE - this->size();
        }
        return ret;
    }

    cursor += schema_size - schema_buf.size();
    std::size_t payload_begin = cursor + TSSD_TARRAYM_HEAD_LENGTH;
    if (auto ret = DumpMergeArrayHeader(cursor, payload_len_, more)) {
        return ret;
    }
    heads_len_ = payload_begin - magic_;
    return OK;
}

TError FBuffer::ParsePayload(std::size_t &more)
{
    if (payload_>=0) return OK;

    if (heads_len_ + TSSD_TARRAYM_HEAD_LENGTH + payload_len_ > this->size()) {
        more = heads_len_ + TSSD_TARRAYM_HEAD_LENGTH + payload_len_ - this->size();
        return ERR_INSUFFICIENT_DATA;
    }
    payload_ = magic_ + heads_len_;
    return OK;
}

TError FBuffer::ParseChecksum(std::size_t more)
{
    if (checksum_ >=0) return OK;
    if (auto ret = DumpMergeArrayHeader(payload_+payload_len_, checksum_len_, more)) {
        if (ret == ERR_FORMAT_ERROR) {
            reset();
        }
        return ret;
    }
    // check checksum leng
    if (heads_len_ + payload_len_ + TSSD_TARRAYM_HEAD_LENGTH + checksum_len_ > this->size()) {
        more = heads_len_ + payload_len_ + TSSD_TARRAYM_HEAD_LENGTH + checksum_len_ - this->size();
        return ERR_INSUFFICIENT_DATA;
    }
    checksum_len_ += TSSD_TARRAYM_HEAD_LENGTH;
    checksum_ = payload_ + payload_len_;
    return OK;
}

TError FBuffer::Feed(const Bytes &data, std::size_t &more)
{
    if (auto ret = DetectMagic(data, more))
        return ret;
AGAIN:
    auto ret = ParseHeads(more);
    if (ret) {
        if (ret==ERR_FORMAT_ERROR) {
            reset();
            if (auto ret2 =DetectMagic(Bytes(), more, 5))
                return ret2;
            goto AGAIN;
        }
        return ret;
    }
    ret = ParsePayload(more);
    if (ret) {
        if (ret==ERR_FORMAT_ERROR) {
            reset();
            if (auto ret2 =DetectMagic(Bytes(), more, 5))
                return ret2;
            goto AGAIN;
        }
        return ret;
    }
    ret = ParseChecksum(more);
    if (ret) {
        if (ret==ERR_FORMAT_ERROR) {
            reset();
            if (auto ret2 =DetectMagic(Bytes(), more, 5))
                return ret2;
            goto AGAIN;
        }
        return ret;
    }
    auto frag =  fragment();
    if (ret = frag->Validate()) return ret;
    frag_ = frag;
    return OK;
}

pFragment FBuffer::fragment()
{
    std::size_t remain = this->size() - heads_len_ - payload_len_ - checksum_len_;
    pFragment frag = std::make_shared<tssd::Fragment>(std::max(TSSD_BUFFER_MTU, remain));
    frag->data.resize(0);
    if (remain>0) {
        auto begin = &(*buffer_)[checksum_ + checksum_len_];
        frag->data.assign(begin, begin+remain);
    }
    frag->data.swap(*buffer_);
    frag->data.resize(heads_len_ + payload_len_ + checksum_len_);
    frag->header = header;
    frag->schema = schema;
    frag->heads = VBytes(&frag->data[0], heads_len_);
    frag->payload = VBytes(&frag->data[payload_], payload_len_);
    frag->checksum = VBytes(&frag->data[checksum_], checksum_len_);
    reset();
    return frag;
}

} // namespace
