#include <meta>
#include <iostream>
#include <print>
#include <vector>
#include <memory>

//#include "flat.h"

class TypeInfo {

    struct Node {
        char const* type_;
        char const* name_;
        std::ptrdiff_t offset_;
        std::ptrdiff_t total_offset_;
        int8_t tssd_type_;
        
        constexpr Node(char const *type, char const *name, ptrdiff_t offset) 
            : name_(name), type_(type), offset_(offset) {}
    };   

    Node node_;
    std::vector<TypeInfo> children_;

    constexpr TypeInfo(char const *type,
            char const *name,
            std::ptrdiff_t offset) : 
            node_(type, name, offset) {}   

    constexpr TypeInfo(char const *type,
            char const *name,
            std::ptrdiff_t offset,
            std::vector<TypeInfo> ch) : 
            node_(type, name, offset), 
            children_(ch) {}

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
                });
            } else 
            {
                if constexpr(std::meta::has_template_arguments(std::meta::type_of(member))) {
                        if  constexpr (std::meta::template_of(std::meta::type_of(member)) == ^^std::vector) {
                        using FieldT = [:std::meta::template_arguments_of(std::meta::type_of(member))[0]:];            
                        std::println("parse template vector");
                        children.push_back(
                        {
                            std::define_static_string(std::meta::display_string_of(std::meta::type_of(member))),
                            std::define_static_string(std::meta::identifier_of(member)),
                            std::meta::offset_of(member).bytes,
                            parse<FieldT>(),
                        });
                        
                    }
                } else {
                    children.push_back(
                    {
                        std::define_static_string(std::meta::display_string_of(std::meta::type_of(member))),
                        std::define_static_string(std::meta::identifier_of(member)),
                        std::meta::offset_of(member).bytes,
                        parse<FieldT>(),
                    });
                }
            }
            std::println("name: {}, children size: {}", std::define_static_string(std::meta::identifier_of(member)), children.size());
        }
        
        return children;
    }               
public:

    constexpr TypeInfo(char const *type,
        std::vector<TypeInfo> ch) : 
        node_(type, type, 0), 
        children_(ch) {}     

    void print() const {
    std::println("result: {} {} {} {}", node_.type_, node_.name_, node_.offset_, children_.size());
    for (int i=0; i<children_.size(); i++) 
        children_[i].print();
    }

    template <typename T> 
    static auto Create(const std::string &type="") {
        return std::make_shared<TypeInfo>(type.c_str(), parse<T>());
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