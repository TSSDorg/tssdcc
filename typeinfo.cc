#include <meta>
#include <iostream>
#include <print>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <list>
#include <set>

#include "tssd.h"
#include "typeinfo.h"

class stringOper : public TypeInfo {
public:
    using TypeInfo::TypeInfo;

    TError save(const std::byte *src, TBuffer &buf) const override {
        buf.append(node_.tssd_type_);
        auto pstr = (const std::string *)src;

        buf.appendSize(pstr->size()); //sizet

        buf.append((const std::byte*)pstr->c_str(), pstr->size());
        return OK;
    }

    TError dump(TBuffer &buf, std::byte *dest) const override {
        if (auto ret = CheckTType(buf))
            return ret;
        auto size = buf.dumpSize();
        if (size < 0) {
            return size;
        }

        auto pstr = (std::string *)dest;

        const char *ptr = (const char *)buf.dump(size);
        if (!ptr) {
            return ERR_INSUFFICIENT_DATA;
        }

        pstr->assign(ptr, size);
        
        return OK;
    }
};

class objectOper : public TypeInfo {
public:
    using TypeInfo::TypeInfo;

    TError save(const std::byte *src, TBuffer &buf) const override {
        buf.append(node_.tssd_type_);   //T
        std::size_t pos = buf.appendSize(0);   //sizet reserve
        buf.appendSize(children_.size()); //sizea
        
        for (auto &it : children_) {
            if (auto ret = it->save(&src[it->node_.offset_],  buf)) 
                return ret;
        }

        buf.updateSize(pos, buf.size() - pos - 2);
        return OK;
    }

    TError dump(TBuffer &buf, std::byte *dest) const override {
        if (auto ret = CheckTType(buf))
            return ret;
        auto sizet = buf.dumpSize();
        if (sizet < 0 || buf.size() < 1 + 2 + sizet) {
            return ERR_INSUFFICIENT_DATA;
        }

        //sizea
        if (buf.dumpSize() != children_.size()) {
            return ERR_FORMAT_ERROR;
        }
        
        for (auto &it : children_) {
            if (auto ret = it->dump(buf, &dest[it->node_.offset_])) {
                return ret;
            }
        }
        return OK;
    }

};


class arrayOper : public TypeInfo {
public:
    using TypeInfo::TypeInfo;

    //array
    TError save(const std::byte *src, TBuffer &buf) const override {
        buf.append(node_.tssd_type_);   //T
        std::size_t pos = buf.appendSize(0);   //sizet reserve

        auto real_size = node_.size_;
        auto addr = src;
        auto &node = children_[0]->node_;
        if (node_.local_type_ == TType::Tvector) {  //for vector, we convert to vector<byte> to calc the real size
            auto *p = (std::vector<std::byte>*)src;
            real_size = p->size()/node.size_;
            addr = p->data();
        }
        buf.appendSize(real_size);
        
        for (int i=0; i<real_size; ++i) {
            if (auto ret = children_[0]->save(&addr[node.size_ * i],  buf)) 
                return ret;
        }

        buf.updateSize(pos, buf.size() - pos - 2);
        return OK;
    }

    TError dump(TBuffer &buf, std::byte *dest) const override {
        if (auto ret = CheckTType(buf))
            return ret;
        auto sizet = buf.dumpSize();
        if (sizet < 0 || buf.size() < 1 + 2 + sizet) {
            return ERR_INSUFFICIENT_DATA;
        }

        //sizea
        auto sizea = buf.dumpSize();
        if (sizea < 0) return ERR_FORMAT_ERROR;

        auto addr = dest;
        auto &node = children_[0]->node_;
        //static array need check node_.size, but dyname array(vector) need skip
        if (node_.local_type_ == TType::Tarray) {
            if (sizea != node_.size_)
                return ERR_FORMAT_ERROR;
        } else { //for vector we need reserve capacity first
            auto *p = (std::vector<std::byte>*)dest;
            p->reserve(node.size_ * sizea);
            p->resize(node.size_ * sizea);
            addr = p->data();
        }
        
        for (int i=0; i<sizea; ++i) {
            if (auto ret = children_[0]->dump(buf, &addr[node.size_ * i])) {
                return ret;
            }
        }
        return OK;
    }
};

