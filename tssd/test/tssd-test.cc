#include <span>
#include "gtest/gtest.h"

#include "tssd.h"
#include "flat.h"
#include "basic.h"

struct BasicType : public Flatable {
    bool  vbool;
    std::int8_t vint8;
    std::uint8_t vuint8;
    std::int16_t vint16;
    std::uint16_t vuint16;
    std::int32_t  vint32;
    std::uint32_t vuint32;
    std::int64_t  vint64;
    std::uint64_t vuint64;
    float        f32;
    double       f64;

    std::string Group() const override {
        return "BasicType";
    }

    std::string Version() const override {
        return "BasicType";
    }
};


TEST(TSSD, MarshalUnmarshaBasicType) {

    BasicType bta1;
    Basic::rand(&bta1.vbool, Basic::SizeofFlat<BasicType>());


    Manager::Register<BasicType>();

    Buffer buf;
    EXPECT_EQ(Manager::MarshalTo(bta1, buf.clear()), OK);

    buf.print("after MarshalTo:");

    Buffer rbuf;  ///read/receive/unmarshal buf
    pFragment frag=std::make_shared<Fragment>(1024);  ///read fragment

    auto list = buf.Fragments();

    for (int i=0; i<list.size(); i++) {
        int remain_pos = 0;
        std::span sp(list[0]->data.data(), list[0]->data.size());
        if (auto ret = frag->Unmarshal(sp, remain_pos)) {
            std::println("Unarshal frag error", ret);
        }
        rbuf.push(frag);
    }

    EXPECT_EQ(rbuf.wanted(),0);

    BasicType bta2;
    //Basic::rand(&bt2.vbool, sizeof(BasicType));
    std::memset(&bta2.vbool, 0, Basic::SizeofFlat<BasicType>());

    EXPECT_TRUE(memcmp(&bta1, &bta2, Basic::SizeofFlat<BasicType>()) != 0);

    EXPECT_EQ(Manager::UnmarshalTo(rbuf, bta2), OK);

    std::span<std::byte> s1, s2;
    bool ret;
    #define CMP(x, y) Basic::BytesEqual(std::span((std::byte*)&x, sizeof(x)), std::span((std::byte*)&y, sizeof(y)))

    //EXPECT_TRUE(CMP(bta1. vint64,bta2. vint64));
    EXPECT_TRUE(CMP(bta1.vbool, bta2.vbool));
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
    #undef CMP

}

struct BasicTypeArray : public Flatable {
    bool            vbool[1];
    std::int8_t     vint8[2];
    std::uint8_t   vuint8[3];
    std::int16_t   vint16[4];
    std::uint16_t vuint16[5];
    std::int32_t   vint32[6];
    std::uint32_t vuint32[7];
    std::int64_t   vint64[5];
    std::uint64_t vuint64[9];
    float             f32[3];
    double            f64[2];

    std::string Group() const override {
        return "BasicType";
    }

    std::string Version() const override {
        return "BasicTypeArray";
    }
};


TEST(TSSD, MarshalUnmarshaBasicSizeofFlat) {

    EXPECT_EQ(Basic::SizeofFlat<BasicType>(), 48);
}


TEST(TSSD, MarshalUnmarshaBasicTypeArray) {

    BasicTypeArray bta1;
    Basic::rand(&bta1.vbool[0], Basic::SizeofFlat<BasicTypeArray>());

    std::cout << "bta1 addr:" << std::addressof(bta1)
              << ", bt1.vint64 addr:" << std::addressof(bta1.vint64[0])
              << ", value:" << bta1.vint64[0] << std::endl;

    Manager::Register<BasicTypeArray>();

    Buffer buf(256);
    EXPECT_EQ(Manager::MarshalTo(bta1, buf.clear()), OK);

    buf.print("after MarshalTo:");

    Buffer rbuf;  ///read/receive/unmarshal buf


    auto list = buf.Fragments();

    for (int i=0; i<list.size(); i++) {
        pFragment frag=std::make_shared<Fragment>(2048);  ///read fragment
        int remain_pos = 0;
        std::span sp(list[i]->data.data(), list[i]->data.size());
        if (auto ret = frag->Unmarshal(sp, remain_pos)) {
            std::println("Unarshal frag error", ret);
        }
        rbuf.push(frag);
    }

    EXPECT_EQ(rbuf.wanted(),0);

    BasicTypeArray bta2;
    memset(&bta2.vbool[0], 0, Basic::SizeofFlat<BasicTypeArray>());
    //Basic::rand(&bt2.vint8, sizeof(BasicTypeArray));
    EXPECT_EQ(Manager::UnmarshalTo(rbuf, bta2), OK);


    Manager::print(bta1.vuint32, sizeof(bta1.vuint32), "bta1.vuint32:");
    Manager::print(bta2.vuint32, sizeof(bta2.vuint32), "bta2.vuint32:");

    Manager::print(bta1.vint64, sizeof(bta1.vint64), "bta1:");
    Manager::print(bta2.vint64, sizeof(bta2.vint64), "bta2:");

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
    #undef CMP

}
