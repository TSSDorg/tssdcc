#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <memory>
#include <span>
#include <unordered_map>

#include "tssd.h"


class Buffer {
private:
    std::unordered_map<int, std::shared_ptr<Fragment>> fragments_;
    Schema schema_;
    Bytes heads_;
    int checksum_len_ = 0;

    std::size_t mtu_ = 3072;
    std::size_t size_ = 0;    //total size
    std::size_t index_ = 0;   // read index
    std::size_t offset_ = 0;  // read offset
    std::size_t windex_ = 0;  // write index
    std::size_t woffset_ = 0;  // write offset

    template<typename T>
    int appendSize(T n)
    {
        append((const std::byte *)&n, sizeof(T));
        return size_;
    }

    template<typename T>
    int dumpSize()
    {
        T size(0);
        if (auto ret = dump(sizeof(T), (std::byte *)&size))
            return ret;
        return size;
    }
public:
    Buffer(int MTU = 0) : mtu_(MTU) {}
    Buffer(VBytes bs) : size_(bs.size()) {
        fragments_[0] = std::make_shared<Fragment>(bs);
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

    //convert a vector ordered by the fragment id(last fragment is -n)
    std::vector<pFragment> Fragments()
    {
        std::vector<pFragment> result(fragments_.size());
        for (int i=0; i<fragments_.size(); i++) {
            result[i] = fragments_[i];
        }
        return std::move(result);
    }

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

    int appendSize2(const int size) {
        return appendSize<std::int16_t>(size);
    }

    int appendSize4(const int size) {
        return appendSize<std::int32_t>(size);
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

    void updateOffset(std::size_t &index, std::size_t &offset, int n) {
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

    void print(const std::string &prefix="", int n = 0) {
        if (!prefix.empty())
            std::cout << prefix;
        for (int i=0; i<fragments_.size(); ++i) {
            fragments_[i]->print( n ? n : fragments_[i]->data.size());
        }
        //fragments_[windex_]->print(woffset_);
    }


    // @desc push a fragment into buffer
    // return
    //  < 0:  Error, the fragment was rejected
    //  0 : push sucess, and Buffer is complete, all fragments arrive
    //  n(>0): missing n-th fragment
    int push(pFragment fragment);

    // @desc query the missing fragment
    // return
    //  0 : Buffer is complete, all fragments arrive
    //  n(>0): missing n-th fragment
    int wanted();
};

#endif
