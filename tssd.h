#ifndef __TSSD_H__
#define __TSSD_H__

#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>


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
    Tvector = 1000,
    Tlist,
    Tset,
    Tmap,
    Tunordered_map,
};

const char MINOR = 1;
const char MAJOR = 0;
const char TSSD_VERSION[2] = {MINOR, MAJOR};
const int  TSSD_BUFFER_MIN_MTU = 256;
const int  TSSD_BUFFER_MTU = 3072;
const int  TSSD_SIZET_LENGTH = 4;
const int  TSSD_SIZEA_LENGTH = 2;

using TError = std::int16_t;
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


struct Fragment {
    Header header;
    Schema schema;
    Bytes data;
    VBytes heads;
    VBytes payload;
    VBytes checksum;
    Fragment(std::size_t mtu=0) : data(mtu){}
    Fragment(VBytes bs) : payload(bs) {}

    TError Unmarshal(VBytes input, int &remain_pos);
    TError Validate(VBytes input) const;

    void print(int offset) {
        std::cout << "Fragment size:" << data.size() << '[';
        for (int i=0; i< offset; i++)
            std::cout << int(data[i]) << '\t';
        std::cout << ']' << std::endl;
    }
};
using pFragment = std::shared_ptr<Fragment>;

#endif
