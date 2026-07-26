#include <cstdlib>
#include <ctime>

#include "gtest/gtest.h"

#include "tssd.h"
#include "flat.h"

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
/*
    pFlatable Build() const override {
        return std::make_shared<BasicType>();
    }
*/
    std::string Group() const override {
        return "BasicType";
    }

    std::string Version() const override {
        return "BasicType";
    }
    unsigned bounded_rand(unsigned range)
    {
        for (unsigned x, r;;) {
            x = std::rand();
            r = x % range;
            if (x - r <= -range)
                return r;
        }
    }

    //produce random value
    void rand()
    {

        std::srand(std::time({})); // use current time as seed for random generator
        auto *p = (std::byte *)&this->vbool;
        memset(p, 0, sizeof(BasicType));
        for (int i=0; i<sizeof(BasicType); i++)
        {
            const int random_value = bounded_rand(256);
            p[i] =  (std::byte) random_value;
        }
    }
};


TEST(TSSD, MarshalUnmarsha) {

    BasicType bt1;
    bt1.rand();

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
    bt2.rand();
    EXPECT_EQ(Manager::UnmarshalTo(rbuf, bt2), OK);
    auto ret = memcmp(&bt1, &bt2, sizeof(BasicType));
    EXPECT_EQ(ret, 0);

}
