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

template <class T=char>
Bytes getTestBytes(T v)
{
    Struct1Flat<T> bta1;
    bta1.struct1.v1 = v;

    Manager::Register<Struct1Flat<T>>();

    Buffer buf;
    EXPECT_EQ(Manager::MarshalTo(bta1, buf), OK);

    buf.print("after MarshalTo:");

    auto list = buf.Fragments();
    return list[0]->data;
}


TEST(RBuffer, unmarshal) {

    Struct1Flat<char> bta1; //('a', "uint8");
    bta1.struct1.v1 = 'a';

    Manager::Register<Struct1Flat<char>>();

    Buffer buf;
    EXPECT_EQ(Manager::MarshalTo(bta1, buf), OK);

    buf.print("after MarshalTo:");

    Buffer rbuf;  ///read/receive/unmarshal buf
    pFragment frag=std::make_shared<Fragment>(1024);  ///read fragment

    auto list = buf.Fragments();

    RBuffer fbuf;
    for (int i=0; i<list.size(); i++) {
        std::size_t more = 0;
        EXPECT_TRUE(!fbuf.Extract(list[0]->data, more));
        EXPECT_EQ(rbuf.Push(fbuf.Fragment()), 0);
        EXPECT_EQ(fbuf.Size(), 0);
    }
    EXPECT_EQ(rbuf.Wanted(),0);


    Struct1Flat<char> bta2; //('b', "uint8");

    EXPECT_EQ(Manager::UnmarshalTo(rbuf, bta2), OK);

    Cpeq<Struct1Flat<char>> cmp;
    EXPECT_TRUE(cmp.Equal(bta1, bta2));
}

TEST(RBuffer, DetechMagic) {
    auto bs = getTestBytes('a');
    RBuffer fbuf;
    std::size_t more = 0;
    EXPECT_TRUE(!fbuf.detectMagic(bs, more));
}

void TestDetechMagic(RBuffer &fbuf, TError expectRet, int expectMagic, size_t expectMore, int count, ...)
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

