#include <span>
#include <vector>
#include "gtest/gtest.h"

#define private public

#include "tssd.h"
#include "flat.h"
#include "basic.h"
#include "buffer.h"



using namespace std;

std::vector<Bytes> getData(Buffer &buf)
{
    std::vector<Bytes> result;
    auto frags = buf.Fragments();

    for (int i=0; i<frags.size(); i++)
        result.push_back(frags[i]->data);
    return result;
}

bool appendTest(size_t mtu, int n,  std::vector<Bytes> expect)
{
    Buffer buf;
    buf.mtu_ = mtu;
    std::vector<std::byte> vin(n);
    for (int i=0; i<n; i++)
        vin[i] = std::byte(100 + i);

    auto rd = Basic::bounded_rand(n);

    buf.append(&vin[0], rd);

    buf.append(&vin[rd], n-rd);

    buf.finish();

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
