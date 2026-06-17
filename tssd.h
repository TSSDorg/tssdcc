#ifndef __TSSD_H__
#define __TSSD_H__

#include <cstdint>

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
    Tarray,
    Tarraym, //merged array, elements including 1 simple fixed length data only
    Tobject, //struct
    Tdict,   //map, pairs of (key, value)
    Tdictk,   //key of a map node
    Tdictv,   //value of a map node
    Traw,    //raw binary data
    Tschema = 83, //'S' schema meta data string
    Theader   = 84, //'T' tssd header
    Tversion= 86, //'V' tssd format version
    Tuser = 127, //user define data

    //local type 
    Tvector = 1000,
    Tmap,
    Tunordered_map,
};

using TError = std::int16_t;
const TError ERR_INSUFFICIENT_DATA = -2;
const TError ERR_FORMAT_ERROR = -1;
const TError OK = 0;



#endif