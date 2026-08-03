#ifndef __TYPEINFO_H__
#define __TYPEINFO_H__

#include <iostream>
#include <print>
#include <vector>
#include <span>
#include <cstring>
#include <meta>
#include <set>
#include <list>
#include <map>
#include <iterator>
#include <vector>
#include <unordered_map>

#include "tssd.h"
#include "buffer.h"

namespace tssd {

struct TypeInfo {

    struct Node {
        char const* type_ = nullptr;
        char const* name_ = nullptr;
        std::ptrdiff_t offset_ = 0;
        std::ptrdiff_t total_offset_ = 0;
        std::size_t size_ = 0;

        //TODO: support reflerence
        bool is_number_ = false;  //is_arithmetic_type
        bool is_float_ = false;
        bool is_signed_ = true;

        TType tssd_type_ = TType::Tobject;
        TType local_type_ = tssd_type_;
        std::shared_ptr<TypeInfo> parent_;
        std::shared_ptr<TypeInfo> root_;
        std::vector<std::byte> types_;
        constexpr Node(char const *type, char const *name, ptrdiff_t offset, std::size_t size, TType local_type)
            : name_(name), type_(type), offset_(offset), size_(size), local_type_(local_type) {}

        constexpr Node(char const *type, char const *name, ptrdiff_t offset, std::size_t size, bool is_number, bool is_float, bool is_signed)
            : name_(name), type_(type), offset_(offset), size_(size), is_number_(is_number), is_float_(is_float), is_signed_(is_signed) {}
        constexpr Node(TType type) : tssd_type_(type) {}
    };

    Node node_;
    std::vector<std::shared_ptr<TypeInfo>> children_;

    std::vector<std::byte> Types() const {
        return node_.root_->node_.types_;
    }

    constexpr TypeInfo(char const *type,
            char const *name,
            std::ptrdiff_t offset, std::size_t size, bool is_number=false, bool is_float=false, bool is_signed=false) :
            node_(type, name, offset, size, is_number, is_float, is_signed) {}

    constexpr TypeInfo(char const *type,
            char const *name,
            std::ptrdiff_t offset,
            std::size_t size,
            TType ttype,
            std::vector<std::shared_ptr<TypeInfo>> ch) :
            node_(type, name, offset, size, ttype),
            children_(ch) {}

    constexpr TypeInfo(TType type) : node_(type) {}

    inline TError CheckTType(Buffer &buf) const {
        return CheckTType(buf, (std::int8_t)node_.tssd_type_);
    }

    inline static TError CheckTType(Buffer &buf, std::int8_t type) {
        std::int8_t t(0);
        if (auto ret = buf.dump(sizeof(t), (std::byte*)&t))
            return ret;
        if ( t != type)
            return ERR_FORMAT_ERROR;

        return OK;
    }

    inline int CheckDumpTS(Buffer &buf) const {
        if (auto ret = CheckTType(buf))
            return ret;
        auto sizet = buf.dumpSize4();
        if (sizet < 0 || buf.size() < (std::size_t)sizet) {
            return ERR_INSUFFICIENT_DATA;
        }
        return sizet;
    }

    virtual TError save(const std::byte *src, Buffer &buf) const {
        buf.append(node_.tssd_type_);
        buf.append(src, node_.size_);
        return OK;
    }

    virtual TError dump(Buffer &buf, std::byte *dest) const {
        if (auto ret = CheckTType(buf))
            return ret;

        return buf.dump(node_.size_, dest);
    }

    virtual void copy(const std::byte *src, std::byte *dest) const {
        std::memcpy(dest, src, node_.size_);
    }

    virtual bool equal(const std::byte *pl, const std::byte *pr) const {
        return !std::memcmp(pl, pr, node_.size_);
    }

    //set tssd_type, total offset, save, dump by the reflect type
    void parse(std::shared_ptr<TypeInfo> parent);

    template <typename T>
    static constexpr std::shared_ptr<TypeInfo> parse(std::ptrdiff_t offset=0, const char *name = "");

    void MakeTypes();

    void UpdateMergedArray(std::shared_ptr<TypeInfo>);

public:
    constexpr TypeInfo(
        const char *type,
        const char *name,
        std::vector<std::shared_ptr<TypeInfo>> ch) :
        node_(type, name, 0, 0, TType::Tobject),
        children_(ch) {}

