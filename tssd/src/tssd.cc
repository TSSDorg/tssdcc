#include <cstring>
#include <algorithm>
#include <unistd.h>

#include "tssd.h"
#include "typeinfo.h"
#include "buffer.h"
#include "flat.h"

namespace tssd {

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

void
FBuffer::moveFront(const size_t pos, int n)
{
    if (pos > 0) {
        for (int i=0; i<n; ++i)
            (*buffer_)[i] = (*buffer_)[pos+i];
    }
    buffer_->resize(n);
}

// return OK if we found magic
// otherwise return ERR_INSUFFICIENT_DATA
TError
FBuffer::detectMagic(const Bytes &data, std::size_t &more, const std::size_t skip)
{
    auto pre_size = buffer_->size();
    auto cpsize = std::min((std::size_t)4, data.size());
    if (magic_ >= 0) {
        append(data);
        goto RETURN;
    }
    if (pre_size >= MAGIC.length()) {
        if ((magic_ = findMagic(*buffer_, skip)) >= 0 ) {
            moveFront(skip+magic_, pre_size - skip -magic_);
            append(data);
            goto RETURN;
        }
        // if got "TSSDVTSSDV...", we met fmt err, need skip 5
        // when FMT_ERR,  size >= 8(Tschema), it is safe copy 4 to front
        moveFront(pre_size-4, 4);
    }
    pre_size = buffer_->size();
    if (pre_size + data.size() < MAGIC.length()) {
        append(data);
        more = TSSD_FRAGMENT_MIN_HEADER_SIZE - Size();
        return ERR_INSUFFICIENT_DATA;
    }

    append(data, cpsize);
    if ((magic_ = findMagic(*buffer_)) < 0 ) {
        auto pos = findMagic(data);
        if (pos < 0) {
            moveFront(pre_size-(4-cpsize), 4-cpsize);
            append(&data[data.size()-cpsize], cpsize);
            more = TSSD_FRAGMENT_MIN_HEADER_SIZE - Size();
            return ERR_INSUFFICIENT_DATA;
        }
        buffer_->resize(0);
        append(&data[pos], data.size() - pos);
        goto RETURN;
    }

    moveFront(magic_, pre_size-magic_);
    append(data);
RETURN:
    if (Size() < TSSD_FRAGMENT_MIN_HEADER_SIZE)
        more = TSSD_FRAGMENT_MIN_HEADER_SIZE - Size();
    magic_ = findMagic(*buffer_);
    return OK;
}

TError FBuffer::dumpMergeArrayHeader(const std::size_t pos, int &len, std::size_t &more)
{
    if (pos + TSSD_TARRAYM_HEAD_LENGTH > this->Size()) {
        more = TSSD_TARRAYM_HEAD_LENGTH - this->Size() - pos;
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

TError FBuffer::parseHeads(std::size_t &more)
{
    if (heads_len_>=0) return OK;
    if (this->Size() < TSSD_FRAGMENT_MIN_HEADER_SIZE) {
        more =  TSSD_FRAGMENT_MIN_HEADER_SIZE - this->Size();
        return ERR_INSUFFICIENT_DATA;
    }

    if ((*buffer_)[magic_+7] != std::byte(TType::Tschema)) {
        // find next magic
        return ERR_FORMAT_ERROR;
    }
    std::memcpy(header.magic, &(*this)[0], 5);
    std::memcpy(header.version, &(*this)[5], 2);

    std::size_t cursor = magic_ + 8; // 5-byte magic + 2-byte version + 1-byte Tschema
    tssd::Buffer schema_buf(VBytes(&(*buffer_)[cursor], buffer_->size() - cursor));
    const auto schema_size = schema_buf.Size();
    if (auto ret = schema.Unmarshal(schema_buf)) {
        if (ret == ERR_INSUFFICIENT_DATA) {
            more = TSSD_FRAGMENT_MIN_HEADER_SIZE - this->Size();
        }
        return ret;
    }

    cursor += schema_size - schema_buf.Size();
    std::size_t payload_begin = cursor + TSSD_TARRAYM_HEAD_LENGTH;
    if (auto ret = dumpMergeArrayHeader(cursor, payload_len_, more)) {
        return ret;
    }
    heads_len_ = payload_begin - magic_;
    return OK;
}

TError FBuffer::parsePayload(std::size_t &more)
{
    if (payload_>=0) return OK;

    if (heads_len_ + TSSD_TARRAYM_HEAD_LENGTH + payload_len_ > this->Size()) {
        more = heads_len_ + TSSD_TARRAYM_HEAD_LENGTH + payload_len_ - this->Size();
        return ERR_INSUFFICIENT_DATA;
    }
    payload_ = magic_ + heads_len_;
    return OK;
}

TError FBuffer::parseChecksum(std::size_t more)
{
    if (checksum_ >=0) return OK;
    if (auto ret = dumpMergeArrayHeader(payload_+payload_len_, checksum_len_, more)) {
        if (ret == ERR_FORMAT_ERROR) {
            reset();
        }
        return ret;
    }
    // check checksum leng
    if (heads_len_ + payload_len_ + TSSD_TARRAYM_HEAD_LENGTH + checksum_len_ > this->Size()) {
        more = heads_len_ + payload_len_ + TSSD_TARRAYM_HEAD_LENGTH + checksum_len_ - this->Size();
        return ERR_INSUFFICIENT_DATA;
    }
    checksum_len_ += TSSD_TARRAYM_HEAD_LENGTH;
    checksum_ = payload_ + payload_len_;
    return OK;
}

TError FBuffer::Feed(const Bytes &data, std::size_t &more)
{
    if (auto ret = detectMagic(data, more))
        return ret;
AGAIN:
    auto ret = parseHeads(more);
    if (ret) {
        if (ret==ERR_FORMAT_ERROR) {
            reset();
            if (auto ret2 =detectMagic(Bytes(), more, 5))
                return ret2;
            goto AGAIN;
        }
        return ret;
    }
    ret = parsePayload(more);
    if (ret) {
        if (ret==ERR_FORMAT_ERROR) {
            reset();
            if (auto ret2 =detectMagic(Bytes(), more, 5))
                return ret2;
            goto AGAIN;
        }
        return ret;
    }
    ret = parseChecksum(more);
    if (ret) {
        if (ret==ERR_FORMAT_ERROR) {
            reset();
            if (auto ret2 =detectMagic(Bytes(), more, 5))
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
    std::size_t remain = this->Size() - heads_len_ - payload_len_ - checksum_len_;
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
