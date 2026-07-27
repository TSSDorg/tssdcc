#include <cstdlib>
#include <ctime>
#include <span>
#include <list>

#include "tssd.h"
#include "flat.h"

using namespace std;

class Basic {
public:
    inline static unsigned bounded_rand(unsigned range)
    {
        for (unsigned x, r;;) {
            x = std::rand();
            r = x % range;
            if (x - r <= -range)
                return r;
        }
    }

    //produce random value
    static void rand(void *dest, std::size_t n)
    {
        std::srand(std::time({})); // use current time as seed for random generator
        auto *p = (std::byte *)dest;
        std::memset(p, 0, n);
        for (auto i=0; i<n; i++)
        {
            const int random_value = bounded_rand(256);
            p[i] =  (std::byte) random_value;
        }
    }

    template<typename T>
    static std::size_t SizeofFlat()
    {
        return sizeof(T) - sizeof(Flatable);
    }

    static bool BytesEqual(const void *p1, std::size_t s1, const void *p2, std::size_t s2)
    {
        return BytesEqual(std::span((std::byte*)p1, s1), std::span((std::byte*)p2, s2));
    }

    static bool BytesEqual(VBytes s1, VBytes s2)
    {
        if (s1.size() != s2.size()) {
            std::cout <<"diff s1: size:" << s1.size() << ", s2 size:" << s2.size() << std::endl;
            return false;
        }

        for (auto i = 0; i<s1.size(); i++)
        {
            if (s1[i] != s2[i]) {
                std::cout <<"Basic::BytesEqual diff i:" << i << "\ts1[i]:" << int(s1[i]) << "\ts2[i]:" << int(s2[i]) << std::endl;
                return false;
            }
        }
        return true;
    }

    template<typename T>
    static bool ListEqual(list<T> la, list<T> lb)
    {
        if (la.size() != lb.size()) {
            std::cout <<"diff la: size:" << la.size() << ", lb size:" << lb.size() << std::endl;
            return false;
        }
        auto it1 = la.cbegin(), it2 = lb.cbegin();
        for (; it1 != la.cend() && it2 != lb.cend(); it1++, it2++) {
            if (!BytesEqual(&(*it1), sizeof(T), &(*it2), sizeof(T)))
                return false;
        }
        return it1 == la.cend() &&  it2 == lb.cend();
    }
};

