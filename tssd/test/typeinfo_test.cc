#include "gtest/gtest.h"

#include <vector>
#include <set>
#include <list>
#include <map>
#include <unordered_map>
#include <time.h>

#include "time_rfc3339.h"
#include "typeinfo.h"
#include "basic.h"
#include "types.h"

using namespace std;
using namespace tssd;


TEST(TypeInfo, MarshalUnmarshaBasicTypeArray) {
    Buffer buf;

    auto tmp = TypeInfo::Create<BasicArray>();
    tmp->print();
    BasicArray bta1, bta2;
    Basic::rand(&bta1, sizeof(bta1));
    std::memset(&bta2, 0, sizeof(bta2));

    tmp->MarshalTo(&bta1, buf);
    buf.print();
    buf.Finish();

    buf.print("after finish");
    EXPECT_FALSE(tmp->UnmarshalTo(buf, &bta2));
    EXPECT_TRUE(Cpeq<BasicArray>().Equal(bta1, bta2));

    BasicArray bta3;
    Basic::rand(&bta3, sizeof(bta3));
    Cpeq<BasicArray>().Copy(bta1, bta3);
    EXPECT_TRUE(Cpeq<BasicArray>().Equal(bta3, bta2));

    std::span<std::byte> s1, s2;
    bool ret;
    #define CMP(x, y) Basic::BytesEqual(std::span((std::byte*)&x[0], sizeof(x)), std::span((std::byte*)&y[0], sizeof(y)))

    //EXPECT_TRUE(CMP(bta1. vint64,bta2. vint64));
    EXPECT_TRUE(CMP(bta1.vbool, bta2.vbool));
    EXPECT_TRUE(CMP(bta1.  vbool,bta2.  vbool));
    EXPECT_TRUE(CMP(bta1.  vint8,bta2.  vint8));
    EXPECT_TRUE(CMP(bta1. vuint8,bta2. vuint8));
    EXPECT_TRUE(CMP(bta1. vint16,bta2. vint16));
    EXPECT_TRUE(CMP(bta1.vuint16,bta2.vuint16));
    EXPECT_TRUE(CMP(bta1. vint32,bta2. vint32));
    EXPECT_TRUE(CMP(bta1.vuint32,bta2.vuint32));
    EXPECT_TRUE(CMP(bta1. vint64,bta2. vint64));
    EXPECT_TRUE(CMP(bta1.vuint64,bta2.vuint64));
    EXPECT_TRUE(CMP(bta1.    f32,bta2.    f32));
    EXPECT_TRUE(CMP(bta1.    f64,bta2.    f64));

    EXPECT_TRUE(CMP(bta1.  vbool1, bta2.  vbool1));
    EXPECT_TRUE(CMP(bta1.  vint81, bta2.  vint81));
    EXPECT_TRUE(CMP(bta1. vuint81, bta2. vuint81));
    EXPECT_TRUE(CMP(bta1. vint161, bta2. vint161));
    EXPECT_TRUE(CMP(bta1.vuint161, bta2.vuint161));
    EXPECT_TRUE(CMP(bta1. vint321, bta2. vint321));
    EXPECT_TRUE(CMP(bta1.vuint321, bta2.vuint321));
    EXPECT_TRUE(CMP(bta1. vint641, bta2. vint641));
    EXPECT_TRUE(CMP(bta1.vuint641, bta2.vuint641));
    EXPECT_TRUE(CMP(bta1.    f321, bta2.    f321));
    EXPECT_TRUE(CMP(bta1.    f641, bta2.    f641));

    #undef CMP
    //EXPECT_EQ(memcmp(&bta1, &bta2, sizeof(BasicArray)), 0);
    //auto s1 = std::span((std::byte*)&bta1, sizeof(BasicArray));
    //auto s2 = std::span((std::byte*)&bta2, sizeof(BasicArray));
    //EXPECT_TRUE(Basic::BytesEqual(s1, s2));
}