    void print() const {
        std::println("result type:{} name:{} offset:{} size:{} total_offset:{}", node_.type_, node_.name_?node_.name_:"annonymous", node_.offset_, node_.size_, node_.total_offset_);
        for (auto &it : children_)
            it->print();
    }

    template <typename T>
    static auto Create() {
        static_assert(std::meta::is_class_type(^^T));
        auto ti = TypeInfo::parse<T>();
        ti->node_.root_ = ti;
        ti->parse(ti);
        ti->MakeTypes();
        return ti;
    }

    TError MarshalTo(const void *obj, Buffer &buf) const {
        return save((const std::byte*)obj, buf);
    }

    TError UnmarshalTo(Buffer &buf, void *obj) const {
        return dump(buf, (std::byte *)obj);
    }

    template <typename T>
    void Copy(const T &src, T &dest) const {
        copy((const std::byte *)&src, (std::byte *)&dest);
    }

    template <typename T>
    bool Equal(const T &obj1, const T &obj2) const {
        return equal((const std::byte *)&obj1, (const std::byte *)&obj2);
    }
};

class stringOper : public TypeInfo {
public:
    using TypeInfo::TypeInfo;

    stringOper(TType type) : TypeInfo(type) {}
    TError save(const std::byte *src, Buffer &buf) const override;
    TError dump(Buffer &buf, std::byte *dest) const override;
    void   copy(const std::byte *src, std::byte *dest) const override;
    bool   equal(const std::byte *pl, const std::byte *pr) const override;
};

class objectOper : public TypeInfo {
public:
    using TypeInfo::TypeInfo;
    TError save(const std::byte *src, Buffer &buf) const override;
    TError dump(Buffer &buf, std::byte *dest) const override;
    void   copy(const std::byte *src, std::byte *dest) const override;
    bool   equal(const std::byte *pl, const std::byte *pr) const override;
};

class arrayOper : public TypeInfo {
public:
    using TypeInfo::TypeInfo;
    //array
    TError save(const std::byte *src, Buffer &buf) const override;
    TError dump(Buffer &buf, std::byte *dest) const override;
    void   copy(const std::byte *src, std::byte *dest) const override;
    bool   equal(const std::byte *pl, const std::byte *pr) const override;
};

template <std::meta::info T>
class vectorOper : public TypeInfo {
public:
    using TypeInfo::TypeInfo;
    using Container = [:T:];
    TError save(const std::byte *src, Buffer &buf) const override {
        buf.append(node_.tssd_type_);   //T
        if (node_.tssd_type_ == TType::Tarraym)
            buf.append(children_[0]->node_.tssd_type_);

        int index(0), offset(0);
        buf.ftell(index, offset);
        std::size_t pos = buf.appendSize4(0);   //sizet reserve

        auto pcontainer = (const Container*)src;
        auto real_size = pcontainer->size();
        buf.appendSize2(real_size);

        if (node_.tssd_type_ == TType::Tarraym) {
            buf.append(pcontainer->data(), real_size * children_[0]->node_.size_);
            goto UPDATE;
        }
        // common array data
        for (const auto& it : *pcontainer) {
            if (auto ret = children_[0]->save((const std::byte*)&it,  buf))
                return ret;
        }
UPDATE:
       buf.updateSize(index, offset, buf.size() - pos);
        return OK;
    }

    void copy(const std::byte *src, std::byte *dest) const
    {
        auto pcontainer1 = (const Container*)src;
        auto pcontainer2 = (Container*)dest;

        auto real_size = pcontainer1->size();
        pcontainer2->reserve(real_size);

        if (node_.tssd_type_ == TType::Tarraym) {
            pcontainer2->resize(real_size);
            std::memcpy(pcontainer2->data(),  pcontainer1->data(),  real_size * children_[0]->node_.size_);
            return;
        }

        typename Container::value_type node;
        pcontainer2->clear();
        for (const auto &it : *pcontainer1)
        {
            children_[0]->copy((const std::byte*)&it, (std::byte*)&node);
            pcontainer2->emplace_back(node);
        }
    }

    bool equal(const std::byte *pl, const std::byte *pr) const override {
        auto pcontainer1 = (const Container*)pl;
        auto pcontainer2 = (const Container*)pr;

        auto real_size = pcontainer1->size();
        if (real_size != pcontainer2->size())
            return false;

        if (node_.tssd_type_ == TType::Tarraym) {
            return !std::memcmp(pcontainer1->data(),  pcontainer2->data(),  real_size * children_[0]->node_.size_);
        }

        std::size_t i(0);
        while (i<real_size)
        {
            if (!children_[0]->equal((const std::byte*)&((*pcontainer1)[i]),  (const std::byte*)&((*pcontainer2)[i])))
                return false;
            ++i;
        }
        return true;
    }

