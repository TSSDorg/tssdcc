#include "flat.h"
#include "tssd.h"
#include "buffer.h"
namespace tssd {

std::pair<std::map<std::string, Manager::family>, std::map<std::string, Manager::VersionInfo>> Manager::families;
std::shared_ptr<TypeInfo> Manager::schemaTypeInfo = TypeInfo::Create<Schema>();
std::function<std::string(const void*, int)> Manager::hash = Manager::hash6;
std::function<std::string(const void*, int)> Manager::checksum = Manager::hash6;
Flatable::~Flatable() {}
Reader::~Reader() {}
Writer::~Writer() {}

std::string Flatable::TID() const
{
    return "";
}

Schema Flatable::Schema() const
{
    auto bs = this->Types();

    Manager::print(&bs[0], bs.size(), Version());
    auto ret = Manager::hash(bs.data(), bs.size());
    std::cout << "hash value:" << ret << std::endl;
    struct Schema s{
        -1,           // FID
        Manager::hash(bs.data(), bs.size()),
        this->TID(),  // TID
        this->Info()};
    return s;
}

std::vector<std::byte> Flatable::Types() const
{
    return Manager::families.first[this->Family()].versions[this->Version()]->typeInfo->Types();
}

TError Manager::MarshalTo(const Flatable &flat, Buffer &buf)
{
    if (!families.first.contains(flat.Family())) return ERR_SCHEMA_NOT_FOUND;

    auto &family = families.first[flat.Family()];
    if (!families.first[flat.Family()].versions.contains(flat.Version())) return ERR_SCHEMA_NOT_FOUND;

    buf.Prepare(flat.Schema());
    if (auto ret = family.versions[flat.Version()]->typeInfo->MarshalTo(&flat, buf)) {
        return ret;
    }

    buf.Finish();
    return OK;
}

TError Manager::decorate(const pFlatable from, Flatable &to)
{
    auto &family = families.first[to.Family()];
    auto it = from;
    for (auto ver = it->Progeny(); !ver.empty() && family.versions.contains(ver); )
    {
        if (to.Version() == ver)
            return to.Decorate(it);
        auto flat = family.versions[ver]->typeInfo->Build();
        if (auto ret = flat->Decorate(it))
            return ret;
        it = flat;
        ver = it->Progeny();
    }
    return ERR_SCHEMA_NOT_MATCH;
}

TError Manager::UnmarshalTo(Buffer &buf, Flatable &flat)
{
    if (!families.first.contains(flat.Family())) return ERR_SCHEMA_NOT_FOUND;

    auto &family = families.first[flat.Family()];
    if (!family.versions.contains(flat.Version())) return ERR_SCHEMA_NOT_FOUND;

    auto vi = TypesToVersionInfo(buf.Schema().Types);
    if ( !vi || (vi->family != flat.Family())) return ERR_SCHEMA_NOT_MATCH;

    // match exactly
    if (vi->version == flat.Version())
        return family.versions[flat.Version()]->typeInfo->UnmarshalTo(buf, &flat);

    auto remote = unmarshal(buf);
    if (!remote) return ERR_SCHEMA_NOT_MATCH;

    return decorate(remote, flat);
}

// unmarshal to the original version
pFlatable Manager::unmarshal(Buffer &buf)
{
    auto vi = TypesToVersionInfo(buf.Schema().Types);
    if (!vi) return nullptr;

    auto flat = families.first[vi->family].versions[vi->version]->typeInfo->Build();

    if (UnmarshalTo(buf, *flat)) return nullptr;
    return flat;
}

// Unmarshal to current version
pFlatable Manager::Unmarshal(Buffer &buf)
{
    auto remote = unmarshal(buf);
    if (!remote) return nullptr;

    auto family = families.first[remote->Family()];
    if (family.current.empty() || remote->Version() == family.current)
        return remote;

    auto flat = family.versions[family.current]->typeInfo->Build();
    return decorate(remote, *flat) ? nullptr : flat;
}


TError Manager::Read(const Reader &reader, Flatable &flat)
{
    tssd::RBuffer rbuf;  // RBuffer to process raw data buffer
    if (auto ret = rbuf.Feed(reader)) {
        return ret;
    }
    if (!rbuf.Ready(flat.Family(), flat.Version())) {
        return ERR_SCHEMA_NOT_MATCH;
    }
    auto dbuf = rbuf.Buffer(flat.Family(), flat.Version());

    return tssd::Manager::UnmarshalTo(*dbuf, flat);
}

TError Manager::Write(const Writer &writer, const Flatable &flat, const int mtu)
{
     tssd::Buffer buf(mtu);
    if (auto ret = tssd::Manager::MarshalTo(flat, buf)) {
        return ret;
    }

    auto frags  = buf.Fragments();
    for (std::size_t i=0; i<frags.size(); i++) {
        int n = writer.Write(frags[i]->data.data(), frags[i]->data.size());
        if (n<=0) return ERR_IO;
    }
    return OK;
}

} //end namespace tssd