struct Containers {
    vector<string> vstr;
    list<int8_t> lint8;
    set<string> ss;
    map<string, int32_t> mp;
};

TEST(TypeInfo, MarshalUnmarshaContainer) {
    Buffer buf;

    auto tmp = TypeInfo::Create<Containers>();
    tmp->print();
    Containers bta1, bta2;
    //Basic::rand(&bta1.vint8[0], bta1.vint8.size());
    bta1.vstr.emplace_back("haha");
    bta1.vstr.emplace_back("blabla");
    bta1.lint8.emplace_back((int8_t)1);
    bta1.lint8.emplace_back((int8_t)2);
    bta1.mp["hello"] = 1234;
    bta1.mp["foo"] = 5678;
    bta1.ss.insert("wrold");

    tmp->MarshalTo(&bta1, buf);
    buf.print();
    buf.Finish();

    buf.print("after finish");
    bta2.vstr.emplace_back("======");
    bta2.ss.insert("yyyy");
    bta2.mp["x"] = 2;
    bta2.lint8.emplace_back((int8_t)10);
    EXPECT_FALSE(tmp->UnmarshalTo(buf, &bta2));

    EXPECT_TRUE(Cpeq<Containers>().Equal(bta1, bta2));

    Containers bta3;
    bta3.vstr.emplace_back("999999");
    bta3.mp["bar"] = 12;
    bta3.ss.insert("xxx");
    bta3.lint8.emplace_back((int8_t)12);
    Cpeq<Containers>().Copy(bta2, bta3);
    EXPECT_TRUE(Cpeq<Containers>().Equal(bta1, bta3));

}

struct EqualTest {
    int8_t vint;
    int16_t  vint16_t[5];
    string  s1[2];
    list<int32_t> li;
    set<int16_t> si;
};


TEST(TypeInfo, TypeInfoEqual) {
    auto ti = TypeInfo::Create<EqualTest>();
    EqualTest et1, et2;

    et1.vint = 123;
    et1.vint16_t[0]= 134;
    et1.vint16_t[1]= 233;
    et1.vint16_t[2]=  0;
    et1.vint16_t[3]=  789;
    et1.vint16_t[4]=  12345;
    et1.s1[0] = "123";
    et1.s1[1] = "456";
    et1.li.emplace_back(5);
    et1.li.emplace_back(6);
    et1.si.insert(788);
    et1.si.insert(6678);

    Buffer buf;

    Containers c1, c2;

    EXPECT_FALSE(ti->MarshalTo(&et1, buf));
    buf.Finish();

    EXPECT_FALSE(ti->UnmarshalTo(buf, &et2));

    EXPECT_TRUE(ti->Equal(et1, et2));
    Cpeq<EqualTest> cmp;
    EXPECT_TRUE(cmp.Equal(et1, et2));
    EXPECT_TRUE(Cpeq<Containers>().Equal(c1, c2));
}


template<typename T, typename Compare = LexCompare<T>>
void TestContainerT(T ba1) {
    auto ti = TypeInfo::Create<ContainerT<T, Compare>>();
    ContainerT<T, Compare> cba1, cba2, cba3;

    cba1.vec.push_back(ba1);
    cba1.vec.push_back(ba1);
    cba1.lst.push_back(ba1);
    cba1.lst.push_back(ba1);
    cba1.st.insert(ba1);
    //cba1.st.insert(ba1);
    cba1.mp["hello"] = ba1;
    cba1.mp["world"] = ba1;
    cba1.ump["foo"] = ba1;
    cba1.ump[""] = ba1;
    cba1.sp = make_shared<T>(ba1);
    cba1.up = make_unique<T>(ba1);

    Buffer buf;

    EXPECT_FALSE(ti->MarshalTo(&cba1, buf));
    buf.Finish();
    EXPECT_FALSE(ti->UnmarshalTo(buf, &cba2));
    EXPECT_TRUE(ti->Equal(cba1, cba2));
    Cpeq<ContainerT<T, Compare>> cmp;

    cmp.Copy(cba1, cba3);
    EXPECT_TRUE(cmp.Equal(cba3, cba2));
}


