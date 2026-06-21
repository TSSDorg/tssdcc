#include <meta>
#include <iostream>
#include <print>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstdlib>
#include <cstring>

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

class TypeInfo {
    typedef TError (TypeInfo::*SaveFunc)(const std::byte *src, TBuffer &buf) const;
    typedef TError (TypeInfo::*DumpFunc)(TBuffer &buf, std::byte *dest) const;
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
        SaveFunc save_ = &TypeInfo::objSave;
        DumpFunc dump_ = &TypeInfo::objDump;
        const TypeInfo *parent_ = nullptr;
        constexpr Node(char const *type, char const *name, ptrdiff_t offset, std::size_t size, TType local_type) 
            : name_(name), type_(type), offset_(offset), size_(size), local_type_(local_type) {}
        
        constexpr Node(char const *type, char const *name, ptrdiff_t offset, std::size_t size, bool is_number, bool is_float, bool is_signed) 
            : name_(name), type_(type), offset_(offset), size_(size), is_number_(is_number), is_float_(is_float), is_signed_(is_signed) {}
    };   

    Node node_;
    std::vector<TypeInfo> children_;

    constexpr TypeInfo(char const *type,
            char const *name,
            std::ptrdiff_t offset, std::size_t size, bool is_number=false, bool is_float=false, bool is_signed=false) : 
            node_(type, name, offset, size, is_number, is_float, is_signed) {}   

    constexpr TypeInfo(char const *type,
            char const *name,
            std::ptrdiff_t offset,
            std::size_t size,
            TType ttype,
            std::vector<TypeInfo> ch) : 
            node_(type, name, offset, size, ttype), 
            children_(ch) {}

    TError memSave(const std::byte *src, TBuffer &buf) const {
        buf.append(node_.tssd_type_);
        buf.append(src, node_.size_);
        return OK;
    }

    inline TError CheckTType(TBuffer &buf) const {
        std::int8_t t(0);
        if (auto ret = buf.dump(sizeof(t), (std::byte*)&t)) 
            return ret;
        if ( t != (std::int8_t)node_.tssd_type_)
            return ERR_FORMAT_ERROR;
        
        return OK;
    }

    TError memDump(TBuffer &buf, std::byte *dest) const {

        if (auto ret = CheckTType(buf))
            return ret;
     
        return buf.dump(node_.size_, dest);
    }

    TError strSave(const std::byte *src, TBuffer &buf) const {
        buf.append(node_.tssd_type_);
        auto pstr = (const std::string *)src;

        buf.appendSize(pstr->size()); //sizet

        buf.append((const std::byte*)pstr->c_str(), pstr->size());
        return OK;
    }

    TError strDump(TBuffer &buf, std::byte *dest) const {
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

    TError objSave(const std::byte *src, TBuffer &buf) const {
        buf.append(node_.tssd_type_);   //T
        std::size_t pos = buf.appendSize(0);   //sizet reserve
        buf.appendSize(children_.size()); //sizea
        
        for (auto &it : children_) {
            if (auto ret = (it.*it.node_.save_)(&src[it.node_.offset_],  buf)) 
                return ret;
        }

        buf.updateSize(pos, buf.size() - pos - 2);
        return OK;
    }

    TError objDump(TBuffer &buf, std::byte *dest) const {
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
            if (auto ret = (it.*it.node_.dump_)(buf, &dest[it.node_.offset_])) {
                return ret;
            }
        }

        return OK;
    }

    //array
    TError arraySave(const std::byte *src, TBuffer &buf) const {
        buf.append(node_.tssd_type_);   //T
        std::size_t pos = buf.appendSize(0);   //sizet reserve
        buf.appendSize(node_.size_); //sizea
        
        auto &node = children_[0].node_;
        for (int i=0; i<node_.size_; ++i) {
            if (auto ret = (children_[0].*node.save_)(&src[node.offset_ + node.size_ * i],  buf)) 
                return ret;
        }

        buf.updateSize(pos, buf.size() - pos - 2);
        return OK;
    }

    TError arrayDump(TBuffer &buf, std::byte *dest) const {
        if (auto ret = CheckTType(buf))
            return ret;
        auto sizet = buf.dumpSize();
        if (sizet < 0 || buf.size() < 1 + 2 + sizet) {
            return ERR_INSUFFICIENT_DATA;
        }

        //sizea
        auto sizea = buf.dumpSize();
        if (sizea < 0) return ERR_FORMAT_ERROR;

        //static array need check node_.size, but dyname array(vector) need skip
        if ( node_.local_type_ == TType::Tarray && sizea != node_.size_) {
            return ERR_FORMAT_ERROR;
        }
        
         auto &node = children_[0].node_;
        for (int i=0; i<sizea; ++i) {
            if (auto ret = (children_[0].*node.dump_)(buf, &dest[node.offset_ + node.size_ * i])) {
                return ret;
            }
        }

        return OK;
    }

    //set tssd_type, total offset, save, dump by the reflect type
    void parse(const TypeInfo *parent) 
    {
        for (auto &it : children_) {
            it.node_.parent_ = parent;
            it.node_.total_offset_ = parent->node_.total_offset_ + it.node_.offset_;
            if (it.node_.is_number_) {
                it.node_.save_ = &TypeInfo::memSave;
                it.node_.dump_ = &TypeInfo::memDump;
                if (it.node_.is_float_)
                    it.node_.tssd_type_ = (it.node_.size_ == 4) ? TType::Tfloat32 : TType::Tfloat64;
                else {
                    switch(it.node_.size_) {
                        case 1:
                            it.node_.tssd_type_ = (it.node_.is_signed_) ? TType::Tint8 : TType::Tuint8;
                            break;
                        case 2:
                            it.node_.tssd_type_ = (it.node_.is_signed_) ? TType::Tint16 : TType::Tuint16;
                            break;
                        case 4:
                            it.node_.tssd_type_ = (it.node_.is_signed_) ? TType::Tint32 : TType::Tuint32;
                            break; 
                        case 8:
                            it.node_.tssd_type_ = (it.node_.is_signed_) ? TType::Tint64 : TType::Tuint64;
                            break;                                                          
                    }
                }
                it.node_.local_type_ = it.node_.tssd_type_;
            } else {
                switch(it.node_.local_type_) {
                    case TType::Tstring:
                        it.node_.tssd_type_ = TType::Tstring;
                        it.node_.save_ = &TypeInfo::strSave;
                        it.node_.dump_ = &TypeInfo::strDump;
                        break;
                    case TType::Tarray:     //it'a static array
                        it.node_.tssd_type_ = TType::Tarray;
                        it.node_.save_ = &TypeInfo::arraySave;
                        it.node_.dump_ = &TypeInfo::arrayDump;
                        it.parse(&it);
                        break;
                    default:
                        it.parse(&it);  //Tobject is the default, just walk throuth children
                }
            }
        }
    }

    template <typename T> 
    constexpr static TypeInfo parse2(std::ptrdiff_t offset=0, const char *name="") {
        //constexpr auto member = ^^T;
        if constexpr (!std::is_class_v<T>)
        {
            if constexpr (std::meta::is_array_type(^^T)) {
                constexpr auto real = std::meta::remove_pointer(std::meta::decay(^^T));
                using FieldT = [:real:];

                return TypeInfo{std::define_static_string(std::meta::display_string_of(^^T)),
                            name,
                            offset,
                            std::meta::size_of(^^T)/std::meta::size_of(real),
                            TType::Tarray,
                            {parse2<FieldT>()}};

            }
            //if constexpr (std::meta::is_arithmetic_type(^^T)) {
            return TypeInfo{
                std::define_static_string(std::meta::display_string_of(^^T)),
                name,
                offset,
                std::meta::size_of(^^T),
                std::meta::is_arithmetic_type(^^T),
                std::meta::is_floating_point_type(^^T),
                std::meta::is_signed_type(^^T)
            };
            
        } else {
            
            //string
            if constexpr (std::is_same_v<T, std::string>) {
                //std::println("parse string");
                return TypeInfo{
                    "std::string",
                    name,
                    offset,
                    std::meta::size_of(^^T),
                    TType::Tstring,
                    {}
                };
            }
            
            if constexpr(std::meta::has_template_arguments(^^T)) {
                //vector
                //std::println("parse template {}", std::meta::display_string_of(std::meta::template_of(member)));
                if  constexpr (std::meta::template_of(^^T) == ^^std::vector)
                {
                    using FieldT = [:std::meta::template_arguments_of(^^T)[0]:];

                    return TypeInfo{
                        std::define_static_string(std::meta::display_string_of(std::meta::template_of(^^T))),
                        name,
                        offset,
                        std::meta::size_of(^^T),
                        TType::Tvector,
                        {parse2<FieldT>()}
                    };

                }
                //TODO map and others
            }
        
            //common class
            constexpr auto ctx = std::meta::access_context::unchecked();
            std::vector<TypeInfo> children;
            template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
                using FieldT = [:std::meta::type_of(member):];
                children.push_back(parse2<FieldT>(std::meta::offset_of(member).bytes, 
                    std::define_static_string(std::meta::identifier_of(member))));
            }
            return TypeInfo {
                std::define_static_string(std::meta::display_string_of(^^T)),
                name,
                offset,
                std::meta::size_of(^^T),
                TType::Tobject,
                children
            };
        }
    }