    TError dump(Buffer &buf, std::byte *dest) const override {
        std::int8_t t(0);
        if (auto ret = buf.dump(sizeof(t), (std::byte*)&t))
                return ret;

        auto &child = children_[0]->node_;
        if (t == (std::int8_t)TType::Tarraym) {
            std::int8_t t2(0);
            if (auto ret = buf.dump(sizeof(t2), (std::byte*)&t2))
                return ret;

            if (t2 != (std::int8_t)child.tssd_type_) {
                return ERR_FORMAT_ERROR;
            }
        } else if ( t != (std::int8_t)TType::Tarray)
            return ERR_FORMAT_ERROR;

        auto sizet = buf.dumpSize4();
        if (sizet < 0 || buf.size() < (std::size_t)sizet) {
            return ERR_INSUFFICIENT_DATA;
        }

        //sizea
        auto sizea = buf.dumpSize2();
        if (sizea < 0) return ERR_FORMAT_ERROR;

        auto pcontainer = (Container*)dest;
        pcontainer->reserve(sizea);
        pcontainer->clear();
        if (t == (std::int8_t)TType::Tarraym) {
            pcontainer->resize(sizea);
            if (auto ret = buf.dump(sizea * child.size_, (std::byte*)pcontainer->data()))
                return ret;
            return OK;
        }

        // common array
        typename Container::value_type node;
        for (int i=0; i<sizea; ++i) {
            if (auto ret = children_[0]->dump(buf, (std::byte*)&node)) {
                return ret;
            }
            pcontainer->emplace_back(node);
        }
        return OK;
    }
};

template <std::meta::info T>
class listOper : public TypeInfo {
public:
    using TypeInfo::TypeInfo;
    using Container = [:T:];
    TError save(const std::byte *src, Buffer &buf) const override {
        buf.append(node_.tssd_type_);   //T
        if (node_.tssd_type_ == TType::Tarraym)
            buf.append(children_[0]->node_.tssd_type_);

        int index(0), offset(0);
        buf.ftell(index, offset);
        std::size_t pos = buf.appendSize4(0);   //sizet reserve
        auto pcontainer = (const Container*)src;
        buf.appendSize2(pcontainer->size());

        if (node_.tssd_type_ == TType::Tarraym) {
            for (const auto& it : *pcontainer) {
                buf.append((const std::byte*)&it, children_[0]->node_.size_);
            }
            goto UPDATE;
        }
        // common array data
        for (const auto& it : *pcontainer) {
            if (auto ret = children_[0]->save((const std::byte*)&it,  buf))
                return ret;
        }
UPDATE:
       buf.updateSize(index, offset, buf.size() - pos);
        return OK;
    }

    void copy(const std::byte *src, std::byte *dest) const
    {
        auto pcontainer1 = (const Container*)src;
        auto pcontainer2 = (Container*)dest;
        pcontainer2->clear();

        typename Container::value_type node;
        for (const auto &it1 : *pcontainer1)
        {
            children_[0]->copy((const std::byte*)&it1, (std::byte*)&node);
            pcontainer2->emplace_back(node);
        }
    }

    bool equal(const std::byte *pl, const std::byte *pr) const override
    {
        auto pcontainer1 = (const Container*)pl;
        auto pcontainer2 = (const Container*)pr;

        if (pcontainer1->size() != pcontainer2->size())
            return false;

        const auto it1 = pcontainer1->cbegin(), it2 = pcontainer2->cbegin();
        while (it1 != pcontainer1->cend() && it2 != pcontainer2->cend())
        {
            if (!children_[0]->equal((const std::byte*)&(*it1),  (const std::byte*)&(*it2)))
                return false;
            ++it1;
            ++it2;
        }
        return it1 == pcontainer1->cend() && it2 == pcontainer2->cend();
    }