TEST(TypeInfo, ContainerT) {
    BasicArray ba1;
    Basic::rand(&ba1, sizeof(ba1));
    TestContainerT<BasicArray>(ba1);
    TestContainerT<int>(Basic::rand<int>());
    TestContainerT<char>(Basic::rand<char>());
    TestContainerT<unsigned char>(Basic::urand<unsigned char>());
    TestContainerT<short>(Basic::rand<short>());
    TestContainerT<unsigned short>(Basic::urand<unsigned short>());
    TestContainerT<int32_t>(Basic::rand<int32_t>());
    TestContainerT<uint32_t>(Basic::urand<uint32_t>());
    TestContainerT<int64_t>(Basic::rand<int64_t>());
    TestContainerT<uint64_t>(Basic::urand<uint64_t>());
    float f;
    Basic::rand(&f, sizeof(f));
    double d;
    Basic::rand(&d, sizeof(d));
    TestContainerT<float>(f);
    TestContainerT<double>(d);
    BasicType bt1;
    Basic::rand(&bt1, sizeof(bt1));
    TestContainerT<BasicType>(bt1);
    TestContainerT<byte>((byte)Basic::urand<unsigned char>());
    TestContainerT<string>(Basic::RandomString());
}


struct CTest : public Struct2<int, string>, BasicArray {};
struct CTest2 : public CTest {
    int y;
};

TEST(TypeInfo, TypeInfoParent) {
    auto ti = TypeInfo::Create<CTest2>();
    ti->print();
    CTest2 et1, et2, et3;

    et1.v1 = 456789;
    et1.v2 = "hello tssd";
    et1.y = 234;

    Buffer buf;

    EXPECT_FALSE(ti->MarshalTo(&et1, buf));
    buf.Finish();

    EXPECT_FALSE(ti->UnmarshalTo(buf, &et2));

    EXPECT_EQ(et1.v1, et2.v1);
    EXPECT_TRUE(et1.v2 == et2.v2);
    EXPECT_EQ(et1.y, et2.y);

    EXPECT_TRUE(ti->Equal(et1, et2));
    Cpeq<CTest2> cmp;
    cmp.Copy(et2, et3);
    EXPECT_TRUE(cmp.Equal(et3, et1));
}


TEST(TypeInfo, TypeInfoBytes) {

    auto ti = TypeInfo::Create<Array1<std::byte, 2>>();
    ti->print();
    EXPECT_EQ(ti->children_[0]->node_.tssd_type_, TType::Tarraym);
    EXPECT_EQ(ti->children_[0]->children_[0]->node_.tssd_type_, TType::Tuint8);
    EXPECT_TRUE(ti->children_[0]->children_[0]->node_.is_number_);
    Array1<std::byte, 2> et1, et2, et3;

    et1.v1[0] = byte(1);
    et1.v1[1] = byte(2);

    Buffer buf;
    EXPECT_FALSE(ti->MarshalTo(&et1, buf));
    buf.Finish();
    buf.print("CByte: ");

    std::memset(&et2, 0, sizeof(et2));
    EXPECT_FALSE(ti->UnmarshalTo(buf, &et2));

    EXPECT_TRUE(ti->Equal(et1, et2));
    Cpeq<Array1<std::byte, 2>> cmp;
    cmp.Copy(et2, et3);
    EXPECT_TRUE(cmp.Equal(et3, et1));
    using array = Array1<std::byte, 2>;
    EXPECT_TRUE(Cpeq<array>().Equal(et3, et1));
    EXPECT_TRUE((Cpeq<Array1<std::byte, 2>>()).Equal(et3, et1));
}

