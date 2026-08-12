#include <span>
#include <cstdarg>
#include "gtest/gtest.h"

#define private public
#include "tssd.h"
#include "flat.h"
#include "basic.h"
#include "types.h"
#undef private

using namespace tssd;
using namespace std;

Bytes getTestBytes()
{
    Struct1Flat<char> bta1; //('a', "uint8");
    bta1.struct1.v1 = 'a';

    Manager::Register<Struct1Flat<char>>();

    Buffer buf;
    EXPECT_EQ(Manager::MarshalTo(bta1, buf), OK);

    buf.print("after MarshalTo:");

    Buffer rbuf;  ///read/receive/unmarshal buf
    pFragment frag=std::make_shared<Fragment>(1024);  ///read fragment

    auto list = buf.Fragments();
    return list[0]->data;
}


TEST(FBuffer, unmarshal) {

    Struct1Flat<char> bta1; //('a', "uint8");
    bta1.struct1.v1 = 'a';

    Manager::Register<Struct1Flat<char>>();

    Buffer buf;
    EXPECT_EQ(Manager::MarshalTo(bta1, buf), OK);

    buf.print("after MarshalTo:");

    Buffer rbuf;  ///read/receive/unmarshal buf
    pFragment frag=std::make_shared<Fragment>(1024);  ///read fragment

    auto list = buf.Fragments();

    FBuffer fbuf;
    for (int i=0; i<list.size(); i++) {
        std::size_t more = 0;
        EXPECT_TRUE(!fbuf.Feed(list[0]->data, more));
        EXPECT_EQ(rbuf.Push(fbuf.Fragment()), 0);
        EXPECT_EQ(fbuf.Size(), 0);
    }
    EXPECT_EQ(rbuf.Wanted(),0);


    Struct1Flat<char> bta2; //('b', "uint8");

    EXPECT_EQ(Manager::UnmarshalTo(rbuf, bta2), OK);

    Cpeq<Struct1Flat<char>> cmp;
    EXPECT_TRUE(cmp.Equal(bta1, bta2));
}

TEST(FBuffer, DetechMagic) {
    auto bs = getTestBytes();
    FBuffer fbuf;
    std::size_t more = 0;
    EXPECT_TRUE(!fbuf.detectMagic(bs, more));
}

void TestDetechMagic(FBuffer &fbuf, TError expectRet, int expectMagic, size_t expectMore, int count, ...)
{
    std::size_t more = 0;
    int ret = 0;
    va_list args; // Hold and make argument list with capacity "count"
    va_start(args, count);
    for (int i=0; i<count; ++i) {
        Bytes b = va_arg(args, Bytes);
        ret = fbuf.detectMagic(b, more);
    }
    va_end(args);
    EXPECT_EQ(ret, expectRet);
    EXPECT_EQ(fbuf.magic_, expectMagic);
    EXPECT_EQ(more, expectMore);
}

TEST(FBuffer, DetechMagic2) {
    FBuffer fbuf;
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-1,
        1, Bytes{byte('T')});
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-4,
        1, Bytes{byte('S'), byte('S'), byte('D')});
    TestDetechMagic(fbuf, OK, 0, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-6,
        1, Bytes{byte('V'), byte('1')});
     TestDetechMagic(fbuf, OK, 0, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-8,
        1, Bytes{byte('V'), byte('1')});
    fbuf.Clear();
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-4,
        1, Bytes{byte('T'), byte('S'), byte('S'),byte('D')});
    fbuf.Clear();
    TestDetechMagic(fbuf, OK, 0, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE - 5,
        1, Bytes{byte('T'), byte('S'), byte('S'),byte('D'), byte('V')});

    fbuf.Clear();
    TestDetechMagic(fbuf, OK, 0, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE - 5,
        1, Bytes{byte('x'), byte('T'), byte('S'), byte('S'),byte('D'), byte('V')});
    fbuf.Clear();

    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE - 3,
        1, Bytes{byte('a'), byte('b'), byte('c')});
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE - 4,
        1, Bytes{byte('d')});
    EXPECT_TRUE(Basic::BytesEqual(fbuf.Data(), Bytes{byte('a'), byte('b'), byte('c'), byte('d')}));
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE - 4,
        1, Bytes{byte('e')});

    EXPECT_TRUE(Basic::BytesEqual(fbuf.Data(), Bytes{byte('b'), byte('c'), byte('d'), byte('e')}));

    fbuf.Clear();
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE - 4,
        1, Bytes{byte('x'), byte('y'), byte('S'), byte('S'),byte('D'), byte('V')});

    fbuf.Clear();
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE - 3,
        1, Bytes{byte('a'), byte('b'), byte('c')});
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE - 4,
        1, Bytes{byte('d'), byte('e'), byte('f'),});

    EXPECT_TRUE(Basic::BytesEqual(fbuf.Data(), Bytes{byte('c'), byte('d'), byte('e'), byte('f')}));
}

