#ifndef __FLAT_H__
#define __FLAT_H__
#include <string>

class Flatable {
    virtual ~Flatable() = 0;
    virtual std::string Name() { return "FLAT";}
};

#endif