    TError dump(Buffer &buf, std::byte *dest) const override {
        std::int8_t t(0);
        if (auto ret = buf.dump(sizeof(t), (std::byte*)&t))
                return ret;

        auto &child = children_[0]->node_;
        if (t == (std::int8_t)TType::Tarraym) {
            std::int8_t t2(0);
            if (auto ret = buf.dump(sizeof(t2), (std::byte*)&t2))
                return ret;

            if (t2 != (std::int8_t)child.tssd_type_) {
                return ERR_FORMAT_ERROR;
            }
        } else if ( t != (std::int8_t)TType::Tarray)
            return ERR_FORMAT_ERROR;

        auto sizet = buf.dumpSize4();
        if (sizet < 0 || buf.size() < (std::size_t)sizet) {
            return ERR_INSUFFICIENT_DATA;
        }

        //sizea
        auto sizea = buf.dumpSize2();
        if (sizea < 0) return ERR_FORMAT_ERROR;
        auto pcontainer = (Container*)dest;
        pcontainer->clear();
        typename Container::value_type node;
        if (t == (std::int8_t)TType::Tarraym) {
            for (int i=0; i<sizea; ++i) {
                if (auto ret = buf.dump(child.size_, (std::byte*)&node)) {
                    return ret;
                }
                pcontainer->emplace_back(node);
            }
            return OK;
        }

        // common list
        for (int i=0; i<sizea; ++i) {
            if (auto ret = children_[0]->dump(buf, (std::byte*)&node)) {
                return ret;
            }
            pcontainer->emplace_back(node);
        }
        return OK;
    }
};


template <std::meta::info T>
class setOper : public TypeInfo {
public:
    using TypeInfo::TypeInfo;
    using Set = [:T:];
    TError save(const std::byte *src, Buffer &buf) const override {
        buf.append(node_.tssd_type_);   //T
        int index(0), offset(0);
        buf.ftell(index, offset);
        std::size_t pos = buf.appendSize4(0);   //sizet reserve

        auto pset = (const Set*)src;
        buf.appendSize2(pset->size());

        for (const auto& key : *pset) {
            if (auto ret = children_[0]->save((const std::byte*)&key,  buf))
                return ret;
        }

        buf.updateSize(index, offset, buf.size() - pos);
        return OK;
    }

    bool equal(const std::byte *pl, const std::byte *pr) const override {
        auto pset1 = (const Set*)pl;
        auto pset2 = (const Set*)pr;

        if (pset1->size() != pset2->size())
            return false;
        for (const auto& key : *pset1) {
            if (!pset2->contains(key))
                return false;
        }
        return true;
    }

    void copy(const std::byte *src, std::byte *dest) const override
    {
        auto pset1 = (const Set*)src;
        auto pset2 = (Set*)dest;
        pset2->clear();  //need clear first
        typename Set::key_type key;
        for (const auto &it : *pset1)
        {
            children_[0]->copy((const std::byte*)&it, (std::byte*)&key);
            pset2->insert(key);
        }
    }

    TError dump(Buffer &buf, std::byte *dest) const override {
        int sizet = CheckDumpTS(buf);
        if (sizet<0) return sizet;
        //sizea
        auto sizea = buf.dumpSize2();
        if (sizea < 0) return ERR_FORMAT_ERROR;

        auto pset = (Set*)dest;
        pset->clear();
        typename Set::key_type key;
        for (int i=0; i<sizea; ++i) {
            if (auto ret = children_[0]->dump(buf, (std::byte*)&key)) {
                return ret;
            }
            pset->insert(key);
        }
        return OK;
    }
};


template <std::meta::info T>
class sharedPtrOper : public TypeInfo {
public:
    using TypeInfo::TypeInfo;
    using SharedPtr = [:T:];
    TError save(const std::byte *src, Buffer &buf) const override {
        auto pshared_ptr = (const SharedPtr*)src;
        if (!*pshared_ptr) {
            std::int8_t t = (std::int8_t)children_[0]->node_.tssd_type_;
            buf.append(std::byte(-t));  //null pointer
            return OK;
        }
        return children_[0]->save((const std::byte*)pshared_ptr->get(), buf);
    }

    bool equal(const std::byte *pl, const std::byte *pr) const override {
        auto pshared_ptr1 = (const SharedPtr*)pl;
        auto pshared_ptr2 = (const SharedPtr*)pr;

        if (!*pshared_ptr1 && !*pshared_ptr2) {
            return true;
        }
        if (*pshared_ptr1 && !*pshared_ptr2 || !*pshared_ptr1 && *pshared_ptr2)
            return false;
        return children_[0]->equal((const std::byte*)pshared_ptr1->get(), (const std::byte*)pshared_ptr2->get());
    }