TEST(TypeInfo, TypeInfoContainerBytes) {

    auto ti = TypeInfo::Create<ContainerT<std::byte>>();
    ti->print();
    EXPECT_EQ(ti->children_[0]->node_.tssd_type_, TType::Tarraym);
    EXPECT_EQ(ti->children_[0]->children_[0]->node_.tssd_type_, TType::Tuint8);
    EXPECT_TRUE(ti->children_[0]->children_[0]->node_.is_number_);
    EXPECT_EQ(ti->children_[1]->node_.tssd_type_, TType::Tarraym);
    EXPECT_EQ(ti->children_[2]->node_.tssd_type_, TType::Tarraym);

    ContainerT<std::byte> et1, et2, et3;
    et1.vec.push_back(byte('a'));
    et1.vec.push_back(byte('b'));
    et1.lst.push_back(byte('c'));
    et1.st.insert(byte('d'));

    Buffer buf;
    EXPECT_FALSE(ti->MarshalTo(&et1, buf));
    buf.Finish();
    buf.print("TypeInfoContainerBytes: ");

    //std::memset(&et2, 0, sizeof(et2));
    EXPECT_FALSE(ti->UnmarshalTo(buf, &et2));

    EXPECT_TRUE(ti->Equal(et1, et2));
    Cpeq<ContainerT<std::byte>> cmp;
    cmp.Copy(et2, et3);
    EXPECT_TRUE(cmp.Equal(et3, et1));
    using array = ContainerT<std::byte>;
    EXPECT_TRUE(Cpeq<array>().Equal(et3, et1));
    EXPECT_TRUE((Cpeq<ContainerT<std::byte>>()).Equal(et3, et1));
}

struct CRefTest{
    int ii_;
    int &ref_;
    string &str;
    list<string> &lst;
    CRefTest(int i, string &s, list<string> lt) : ii_(i), ref_(ii_), str(s), lst(lt) {}
};


TEST(TypeInfo, TypeInfoRefTest) {

    auto ti = TypeInfo::Create<CRefTest>();
    ti->print();

    string str("hello tssd"), str2;

    list<string> l1, l2;

    CRefTest et1(5, str, l1), et2(6, str2, l2), et3(7, str2, l2);
    et1.lst.push_back("hello");
    et1.lst.push_back("fooo");

    Buffer buf;
    EXPECT_FALSE(ti->MarshalTo(&et1, buf));
    buf.Finish();
    buf.print("TypeInfoRefTest: ");

    //std::memset(&et2, 0, sizeof(et2));
    EXPECT_FALSE(ti->UnmarshalTo(buf, &et2));
    EXPECT_TRUE(et2.str==str);
    EXPECT_TRUE(ti->Equal(et1, et2));
    //str2 = "xxxx";
    Cpeq<CRefTest> cmp;
    cmp.Copy(et2, et3);
    EXPECT_TRUE(cmp.Equal(et3, et1));
    EXPECT_TRUE(str == str2);
}

struct sttime {
    timespec ts;
    std::chrono::time_point<std::chrono::system_clock,
                            std::chrono::nanoseconds> t;
};


TEST(TypeInfo, TypeInfoTimePoint)
{

    auto ti = TypeInfo::Create<sttime>();
    ti->print();
    // The new way
    auto const now = std::chrono::system_clock::now();
    //std::time_t newt = std::chrono::system_clock::to_time_t(now);

    sttime st{
        time_rfc3339::Time::timepointToTimespec(now),
        now
     }, st2;


    Buffer buf;
    EXPECT_FALSE(ti->MarshalTo(&st, buf));
    buf.Finish();
    buf.print("TypeInfoTimePoint: ");

    EXPECT_FALSE(ti->UnmarshalTo(buf, &st2));
    EXPECT_TRUE(ti->Equal(st, st2));
    cout << "st:" << time_rfc3339::Time(st.t).formatNano() << endl;
    cout << "st2:" << time_rfc3339::Time(st2.t).formatNano() << endl;
}