TEST(RBuffer, DetechMagic2) {
    RBuffer fbuf;
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

TEST(RBuffer, DetechMagic3) {
    RBuffer fbuf;
    TestDetechMagic(fbuf, OK, 0, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-11,
        1, Bytes{byte('a'), byte('T'), byte('S'), byte('S'), byte('D'), byte('V'), byte('1'), byte('2'), byte('3'), byte('4'), byte('5'), byte('b')});

    auto bs = getTestBytes('a');
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
    EXPECT_EQ(fbuf.Extract(Bytes(), more), OK);
    EXPECT_EQ(more, 0);

    EXPECT_TRUE(fbuf.Fragment());

    EXPECT_EQ(fbuf.Size(), 2);
    EXPECT_TRUE(Basic::BytesEqual(fbuf.Data(), Bytes{byte('a'), byte('b')}));
}

TEST(RBuffer, DetechMagicChecksumFailure) {
    RBuffer fbuf;
    TestDetechMagic(fbuf, OK, 0, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-11,
        1, Bytes{byte('a'), byte('T'), byte('S'), byte('S'), byte('D'), byte('V'), byte('1'), byte('2'), byte('3'), byte('4'), byte('5'), byte('b')});

    auto bs = getTestBytes('a');
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
    EXPECT_EQ(fbuf.Extract(Bytes(), more), ERR_CHECKSUM_FAILURE);
    EXPECT_EQ(more, 0);
    EXPECT_EQ(fbuf.magic_, -1);

    EXPECT_EQ(fbuf.Size(), 2);
    EXPECT_TRUE(Basic::BytesEqual(fbuf.Data(), Bytes{byte('a'), byte('b')}));
}

TEST(RBuffer, DetechMagic4) {
    RBuffer fbuf;
    TestDetechMagic(fbuf, OK, 0, tssd::TSSD_FRAGMENT_MIN_HEADER_SIZE-11,
        1, Bytes{byte('a'), byte('T'), byte('S'), byte('S'), byte('D'), byte('V'), byte('1'), byte('2'), byte('3'), byte('4'), byte('5'), byte('b')});

    auto bs = getTestBytes('a');
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
    EXPECT_EQ(fbuf.Extract(Bytes(), more), ERR_INSUFFICIENT_DATA);
    EXPECT_EQ(more, TSSD_FRAGMENT_MIN_HEADER_SIZE-4);
}

TEST(RBuffer, DetechMagic5) {
    RBuffer fbuf;
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

void testExtract(int expect_frags, int count, ...) {

    Bytes mbs;
    va_list args;
    va_start(args, count);
    for (int i=0; i<count; ++i) {
        Bytes b = va_arg(args, Bytes);
        mbs.insert(mbs.end(), b.cbegin(), b.cend());
    }
    va_end(args);

    RBuffer fbuf;
    Buffer tbuf;
    std::size_t more = 0;

    auto ret = fbuf.Extract(mbs, more);
    int i = 0;
    do {
        EXPECT_TRUE( ret == OK && !more);
        auto frag = fbuf.Fragment();
        EXPECT_TRUE(frag);

        tbuf.Clear();
        EXPECT_EQ(tbuf.Push(frag), 0);

        Struct1Flat<char> out;

        EXPECT_EQ(Manager::UnmarshalTo(tbuf, out), OK);
        EXPECT_EQ(out.struct1.v1, 'a' + i);

        more = 0;
        ret = fbuf.Extract(more);
        --expect_frags;
        i++;
    } while (expect_frags);

    more = 0;
    EXPECT_EQ(fbuf.Extract(more), ERR_INSUFFICIENT_DATA);
    EXPECT_TRUE(more);
}

TEST(RBuffer, FeedBytes) {
    testExtract(1, 1, getTestBytes('a'));
}

TEST(RBuffer, FeedBytes2) {
    auto bs = getTestBytes('a');
    auto bs2 = getTestBytes('b');
    testExtract(2, 2, bs, bs2);
}

TEST(RBuffer, FeedBytes3) {
    auto bs = getTestBytes('a');
    auto bs2 = getTestBytes('b');

    Bytes other = { byte('T'), byte('S'), byte('S'), byte('D'), byte('V')};
    Bytes other2 = { byte('b')};

    testExtract(2, 4, other, bs, other2, bs2);
    testExtract(2, 4, other, bs, other, bs2);
    testExtract(2, 4, other2, bs, other2, bs2);
    testExtract(2, 4, other2, bs, other, bs2);
}

TEST(RBuffer, FeedBytes4) {
    auto bs = getTestBytes('a');
    auto bs2 = getTestBytes('b');
    auto other2 = getTestBytes('a');

    Bytes other = { byte('T'), byte('S'), byte('S'), byte('D'), byte('V')};
    other2[7] = byte('x');

    testExtract(2, 4, other, bs, other2, bs2);
    testExtract(2, 4, other, bs, other, bs2);
    testExtract(2, 4, other2, bs, other2, bs2);
    testExtract(2, 4, other2, bs, other, bs2);
}

class MockReader : public tssd::Reader
{
    list<Bytes> datas;
public:
    void Set(Bytes data) {
        datas.emplace_back(data);
    }

    void Set(int count, ...)
    {
        va_list args;
        va_start(args, count);
        for (int i=0; i<count; ++i) {
            Bytes b = va_arg(args, Bytes);
            Set(b);
        }
        va_end(args);
    }

    int Read(void *dest, std::size_t numb) const {
        if (datas.empty()) return 0;
        auto d = datas.front();
        auto n = std::min(numb, d.size());
        auto p = (byte*)dest;
        memcpy(p, d.data(), n);

        if (n<d.size()) {
            Bytes bs(d.size()-n);
            for (size_t i = 0; i<d.size()-n; i++)
                bs[i] = d[i+n];
            datas.pop_front();
            datas.push_front(bs);
        } else
            datas.pop_front();

        return n;
    }
    //MOCK_METHOD(int, Read, (void *, std::size_t), (const, override));
};

TEST(RBuffer, ExtractReader) {

    MockReader mockReader;
    mockReader.Set(getTestBytes('a'));

    Struct1Flat<char> out;
    RBuffer rbuf;
    EXPECT_EQ(rbuf.Read(mockReader, out), OK);
    EXPECT_EQ(out.struct1.v1, 'a');
}

TEST(RBuffer, ExtractReader2) {

    MockReader mockReader;
    mockReader.Set(3, Bytes{byte('a')}, Bytes{byte('b')},
        getTestBytes('a'));

    Struct1Flat<char> out;
    RBuffer rbuf;
    EXPECT_EQ(rbuf.Read(mockReader, out), OK);
    EXPECT_EQ(out.struct1.v1, 'a');
}

TEST(RBuffer, ExtractReader3) {

    MockReader mockReader;
    mockReader.Set(3, Bytes{byte('T'), byte('S'), byte('S'), byte('D')}, Bytes{byte('V')},
        getTestBytes('a'));

    Struct1Flat<char> out;
    RBuffer rbuf;
    EXPECT_EQ(rbuf.Read(mockReader, out), OK);
    EXPECT_EQ(out.struct1.v1, 'a');
}

TEST(RBuffer, ExtractReader4) {

    MockReader mockReader;
    mockReader.Set(5, Bytes{byte('T'), byte('S'), byte('S'), byte('D')}, Bytes{byte('V')},
        getTestBytes('a'), Bytes{byte('T'), byte('S'), byte('S'), byte('D'), byte{'V'}}, getTestBytes<int>(123));

    Struct1Flat<int> out;
    RBuffer rbuf;
    EXPECT_EQ(rbuf.Read(mockReader, out), OK);
    EXPECT_EQ(out.struct1.v1, 123);

    Struct1Flat<char> out2;
    EXPECT_EQ(rbuf.Read(mockReader, out2), OK);
    EXPECT_EQ(out2.struct1.v1, 'a');
}

TEST(RBuffer, ExtractReader5) {

    MockReader mockReader;
    auto bs = getTestBytes('a');
    auto bs2 = getTestBytes<int>(123);
    Bytes mbs, magic{byte('T'), byte('S'), byte('S'), byte('D'), byte{'V'}}, other{byte('x'), byte('y')};
    mbs.insert(mbs.end(), other.cbegin(), other.cend());

    mbs.insert(mbs.end(), bs.cbegin(), bs.cend());
    mbs.insert(mbs.end(), magic.cbegin(), magic.cend());
    mbs.insert(mbs.end(), bs2.cbegin(), bs2.cend());

    mockReader.Set(1, mbs);
    RBuffer rbuf;

    Struct1Flat<int> out;
    EXPECT_EQ(rbuf.Read(mockReader, out), OK);
    EXPECT_EQ(out.struct1.v1, 123);

    Struct1Flat<char> out2;
    EXPECT_EQ(rbuf.Read(mockReader, out2), OK);
    EXPECT_EQ(out2.struct1.v1, 'a');
}

TEST(RBuffer, ExtractReader6) {

    MockReader mockReader;
    auto bs = getTestBytes('a');
    auto bs2 = getTestBytes<int>(123);
    Bytes mbs, magic{byte('T'), byte('S'), byte('S'), byte('D'), byte{'V'}}, other{byte('x'), byte('y')};
    mbs.insert(mbs.end(), other.cbegin(), other.cend());

    mbs.insert(mbs.end(), bs.cbegin(), bs.cend());
    mbs.insert(mbs.end(), magic.cbegin(), magic.cend());
    mbs.insert(mbs.end(), bs2.cbegin(), bs2.cend());

    mockReader.Set(1, mbs);
    RBuffer rbuf;

    Struct1Flat<char> out2;
    EXPECT_EQ(rbuf.Read(mockReader, out2), OK);
    EXPECT_EQ(out2.struct1.v1, 'a');

    Struct1Flat<int> out;
    EXPECT_EQ(rbuf.Read(mockReader, out), OK);
    EXPECT_EQ(out.struct1.v1, 123);
}


TEST(RBuffer, ExtractReader7) {

    MockReader mockReader;
    auto bs = getTestBytes('a');
    auto bs2 = getTestBytes<int>(123);
    Bytes mbs, magic{byte('T'), byte('S'), byte('S'), byte('D'), byte{'V'}}, other{byte('x'), byte('y')};
    mbs.insert(mbs.end(), other.cbegin(), other.cend());

    mbs.insert(mbs.end(), bs.cbegin(), bs.cend());
    mbs.insert(mbs.end(), magic.cbegin(), magic.cend());
    mbs.insert(mbs.end(), bs2.cbegin(), bs2.cend());

    mockReader.Set(4, other, mbs, magic, getTestBytes<int>(456));
    RBuffer rbuf;

    Struct1Flat<int> out;
    EXPECT_EQ(rbuf.Read(mockReader, out), OK);
    EXPECT_EQ(out.struct1.v1, 123);

    Struct1Flat<char> out2;
    EXPECT_EQ(rbuf.Read(mockReader, out2), OK);
    EXPECT_EQ(out2.struct1.v1, 'a');

    EXPECT_EQ(rbuf.Read(mockReader, out), OK);
    EXPECT_EQ(out.struct1.v1, 456);
}
