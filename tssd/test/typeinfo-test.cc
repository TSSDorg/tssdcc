#include "gtest/gtest.h"

#include "typeinfo.h"


struct BasicTypeArray {
    std::int8_t     vint8[2];
};

TEST(TypeInfo, MarshalUnmarshaBasicTypeArray) {
    Buffer buf;

    auto tmp = TypeInfo::Create<BasicTypeArray>();
    tmp->print();

    BasicTypeArray bta1, bta2;
    bta1.vint8[0] = 1;
    bta1.vint8[1] = 2;

    tmp->MarshalTo(&bta1, buf);
    buf.print();
    buf.finish();

    buf.print("after finish");
    tmp->UnmarshalTo(buf, &bta2);

    EXPECT_EQ(bta1.vint8[0], bta2.vint8[0]);
    EXPECT_EQ(bta1.vint8[1], bta2.vint8[1]);

}
