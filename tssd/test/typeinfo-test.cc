#include "gtest/gtest.h"

#include <vector>

#include "typeinfo.h"
#include "basic.h"

using namespace std;

struct BasicArray {
    bool            vbool[2];
    std::int8_t     vint8[2];
    std::uint8_t   vuint8[3];
    std::int16_t   vint16[4];
    std::uint16_t vuint16[5];
    std::int32_t   vint32[6];
    std::uint32_t vuint32[7];
    std::int64_t   vint64[8];
    std::uint64_t vuint64[9];
    float             f32[3];
    double            f64[2];
    bool            vbool1[1];
    std::int8_t     vint81[1];
    std::uint8_t   vuint81[1];
    std::int16_t   vint161[1];
    std::uint16_t vuint161[1];
    std::int32_t   vint321[1];
    std::uint32_t vuint321[1];
    std::int64_t   vint641[1];
    std::uint64_t vuint641[1];
    float             f321[1];
    double            f641[1];
};

TEST(TypeInfo, MarshalUnmarshaBasicTypeArray) {
    Buffer buf;

    auto tmp = TypeInfo::Create<BasicArray>();
    tmp->print();
    BasicArray bta1, bta2;
    Basic::rand(&bta1, sizeof(bta1));
    std::memset(&bta2, 0, sizeof(bta2));

    tmp->MarshalTo(&bta1, buf);
    buf.print();
    buf.finish();

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
    buf.finish();

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
    buf.finish();

    EXPECT_FALSE(ti->UnmarshalTo(buf, &et2));

    EXPECT_TRUE(ti->Equal(et1, et2));
    Cpeq<EqualTest> cmp;
    EXPECT_TRUE(cmp.Equal(et1, et2));
    EXPECT_TRUE(Cpeq<Containers>().Equal(c1, c2));
}
