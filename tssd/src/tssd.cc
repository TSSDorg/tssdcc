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

void RBuffer::append(const std::byte *data, const std::size_t nsize)
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
RBuffer::moveFront(const size_t pos, int n)
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
RBuffer::detectMagic(const Bytes &data, std::size_t &more, const std::size_t skip)
{
    auto pre_size = buffer_->size();
    auto cpsize = std::min((std::size_t)4, data.size());
    if (magic_ >= 0) {
        append(data);
        if (Size() < TSSD_FRAGMENT_MIN_HEADER_SIZE)
            more = TSSD_FRAGMENT_MIN_HEADER_SIZE - Size();
        return OK;
    }
    if (pre_size >= MAGIC.length()) {
        if ((magic_ = findMagic(*buffer_, skip)) >= 0 ) {
            moveFront(skip+magic_, pre_size - skip - magic_);
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

TError RBuffer::dumpMergeArrayHeader(const std::size_t pos, int &len, std::size_t &more)
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

TError RBuffer::parseHeads(std::size_t &more)
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
            more = TSSD_FRAGMENT_MIN_HEADER_SIZE;
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

TError RBuffer::parsePayload(std::size_t &more)
{
    if (payload_>=0) return OK;

    if (heads_len_ + TSSD_TARRAYM_HEAD_LENGTH + payload_len_ > this->Size()) {
        more = heads_len_ + TSSD_TARRAYM_HEAD_LENGTH + payload_len_ - this->Size();
        return ERR_INSUFFICIENT_DATA;
    }
    payload_ = magic_ + heads_len_;
    return OK;
}

TError RBuffer::parseChecksum(std::size_t more)
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

TError RBuffer::Extract(const Bytes &data, std::size_t &more)
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
    more = 0;
    auto cks = VBytes(&(*this)[heads_len_ + payload_len_+TSSD_TARRAYM_HEAD_LENGTH], checksum_len_ - TSSD_TARRAYM_HEAD_LENGTH);
    ret = Fragment::Validate(VBytes(&(*this)[magic_], heads_len_ + payload_len_), cks);
    if (ret) {
        auto len = heads_len_ + payload_len_ + checksum_len_;
        moveFront(magic_+len, this->Size() - len);
        reset();
    }
    return ret;
}

pFragment RBuffer::Fragment()
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

pBuffer RBuffer::Buffer(const std::string &family, const std::string &version)
{
    for (auto it = results_[family][version].begin(); it != results_[family][version].end(); ++it) {
        if (!it->second->Wanted()) {
            auto ret = it->second;
            results_[family][version].erase(it);
            return ret;
        }
    }
    return nullptr;
}

TError RBuffer::Extract(Reader &reader)
{
    Bytes bs(TSSD_BUFFER_MTU);
    std::size_t more(TSSD_BUFFER_MTU);
    bool exit(false), got(false);
    TError ret = OK;
    do {
        if (more) {
            more = std::min(more, TSSD_BUFFER_MTU);
            bs.resize(more);
            int n = reader.Read(bs.data(), more);
            if (n<=0) {
                exit = true;
                ret = ERR_IO;
            }
            bs.resize(n<0 ? 0 : n);
        }
        while(!(ret = Extract(bs, more)))
        {
            exit = !pushFragment();  // exit out loop after got a buffer
            if (!got) got = exit;    // save if we got a TSSD Buffer
            bs.resize(0);
            //repeate extract cache data until failure
        }

        if (ret==ERR_INSUFFICIENT_DATA) {
            continue;
        }
        if (ret) return ret;
    } while(!exit);
    return got ? OK : ret;   // return OK, if we got a TSSD Buffer
}

TError RBuffer::pushFragment()
{
    auto frag = this->Fragment();
    auto &types = frag->schema.Types;
    auto version = Manager::TypesToVersionInfo(types);
    if (!version) {
        if (!unregistered_.contains(types))
            unregistered_[types] = TBuffers();

        auto &tbuf = unregistered_[types];
        if (!tbuf.contains(frag->schema.TID))
            tbuf[frag->schema.TID] = std::make_shared<tssd::Buffer>();
        tbuf[frag->schema.TID]->Push(frag);
        return ERR_SCHEMA_NOT_MATCH;
    }
    if (!results_.contains(version->family))
        results_[version->family] = VBuffers();

    auto &bufs = results_[version->family];

    if (!bufs.contains(version->version))
        bufs[version->version] = TBuffers();
    if (!bufs[version->version].contains(frag->schema.TID))
        bufs[version->version][frag->schema.TID] = std::make_shared<tssd::Buffer>();

    return bufs[version->version][frag->schema.TID]->Push(frag);
}

TError RBuffer::Read(Reader &reader, Flatable &flat)
{
    pBuffer dbuf;
    while(!(dbuf = this->Buffer(flat.Family(), flat.Version())))
    {
        if (auto ret = this->Extract(reader)) {
            return ret;
        }
    }

    return Manager::UnmarshalTo(*dbuf, flat);
}

TError RBuffer::Read(Flatable &flat)
{
    pBuffer dbuf = this->Buffer(flat.Family(), flat.Version());
    if(!dbuf)
        return ERR_INSUFFICIENT_DATA;

    return Manager::UnmarshalTo(*dbuf, flat);
}

TError RBuffer::Write(const Flatable &flat, Writer &writer, const int mtu)
{
    tssd::Buffer buf(mtu);
    if (auto ret = tssd::Manager::MarshalTo(flat, buf)) {
        return ret;
    }

    auto frags  = buf.Fragments();
    for (std::size_t i=0; i<frags.size(); i++) {
        int n = writer.Write(frags[i]->data.data(), frags[i]->data.size());
        if (n<=0) return ERR_IO;
    }
    return OK;
}

} // namespace