    void copy(const std::byte *src, std::byte *dest) const override
    {
        auto pshared_ptr1 = (const SharedPtr*)src;
        auto pshared_ptr2 = (SharedPtr*)dest;
        if (!*pshared_ptr1) {
            if (*pshared_ptr2)
                pshared_ptr2->reset();
            return;
        }
        if (!*pshared_ptr2)
            *pshared_ptr2 = std::make_shared<typename SharedPtr::element_type>();
        children_[0]->copy((const std::byte*)pshared_ptr1->get(), (std::byte*)pshared_ptr2->get());
    }

    TError dump(Buffer &buf, std::byte *dest) const override {
        std::int8_t t(0);
        auto pshared_ptr = (SharedPtr*)dest;
        if (auto ret = buf.peekByte((std::byte&)t)) return ret;
        if (-t == (std::int8_t)children_[0]->node_.tssd_type_) {  //null pointer
            buf.dump(sizeof(t), (std::byte*)&t);  //consume the null pointer type
            if (*pshared_ptr)
                pshared_ptr->reset();
            return OK;
        }
        if (!*pshared_ptr)
            *pshared_ptr = std::make_shared<typename SharedPtr::element_type>();

        return children_[0]->dump(buf, (std::byte*)pshared_ptr->get());
    }
};

template <std::meta::info T>
class mapOper : public TypeInfo {
public:
    using TypeInfo::TypeInfo;
    using Map = [:T:];
    TError save(const std::byte *src, Buffer &buf) const override {
        buf.append(node_.tssd_type_);   //T
        int index(0), offset(0);
        buf.ftell(index, offset);
        std::size_t pos = buf.appendSize4(0);   //sizet reserve

        auto pmap = (const Map*)src;
        auto real_size = pmap->size();
        buf.appendSize2(real_size);

        for (const auto& [key, value] : *pmap) {
            buf.append(TType::Tdictk);
            if (auto ret = children_[0]->save((const std::byte*)&key,  buf))
                return ret;
            buf.append(TType::Tdictv);
            if (auto ret = children_[1]->save((const std::byte*)&value,  buf))
                return ret;
        }

        buf.updateSize(index, offset, buf.size() - pos);
        return OK;
    }

    void copy(const std::byte *src, std::byte *dest) const override
    {
        auto pmap1 = (const Map*)src;
        auto pmap2 = (Map*)dest;
        pmap2->clear();
        for (const auto& [key, value] : *pmap1) {
            (*pmap2)[key] = value;
        }
    }

    bool equal(const std::byte *pl, const std::byte *pr) const override {
        auto pmap1 = (const Map*)pl;
        auto pmap2 = (const Map*)pr;

        if (pmap1->size() != pmap2->size())
            return false;
        for (const auto& [key, value] : *pmap1) {
            auto search = pmap2->find(key);
            if ( search == pmap2->end())
                return false;
            if (!children_[1]->equal((const std::byte*)&value, (const std::byte*)&search->second))
                return false;
        }
        return true;
    }

