#ifndef __TSSD_H__
#define __TSSD_H__

#include <cstdint>

enum class TType : std::int8_t {
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
};

enum class TTypeLocal : int {
    Tvector = 1000,
    Tmap,
    Tunordered_map,
};

enum class TError {
    T_FORMAT_ERROR = -1,
    T_OK = 0,
    T_INSUFFICIENT_DATA = 1,
};


#endif