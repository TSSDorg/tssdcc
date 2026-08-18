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

namespace tssd {

struct FlatInfo {
    std::string version;
    std::string progeny;
    Schema schema;
    const std::shared_ptr<TypeInfo> typeInfo;
};
using pFlatInfo = std::shared_ptr<FlatInfo>;

template<typename T>
class Cpeq {
    const std::shared_ptr<TypeInfo> typeInfo_;
public:
    Cpeq() : typeInfo_(TypeInfo::Create<T>()) {}
    void Copy(const T &src, T &dest) const
    {
        typeInfo_->Copy(src, dest);
    }
    bool Equal(const T &t1, const T &t2) const
    {
        return typeInfo_->Equal(t1, t2);
    }
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

    struct family {
        std::string current;
        std::map<std::string, pFlatInfo> versions;  //query by version;
    };

    struct VersionInfo {
        std::string family;
        std::string version;
    };

    static std::pair<std::map<std::string, family>, std::map<std::string, VersionInfo>> families;
    static std::shared_ptr<TypeInfo> schemaTypeInfo;
    static std::function<std::string(const void*, int)> hash;
    static std::function<std::string(const void*, int)> checksum;

public:
    static inline void print(const void *data, int size, const std::string &prefix="")
    {
        auto p = (const std::byte *)data;
        std::cout << prefix << '(' << size << ")[";
        for (int i=0; i<size; i++)
            std::cout <<(int)p[i] << ' ';
        std::cout << ']' << std::endl;
    }

    template<typename T>
    static void Register() {
        T flat;
        if (!families.first.contains(flat.Family())) {
            families.first[flat.Family()] = family {
                flat.Version(),
                std::map<std::string, pFlatInfo>()
            };
        }

        auto &family = families.first[flat.Family()];
        if (family.versions.contains(flat.Version()))
            return;

        pFlatInfo fi = std::make_shared<FlatInfo>(
            flat.Version(),
            flat.Progeny(),
            Schema{},
            TypeInfo::CreateT<T>());

        family.versions[flat.Version()] = fi;
        fi->schema = flat.Schema();
        families.second[fi->schema.Types] = VersionInfo {
            flat.Family(),
            flat.Version()
        };
    }

    static inline VersionInfo* TypesToVersionInfo(const std::string &types) {
        if (!families.second.contains(types)) return nullptr;
        return &families.second[types];
    }

    static TError MarshalTo(const Flatable& flat, Buffer &buf);
    static TError UnmarshalTo(Buffer &buf, Flatable& flat);
    static pFlatable Unmarshal(Buffer &buf);

    static TError Read(const Reader &reader, Flatable &flat);
    static TError Write(const Writer &writer, const Flatable &flat, const int mtu = TSSD_BUFFER_MTU);
};

}  //end namespace tssd
#endif
