#ifndef __FLAT_H__
#define __FLAT_H__
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <iostream>

#include "tssd.h"
#include "typeinfo.h"
#include "buffer.h"
#include "md5.h"

class Manager;
class FlatInfo;
class Flatable;
using pMgr = std::shared_ptr<Manager>;
using pFlatInfo = std::shared_ptr<FlatInfo>;
using pBuffer = std::shared_ptr<Buffer>;
using pFlatable = std::shared_ptr<Flatable>;

class Flatable {
public:
    virtual ~Flatable() = 0;
    //virtual pFlatable Build() const = 0;
    virtual Schema schema() const;
    virtual std::string Group() const = 0;
    virtual std::string Version() const = 0;
    virtual std::string TID() const;
    virtual std::string Progeny() const { return "";}
    virtual Flatable &Decorate(Flatable &other) { return *this;};
    Bytes Types() const;
};

struct FlatInfo {
    std::string version;
    //std::string hash;
    std::string progeny;
    Schema schema;
    const std::shared_ptr<TypeInfo> typeInfo;
};


class Manager {
    friend class Flatable;
    friend struct Schema;
    friend struct Fragment;
    friend class Buffer;
    static std::string hash6(const void *data, int size)
    {
        const int LEN = 6;
        std::string str = md5(data, size);
        std::cout << "hash6 result:" << str << std::endl;
        return str.substr(0, LEN) + str.substr(str.length()-LEN, LEN);
    }
    static inline void print(const void *data, int size, const std::string &prefix="")
    {
        auto p = (const std::byte *)data;
        std::cout << prefix << '(' << size << ")[";
        for (int i=0; i<size; i++)
            std::cout <<(int)p[i] << ' ';
        std::cout << ']' << std::endl;
    }
    struct group {
        std::string current;
        std::map<std::string, pFlatInfo> versions;  //query by version;
        std::map<std::string, pFlatInfo> hashes;  //query by schema's hash;
    };

    static std::map<std::string, group> groups;
    static std::shared_ptr<TypeInfo> schemaTypeInfo;
    static std::function<std::string(const void*, int)> hash;
    static std::function<std::string(const void*, int)> checksum;
    static constexpr std::string MAGIC = "TSSDV";
public:
    static int findMagic(VBytes bs) {
        auto view = std::string_view(reinterpret_cast<const char*>(bs.data()), bs.size());
        auto pos = view.find(MAGIC);
        return pos == view.npos ? -1 : pos;
    }

    template<typename T>
    static void Register() {
        T flat;
        if (!groups.contains(flat.Group())) {
            groups[flat.Group()] = group {
                flat.Version(),
                std::map<std::string, pFlatInfo>(),
                std::map<std::string, pFlatInfo>()
            };
        }

        auto &group = groups[flat.Group()];
        if (group.versions.contains(flat.Version()))
            return;

        pFlatInfo fi = std::make_shared<FlatInfo>(
            flat.Version(),
            flat.Progeny(),
            Schema{},
            TypeInfo::Create<T>());

        group.versions[flat.Version()] = fi;
        fi->schema = flat.schema();
        group.hashes[fi->schema.hash] = fi;
    }

    static TError MarshalTo(const Flatable& flat, Buffer &buf);
    static TError UnmarshalTo(Buffer &buf, Flatable& flat);
};

#endif
