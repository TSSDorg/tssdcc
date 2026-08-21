#include <span>
#include "gtest/gtest.h"

#include "tssd.h"
#include "flat.h"
#include "basic.h"
#include "types.h"
using namespace tssd;
using namespace std;

// simple mode, override TID, Info
struct SchemaTest : public tssd::Flatable {
    static int gid;
    string name;
    int id;

    SchemaTest() : id(gid++){
    }

    std::string TID() const override {
        return std::to_string(id);
    }
    std::string Info() const override {
        return "SchemaTestInfo";
    }
    std::string Family() const override {
        return "SchemaTestFamily";
    }
    std::string Version() const override {
        return "SchemaTest-V1";
    }
};

int SchemaTest::gid = 0;

TEST(Schema, tidinfo) {

    Manager::Register<SchemaTest>();

    SchemaTest st, stout;

    Buffer buf(256);
    EXPECT_EQ(Manager::MarshalTo(st, buf), OK);

    EXPECT_EQ(buf.Schema().TID, std::to_string(st.id));
    EXPECT_EQ(buf.Schema().Info, "SchemaTestInfo");

    buf.print("after MarshalTo:");

    EXPECT_EQ(Manager::UnmarshalTo(buf, stout), OK);

    Cpeq<SchemaTest> cmp;
    EXPECT_TRUE(cmp.Equal(st, stout));
}

// expert mode, override Schema() directly
struct SchemaTest2 : public tssd::Flatable {
    static int gid;
    string name;
    int id;

    SchemaTest2() : id(gid++){
    }

    tssd::Schema Schema() const {
        auto bs = this->Types();
        struct tssd::Schema s{
            -1,           // FID
            "xyz-" + Manager::hash(bs.data(), bs.size()),
            std::to_string(id),  // TID
            "SchemaTest2Info"};
        return s;
    }
    std::string Family() const override {
        return "SchemaTestFamily";
    }
    std::string Version() const override {
        return "SchemaTest2-V1";
    }
};

int SchemaTest2::gid = 0;

TEST(Schema, schema) {

    Manager::Register<SchemaTest2>();

    SchemaTest2 st, stout;

    Buffer buf(256);
    EXPECT_EQ(Manager::MarshalTo(st, buf), OK);

    EXPECT_EQ(buf.Schema().FID, -1);
    EXPECT_EQ(buf.Schema().Types, "xyz-" + Manager::hash(st.Types().data(), st.Types().size()));
    EXPECT_EQ(buf.Schema().TID, std::to_string(st.id));
    EXPECT_EQ(buf.Schema().Info, "SchemaTest2Info");

    buf.print("after MarshalTo:");
    EXPECT_EQ(Manager::UnmarshalTo(buf, stout), OK);

    Cpeq<SchemaTest2> cmp;
    EXPECT_TRUE(cmp.Equal(st, stout));
}
