#include "flat.h"

std::map<std::string, Manager::group> Manager::groups;

Flatable::~Flatable() {}

std::vector<std::byte> Flatable::Types() const
{
    return Manager::groups[this->Group()].versions[this->Version()]->typeInfo->Types();
}

std::string
Flatable::Hash(std::vector<std::byte> in) const 
{
    return "";
}

CSchema
Flatable::Schema() const 
{
    struct CSchema s{
        this->Hash(this->Types()),
        "",
        ""};
    return s;
}

TError Manager::MarshalTo(const Flatable &flat, TBuffer &buf)
{
    if (!groups.contains(flat.Group())) return ERR_SCHEMA_NOT_FOUND;

    auto &group = groups[flat.Group()];
    if (!group.versions.contains(flat.Version())) return ERR_SCHEMA_NOT_FOUND;

    //TODO: add TSSD header

    return group.versions[flat.Version()]->typeInfo->MarshalTo(&flat, buf);
}

TError Manager::UnmarshalTo(TBuffer &buf, Flatable &flat)
{
    if (!groups.contains(flat.Group())) return ERR_SCHEMA_NOT_FOUND;

    auto &group = groups[flat.Group()];
    if (!group.versions.contains(flat.Version())) return ERR_SCHEMA_NOT_FOUND;

    //TODO dump TSSD header

    return group.versions[flat.Version()]->typeInfo->UnmarshalTo(buf, &flat);
}