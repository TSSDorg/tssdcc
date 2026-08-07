#ifndef __TSSD_H__
#define __TSSD_H__

#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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
const std::size_t TSSD_FRAGMENT_MIN_HEADER_SIZE = 64;
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
struct Schema {
    std::int16_t fragment;    //fragment id: [1,2, ... -n]
    std::string  hash;
    std::string  tid;
    std::string  extent;

    TError Marshal(Buffer &buf);
    TError Unmarshal(Buffer &buf);
};


class Fragment {
    friend class Buffer;
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

    TError Unmarshal(VBytes input, std::size_t &remain_pos, std::size_t &more);
    TError Unmarshal(Bytes input, std::size_t &remain_pos, std::size_t &more) {
        auto sp = std::span<std::byte>(input.data(), input.size());
        return Unmarshal(sp, remain_pos, more);
    }
    TError Read(int fd);
    TError Validate(VBytes input, VBytes checksum) const;

    void print(int offset) {
        std::cout << "Fragment size:" << data.size() << '[';
        for (int i=0; i< offset; i++)
            std::cout << int(data[i]) << '\t';
        std::cout << ']' << std::endl;
    }
};
using pFragment = std::shared_ptr<Fragment>;

} //end namespace tssd

#endif
