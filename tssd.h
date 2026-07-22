#ifndef __TSSD_H__
#define __TSSD_H__

#include <cstdint>
#include <iostream>
#include <string>

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

//const char *MAGIC = "TSSDV";
const char MINOR = 1;
const char MAJOR = 0;
const char TSSD_VERSION[2] = {MINOR, MAJOR};

using TError = std::int16_t;
const TError ERR_SCHEMA_NOT_FOUND = -3;
const TError ERR_INSUFFICIENT_DATA = -2;
const TError ERR_FORMAT_ERROR = -1;
const TError OK = 0;

struct Header {
	char magic[4];
	std::int8_t version[2];  //TSSD version
};

struct Schema {
    std::int16_t fragment;    //fragment id: [1,2, ... -n]
    std::string  hash;
    std::string  tid;
    std::string  extent;
};

typedef std::vector<std::byte> Bytes;

struct Fragment {
    Header header;
    Schema schema;
    Bytes data;
    Bytes checksum;
    Fragment(std::size_t mtu) : data(mtu){
        //memcpy(header.magic, MAGIC, sizeof(header.magic));
        //header.version[0] = 1;
        //header.version[1] = 0;
        //std::cout << "Fragment data size: " << data.size() << ",cap:" << data.capacity() << std::endl;
        data.resize(data.capacity());
        std::cout << "Fragment data size2: " << data.size() << ",cap:" << data.capacity() << std::endl;
        //std::cout << std::addressof(this) << '\t' << std::addressof(&data[0]) << std::endl;
    }
    void print(int offset) {
		std::cout << "Fragment[";
		for (int i=0; i< offset; i++)
			std::cout << int(data[i]) << '\t';
		std::cout << ']' << std::endl;
	}
};


#endif
