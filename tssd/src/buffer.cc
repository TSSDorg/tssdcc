#include <cstring>
#include <memory>
#include <span>

#include "buffer.h"
#include "flat.h"

namespace tssd {

TError Buffer::Prepare(Schema schema)
{
    if (mtu_ == 0) mtu_ = TSSD_BUFFER_MTU;
    mtu_ = std::max(mtu_, TSSD_BUFFER_MIN_MTU);

    this->schema_ = schema;

    Buffer nbuf(mtu_/3);
    nbuf.Append((std::byte *)Manager::MAGIC.c_str(), Manager::MAGIC.length());
    nbuf.Append(std::byte(MINOR));
    nbuf.Append(std::byte(MAJOR));
    nbuf.Append(std::byte(TType::Tschema));
    if (auto ret = schema.Marshal(nbuf)) return ret;


    nbuf.Append(std::byte(TType::Tarraym));
    nbuf.Append(std::byte(TType::Tuint8));

    checksum_len_ = 8 + Manager::checksum(Manager::MAGIC.c_str(), Manager::MAGIC.length()).length(); //8 bytes for [Tarraym][Tuint8][sizet/4B][sizea/2B]
    int avail = mtu_ - nbuf.Size() - TSSD_SIZET_LENGTH - TSSD_SIZEA_LENGTH - checksum_len_;
    nbuf.AppendSize4(avail + TSSD_SIZEA_LENGTH);
    nbuf.AppendSize2(avail);
    if (nbuf.Size() > mtu_ || nbuf.fragments_.size() > 1) {
        return ERR_TSSD_MTU_TOO_SMALL;
    }
    heads_.resize(nbuf.Size());
    memcpy(&heads_[0], &nbuf.fragments_[0]->data[0], nbuf.Size());
    std::cout << "2 heads_ size:" << heads_.size() << ",cap:" << heads_.capacity() << " nbuf size:" << nbuf.Size() << std::endl;
    return OK;
}

// [TSSD][Tversion][TSSD_VERSION_MINOR][TSSD_VERSION_MAJOR][Tschema][Tobject][sizet/4B][sizea/2B][FID][...]
void Buffer::updateFragmentID(int index, int n)
{
    if (heads_.empty() || fragments_[index]->data.size() < 17)
        return;
    memcpy(&fragments_[index]->data[16], &n, TSSD_SIZEA_LENGTH);
}

void Buffer::appendChecksum(int index, int pos)
{
    auto bs = Manager::checksum(fragments_[index]->data.data(), pos);
    fragments_[index]->data[pos] = std::byte(TType::Tarraym);
    fragments_[index]->data[pos+1] = std::byte(TType::Tuint8);
    int len = TSSD_SIZEA_LENGTH + bs.length();
    int arrayN = bs.length();
    memcpy(&fragments_[index]->data[pos+2], &len, TSSD_SIZET_LENGTH);
    memcpy(&fragments_[index]->data[pos+6], &arrayN, TSSD_SIZEA_LENGTH);
    memcpy(&fragments_[index]->data[pos+8], bs.c_str(), bs.length());
}

void Buffer::Finish()
{
    if (size_ == 0) return;
    //this->print("finish 1:", heads_.size() + size_);
    if (heads_.empty()) {
        auto size = this->Size();
        for (std::size_t i=0; i<windex_; ++i)
        {
            auto csize = fragments_[i]->data.size();
            fragments_[i]->payload = std::span(&fragments_[i]->data[0], csize);
            size -= csize;
        }
        fragments_[windex_]->data.resize(size);
        fragments_[windex_]->payload = std::span(&fragments_[windex_]->data[0], size);
        return;
    }

    int pos = heads_.size();
    int length = woffset_ - pos;
    updateFragmentID(windex_, -(windex_+1));

    auto sizet = length + TSSD_SIZEA_LENGTH;
    memcpy(&fragments_[windex_]->data[pos-TSSD_SIZET_LENGTH-TSSD_SIZEA_LENGTH], &sizet, TSSD_SIZET_LENGTH);
    memcpy(&fragments_[windex_]->data[pos-TSSD_SIZEA_LENGTH], &length, TSSD_SIZEA_LENGTH);
    appendChecksum(windex_, heads_.size() + length);
    fragments_[windex_]->payload = std::span(&fragments_[windex_]->data[heads_.size()], length);

    //reset last fragment's size
    fragments_[windex_]->heads = std::span(&fragments_[windex_]->data[0], heads_.size());
    fragments_[windex_]->data.resize(heads_.size() + length + checksum_len_);
    fragments_[windex_]->checksum = std::span(&fragments_[windex_]->data[heads_.size() + length], checksum_len_);
    for (std::size_t i=0; i<windex_; ++i)
    {
        appendChecksum(i, avail(i));
        fragments_[i]->heads = std::span(&fragments_[i]->data[0], heads_.size());
        fragments_[i]->payload = std::span(&fragments_[i]->data[heads_.size()], avail(i) - heads_.size());
        fragments_[i]->checksum = std::span(&fragments_[i]->data[mtu_ - checksum_len_], checksum_len_);
    }
}

Buffer& Buffer::Append(const std::span<std::byte> bs)
{
    if (bs.empty()) return *this;
    std::size_t written = 0;
    while (written < bs.size())
    {
        if (windex_ == fragments_.size()) {
            auto fra = std::make_shared<Fragment>(mtu_);
            fragments_[windex_] = fra;
            if (!heads_.empty()) {
                std::memcpy(&fra->data[0], &heads_[0], heads_.size());
                updateFragmentID(windex_, windex_+1);
                woffset_ += heads_.size();
            }
            fra->heads = std::span<std::byte>(&fra->data[0], heads_.size());
        }

        auto fra = fragments_[windex_];
        if ( woffset_ + bs.size() - written <= avail(windex_) ) {
            memcpy(&fra->data[woffset_], &bs[written], bs.size() - written);
            size_ += bs.size() - written;
            woffset_ += bs.size() - written;
            return *this;
        }

        int fill = avail(windex_) - woffset_;
        if (!fill) {  //we are at the end of line
            woffset_ = 0;
            windex_ ++;
            continue;
        }
        memcpy(&fra->data[woffset_], &bs[written], fill);
        woffset_ += fill;
        size_ += fill;
        written += fill;
    }
    return *this;
}


TError Buffer::Dump(std::size_t size, std::byte *dest) {
    if (size > size_) return ERR_INSUFFICIENT_DATA;

    std::size_t read = 0;
    while (read < size)
    {
        if ( offset_ + size - read <= fragments_[index_]->payload.size()) {
            memcpy(&dest[read], &fragments_[index_]->payload[offset_], size - read);
            offset_ += size - read;
            size_ -= size - read;
            return OK;
        }

        int remain = fragments_[index_]->payload.size() - offset_;
        if (!remain) {
            offset_ = 0;
            index_ ++;
            continue;
        }
        memcpy(&dest[read], &fragments_[index_]->payload[offset_], remain);
        size_ -= remain;
        read += remain;
        offset_ += remain;
    }
    return OK;
}


Buffer& Buffer::Append(const std::byte bt)
{
    return this->Append(&bt, 1);
}

Buffer& Buffer::Append(const TType bt)
{
    return Append(std::byte(bt));
}

int Buffer::DumpSize2()
{
    return dumpSize<std::int16_t>();
}

int Buffer::DumpSize4()
{
    return dumpSize<std::int32_t>();
}

int Buffer::Push(pFragment fragment)
{
    if (schema_.hash.empty()) {
        schema_ = fragment->schema;
        heads_.assign(fragment->heads.begin(), fragment->heads.end());
        checksum_len_ = fragment->checksum.size();
    }
    if (schema_.hash != fragment->schema.hash || schema_.tid != fragment->schema.tid) {
        return ERR_SCHEMA_NOT_MATCH;
    }
    if (fragment->schema.fragment == 0) {
        return ERR_FORMAT_ERROR;
    }

    int fid = fragment->schema.fragment;
    fid = fid < 0 ? -fid : fid;
    if (!fragments_.contains(fid-1)) {
        size_ +=  fragment->payload.size();
    }
    fragments_[fid-1] = fragment;

    return (int)Wanted();
}

std::size_t Buffer::Wanted()
{
    std::size_t i = 0;
    for (; i<fragments_.size(); i++) {
        if (!fragments_.contains(i))
            return i+1;
        if (fragments_[i]->schema.fragment < 0) {
            return 0;
        }
    }
    return i + 1;
}

void Buffer::Merge()
{
    if (fragments_.size() < 2) return;
    Rewind();
    Split(this->Size() + heads_.size() + checksum_len_);
}

void Buffer::Split(std::size_t mtu)
{
    if (fragments_.empty() || mtu <= heads_.size() + checksum_len_) return;
    mtu_ = mtu;
    size_ = 0;
    windex_ = index_ = 0;
    woffset_ = offset_ = 0;

    std::unordered_map<std::size_t, std::shared_ptr<Fragment>> nfrags;
    std::swap(fragments_, nfrags);

    for (std::size_t i=0; i<nfrags.size(); ++i)
        this->Append(nfrags[i]->payload);
    this->Finish();
}

} //end namespace tssd