TEST(FBuffer, DetechMagic3) {
    FBuffer fbuf;
    TestDetechMagic(fbuf, OK, 0, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-11,
        1, Bytes{byte('a'), byte('T'), byte('S'), byte('S'), byte('D'), byte('V'), byte('1'), byte('2'), byte('3'), byte('4'), byte('5'), byte('b')});

    auto bs = getTestBytes();
    bs.push_back(byte('a'));
    bs.push_back(byte('b'));
    size_t more(0);
    EXPECT_EQ(fbuf.detectMagic(bs, more), OK);
    EXPECT_EQ(fbuf.Size(), 11 + bs.size());
    EXPECT_EQ(more, 0);

    EXPECT_EQ(fbuf.parseHeads(more), ERR_FORMAT_ERROR);
    EXPECT_EQ(more, 0);
    EXPECT_EQ(fbuf.magic_, 0);
    EXPECT_EQ(fbuf.Size(), 11 + bs.size());
    fbuf.reset();
    EXPECT_EQ(fbuf.detectMagic(Bytes(), more, 5), OK);
    EXPECT_EQ(more, 0);
    EXPECT_EQ(fbuf.magic_, 0);
    EXPECT_EQ(fbuf.Size(), bs.size());
    EXPECT_EQ(fbuf.Feed(Bytes(), more), OK);
    EXPECT_EQ(more, 0);

    EXPECT_TRUE(fbuf.Fragment());

    EXPECT_EQ(fbuf.Size(), 2);
    EXPECT_TRUE(Basic::BytesEqual(fbuf.Data(), Bytes{byte('a'), byte('b')}));
}

TEST(FBuffer, DetechMagicChecksumFailure) {
    FBuffer fbuf;
    TestDetechMagic(fbuf, OK, 0, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-11,
        1, Bytes{byte('a'), byte('T'), byte('S'), byte('S'), byte('D'), byte('V'), byte('1'), byte('2'), byte('3'), byte('4'), byte('5'), byte('b')});

    auto bs = getTestBytes();
    bs[bs.size()-30] = byte('x');
    bs[bs.size()-31] = byte('x');
    bs.push_back(byte('a'));
    bs.push_back(byte('b'));
    size_t more(0);
    EXPECT_EQ(fbuf.detectMagic(bs, more), OK);
    EXPECT_EQ(fbuf.Size(), 11 + bs.size());
    EXPECT_EQ(more, 0);

    EXPECT_EQ(fbuf.parseHeads(more), ERR_FORMAT_ERROR);
    EXPECT_EQ(more, 0);
    EXPECT_EQ(fbuf.magic_, 0);
    EXPECT_EQ(fbuf.Size(), 11 + bs.size());
    fbuf.reset();
    EXPECT_EQ(fbuf.detectMagic(Bytes(), more, 5), OK);
    EXPECT_EQ(more, 0);
    EXPECT_EQ(fbuf.magic_, 0);
    EXPECT_EQ(fbuf.Size(), bs.size());
    EXPECT_EQ(fbuf.Feed(Bytes(), more), ERR_CHECKSUM_FAILURE);
    EXPECT_EQ(more, 0);
    EXPECT_EQ(fbuf.magic_, -1);

    EXPECT_EQ(fbuf.Size(), 2);
    EXPECT_TRUE(Basic::BytesEqual(fbuf.Data(), Bytes{byte('a'), byte('b')}));
}

