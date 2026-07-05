#ifndef __FLAT_H__
#define __FLAT_H__
#include <memory>
#include <string>
#include <vector>
#include <map>

#include "tssd.h"
#include "typeinfo.h"

class Flatable {
public:
    virtual ~Flatable() = 0;
    virtual Flatable* Build() = 0;
    virtual std::string Hash(std::vector<std::byte>) const;
    virtual std::vector<std::byte> Types() const;
    virtual CSchema Schema() const;
    virtual TError OnHeader (const Header &) const { return OK;}
    virtual std::string Group() const = 0;
    virtual std::string Version() const = 0;
    virtual std::string Progeny() const { return "";}
    virtual Flatable &Decorate(Flatable &other) { return *this;};
};

struct FlatInfo {
    std::string version;
    //std::string hash;
    std::string progeny;
    CSchema schema;
    const std::shared_ptr<TypeInfo> typeInfo;
};

class Manager;
using spMgr = std::shared_ptr<Manager>;
using spFlatInfo = std::shared_ptr<FlatInfo>;

class Manager {
    friend class Flatable;
    struct group {
        std::string current;
        std::map<std::string, spFlatInfo> versions;  //query by version;
        std::map<std::string, spFlatInfo> hashes;  //query by schema's hash;
    };

     static std::map<std::string, group> groups;
public:
    template<typename T>
    static void Register() {
        T flat;
        if (!groups.contains(flat.Group())) {
            groups[flat.Group()] = group {
                flat.Version(),
                std::map<std::string, spFlatInfo>(),
                std::map<std::string, spFlatInfo>()
            };
        }

        auto &group = groups[flat.Group()];
        if (group.versions.contains(flat.Version())) 
            return;

        spFlatInfo fi = std::make_shared<FlatInfo>(
            flat.Version(),
            flat.Progeny(),
            CSchema{},
            Create<T>());

        group.versions[flat.Version()] = fi;
        fi->schema = flat.Schema();
        group.hashes[fi->schema.hash] = fi;
    }

    static TError MarshalTo(const Flatable& flat, TBuffer &buf);
    static TError UnmarshalTo(TBuffer &buf, Flatable& flat);
};

#endif