#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <memory>
#include <span>
#include <unordered_map>

#include "tssd.h"

namespace tssd {

class Buffer {
private:
    std::unordered_map<std::size_t, std::shared_ptr<Fragment>> fragments_;
    tssd::Schema schema_;
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
        Append((const std::byte *)&n, sizeof(T));
        return size_;
    }

    template<typename T>
    int dumpSize()
    {
        T size(0);
        if (auto ret = Dump(sizeof(T), (std::byte *)&size))
            return ret;
        return size;
    }

    void updateFragmentID(int index, int n);
    void appendChecksum(int index, int pos);

    inline std::size_t avail(std::size_t index) {
        //std::cout << "avail index:" << index << ", size:" <<  fragments_[index]->data.size() << std::endl;
        //std::cout << std::addressof(&fragments_[index]) << '\t' << std::addressof(&fragments_[index][0]) << std::endl;
        return fragments_[index]->data.size() - checksum_len_;
    }

public:
    Buffer(size_t MTU = 0) : mtu_(std::max(MTU, TSSD_BUFFER_MIN_MTU)) {}
    Buffer(VBytes bs) : size_(bs.size()) {
        fragments_[0] = std::make_shared<Fragment>(bs);
    }

    inline Buffer& Clear() {
        size_ = index_ = offset_ = 0;
        windex_ = woffset_ = 0;
        heads_.clear();
        checksum_len_ = 0;
        fragments_.clear();
        schema_.Types.clear();
        schema_.TID.clear();
        schema_.FID = 0;
        return *this;
    }

    TError Prepare(Schema schema);
    void Finish();
    //get Schema info
    tssd::Schema Schema() {
        return schema_;
    }

    //convert to a vector ordered by the fragment id(last fragment is -n)
    inline std::vector<pFragment> Fragments()
    {
        std::vector<pFragment> result(fragments_.size());
        for (std::size_t i=0; i<fragments_.size(); i++) {
            result[i] = fragments_[i];
        }
        // g++ does't like the move
        // return std::move(result);
        return result;
    }

    Buffer& Append(const std::span<std::byte> bs);
    Buffer& Append(const std::byte bt);
    Buffer& Append(const TType bt);

    inline Buffer& Append(const Bytes &bs) {
        return Append(bs.data(), bs.size());
    }

    inline Buffer& Append(const void* ptr, const std::size_t size) {
        return Append((const std::byte*)ptr, size);
    }

    inline Buffer& Append(const std::byte* ptr, const std::size_t size) {
        auto span = std::span<std::byte>((std::byte*)ptr, size);
        return Append(span);
    }

    TError Dump(std::size_t size, std::byte *dest);
    std::byte *Dump(std::size_t size);

    inline TError PeekByte(std::byte &bt) {
        if (size_ == 0) return ERR_INSUFFICIENT_DATA;

        bt = fragments_[index_]->payload[offset_];
        return OK;
    }


    inline std::size_t Size() { return size_; }

    inline int AppendSize2(const int size) {
        return appendSize<std::int16_t>(size);
    }

    inline int AppendSize4(const int size) {
        return appendSize<std::int32_t>(size);
    }

    int DumpSize2();
    int DumpSize4();

    inline void UpdateSize(const int index, const int offset, const int size) {
        // back current write pos
        int windex_bak = windex_;
        int woffset_bak = woffset_;
        int size_bak = size_;
        // seek to the pos, append the size
        windex_ = index;
        woffset_ = offset;
        AppendSize4(size);
        // recover the pos and size
        windex_ = windex_bak;
        woffset_ = woffset_bak;
        size_ = size_bak;
    }

    inline void Ftell(int &index, int &offset) const {
        index = windex_;
        offset = woffset_;
    }

    inline Buffer &Rewind() {
        index_ = offset_ = size_ = 0;
        for (std::size_t i=0; i<fragments_.size(); ++i)
            size_ += fragments_[i]->payload.size();
        return *this;
    }

    inline void print(const std::string &prefix="", int n = 0) {
        if (!prefix.empty())
            std::cout << prefix << " size:" << fragments_.size() << std::endl;
        for (std::size_t i=0; i<fragments_.size(); ++i) {
            fragments_[i]->print( n ? n : fragments_[i]->data.size());
        }
        //fragments_[windex_]->print(woffset_);
    }

    // @desc push a fragment into buffer
    // return
    //  < 0:  Error, the fragment was rejected
    //  0 : push sucess, and Buffer is complete, all fragments arrive
    //  n(>0): missing n-th fragment
    int Push(pFragment fragment);

    // @desc query the missing fragment
    // return
    //  0 : Buffer is complete, all fragments arrive
    //  n(>0): missing n-th fragment
    std::size_t Wanted();

    // merge all fragments into one, useful for storage
    void Merge();

    // split fragments with specify mtu
    void Split(std::size_t mtu);
/*
    TError ReadFragments(int fd) {
        while (Wanted()) {
            pFragment frag = std::make_shared<Fragment>();
            if (auto ret = frag->Read(fd)) return ret;
            auto ret = Push(frag);
            if (ret <= 0) return ret;
        }
        return OK;
    }*/
};

}  //end of namespace tssd
#endif
