#ifndef __FLAT_H__
#define __FLAT_H__
#include <memory>
#include <string>
#include <vector>
#include <map>

#include "tssd.h"
#include "typeinfo.h"
#include "buffer.h"

class Manager;
class FlatInfo;
using pMgr = std::shared_ptr<Manager>;
using pFlatInfo = std::shared_ptr<FlatInfo>;
using pBuffer = std::shared_ptr<Buffer>;



class Flatable {
public:
    virtual ~Flatable() = 0;
    virtual Flatable* Build() const = 0;
    virtual Schema schema() const;
    virtual std::string Group() const = 0;
    virtual std::string Version() const = 0;
    virtual std::string TID() const;
    virtual std::string Hash(const Bytes &in) const;
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
    struct group {
        std::string current;
        std::map<std::string, pFlatInfo> versions;  //query by version;
        std::map<std::string, pFlatInfo> hashes;  //query by schema's hash;
    };

     static std::map<std::string, group> groups;
public:
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