public:
    constexpr TypeInfo(
        const char *type,
        const char *name,
        std::vector<TypeInfo> ch) : 
        node_(type, name, 0, 0, TType::Tobject), 
        children_(ch) {}

    void print() const {
        std::println("result type:{} name:{} offset:{} size:{} total_offset:{}", node_.type_, node_.name_, node_.offset_, node_.size_, node_.total_offset_);
        for (auto &it : children_) 
            it.print();
    }

    template <typename T> 
    static auto Create() {
        static_assert(std::meta::is_class_type(^^T));
        auto ti = parse2<T>();
        ti.parse(&ti);
        return ti;
    }

    TError MarshalTo(const void *obj, TBuffer &buf) const {
        return (this->*node_.save_)((const std::byte*)obj, buf);
    }

    TError UnmarshalTo(TBuffer &buf, void *obj) const {
        return (this->*node_.dump_)(buf, (std::byte *)obj);
    }

};

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
};


int main() {

    
    //TypeInfo ti("MyStruct", "MyStruct", 0, TypeInfo::parse<MyStruct>());

    std::println("std::string has template: {}, {}", 
        std::meta::has_template_arguments(^^std::vector<char>),
        std::meta::is_same_type(^^std::string, ^^std::string)
    );

    TBuffer buf;
    MyStr m{"foo", true}, m2;

    auto ti = TypeInfo::Create<MyStr>();
    ti.print();

    ti.MarshalTo((const std::byte*)&m, buf);

    buf.print();

    ti.UnmarshalTo(buf, &m2);
    std::println("MyStr: {}, {} => {} {}", m.str, m.b, m2.str, m2.b);
   

    Point in{1, 2}, out;

    auto tip = TypeInfo::Create<Point>();
    tip.print();

    tip.MarshalTo(&in, buf.clear());
    buf.print();
    tip.UnmarshalTo(buf, &out);

    std::println("{},{} = {},{}", (int)in.x, (int)in.y, (int)out.x, (int)out.y);
    

    //MyStruct ms{"foo", 2, {3, 4}};


    //ti->MarshalTo((const std::byte*)&ms, buf);

    //buf.print();

    auto ta = TypeInfo::Create<MyArray>();
    ta.print();
    MyArray ma{1, 3}, mb;

    ta.MarshalTo(&ma, buf.clear());
    buf.print();

    ta.UnmarshalTo(buf, &mb);

    std::println("mb: {} {}", int(mb.c[0]), int(mb.c[1]));


    return 0;
}