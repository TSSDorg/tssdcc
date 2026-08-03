#include <span>
#include "gtest/gtest.h"

#include "tssd.h"
#include "flat.h"
#include "basic.h"
#include "types.h"

TEST(TSSD, MarshalUnmarshaBasicType) {

    BasicTypeFlat bta1;
    Basic::rand(&bta1.basicType.vbool, sizeof(BasicType));

    Manager::Register<BasicTypeFlat>();

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

    BasicTypeFlat bta2;
    //Basic::rand(&bt2.vbool, sizeof(BasicType));
    std::memset(&bta2.basicType.vbool, 0, sizeof(BasicType));

    EXPECT_TRUE(memcmp(&bta1, &bta2, Basic::SizeofFlat<BasicTypeFlat>()) != 0);

    EXPECT_EQ(Manager::UnmarshalTo(rbuf, bta2), OK);

    EXPECT_TRUE(Cpeq<BasicTypeFlat>().Equal(bta1, bta2));


    std::span<std::byte> s1, s2;
    bool ret;
    #define CMP(x, y) Basic::BytesEqual(std::span((std::byte*)&x, sizeof(x)), std::span((std::byte*)&y, sizeof(y)))

    //EXPECT_TRUE(CMP(bta1. vint64,bta2. vint64));
    EXPECT_TRUE(CMP(bta1.basicType.vbool,  bta2.basicType.vbool));
    EXPECT_TRUE(CMP(bta1.basicType.vbool,  bta2.basicType.vbool));
    EXPECT_TRUE(CMP(bta1.basicType.  vbool,bta2.basicType.  vbool));
    EXPECT_TRUE(CMP(bta1.basicType.  vint8,bta2.basicType.  vint8));
    EXPECT_TRUE(CMP(bta1.basicType. vuint8,bta2.basicType. vuint8));
    EXPECT_TRUE(CMP(bta1.basicType. vint16,bta2.basicType. vint16));
    EXPECT_TRUE(CMP(bta1.basicType.vuint16,bta2.basicType.vuint16));
    EXPECT_TRUE(CMP(bta1.basicType. vint32,bta2.basicType. vint32));
    EXPECT_TRUE(CMP(bta1.basicType.vuint32,bta2.basicType.vuint32));
    EXPECT_TRUE(CMP(bta1.basicType. vint64,bta2.basicType. vint64));
    EXPECT_TRUE(CMP(bta1.basicType.vuint64,bta2.basicType.vuint64));
    EXPECT_TRUE(CMP(bta1.basicType.    f32,bta2.basicType.    f32));
    EXPECT_TRUE(CMP(bta1.basicType.    f64,bta2.basicType.    f64));
    #undef CMP

}


TEST(TSSD, MarshalUnmarshaBasicSizeofFlat) {

    EXPECT_EQ(Basic::SizeofFlat<BasicTypeFlat>(), 48);
}


TEST(TSSD, MarshalUnmarshaBasicTypeArray) {

    BasicArrayFlat bta1;
    Basic::rand(&bta1.basicArray.vbool[0],  sizeof(BasicArray));

    std::cout << "bta1 addr:" << std::addressof(bta1)
              << ", bt1.vint64 addr:" << std::addressof(bta1.basicArray.vint64[0])
              << ", value:" << bta1.basicArray.vint64[0] << std::endl;

    Manager::Register<BasicArrayFlat>();

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

    BasicArrayFlat bta2;
    memset(&bta2.basicArray.vbool[0], 0, sizeof(BasicArray));
    //Basic::rand(&bt2.vint8, sizeof(BasicTypeArray));
    EXPECT_EQ(Manager::UnmarshalTo(rbuf, bta2), OK);

    EXPECT_TRUE(Cpeq<BasicArrayFlat>().Equal(bta1, bta2));


    Manager::print(bta1.basicArray.vuint32, sizeof(bta1.basicArray.vuint32), "bta1.vuint32:");
    Manager::print(bta2.basicArray.vuint32, sizeof(bta2.basicArray.vuint32), "bta2.vuint32:");
    Manager::print(bta1.basicArray.vint64,  sizeof(bta1.basicArray.vint64), "bta1:");
    Manager::print(bta2.basicArray.vint64,  sizeof(bta2.basicArray.vint64), "bta2:");

    std::span<std::byte> s1, s2;
    bool ret;
    #define CMP(x, y) Basic::BytesEqual(std::span((std::byte*)&x[0], sizeof(x)), std::span((std::byte*)&y[0], sizeof(y)))

    //EXPECT_TRUE(CMP(bta1. vint64,bta2. vint64));
    EXPECT_TRUE(CMP(bta1.basicArray.  vbool,bta2.basicArray.  vbool));
    EXPECT_TRUE(CMP(bta1.basicArray.  vint8,bta2.basicArray.  vint8));
    EXPECT_TRUE(CMP(bta1.basicArray. vuint8,bta2.basicArray. vuint8));
    EXPECT_TRUE(CMP(bta1.basicArray. vint16,bta2.basicArray. vint16));
    EXPECT_TRUE(CMP(bta1.basicArray.vuint16,bta2.basicArray.vuint16));
    EXPECT_TRUE(CMP(bta1.basicArray. vint32,bta2.basicArray. vint32));
    EXPECT_TRUE(CMP(bta1.basicArray.vuint32,bta2.basicArray.vuint32));
    EXPECT_TRUE(CMP(bta1.basicArray. vint64,bta2.basicArray. vint64));
    EXPECT_TRUE(CMP(bta1.basicArray.vuint64,bta2.basicArray.vuint64));
    EXPECT_TRUE(CMP(bta1.basicArray.    f32,bta2.basicArray.    f32));
    EXPECT_TRUE(CMP(bta1.basicArray.    f64,bta2.basicArray.    f64));
    #undef CMP

}