template <std::meta::info T>
class conainerOper : public TypeInfo {  //for single node container: vector, list, set, dequeue ?
public:
    using TypeInfo::TypeInfo;

    TError save(const std::byte *src, TBuffer &buf) const override {
        buf.append(node_.tssd_type_);   //T
        std::size_t pos = buf.appendSize(0);   //sizet reserve

        using Container = [:T:];
        auto pcontainer = (Container*)src;

        auto real_size = pcontainer->size();

        buf.appendSize(real_size);

        for (const auto& it : *pcontainer) {
            if (auto ret = children_[0]->save((std::byte*)&it,  buf)) 
                return ret;   
        }

        buf.updateSize(pos, buf.size() - pos - 2);
        return OK;
    }

    TError dump(TBuffer &buf, std::byte *dest) const override {
        if (auto ret = CheckTType(buf))
            return ret;
        auto sizet = buf.dumpSize();
        if (sizet < 0 || buf.size() < 1 + 2 + sizet) {
            return ERR_INSUFFICIENT_DATA;
        }

        //sizea
        auto sizea = buf.dumpSize();
        if (sizea < 0) return ERR_FORMAT_ERROR;

        using Container = [:T:];
        auto pcontainer = (Container*)dest;
        if (this->node_.local_type_ == TType::Tvector) {
            pcontainer->reserve(sizea);
        }

        typename Container::value_type node;
        for (int i=0; i<sizea; ++i) {
            if (auto ret = children_[0]->dump(buf, (std::byte*)&node)) {
                return ret;
            }
            pcontainer->insert(pcontainer->end(), node);
        }
        return OK;
    }
};


template <std::meta::info T>
class mapOper : public TypeInfo {
public:
    using TypeInfo::TypeInfo;

    TError save(const std::byte *src, TBuffer &buf) const override {
        buf.append(node_.tssd_type_);   //T
        std::size_t pos = buf.appendSize(0);   //sizet reserve

        using Map = [:T:];
        auto pmap = (Map*)src;

        auto real_size = pmap->size();

        buf.appendSize(real_size);

        for (const auto& [key, value] : *pmap) {
            buf.append(TType::Tdictk);
            if (auto ret = children_[0]->save((std::byte*)&key,  buf)) 
                return ret;
            buf.append(TType::Tdictv);
            if (auto ret = children_[1]->save((std::byte*)&value,  buf)) 
                return ret;    
        }

        buf.updateSize(pos, buf.size() - pos - 2);
        return OK;
    }

