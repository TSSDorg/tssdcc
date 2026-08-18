#include <memory>
#include <span>
#include "gtest/gtest.h"

#define private public
#include "tssd.h"
#include "flat.h"
#include "basic.h"
#include "types.h"
#undef private
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

TEST(Decoreate, V2ToV3) {
    DecorateTest2 dtin;
    dtin.name = Basic::RandomString(8);
    dtin.addr = Basic::RandomString(16);
    Basic::rand(&dtin.age, sizeof(dtin.age));

    Manager::Register<DecorateTest2>();

    Buffer buf(256);
    EXPECT_EQ(Manager::MarshalTo(dtin, buf.Clear()), OK);

    buf.print("after MarshalTo:");

    DecorateTest2 dtout;
    EXPECT_EQ(Manager::UnmarshalTo(buf, dtout), OK);
    EXPECT_TRUE(Cpeq<DecorateTest2>().Equal(dtin, dtout));

    //v2 - > v3
    Manager::Register<DecorateTest3>();
    DecorateTest3 dt3out;
    buf.Rewind();
    EXPECT_EQ(Manager::UnmarshalTo(buf, dt3out), OK);
    EXPECT_EQ(dtin.name, dt3out.name);
    EXPECT_EQ(dtin.age, dt3out.age);
    EXPECT_EQ(dt3out.addr[0], dtin.addr);
    EXPECT_EQ(dt3out.addr[1], "default address 2");

    //v2 -> v1 should fail
    Manager::Register<DecorateTest>();
    DecorateTest dt1out;
    buf.Rewind();
    EXPECT_TRUE(Manager::UnmarshalTo(buf, dt1out) != OK);
}

TEST(Decoreate, V3ToV3) {
    DecorateTest3 dtin;
    dtin.name = Basic::RandomString(8);
    dtin.addr[0] = Basic::RandomString(16);
    dtin.addr[1] = Basic::RandomString(16);
    Basic::rand(&dtin.age, sizeof(dtin.age));

    Manager::Register<DecorateTest3>();

    Buffer buf(256);
    EXPECT_EQ(Manager::MarshalTo(dtin, buf.Clear()), OK);

    buf.print("after MarshalTo:");

    DecorateTest3 dtout;
    EXPECT_EQ(Manager::UnmarshalTo(buf, dtout), OK);
    EXPECT_TRUE(Cpeq<DecorateTest3>().Equal(dtin, dtout));

    //v3 -> v2 should fail
    Manager::Register<DecorateTest2>();
    DecorateTest2 dt2out;
    buf.Rewind();
    EXPECT_TRUE(Manager::UnmarshalTo(buf, dt2out) != OK);

    //v3->v1 should fail
    Manager::Register<DecorateTest>();
    DecorateTest dt1out;
    buf.Rewind();
    EXPECT_TRUE(Manager::UnmarshalTo(buf, dt1out) != OK);
}


TEST(Decoreate, unmarshal) {
    DecorateTest2 dtin;
    dtin.name = Basic::RandomString(8);
    dtin.addr = Basic::RandomString(16);
    Basic::rand(&dtin.age, sizeof(dtin.age));

    Manager::RegisterCurrent<DecorateTest2>();
    Manager::Register<DecorateTest3>();

    Buffer buf(256);
    EXPECT_EQ(Manager::MarshalTo(dtin, buf.Clear()), OK);
    // unmarshal the data version v2
    auto out = Manager::unmarshal(buf);
    EXPECT_TRUE(out);
    auto pout = std::dynamic_pointer_cast<DecorateTest2>(out);
    EXPECT_TRUE(pout);
    EXPECT_TRUE(Cpeq<DecorateTest2>().Equal(dtin, *pout));

    Manager::Register<DecorateTest3>();
    DecorateTest3 dt3out;
    buf.Rewind();
    out = Manager::Unmarshal(buf);
    EXPECT_TRUE(out);
    auto pout3 = std::dynamic_pointer_cast<DecorateTest3>(out);
    EXPECT_TRUE(pout3);
    EXPECT_EQ(pout3->name, dtin.name);
    EXPECT_EQ(pout3->age, dtin.age);
    EXPECT_EQ(pout3->addr[0], dtin.addr);
    EXPECT_EQ(pout3->addr[1], "default address 2");
}

TEST(Decoreate, Unmarshal) {
    DecorateTest2 dtin;
    dtin.name = Basic::RandomString(8);
    dtin.addr = Basic::RandomString(16);
    Basic::rand(&dtin.age, sizeof(dtin.age));

    Manager::RegisterCurrent<DecorateTest2>();

    Buffer buf(256);
    EXPECT_EQ(Manager::MarshalTo(dtin, buf.Clear()), OK);
    auto out = Manager::Unmarshal(buf);
    EXPECT_TRUE(out);
    auto pout = std::dynamic_pointer_cast<DecorateTest2>(out);
    EXPECT_TRUE(pout);
    EXPECT_TRUE(Cpeq<DecorateTest2>().Equal(dtin, *pout));

    Manager::Register<DecorateTest3>();
    DecorateTest3 dt3out;
    buf.Rewind();
    out = Manager::Unmarshal(buf);
    EXPECT_TRUE(out);
    auto pout3 = std::dynamic_pointer_cast<DecorateTest3>(out);
    EXPECT_TRUE(pout3);
    EXPECT_EQ(pout3->name, dtin.name);
    EXPECT_EQ(pout3->age, dtin.age);
    EXPECT_EQ(pout3->addr[0], dtin.addr);
    EXPECT_EQ(pout3->addr[1], "default address 2");
}
