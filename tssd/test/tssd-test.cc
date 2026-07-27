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

    BasicType bt1;
    Basic::rand(&bt1.vbool, Basic::SizeofFlat<BasicType>());

    std::cout << "bt1 addr:" << std::addressof(bt1)
              << ", bt1.vbool addr:" << std::addressof(bt1.vbool) << std::endl;

    Manager::Register<BasicType>();

    Buffer buf;
    EXPECT_EQ(Manager::MarshalTo(bt1, buf.clear()), OK);

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

    BasicType bt2;
    //Basic::rand(&bt2.vbool, sizeof(BasicType));
    std::memset(&bt2.vbool, 0, Basic::SizeofFlat<BasicType>());

    EXPECT_TRUE(memcmp(&bt1, &bt2, Basic::SizeofFlat<BasicType>()) != 0);

    EXPECT_EQ(Manager::UnmarshalTo(rbuf, bt2), OK);
    //EXPECT_EQ(memcmp(&bt1.vbool, &bt2.vbool, Basic::SizeofFlat<BasicType>()), 0);
    auto s1 = std::span((std::byte*)&bt1.vbool, Basic::SizeofFlat<BasicType>());
    auto s2 = std::span((std::byte*)&bt2.vbool, Basic::SizeofFlat<BasicType>());
    EXPECT_TRUE(Basic::BytesEqual(s1, s2));
}

struct BasicTypeArray : public Flatable {
    bool            vbool[1];
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

    BasicTypeArray bt1;
    Basic::rand(&bt1.vbool, Basic::SizeofFlat<BasicType>());

    //std::cout << "bt1 addr:" << std::addressof(bt1)
    //          << ", bt1.vbool addr:" << std::addressof(bt1.vbool) << std::endl;

    Manager::Register<BasicTypeArray>();

    Buffer buf;
    EXPECT_EQ(Manager::MarshalTo(bt1, buf.clear()), OK);


    buf.print("after MarshalTo:");

    Buffer rbuf;  ///read/receive/unmarshal buf
    pFragment frag=std::make_shared<Fragment>(2048);  ///read fragment

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

    BasicTypeArray bt2;
    memset(&bt2.vbool, 0, Basic::SizeofFlat<BasicType>());
    //Basic::rand(&bt2.vint8, sizeof(BasicTypeArray));
    EXPECT_EQ(Manager::UnmarshalTo(rbuf, bt2), OK);

    auto s1 = std::span((std::byte*)&bt1.vbool, Basic::SizeofFlat<BasicType>());
    auto s2 = std::span((std::byte*)&bt2.vbool, Basic::SizeofFlat<BasicType>());
    EXPECT_TRUE(Basic::BytesEqual(s1, s2));
}
