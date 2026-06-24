#include <meta>
#include <iostream>
#include <print>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>

#include "tssd.h"
#include "flat.h"

class TBuffer : public std::vector<std::byte> {
    size_t current = 0;
    void grow(const std::size_t require=1) {
        if (size() +  require > capacity()) {
            reserve(capacity()*2);
        }
    }

    void append(const std::span<std::byte> content) {
        grow(content.size());
        append_range(content);
    }

public:

    TBuffer(std::size_t size=2048) {
        reserve(std::max(size, size_t(1)));  //make sure we reserve 1 byte at least
    }

    TBuffer& clear() {
        std::vector<std::byte>::clear();
        current = 0;
        return *this;
    }

    TBuffer& reset() {
        return this->clear();
    }

    void print() const {

        std::cout<<"TBuffer print:[";
        for (int i=0; i<size(); ++i)
            std::cout << (int)(*this)[i] << '\t';
        std::println("]");
    }

    void append(const TType t) {
        grow();
        emplace_back(std::byte(t));
    }

    //append two bytes size, return the pos of it
    //you may update it at buf[pos]
    std::size_t appendSize(const size_t size) {
        std::uint16_t size2 = std::uint16_t(size);
        auto pos = this->size();
        append((std::byte*)&size2, sizeof(size2));
        return pos;
    }

    void updateSize(const size_t pos, const size_t size) {
        std::uint16_t *ptr = (std::uint16_t *)&(*this)[pos];
        *ptr = (std::uint16_t)size;
    }

    void append(const std::byte *ptr, std::size_t size) {
        auto span = std::span<std::byte>((std::byte*)ptr, size);
        append(span);
    }

    std::int16_t dumpSize() {
        std::int16_t size(0);
        if (auto ret = dump(sizeof(size), (std::byte *)&size))
            return ret;
        return size;
    }

    TError dump(std::size_t size, std::byte *dest) {
        if (this->size() < size + current) return ERR_INSUFFICIENT_DATA;
        memcpy(dest, &(*this)[current], size);
        current +=  size;
        return OK;
    }

    std::byte *dump(std::size_t size) {
        if (this->size() < size + current ) return nullptr;
        auto ret = &(*this)[current];
        current += size;
        return ret;
    }

};

class Oper {
public:
    virtual ~Oper() {}
    virtual TError save(const std::byte *src, TBuffer &buf) const = 0;
    virtual TError dump(TBuffer &buf, std::byte *dest) const = 0;
};

class objectOper;
class stringOper;
class arrayOper;
struct TypeInfo : public Oper {

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
        constexpr Node(char const *type, char const *name, ptrdiff_t offset, std::size_t size, TType local_type) 
            : name_(name), type_(type), offset_(offset), size_(size), local_type_(local_type) {}
        
        constexpr Node(char const *type, char const *name, ptrdiff_t offset, std::size_t size, bool is_number, bool is_float, bool is_signed) 
            : name_(name), type_(type), offset_(offset), size_(size), is_number_(is_number), is_float_(is_float), is_signed_(is_signed) {}
    };

    Node node_;
    std::vector<std::shared_ptr<TypeInfo>> children_;

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

    TError save(const std::byte *src, TBuffer &buf) const override {
        buf.append(node_.tssd_type_);
        buf.append(src, node_.size_);
        return OK;
    }

    inline TError CheckTType(TBuffer &buf, std::int8_t type= 0) const {
        std::int8_t t(0);
        if (auto ret = buf.dump(sizeof(t), (std::byte*)&t)) 
            return ret;
        type = type != 0 ? type : (std::int8_t)node_.tssd_type_;
        if ( t != type)
            return ERR_FORMAT_ERROR;
        
        return OK;
    }

    TError dump(TBuffer &buf, std::byte *dest) const override {

        if (auto ret = CheckTType(buf))
            return ret;
     
        return buf.dump(node_.size_, dest);
    }

    //set tssd_type, total offset, save, dump by the reflect type
    void parse(std::shared_ptr<TypeInfo> parent) 
    {
        for (auto &it : children_) {
            it->node_.parent_ = parent;
            it->node_.total_offset_ = parent->node_.total_offset_ + it->node_.offset_;
            if (it->node_.is_number_) {
                if (it->node_.is_float_)
                    it->node_.tssd_type_ = (it->node_.size_ == 4) ? TType::Tfloat32 : TType::Tfloat64;
                else {
                    switch(it->node_.size_) {
                        case 1:
                            it->node_.tssd_type_ = (it->node_.is_signed_) ? TType::Tint8 : TType::Tuint8;
                            break;
                        case 2:
                            it->node_.tssd_type_ = (it->node_.is_signed_) ? TType::Tint16 : TType::Tuint16;
                            break;
                        case 4:
                            it->node_.tssd_type_ = (it->node_.is_signed_) ? TType::Tint32 : TType::Tuint32;
                            break; 
                        case 8:
                            it->node_.tssd_type_ = (it->node_.is_signed_) ? TType::Tint64 : TType::Tuint64;
                            break;                                                          
                    }
                }
                it->node_.local_type_ = it->node_.tssd_type_;
            } else {
                switch(it->node_.local_type_) {
                    case TType::Tstring:
                        it->node_.tssd_type_ = TType::Tstring;
                        break;
                    case TType::Tarray:     //it'a static array
                    case TType::Tvector:     //dynamic array
                        it->node_.tssd_type_ = TType::Tarray;
                        it->parse(it);
                        break;
                    case TType::Tdict:
                        it->node_.tssd_type_ = TType::Tdict;
                        it->parse(it);
                        break;
                    default:
                        it->parse(it);  //Tobject is the default, just walk throuth children
                }
            }
        }
    }

    template <typename T> 
    constexpr static std::shared_ptr<TypeInfo> parse2(std::ptrdiff_t offset=0, const char *name = "");

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
        auto ti = parse2<T>();
        ti->parse(ti);
        return ti;
    }

    TError MarshalTo(const void *obj, TBuffer &buf) const {
        return save((const std::byte*)obj, buf);
    }

    TError UnmarshalTo(TBuffer &buf, void *obj) const {
        return dump(buf, (std::byte *)obj);
    }
};

//template <std::meta::info Type>
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

//template <std::meta::info Type>
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

//template <std::meta::info Type>
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
            if  constexpr (std::meta::template_of(^^T) == ^^std::vector)
            {
                using FieldT = [:std::meta::template_arguments_of(^^T)[0]:];

                return std::make_shared<arrayOper>(
                    std::define_static_string(std::meta::display_string_of(std::meta::template_of(^^T))),
                    name,
                    offset,
                    std::meta::size_of(^^T),
                    TType::Tvector,
                    std::vector<std::shared_ptr<TypeInfo>>{parse2<FieldT>()}
                );
            }
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