#include <span>
#include <vector>
#include "gtest/gtest.h"

#define private public
#include "tssd.h"
#include "flat.h"
#include "basic.h"
#include "buffer.h"
#include "types.h"
#undef private

using namespace std;
using namespace tssd;
std::vector<Bytes> getData(Buffer &buf)
{
    std::vector<Bytes> result;
    auto frags = buf.Fragments();

    for (int i=0; i<frags.size(); i++)
        result.push_back(frags[i]->data);
    return result;
}

void append(Buffer &buf, int n) {
    std::vector<std::byte> vin(n);
    for (int i=0; i<n; i++)
        vin[i] = std::byte(100 + i);

    auto rd = Basic::bounded_rand(n);

    buf.append(&vin[0], rd);

    buf.append(&vin[rd], n-rd);

    buf.finish();
}

bool appendTest(size_t mtu, int n,  std::vector<Bytes> expect)
{
    Buffer buf;
    buf.mtu_ = mtu;
    append(buf, n);
    auto result = getData(buf);

    if (result.size() != expect.size()) {
        cout << "appendTest size diff:" << result.size() << '\t' << expect.size() << endl;
        return false;
    }

    for (int i=0; i<expect.size(); i++)
        if (!Basic::BytesEqual(expect[i], result[i])) {
            cout << "appendTest i:" << i << endl;
            return false;
        }
    return true;
}

TEST(Buffer, append) {
    EXPECT_TRUE(appendTest(1, 1, vector<Bytes> {
                        Bytes {byte(100)},
                    }
    ));
    EXPECT_TRUE(appendTest(1, 2, vector<Bytes> {
                        Bytes {byte(100)}, Bytes {byte(101)},
                    }
    ));

    EXPECT_TRUE(appendTest(2, 1, vector<Bytes> {
                        Bytes {byte(100)},
                    }
    ));

    EXPECT_TRUE(appendTest(3, 3, vector<Bytes> {
                        Bytes {byte(100), byte(101), byte(102)},
                        //Bytes {byte(103),}
                    }
    ));

    EXPECT_TRUE(appendTest(3, 4, vector<Bytes> {
                        Bytes {byte(100), byte(101), byte(102)},
                        Bytes {byte(103),}
                    }
    ));

    EXPECT_TRUE(appendTest(3, 5, vector<Bytes> {
                        Bytes {byte(100), byte(101), byte(102)},
                        Bytes {byte(103),byte(104)},
                    }
    ));

    EXPECT_TRUE(appendTest(3, 6, vector<Bytes> {
                        Bytes {byte(100), byte(101), byte(102)},
                        Bytes {byte(103),byte(104), byte(105)},
                    }
    ));

    EXPECT_TRUE(appendTest(5, 3, vector<Bytes> {
                        Bytes {byte(100), byte(101), byte(102)},
                    }
    ));

    EXPECT_TRUE(appendTest(5, 5, vector<Bytes> {
                        Bytes {byte(100), byte(101), byte(102), byte(103), byte(104)},
                    }
    ));

    EXPECT_TRUE(appendTest(5, 6, vector<Bytes> {
                        Bytes {byte(100), byte(101), byte(102), byte(103), byte(104)},
                        Bytes {byte(105),}
                    }
    ));

    EXPECT_TRUE(appendTest(5, 8, vector<Bytes> {
                        Bytes {byte(100), byte(101), byte(102), byte(103), byte(104)},
                        Bytes {byte(105), byte(106), byte(107)}
                    }
    ));

    EXPECT_TRUE(appendTest(5, 10, vector<Bytes> {
                        Bytes {byte(100), byte(101), byte(102), byte(103), byte(104)},
                        Bytes {byte(105), byte(106), byte(107), byte(108), byte(109)},
                    }
    ));
    EXPECT_TRUE(appendTest(5, 12, vector<Bytes> {
                        Bytes {byte(100), byte(101), byte(102), byte(103), byte(104)},
                        Bytes {byte(105), byte(106), byte(107), byte(108), byte(109)},
                        Bytes{ byte(110), byte(111)}
                    }
    ));
    EXPECT_TRUE(appendTest(5, 14, vector<Bytes> {
                        Bytes {byte(100), byte(101), byte(102), byte(103), byte(104)},
                        Bytes {byte(105), byte(106), byte(107), byte(108), byte(109)},
                        Bytes{ byte(110), byte(111), byte(112), byte(113)}
                    }
    ));
}

bool dumpTest(Buffer &buf, int n, Bytes expect)
{
    byte bs[100];
    if (buf.dump(n, bs)) return false;
    return Basic::BytesEqual(&expect[0], expect.size(), bs, n);
}

TEST(Buffer, dump) {
    Buffer buf(5);
    buf.mtu_ = 5;
    append(buf, 14);
    EXPECT_EQ(buf.Size(), 14);
    EXPECT_EQ(buf.fragments_.size(), 3);
    EXPECT_TRUE(dumpTest(buf, 1, Bytes{byte(100)}));
    EXPECT_TRUE(dumpTest(buf, 2, Bytes{byte(101), byte(102)}));
    EXPECT_TRUE(dumpTest(buf, 2, Bytes{byte(103), byte(104)}));
    EXPECT_TRUE(dumpTest(buf, 1, Bytes{byte(105)}));
    EXPECT_TRUE(dumpTest(buf, 3, Bytes{byte(106), byte(107), byte(108)}));
    EXPECT_TRUE(dumpTest(buf, 4, Bytes{byte(109), byte(110), byte(111), byte(112)}));
    EXPECT_FALSE(dumpTest(buf, 2, Bytes{byte(101), byte(102)}));
}

