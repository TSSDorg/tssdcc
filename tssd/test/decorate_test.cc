#include <memory>
#include <span>
#include "gtest/gtest.h"

#include "tssd.h"
#include "flat.h"
#include "basic.h"
#include "types.h"
using namespace tssd;
using namespace std;


struct DecorateTest : public tssd::Flatable {
    string name;
    uint16_t age;
    std::string Family() const override {
        return "DecorateTestFamily";
    }
    std::string Version() const override {
        return "DecorateTest1";
    }
    std::string Progeny() const { return "DecorateTest2"; }
};

struct DecorateTest2 : public tssd::Flatable {
    string name;
    uint16_t age;
    string addr;
    std::string Family() const override {
        return "DecorateTestFamily";
    }
    std::string Version() const override {
        return "DecorateTest2";
    }
    std::string Progeny() const { return "DecorateTest3"; }

    TError Decorate(const pFlatable other) {
        auto v1 = std::dynamic_pointer_cast<DecorateTest>(other);
        if (!v1) return ERR_SCHEMA_NOT_MATCH;
        name = v1->name;
        age = v1->age;
        addr = "default address";
        return OK;
    }
};

struct DecorateTest3 : public tssd::Flatable {
    string name;
    uint16_t age;
    string addr[2];
    std::string Family() const override {
        return "DecorateTestFamily";
    }
    std::string Version() const override {
        return "DecorateTest3";
    }

    TError Decorate(const pFlatable other) {
        auto v2 = std::dynamic_pointer_cast<DecorateTest2>(other);
        if (!v2) return ERR_SCHEMA_NOT_MATCH;
        name = v2->name;
        age = v2->age;
        addr[0] = v2->addr;
        addr[1] = "default address 2";
        return OK;
    }
};

TEST(Decoreate, V1ToV2) {

    DecorateTest dtin;
    dtin.name = Basic::RandomString(8);
    Basic::rand(&dtin.age, sizeof(dtin.age));

    Manager::Register<DecorateTest>();

    Buffer buf(256);
    EXPECT_EQ(Manager::MarshalTo(dtin, buf.Clear()), OK);

    buf.print("after MarshalTo:");

    DecorateTest dtout;
    EXPECT_EQ(Manager::UnmarshalTo(buf, dtout), OK);

    EXPECT_TRUE(Cpeq<DecorateTest>().Equal(dtin, dtout));

    //v1 - > v2
    Manager::Register<DecorateTest2>();
    DecorateTest2 dt2out;
    buf.Rewind();
    EXPECT_EQ(Manager::UnmarshalTo(buf, dt2out), OK);
    EXPECT_EQ(dtin.name, dt2out.name);
    EXPECT_EQ(dtin.age, dt2out.age);
    EXPECT_EQ(dt2out.addr, "default address");
}

TEST(Decoreate, V1ToV3) {
    DecorateTest dtin;
    dtin.name = Basic::RandomString(8);
    Basic::rand(&dtin.age, sizeof(dtin.age));

    Manager::Register<DecorateTest>();

    Buffer buf(256);
    EXPECT_EQ(Manager::MarshalTo(dtin, buf.Clear()), OK);

    buf.print("after MarshalTo:");

    DecorateTest dtout;
    EXPECT_EQ(Manager::UnmarshalTo(buf, dtout), OK);

    EXPECT_TRUE(Cpeq<DecorateTest>().Equal(dtin, dtout));

    //v1 - > v2
    Manager::Register<DecorateTest2>();
    Manager::Register<DecorateTest3>();
    DecorateTest3 dt3out;
    buf.Rewind();
    EXPECT_EQ(Manager::UnmarshalTo(buf, dt3out), OK);
    EXPECT_EQ(dtin.name, dt3out.name);
    EXPECT_EQ(dtin.age, dt3out.age);
    EXPECT_EQ(dt3out.addr[0], "default address");
    EXPECT_EQ(dt3out.addr[1], "default address 2");
}
