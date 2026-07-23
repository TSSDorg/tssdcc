#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <memory>
#include <span>
#include <vector>

#include "tssd.h"


struct Buffer {

    std::vector<std::shared_ptr<Fragment>> fragments_;
    Schema schema_;
    Bytes heads_;
    int checksum_len_ = 0;

    int mtu_ = 3072;
    int size_ = 0;    //total size
    int index_ = 0;   // read index
    int offset_ = 0;  // read offset
    int windex_ = 0;  // write index
    int woffset_ = 0;  // write offset

    Buffer(int MTU = 0) : mtu_(MTU) {}
    Buffer(VBytes bs) : size_(bs.size()) {
        fragments_.emplace_back(std::make_shared<Fragment>(bs));
    }

    Buffer& clear() {
        size_ = index_ = offset_ = 0;
        windex_ = woffset_ = 0;
        heads_.clear();
        checksum_len_ = 0;
        fragments_.clear();
        return *this;
    }

    TError prepare(Schema schema);
    void finish();

    Buffer& append(const std::span<std::byte> bs);
    Buffer& append(const std::byte bt);
    Buffer& append(const TType bt);

    Buffer& append(const Bytes &bs) {
        return append(bs.data(), bs.size());
    }

    Buffer& append(const std::byte* ptr, const std::size_t size) {
        auto span = std::span<std::byte>((std::byte*)ptr, size);
        return append(span);
    }

    TError dump(std::size_t size, std::byte *dest);
    std::byte *dump(std::size_t size);

    int size() { return size_; }

    template<typename T>
    int appendSize(T n)
    {
        //int pos = size_;
        //Bytes bs(sizeof(T));
        //memcpy(&bs[0], &n, sizeof(T));
        //append(bs);
        append((const std::byte *)&n, sizeof(T));
        return size_;
    }

    int appendSize2(const int size) {
        return appendSize<std::int16_t>(size);
    }

    int appendSize4(const int size) {
        return appendSize<std::int32_t>(size);
    }


    template<typename T>
    int dumpSize()
    {
        T size(0);
        if (auto ret = dump(sizeof(T), (std::byte *)&size))
            return ret;
        return size;
    }
    int dumpSize2();
    int dumpSize4();

    void updateFragmentID(int index, int n);
    void appendChecksum(int index, int pos);

    void updateSize(const int index, const int offset, const int size) {
        // back current write pos
        int windex_bak = windex_;
        int woffset_bak = woffset_;
        int size_bak = size_;
        // seek to the pos, append the size
        windex_ = index;
        woffset_ = offset;
        appendSize4(size);
        // recover the pos and size
        windex_ = windex_bak;
        woffset_ = woffset_bak;
        size_ = size_bak;
    }

    void ftell(int &index, int &offset) const {
        index = windex_;
        offset = woffset_;
    }

    void updateOffset(int &index, int &offset, int n) {
        offset += n;
        if (offset >= avail(index_)) {
            offset -= avail(index_);
            index ++;
        }
    }

    int avail(int index) {
        //std::cout << "avail index:" << index << ", size:" <<  fragments_[index]->data.size() << std::endl;
        //std::cout << std::addressof(&fragments_[index]) << '\t' << std::addressof(&fragments_[index][0]) << std::endl;
        return fragments_[index]->data.size() - heads_.size() - checksum_len_;
    }


    void print(const std::string &prefix="", int n = 0) const {
        if (!prefix.empty())
            std::cout << prefix;
        for (int i=0; i<fragments_.size(); ++i)
            fragments_[i]->print( n ? n : fragments_[i]->data.size());
        //fragments_[windex_]->print(woffset_);
    }
};

#endif
