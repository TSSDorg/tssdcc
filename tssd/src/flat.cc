#include "flat.h"
#include "tssd.h"
#include "buffer.h"
namespace tssd {

std::map<std::string, Manager::family> Manager::families;
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
        -1,           // FID
        this->TID(),  // TID
        Manager::hash(bs.data(), bs.size()),
        this->Family(),
        this->Info()};
    return s;
}

std::vector<std::byte> Flatable::Types() const
{
    return Manager::families[this->Family()].versions[this->Version()]->typeInfo->Types();
}

TError Manager::MarshalTo(const Flatable &flat, Buffer &buf)
{
    if (!families.contains(flat.Family())) return ERR_SCHEMA_NOT_FOUND;

    auto &family = families[flat.Family()];
    if (!families[flat.Family()].versions.contains(flat.Version())) return ERR_SCHEMA_NOT_FOUND;

    buf.Prepare(flat.Schema());
    if (auto ret = family.versions[flat.Version()]->typeInfo->MarshalTo(&flat, buf)) {
        return ret;
    }

    buf.Finish();
    return OK;
}

TError Manager::UnmarshalTo(Buffer &buf, Flatable &flat)
{
    if (!families.contains(flat.Family())) return ERR_SCHEMA_NOT_FOUND;

    auto &family = families[flat.Family()];
    if (!family.versions.contains(flat.Version())) return ERR_SCHEMA_NOT_FOUND;

    //TODO dump TSSD header

    return family.versions[flat.Version()]->typeInfo->UnmarshalTo(buf, &flat);
}

} //end namespace tssd
