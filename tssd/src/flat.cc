#include "flat.h"
#include "tssd.h"
#include "buffer.h"
namespace tssd {

std::map<std::string, Manager::group> Manager::groups;
std::shared_ptr<TypeInfo> Manager::schemaTypeInfo = TypeInfo::Create<Schema>();
std::function<std::string(const void*, int)> Manager::hash = Manager::hash6;
std::function<std::string(const void*, int)> Manager::checksum = Manager::hash6;
Flatable::~Flatable() {}

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
        -1,
        //Manager::hash(bs.data(), bs.size()),
        ret,
        this->TID(),
        ""};
    return s;
}

std::vector<std::byte> Flatable::Types() const
{
    return Manager::groups[this->Group()].versions[this->Version()]->typeInfo->Types();
}

TError Manager::MarshalTo(const Flatable &flat, Buffer &buf)
{
    if (!groups.contains(flat.Group())) return ERR_SCHEMA_NOT_FOUND;

    auto &group = groups[flat.Group()];
    if (!groups[flat.Group()].versions.contains(flat.Version())) return ERR_SCHEMA_NOT_FOUND;

    buf.Prepare(flat.Schema());
    if (auto ret = group.versions[flat.Version()]->typeInfo->MarshalTo(&flat, buf)) {
        return ret;
    }

    buf.finish();
    return OK;
}

TError Manager::UnmarshalTo(Buffer &buf, Flatable &flat)
{
    if (!groups.contains(flat.Group())) return ERR_SCHEMA_NOT_FOUND;

    auto &group = groups[flat.Group()];
    if (!group.versions.contains(flat.Version())) return ERR_SCHEMA_NOT_FOUND;

    //TODO dump TSSD header

    return group.versions[flat.Version()]->typeInfo->UnmarshalTo(buf, &flat);
}

} //end namespace tssd
