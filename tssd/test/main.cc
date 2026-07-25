#include <iostream>
#include <print>
#include <string>

#include "flat.h"
#include "buffer.h"
#include "tssd.h"

#include "gtest/gtest.h"

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



int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);

    //TypeInfo ti("MyStruct", "MyStruct", 0, TypeInfo::parse<MyStruct>());
/*
    std::println("std::string has template: {}, {}",
        std::meta::has_template_arguments(^^std::vector<char>),
        std::meta::is_same_type(^^std::string, ^^std::string)
    );

    Buffer buf;

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
*/

    std::println("succ");

    return RUN_ALL_TESTS();
}
