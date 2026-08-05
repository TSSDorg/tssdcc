#include <iostream>
#include <print>
#include <string>

#include "flat.h"
#include "buffer.h"
#include "tssd.h"

#include "gtest/gtest.h"



int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);

    std::println("succ");

    return RUN_ALL_TESTS();
}