    TError dump(TBuffer &buf, std::byte *dest) const override {
        if (auto ret = CheckTType(buf))
            return ret;
        auto sizet = buf.dumpSize();
        if (sizet < 0 || buf.size() < 1 + 2 + sizet) {
            return ERR_INSUFFICIENT_DATA;
        }

        //sizea
        auto sizea = buf.dumpSize();
        if (sizea < 0) return ERR_FORMAT_ERROR;

        using Map = [:T:];
        auto pmap = (Map*)dest;

        typename Map::key_type key;
        typename Map::mapped_type value;

        auto addr = dest;
        auto &knode = children_[0]->node_;
        auto &vnode = children_[1]->node_;

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
constexpr std::shared_ptr<TypeInfo> TypeInfo::parse2(std::ptrdiff_t offset, const char *name)
{
    if constexpr (!std::is_class_v<T>)
    {
        if constexpr (std::meta::is_array_type(^^T)) {
            constexpr auto real = std::meta::remove_pointer(std::meta::decay(^^T));
            using FieldT = [:real:];

            return std::make_shared<arrayOper>(std::define_static_string(std::meta::display_string_of(^^T)),
                        name,
                        offset,
                        std::meta::size_of(^^T)/std::meta::size_of(real),
                        TType::Tarray,
                        std::vector<std::shared_ptr<TypeInfo>>{parse2<FieldT>()});

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

            //TODO map and others
            if  constexpr (std::meta::template_of(^^T) == ^^std::map)
            {
                using FieldK = [:std::meta::template_arguments_of(^^T)[0]:];
                using FieldV = [:std::meta::template_arguments_of(^^T)[1]:];

                return std::make_shared<mapOper<^^T>>(
                    std::define_static_string(std::meta::display_string_of(std::meta::template_of(^^T))),
                    name,
                    offset,
                    std::meta::size_of(^^T),
                    TType::Tdict,
                    std::vector<std::shared_ptr<TypeInfo>>{parse2<FieldK>(), parse2<FieldV>()}
                );
            }

#define  create(x) { \
                using FieldT = [:std::meta::template_arguments_of(^^T)[0]:]; \
                return std::make_shared<conainerOper<^^T>>( \
                    std::define_static_string(std::meta::display_string_of(std::meta::template_of(^^T))), \
                    name, \
                    offset, \
                    std::meta::size_of(^^T), \
                    x, \
                    std::vector<std::shared_ptr<TypeInfo>>{parse2<FieldT>()} \
                ); }

            if  constexpr (std::meta::template_of(^^T) == ^^std::vector)
                create(TType::Tvector);
            if  constexpr (std::meta::template_of(^^T) == ^^std::list)
                create(TType::Tlist);
            if  constexpr (std::meta::template_of(^^T) == ^^std::set)
                create(TType::Tset);
        }
    
        //common class
        constexpr auto ctx = std::meta::access_context::unchecked();
        std::vector<std::shared_ptr<TypeInfo>> children;
        template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
            using FieldT = [:std::meta::type_of(member):];
            
            constexpr auto name = std::meta::has_identifier(member) ? std::define_static_string(std::meta::identifier_of(member)) : "";
            //std::println("member: {} has id {} name {}", std::meta::display_string_of(std::meta::type_of(member)), std::meta::has_identifier(member), name);
            children.push_back(parse2<FieldT>(std::meta::offset_of(member).bytes, name));
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


struct Point {
    short x;
    char y;
};

struct MyStr {
    std::string str;
    bool b;
};

struct MyStruct {
    std::string str;
    long long a;
    //int a;
    double b;
    //std::vector<Point> points;
    Point point;
    std::string Name() {
        return "MyStruct";
    }
};

struct MyArray {
    char c[2];
    std::vector<std::int16_t> vec;
};

struct MyMap {
    std::map<int, MyStr> mp;
};


int main() {
    //TypeInfo ti("MyStruct", "MyStruct", 0, TypeInfo::parse<MyStruct>());

    std::println("std::string has template: {}, {}", 
        std::meta::has_template_arguments(^^std::vector<char>),
        std::meta::is_same_type(^^std::string, ^^std::string)
    );

    TBuffer buf;

    auto tmp = TypeInfo::Create<MyMap>();
    tmp->print();

    MyMap mmp, mmp2;
    mmp.mp[1]={"abc", true};
    mmp.mp[2]={"def", false};

    tmp->MarshalTo(&mmp, buf);
    buf.print();

    tmp->UnmarshalTo(buf, &mmp2);

     for (const auto& [key, value] : mmp2.mp)
        std::cout << '[' << key << "] = " << value.str << "," << value.b << "; \n";

    buf.clear();



    //MyStr m{"foo", true}, m2;

    auto ti = TypeInfo::Create<MyStr>();
    ti->print();

    //ti->MarshalTo((const std::byte*)&m, buf);

    //buf.print();

    //ti->UnmarshalTo(buf, &m2);
    ///std::println("MyStr: {}, {} => {} {}", m.str, m.b, m2.str, m2.b);
   

    Point in{1, 2}, out;

    auto tip = TypeInfo::Create<Point>();
    tip->print();

    tip->MarshalTo(&in, buf.clear());
    buf.print();
    tip->UnmarshalTo(buf, &out);

    std::println("{},{} = {},{}", (int)in.x, (int)in.y, (int)out.x, (int)out.y);
    

    //MyStruct ms{"foo", 2, {3, 4}};


    //ti->MarshalTo((const std::byte*)&ms, buf);

    //buf.print();

    auto ta = TypeInfo::Create<MyArray>();
    ta->print();
    MyArray ma{
        {1, 3},
        { 2, 4, 5}
    }, mb;

    ta->MarshalTo(&ma, buf.clear());
    buf.print();

    ta->UnmarshalTo(buf, &mb);

    std::println("mb: {} {} {}", (int)mb.c[0], (int)mb.c[1], mb.vec.size());


    return 0;
}