TEST(Buffer, dump2) {
    Buffer buf(5);
    buf.mtu_ = 5;
    append(buf, 16);
    EXPECT_EQ(buf.Size(), 16);
    EXPECT_EQ(buf.fragments_.size(), 4);
    EXPECT_TRUE(dumpTest(buf, 1, Bytes{byte(100)}));
    EXPECT_TRUE(dumpTest(buf, 9, Bytes{byte(101), byte(102), byte(103), byte(104), byte(105), byte(106), byte(107), byte(108), byte(109)}));
}

TEST(Buffer, dump3) {
    Buffer buf(5);
    buf.mtu_ = 5;
    append(buf, 16);
    EXPECT_EQ(buf.Size(), 16);
    EXPECT_EQ(buf.fragments_.size(), 4);
    EXPECT_TRUE(dumpTest(buf, 1, Bytes{byte(100)}));
    EXPECT_TRUE(dumpTest(buf, 10, Bytes{byte(101), byte(102), byte(103), byte(104), byte(105), byte(106), byte(107), byte(108), byte(109), byte(110)}));
}

TEST(Buffer, merge) {
    Buffer buf(5);
    buf.mtu_ = 5;
    append(buf, 16);
    EXPECT_EQ(buf.Size(), 16);
    EXPECT_EQ(buf.fragments_.size(), 4);
    buf.Merge();
    EXPECT_EQ(buf.mtu_, 16);
    EXPECT_EQ(buf.fragments_.size(), 1);
    EXPECT_TRUE(dumpTest(buf, 1, Bytes{byte(100)}));
    EXPECT_TRUE(dumpTest(buf, 10, Bytes{byte(101), byte(102), byte(103), byte(104), byte(105), byte(106), byte(107), byte(108), byte(109), byte(110)}));
}

TEST(Buffer, split) {
    Buffer buf(5);
    buf.mtu_ = 5;
    append(buf, 16);
    EXPECT_EQ(buf.Size(), 16);
    EXPECT_EQ(buf.fragments_.size(), 4);
    buf.Split(7);
    EXPECT_EQ(buf.mtu_, 7);
    EXPECT_EQ(buf.fragments_.size(), 3);
    EXPECT_TRUE(dumpTest(buf, 1, Bytes{byte(100)}));
    EXPECT_TRUE(dumpTest(buf, 10, Bytes{byte(101), byte(102), byte(103), byte(104), byte(105), byte(106), byte(107), byte(108), byte(109), byte(110)}));
}

TEST(Buffer, MergeWithHeads) {
    BasicArrayFlat bta1;
    Basic::rand(&bta1.basicArray.vbool[0], sizeof(BasicArray));

    Manager::Register<BasicArrayFlat>();

    Buffer buf;

    EXPECT_EQ(Manager::MarshalTo(bta1, buf.Clear()), OK);
    auto pre_size = buf.Size();
    buf.print("after MarshalTo:");
    BasicArrayFlat bta2;

    EXPECT_EQ(Manager::UnmarshalTo(buf, bta2), OK);

    EXPECT_TRUE(Cpeq<BasicArrayFlat>().Equal(bta1, bta2));

    buf.Rewind();
    auto pre_frags = buf.fragments_.size();
    EXPECT_TRUE(pre_frags>1);

    buf.Merge();
    EXPECT_EQ(buf.mtu_, pre_size + buf.heads_.size() + buf.checksum_len_);
    EXPECT_EQ(buf.fragments_.size(), 1);

    BasicArrayFlat bta3;
    Basic::rand(&bta3.basicArray.vbool[0], sizeof(BasicArray));

    EXPECT_EQ(Manager::UnmarshalTo(buf, bta3), OK);
    EXPECT_TRUE(Cpeq<BasicArrayFlat>().Equal(bta2, bta3));
}

TEST(Buffer, SplitWithHeads) {
    BasicArrayFlat bta1;
    Basic::rand(&bta1.basicArray.vbool[0], sizeof(BasicArray));

    Manager::Register<BasicArrayFlat>();

    Buffer buf;

    EXPECT_EQ(Manager::MarshalTo(bta1, buf.Clear()), OK);
    auto pre_size = buf.Size();
    buf.print("after MarshalTo:");
    BasicArrayFlat bta2;

    EXPECT_EQ(Manager::UnmarshalTo(buf, bta2), OK);

    EXPECT_TRUE(Cpeq<BasicArrayFlat>().Equal(bta1, bta2));

    buf.Rewind();
    auto pre_frags = buf.fragments_.size();
    EXPECT_TRUE(pre_frags>1);

    buf.Split(384);
    EXPECT_EQ(buf.mtu_, 384);
    EXPECT_EQ(buf.fragments_.size(), 2);

    BasicArrayFlat bta3;
    Basic::rand(&bta3.basicArray.vbool[0], sizeof(BasicArray));

    EXPECT_EQ(Manager::UnmarshalTo(buf, bta3), OK);
    EXPECT_TRUE(Cpeq<BasicArrayFlat>().Equal(bta2, bta3));
}
