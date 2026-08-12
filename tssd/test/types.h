#ifndef __TSSD_TEST_TYPES_H__
#define __TSSD_TEST_TYPES_H__

//////////////this file is define struct/class for test///////////////////////
#include<string>
#include<map>
#include<vector>
#include<unordered_map>
#include<memory>
#include<list>
#include<set>
#include <iostream>

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
    std::string Family() const override {
        return "BasicFlatFamily";
    }
    std::string Version() const override {
        return "BasicFlat-V1";
    }
};

struct BasicArrayFlat : public tssd::Flatable {
    BasicArray basicArray;
    std::string Family() const override {
        return "BasicArrayFlatFamily";
    }

    std::string Version() const override {
        return "BasicArrayFlat-V1";
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
    std::vector<T> vec;
    std::list<T> lst;
    std::set<T, Compare> st;
    std::map<std::string, T> mp;
    std::unordered_map<std::string, T> ump;
    std::shared_ptr<T> sp;
    std::unique_ptr<T> up;
};

template<typename T>
struct Struct1Flat : public tssd::Flatable {
    Struct1<T> struct1;
    //std::string type_;
    //Struct1Flat(T v, const string type="") : struct1.v1(v), type_(type){}
    std::string Family() const override {
        return "Struct1FlatFamily";
    }
    std::string Version() const override {
        return "Struct1Flat-V1";
    }
};

struct Course {
    std::string title;
    std::string teacher;
    float  score;
};

struct Contact {
    std::string name;
    std::string relation;
    std::string phone;
    std::string address;
};

struct Paper {
    std::string title;
    std::string tags[3];
    std::string content;
};

class Student : public tssd::Flatable {
    std::uint64_t ID;
public:
    std::string   name;
    std::int16_t  age;
    bool     IsMale;

    std::vector<Contact> contacts;
    std::map<std::string, Course> courses;
    std::list<Paper> papers;

    Student(std::uint16_t id=0) : ID(id) {}
    void print() {
        std::cout << "Student ID" << ID << ", name:" << name << std::endl;
        for (const auto &it : contacts) {
             std::cout << "contact name:" << it.name << ", address:" << it.address << std::endl;
        }
        for (const auto& [key, value] : courses) {
             std::cout << "course title:" << value.title << ", teacher:" << value.teacher << std::endl;
        }
        for (const auto &it : papers) {
             std::cout << "paper title:" << it.title << ", tags:" << it.tags[0] << std::endl;
        }
    }

    std::string Family() const override {
        return "StudentFamily";
    }
    std::string Version() const override {
        return "StudentV1";
    }
};

// request to  server to get a specify(fid, types, tid) Fragment
class Request : public tssd::Flatable {
public:
    std::int16_t fid;
    std::string types;
    std::string tid;
    std::string Family() const override {
        return "RequestFamily";
    }
    std::string Version() const override {
        return "RequestV1";
    }
};

class SocketReader : public tssd::Reader {
    int sockfd_;
    int flags_;
public:
    SocketReader(int sockfd, int flags=0) : sockfd_(sockfd), flags_(flags) {}
    int Read(void *dest, std::size_t numb) const override {
        auto n = recv(sockfd_, dest, numb, flags_);
        std::cout << " recv " << n << " bytes" << std::endl;
        return n;
    }
};

class SocketWriter : public tssd::Writer {
    int sockfd_;
    int flags_;
public:
    SocketWriter(int sockfd, int flags=0) : sockfd_(sockfd), flags_(flags) {}
    int Write(void *data, std::size_t numb) const override {
        auto n = send(sockfd_, data, numb, flags_);
        std::cout << " send " << n << " bytes" << std::endl;
        return n;
    }
};

#endif // __TSSD_TEST_TYPES_H__
