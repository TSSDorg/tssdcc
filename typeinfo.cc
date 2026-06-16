#include <meta>
#include <iostream>
#include <print>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstdlib>

#include "tssd.h"
#include "flat.h"

class TBuffer : public std::vector<std::byte> {
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

    void append(std::byte *ptr, std::size_t size) {
        auto span = std::span<std::byte>(ptr, size);
        append(span);
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
        TType tssd_type_ = TType::Tbool;
        TType local_type_ = TType::Tbool;
        SaveFunc save;  // = &TypeInfo::memSave;
        DumpFunc dump = nullptr;
        constexpr Node(char const *type, char const *name, ptrdiff_t offset, std::size_t size) 
            : name_(name), type_(type), offset_(offset), size_(size) {}
    };   

    Node node_;
    std::vector<TypeInfo> children_;

    constexpr TypeInfo(char const *type,
            char const *name,
            std::ptrdiff_t offset, std::size_t size) : 
            node_(type, name, offset, size) {}   

    constexpr TypeInfo(char const *type,
            char const *name,
            std::ptrdiff_t offset,
            std::size_t size,
            std::vector<TypeInfo> ch) : 
            node_(type, name, offset, size), 
            children_(ch) {}

    TError memSave(const std::byte *src, TBuffer &buf) const {
        buf.append(node_.tssd_type_);
        std::byte * ptr = (std::byte *)src;

        buf.append(ptr+node_.total_offset_, node_.size_);
        return TError::T_OK;
    }

    TError objSave(const std::byte *src, TBuffer &buf) const {
        buf.append(node_.tssd_type_);   //T
        auto pos = buf.appendSize(0);   //sizet reserve
        buf.appendSize(children_.size()); //sizea
        
        for (auto it : children_) {
            (it.*it.node_.save)(src + it.node_.total_offset_,  buf);
        }

        buf.updateSize(pos, buf.size() - pos - 2);
        return TError::T_OK;
    }

    template <typename T> 
    constexpr static auto parse() {
        constexpr auto ctx = std::meta::access_context::unchecked();
        std::vector<TypeInfo> children;
        
        template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
            using FieldT = [:std::meta::type_of(member):];
            
            if constexpr (!std::is_class_v<FieldT>)
            {
                children.push_back(
                {
                    std::define_static_string(std::meta::display_string_of(std::meta::type_of(member))),
                    std::define_static_string(std::meta::identifier_of(member)),
                    std::meta::offset_of(member).bytes,
                    std::meta::size_of(member)
                });
            } else 
            {
                if constexpr(std::meta::has_template_arguments(std::meta::type_of(member))) {
                        if  constexpr (std::meta::template_of(std::meta::type_of(member)) == ^^std::vector) {
                        using FieldT = [:std::meta::template_arguments_of(std::meta::type_of(member))[0]:];            
                        std::println("parse template vector");
                        children.push_back(
                        {
                            std::define_static_string(std::meta::display_string_of(std::meta::template_of(std::meta::type_of(member)))),
                            std::define_static_string(std::meta::identifier_of(member)),
                            std::meta::offset_of(member).bytes,
                            std::meta::size_of(member),
                            parse<FieldT>(),
                        });
                        
                    }
                } else {
                    children.push_back(
                    {
                        std::define_static_string(std::meta::display_string_of(std::meta::type_of(member))),
                        std::define_static_string(std::meta::identifier_of(member)),
                        std::meta::offset_of(member).bytes,
                        std::meta::size_of(member),
                        parse<FieldT>(),
                    });
                }
            }
            std::println("name: {}, children size: {}", std::define_static_string(std::meta::identifier_of(member)), children.size());
        }
        
        return children;
    }

    //set tssd_type, total offset, save, dump by the reflect type
    void parse() {
    }


public:

    constexpr TypeInfo(char const *type,
        std::vector<TypeInfo> ch) : 
        node_(type, type, 0, 0), 
        children_(ch) {}

    void print() const {
        std::println("result: {} {} {} {} {}", node_.type_, node_.name_, node_.offset_, node_.size_, children_.size());
        for (int i=0; i<children_.size(); i++) 
            children_[i].print();
    }

    template <typename T> 
    static auto Create(const std::string &type="") {
        return std::make_shared<TypeInfo>(type.c_str(), parse<T>());
    }

    TError MarshalTo(const std::byte *flat, TBuffer &buf) const {
        return (this->*node_.save)(flat, buf);
    }

};

struct Point {
    int x;
    int y;
};

struct MyStruct { 
    Point point;
    int a;
    double b;
    std::vector<Point> points;
    std::string Name() {
        return "MyStruct";
    }
};


int main() {

    
    //TypeInfo ti("MyStruct", "MyStruct", 0, TypeInfo::parse<MyStruct>());

    auto ti = TypeInfo::Create<MyStruct>("MyStruct");

    auto has_template = std::meta::has_template_arguments(^^MyStruct);

    constexpr auto no_check = std::meta::access_context::unchecked();
    constexpr auto rx = std::meta::nonstatic_data_members_of(^^MyStruct, no_check)[0];

    MyStruct ms;

    auto has_template2 = std::meta::has_template_arguments(rx);

    /*std::println("main size: {} has:{} {} {}", ti.size(), has_template, 
     std::define_static_string(std::meta::display_string_of(std::meta::type_of(rx))),
                std::define_static_string(std::meta::identifier_of(rx))
    );*/


    //template for (constexpr auto member2 : std::define_static_array(std::meta::template_arguments_of(^^))) {
    //    std::println("===================={}", std::meta::display_string_of(member2));
    //}

    //for (auto it : ti) {
        ti->print();
    //}
    
    return 0;
}