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
        this->TID(),  // TID
        Manager::hash(bs.data(), bs.size()),
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

TError Manager::UnmarshalTo(Buffer &buf, Flatable &flat)
{
    if (!families.first.contains(flat.Family())) return ERR_SCHEMA_NOT_FOUND;

    auto &family = families.first[flat.Family()];
    if (!family.versions.contains(flat.Version())) return ERR_SCHEMA_NOT_FOUND;

    return family.versions[flat.Version()]->typeInfo->UnmarshalTo(buf, &flat);
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