    TError dump(Buffer &buf, std::byte *dest) const override {
        int sizet = CheckDumpTS(buf);
        if (sizet<0) return sizet;

        //sizea
        auto sizea = buf.dumpSize2();
        if (sizea < 0) return ERR_FORMAT_ERROR;
        auto pmap = (Map*)dest;
        pmap->clear();

        typename Map::key_type key;
        typename Map::mapped_type value;

        for (int i=0; i<sizea; ++i) {
            if (auto ret = CheckTType(buf, (std::int8_t)TType::Tdictk)) return ret;
            if (auto ret = children_[0]->dump(buf, (std::byte*)&key)) {
                return ret;
            }
            if (auto ret = CheckTType(buf, (std::int8_t)TType::Tdictv)) return ret;
            if (auto ret = children_[1]->dump(buf, (std::byte*)&value)) {
                return ret;
            }
            (*pmap)[key] = value;
        }
        return OK;
    }
};


template <typename T>
constexpr std::shared_ptr<TypeInfo>
TypeInfo::parse(std::ptrdiff_t offset, const char *name)
{
    if constexpr (!std::is_class_v<T>)
    {
        if constexpr (std::meta::is_array_type(^^T)) {
            constexpr auto real = std::meta::remove_pointer(std::meta::decay(^^T));
            using FieldT = [:real:];
            return std::make_shared<arrayOper>(std::define_static_string(std::meta::display_string_of(^^T)),
                        name,
                        offset,
                        std::meta::size_of(^^T)/std::meta::size_of(real),   //arrayN
                        TType::Tarray,
                        std::vector<std::shared_ptr<TypeInfo>>{parse<FieldT>()});

        }
        //if constexpr (std::meta::is_arithmetic_type(^^T)) {
        return std::make_shared<TypeInfo>(
            std::define_static_string(std::meta::display_string_of(^^T)),
            name,
            offset,
            std::meta::size_of(^^T),
            std::meta::is_arithmetic_type(^^T),
            std::meta::is_floating_point_type(^^T),
            std::meta::is_signed_type(^^T)
        );

    } else {

        //string
        if constexpr (std::is_same_v<T, std::string>) {
            //std::println("parse string");
            return std::make_shared<stringOper>(
                "std::string",
                name,
                offset,
                std::meta::size_of(^^T),
                TType::Tstring,
                std::vector<std::shared_ptr<TypeInfo>>{}
            );
        }

        if constexpr(std::meta::has_template_arguments(^^T)) {
            //vector
            //std::println("parse template {}", std::meta::display_string_of(std::meta::template_of(member)));

#define createMap(x, y) { \
                using KeyT = [:std::meta::template_arguments_of(^^T)[0]:]; \
                using ValueT = [:std::meta::template_arguments_of(^^T)[1]:]; \
                return std::make_shared<x<^^T>>( \
                    std::define_static_string(std::meta::display_string_of(std::meta::template_of(^^T))), \
                    name, \
                    offset, \
                    std::meta::size_of(^^T), \
                    y, \
                    std::vector<std::shared_ptr<TypeInfo>>{parse<KeyT>(), parse<ValueT>()} \
                ); }

            if  constexpr (std::meta::template_of(^^T) == ^^std::map)
                createMap(mapOper, TType::Tmap);
            if constexpr (std::meta::template_of(^^T) == ^^std::unordered_map)
                createMap(mapOper, TType::Tunordered_map);
#undef createMap
#define createContainer(x, y) { \
                using FieldT = [:std::meta::template_arguments_of(^^T)[0]:]; \
                return std::make_shared<x<^^T>>( \
                    std::define_static_string(std::meta::display_string_of(std::meta::template_of(^^T))), \
                    name, \
                    offset, \
                    std::meta::size_of(^^T), \
                    y, \
                    std::vector<std::shared_ptr<TypeInfo>>{parse<FieldT>()} \
                ); }

            if  constexpr (std::meta::template_of(^^T) == ^^std::vector)
                createContainer(vectorOper, TType::Tvector);
            if  constexpr (std::meta::template_of(^^T) == ^^std::list)
                createContainer(listOper, TType::Tlist);
            if  constexpr (std::meta::template_of(^^T) == ^^std::set)
                createContainer(setOper, TType::Tset);
            if  constexpr (std::meta::template_of(^^T) == ^^std::shared_ptr)
                createContainer(sharedPtrOper, TType::Tshared_ptr);
#undef createContainer
        }
        constexpr auto ctx = std::meta::access_context::unchecked();
        std::vector<std::shared_ptr<TypeInfo>> children;
        // should parse parent info after stl container but before class itslef
        if constexpr (std::meta::has_parent(^^T)) {
            template for (constexpr auto parent : std::define_static_array(std::meta::bases_of(^^T, ctx))) {
                using FieldT = [:std::meta::type_of(parent):];
                children.push_back(parse<FieldT>(std::meta::offset_of(parent).bytes, ""));
            }
        }

        //common class
        template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
            using FieldT = [:std::meta::type_of(member):];
            constexpr auto name = std::meta::has_identifier(member) ? std::define_static_string(std::meta::identifier_of(member)) : "";
            //std::println("member: {} has id {} name {}", std::meta::display_string_of(std::meta::type_of(member)), std::meta::has_identifier(member), name);
            children.push_back(parse<FieldT>(std::meta::offset_of(member).bytes, name));
        }
        return std::make_shared<objectOper>(
            std::define_static_string(std::meta::display_string_of(^^T)),
            name,
            offset,
            std::meta::size_of(^^T),
            TType::Tobject,
            children
        );
    }
}

} // end namespace tssd
#endif
