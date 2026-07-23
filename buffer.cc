#include <cstring>
#include <memory>
#include <span>

#include "buffer.h"
#include "flat.h"


TError Buffer::prepare(Schema schema)
{
    if (mtu_ == 0) mtu_ = TSSD_BUFFER_MTU;
    mtu_ = std::max(mtu_, TSSD_BUFFER_MIN_MTU);

    this->schema_ = schema;

    Buffer nbuf(mtu_/3);
    nbuf.append((std::byte *)Manager::MAGIC.c_str(), Manager::MAGIC.length());
    nbuf.append(std::byte(MINOR));
    nbuf.append(std::byte(MAJOR));
    nbuf.append(std::byte(TType::Tschema));
    nbuf.print("before schema:");
    if (auto ret = schema.Marshal(nbuf)) return ret;
    nbuf.print("after schema:");

    nbuf.append(std::byte(TType::Tarraym));
    nbuf.append(std::byte(TType::Tuint8));

    checksum_len_ = 8 + Manager::checksum(Manager::MAGIC.c_str(), Manager::MAGIC.length()).length(); //8 bytes for [Tarraym][Tuint8][sizet/4B][sizea/2B]
    int avail = mtu_ - nbuf.size() - TSSD_SIZET_LENGTH - TSSD_SIZEA_LENGTH - checksum_len_;
    nbuf.appendSize4(avail + TSSD_SIZEA_LENGTH);
    nbuf.appendSize2(avail);
    if (nbuf.size() > mtu_ || nbuf.fragments_.size() > 1) {
        return ERR_TSSD_MTU_TOO_SMALL;
    }
    heads_.resize(nbuf.size());
    memcpy(&heads_[0], &nbuf.fragments_[0]->data[0], nbuf.size());
    nbuf.print();
    std::cout << "2 heads_ size:" << heads_.size() << ",cap:" << heads_.capacity() << " nbuf size:" << nbuf.size() << std::endl;
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
    memcpy(&fragments_[index]->data[pos+5], &arrayN, TSSD_SIZEA_LENGTH);
    memcpy(&fragments_[index]->data[pos+7], bs.c_str(), bs.length());
}

void Buffer::finish()
{
    if (size_ == 0) return;
    int pos = heads_.size();
    int length = woffset_ - pos;
    if (!length) {
        windex_ --;
        length = avail(windex_);
    }

    updateFragmentID(windex_, -(windex_+1));
    memcpy(&fragments_[windex_]->data[pos-TSSD_SIZET_LENGTH-TSSD_SIZEA_LENGTH], &length, TSSD_SIZET_LENGTH);
    memcpy(&fragments_[windex_]->data[pos-TSSD_SIZEA_LENGTH], &length, TSSD_SIZEA_LENGTH);
    appendChecksum(windex_, heads_.size() + length);

    //reset last fragment's size
    fragments_[windex_]->data.resize(heads_.size() + length + checksum_len_);

    for (int i=0; i<windex_; ++i)
    {
        appendChecksum(i, heads_.size() + avail(i));
    }
}

Buffer& Buffer::append(const std::span<std::byte> bs)
{
    if (bs.empty()) return *this;
    int written = 0;
    while (written < bs.size())
    {
        if (windex_ == fragments_.size()) {
            auto fra = std::make_shared<Fragment>(mtu_);
            fragments_.emplace_back(fra);
            if (!heads_.empty()) {
                memcpy(&fra->data[0], &heads_[0], heads_.size());
                updateFragmentID(windex_, windex_+1);
                woffset_ += heads_.size();
            }
        }

        auto fra = fragments_[windex_];
        if ( woffset_ + bs.size() - written <= avail(windex_) ) {
            memcpy(&fra->data[woffset_], &bs[written], bs.size() - written);
            size_ += bs.size() - written;
            updateOffset(windex_, woffset_, bs.size() - written);
            return *this;
        }

        int fill = avail(windex_) - woffset_;
        memcpy(&fra->data[woffset_], &bs[written], fill);
        updateOffset(windex_, woffset_, bs.size() - written);
        size_ += fill;
        written += fill;
    }
    return *this;
}


TError Buffer::dump(std::size_t size, std::byte *dest) {
    if (size > size_) return ERR_INSUFFICIENT_DATA;

    int read = 0;
    while (read < size)
    {
        if ( offset_ + size - read <= avail(index_)) {
            memcpy(dest, &fragments_[index_]->data[offset_], size - read);
            updateOffset(index_, offset_, size - read);
            size_ -= size - read;
            return OK;
        }

        int remain = avail(index_) - offset_;
        memcpy(dest, &fragments_[index_]->data[offset_], remain);
        size_ -= remain;
        updateOffset(index_, offset_, remain);
        read += remain;
    }
    return OK;
}


Buffer& Buffer::append(const std::byte bt)
{
    return this->append(&bt, 1);
}

Buffer& Buffer::append(const TType bt)
{
    return append(std::byte(bt));
}

int Buffer::dumpSize2()
{
    return dumpSize<std::int16_t>();
}

int Buffer::dumpSize4()
{
    return dumpSize<std::int32_t>();
}