TEST(FBuffer, DetechMagic4) {
    FBuffer fbuf;
    TestDetechMagic(fbuf, OK, 0, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-11,
        1, Bytes{byte('a'), byte('T'), byte('S'), byte('S'), byte('D'), byte('V'), byte('1'), byte('2'), byte('3'), byte('4'), byte('5'), byte('b')});

    auto bs = getTestBytes();
    bs[0] = byte('x');
    size_t more(0);
    EXPECT_EQ(fbuf.detectMagic(bs, more), OK);
    EXPECT_EQ(fbuf.Size(), 11 + bs.size());
    EXPECT_EQ(more, 0);

    EXPECT_EQ(fbuf.parseHeads(more), ERR_FORMAT_ERROR);
    EXPECT_EQ(more, 0);
    EXPECT_EQ(fbuf.magic_, 0);
    EXPECT_EQ(fbuf.Size(), 11 + bs.size());
    fbuf.reset();

    EXPECT_EQ(fbuf.detectMagic(Bytes(), more, 5), ERR_INSUFFICIENT_DATA);
    EXPECT_EQ(more, TSSD_FRAGMENT_MIN_HEADER_SIZE-4);
    EXPECT_EQ(fbuf.magic_, -1);
    EXPECT_EQ(fbuf.Size(), 4);
    EXPECT_EQ(fbuf.Feed(Bytes(), more), ERR_INSUFFICIENT_DATA);
    EXPECT_EQ(more, TSSD_FRAGMENT_MIN_HEADER_SIZE-4);
}

TEST(FBuffer, DetechMagic5) {
    FBuffer fbuf;
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-1,
        1, Bytes{byte('a')});
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-4,
        1, Bytes{byte('b'), byte('c'), byte('d'), byte('e')});
    EXPECT_TRUE(Basic::BytesEqual(fbuf.Data(), Bytes{byte('b'), byte('c'), byte('d'), byte('e')}));
    fbuf.Clear();
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-2,
        1, Bytes{byte('a'), byte('b')});
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-4,
        1, Bytes{byte('c'), byte('d'), byte('e')});
    EXPECT_TRUE(Basic::BytesEqual(fbuf.Data(), Bytes{byte('b'), byte('c'), byte('d'), byte('e')}));
    fbuf.Clear();
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-3,
        1, Bytes{byte('a'), byte('b'), byte('c')});
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-4,
        1, Bytes{byte('d'), byte('e'),});
    EXPECT_TRUE(Basic::BytesEqual(fbuf.Data(), Bytes{byte('b'), byte('c'), byte('d'), byte('e')}));
    fbuf.Clear();
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-3,
        1, Bytes{byte('a'), byte('b'), byte('c')});
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-4,
        1, Bytes{byte('d'), byte('e'),byte('f')});
    EXPECT_TRUE(Basic::BytesEqual(fbuf.Data(), Bytes{byte('c'), byte('d'), byte('e'), byte('f')}));
    fbuf.Clear();
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-3,
        1, Bytes{byte('a'), byte('T'), byte('S')});
    TestDetechMagic(fbuf, OK, 0, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-5,
        1, Bytes{byte('S'), byte('D'),byte('V')});
    EXPECT_TRUE(Basic::BytesEqual(fbuf.Data(), Bytes{byte('T'), byte('S'), byte('S'), byte('D'), byte('V')}));
    fbuf.Clear();
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-2,
        1, Bytes{byte('a'), byte('T')});
    TestDetechMagic(fbuf, OK, 0, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-5,
        1, Bytes{byte('S'), byte('S'), byte('D'),byte('V')});
    EXPECT_TRUE(Basic::BytesEqual(fbuf.Data(), Bytes{byte('T'), byte('S'), byte('S'), byte('D'), byte('V')}));

    fbuf.Clear();
    TestDetechMagic(fbuf, tssd::ERR_INSUFFICIENT_DATA, -1, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-4,
        1, Bytes{byte('a'), byte('T'), byte('S'), byte('S')});
    TestDetechMagic(fbuf, OK, 0, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-6,
        1, Bytes{byte('D'),byte('V'), byte('a')});
    EXPECT_TRUE(Basic::BytesEqual(fbuf.Data(), Bytes{byte('T'), byte('S'), byte('S'), byte('D'), byte('V'), byte('a')}));
}
