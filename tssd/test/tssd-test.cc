#include "gtest/gtest.h"

#include "tssd.h"
#include "flat.h"

struct Student : public Flatable {
    std::string name;
    std::int16_t age;

    Flatable *Build() const {
        return new Student;
    }

    std::string Group() const {
        return "main.Student";
    }

    std::string Version() const {
        return "main.Student.V1";
    }
};


TEST(TSSD, MarshalUnmarsha) {
    Student st;
    st.name = "DT";
    st.age = 80;

    Manager::Register<Student>();

    Buffer buf;
    EXPECT_EQ(Manager::MarshalTo(st, buf.clear()), OK);

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

    Student st2;
    EXPECT_EQ(Manager::UnmarshalTo(rbuf, st2), OK);

    EXPECT_EQ(st.age, st2.age);
    EXPECT_EQ(st.name, st2.name);
}
