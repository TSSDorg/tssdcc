#ifndef __TSSD_H__
#define __TSSD_H__

#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

namespace tssd {

enum class TType {
    //TSSD type 1 byte only
    Tbool = 11,
    Tint8,
    Tuint8,
    Tint16,
    Tuint16,
    Tint32,
    Tuint32,
    Tint64,
    Tuint64,
    Tfloat32,
    Tfloat64,
    Tstring, //dynamic length data
    Ttime,   //RFC3339Nano string
    Tenum,   //enum in string format
    Tarray,
    Tarraym, //merged array, elements including 1 simple fixed length data only
    Tobject, //struct
    Tdict,   //map, pairs of (key, value)
    Tdictk,   //key of a map node
    Tdictv,   //value of a map node
    Traw,    //raw binary data
    Tschema = 77, //'M' schema meta data string
    Theader   = 84, //'T' tssd header
    Tversion= 86, //'V' tssd format version
    Tuser = 127, //user define data

    //local type
    Tunknown = 1000,
    Tvector,
    Tlist,
    Tset,
    Tmap,
    Tunordered_map,
    Tref,
    Tshared_ptr,
    Tunique_ptr,
};

const char MINOR = 1;
const char MAJOR = 0;
const char TSSD_VERSION[2] = {MINOR, MAJOR};
const std::size_t TSSD_FRAGMENT_MIN_HEADER_SIZE = 41;
const std::size_t TSSD_BUFFER_MIN_MTU = 256;
const std::size_t TSSD_BUFFER_MTU = 2048;
const std::size_t TSSD_TARRAYM_HEAD_LENGTH      = 8;  // [Tarraym][Tuint8][sizet/4B][sizea/2B]
const std::size_t TSSD_SIZET_LENGTH = 4;
const std::size_t TSSD_SIZEA_LENGTH = 2;

using TError = std::int16_t;
const TError ERR_IO = -6;
const TError ERR_SCHEMA_NOT_MATCH = -3;
const TError ERR_CHECKSUM_FAILURE = -5;
const TError ERR_TSSD_MTU_TOO_SMALL = -4;
const TError ERR_SCHEMA_NOT_FOUND = -3;
const TError ERR_INSUFFICIENT_DATA = -2;
const TError ERR_FORMAT_ERROR = -1;

const TError OK = 0;

using Bytes = std::vector<std::byte>;
using VBytes = std::span<std::byte>;

struct Header {
    char magic[5];
    std::int8_t version[2];  //TSSD version
};

class Buffer;
class FBuffer;
struct Schema {
    std::int16_t FID;    //fragment id: [1,2, ... -n]
    std::string  TID;
    std::string  Types;
    std::string  Info;

    TError Marshal(Buffer &buf);
    TError Unmarshal(Buffer &buf);
};


class Fragment {
    friend class Buffer;
    friend class FBuffer;
private:
    VBytes heads;    // header's byte stream, including payload's Tarraym header
    VBytes payload;  // user payload, excluding itself Tarraym header
    VBytes checksum; // checksum, including itself Tarraym header
public:
    Header header;
    Schema schema;
    Bytes data;
    Fragment(std::size_t mtu=0) : data(mtu){}
    Fragment(VBytes bs) : payload(bs) {}

    // return heads excluding payload's Tarraym header
    VBytes Heads() const {
        return heads.subspan(0, heads.size() - TSSD_TARRAYM_HEAD_LENGTH);
    }

    VBytes Payload() const { return payload; }
    VBytes Checksum() const { return checksum.subspan(TSSD_TARRAYM_HEAD_LENGTH); }

    inline TError Validate() {
        return Validate(VBytes(&this->data[0], heads.size() + payload.size()), Checksum());
    }
    static TError Validate(VBytes input, VBytes checksum);

    void print(int offset) {
        std::cout << "Fragment size:" << data.size() << '[';
        for (int i=0; i< offset; i++)
            std::cout << int(data[i]) << '\t';
        std::cout << ']' << std::endl;
    }
};
using pFragment = std::shared_ptr<Fragment>;
using pBuffer = std::shared_ptr<Buffer>;

class Reader {
public:
    virtual ~Reader() = 0;
    virtual int Read(void *dest, std::size_t numb) const = 0;
};

class FBuffer {
private:
    std::shared_ptr<Bytes>  buffer_;
    Header header;
    Schema schema;
    int magic_  = -1;     //magic_pos, heads begin
    int heads_len_ =  -1;  // heads len
    int payload_ = -1;    // payload begin pos
    int payload_len_ = -1;
    int checksum_ = -1;   // checksum begin
    int checksum_len_ = -1;
    using HBuffers = std::unordered_map<std::string, pBuffer>;  // (version, pBuffer)
    std::unordered_map<std::string, HBuffers> results_;  //(family, buffers)
    HBuffers unregistered_;

    inline void append(const Bytes &data)
    {
        append(data, data.size());
    }
    inline void append(const Bytes &data, const std::size_t nsize)
    {
        append(data.data(), nsize);
    }
    void append(const std::byte *data, const std::size_t nsize);

    inline int findMagic(const Bytes &data, const std::size_t skip=0) {
        auto view = std::string_view(reinterpret_cast<const char*>(&data[skip]), data.size()-skip);
        auto pos = view.find(MAGIC);
        return pos == view.npos ? -1 : pos;
    }

    // reset doesn't clear data, clear does.
    inline void reset()
    {
        //clear all status
        magic_ = heads_len_ = payload_ = payload_len_ = checksum_ = checksum_len_ = -1;
        //frag_.reset();
    }

    //4 step to parse Fragment
    // 0. detectMagic, set magic_
    // 1. parseHeads,  set heads_len_
    // 2. parsePayload, set payload_ and payload_len_
    // 3. parseChecksum, set checksum_ and checksum_len;
    // if meet fmt error, we need goto step 0
    TError detectMagic(const Bytes &data, std::size_t &more, const std::size_t skip = 0);
    TError dumpMergeArrayHeader(const std::size_t pos, int &len, std::size_t &more);
    TError parseHeads(std::size_t &more);
    TError parsePayload(std::size_t &more);
    TError parseChecksum(std::size_t more);
    void moveFront(const std::size_t pos, const int n);

public:
    static constexpr std::string MAGIC = "TSSDV";
    FBuffer() : buffer_(std::make_shared<Bytes>(TSSD_BUFFER_MTU)) {
        buffer_->resize(0);
    }
    inline Bytes Data() const { return *buffer_; }
    inline void Clear() { buffer_->resize(0); reset(); }
    inline std::size_t Size() const { return buffer_->size(); }
    inline std::byte &operator[](const std::size_t pos) {
        return (*buffer_)[pos];
    }
    bool Ready(const std::string &family, const std::string &version);
    TError Feed(const Bytes &data, std::size_t &more);
    pFragment Fragment();

    // Feed got OK, then we can call it to get a Fragment;
    // pFragment Fragment(const Flatable *flat) const;
    pBuffer Buffer(const std::string &family, const std::string &version) {
        if (!Ready(family, version)) return nullptr;
        return results_[family][version];
    }

    TError Feed(const Reader &reader);
};


} //end namespace tssd

#endif
