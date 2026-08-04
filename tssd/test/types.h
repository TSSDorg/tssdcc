#ifndef __TSSD_TEST_TYPES_H__
#define __TSSD_TEST_TYPES_H__

#include "flat.h"

struct BasicType {
    bool  vbool;
    std::int8_t vint8;
    std::uint8_t vuint8;
    std::int16_t vint16;
    std::uint16_t vuint16;
    std::int32_t  vint32;
    std::uint32_t vuint32;
    std::int64_t  vint64;
    std::uint64_t vuint64;
    float        f32;
    double       f64;
};

struct BasicArray {
    bool            vbool[2];
    std::int8_t     vint8[2];
    std::uint8_t   vuint8[3];
    std::int16_t   vint16[4];
    std::uint16_t vuint16[5];
    std::int32_t   vint32[6];
    std::uint32_t vuint32[7];
    std::int64_t   vint64[8];
    std::uint64_t vuint64[9];
    float             f32[3];
    double            f64[2];
    bool            vbool1[1];
    std::int8_t     vint81[1];
    std::uint8_t   vuint81[1];
    std::int16_t   vint161[1];
    std::uint16_t vuint161[1];
    std::int32_t   vint321[1];
    std::uint32_t vuint321[1];
    std::int64_t   vint641[1];
    std::uint64_t vuint641[1];
    float             f321[1];
    double            f641[1];
};

struct BasicTypeFlat : public tssd::Flatable {
    BasicType basicType;
    std::string Group() const override {
        return "BasicFlat";
    }
    std::string Version() const override {
        return "BasicFlatGroup";
    }
};

struct BasicArrayFlat : public tssd::Flatable {
    BasicArray basicArray;
    std::string Group() const override {
        return "BasicArrayFlat";
    }

    std::string Version() const override {
        return "BasicArrayFlatGroup";
    }
};

template <typename T>
struct Struct1 {
    T v1;
};

template <typename T1, typename T2>
struct Struct2 {
    T1 v1;
    T2 v2;
};

template <typename T, std::size_t N>
struct Array1 {
    T v1[N];
};

template <typename T1, typename T2, std::size_t N>
struct Array2 {
    T1 v1[N];
    T2 v2[N];
};


template <typename T1, typename T2, typename T3>
struct Struct3 {
    T1 v1;
    T2 v2;
    T3 v3;
};

template<typename T>
struct LexCompare {
    bool operator()(const T& a, const T& b) const {
        return a < b;
    }
};

template<>
struct LexCompare<BasicArray> {
    bool operator()(const BasicArray& a, const BasicArray& b) const {
        return a.vint32[0] < b.vint32[0];
    }
};

template<>
struct LexCompare<BasicType> {
    bool operator()(const BasicType& a, const BasicType& b) const {
        return a.vint32 < b.vint32;
    }
};

template<typename T, typename Compare = LexCompare<T>>
struct ContainerT {
    vector<T> vec;
    list<T> lst;
    set<T, Compare> st;
    map<string, T> mp;
    unordered_map<string, T> ump;
    std::shared_ptr<T> sp;
};


#endif // __TSSD_TEST_TYPES_H__
