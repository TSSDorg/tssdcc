#ifndef __TSSD_TEST_TYPES_H__
#define __TSSD_TEST_TYPES_H__

struct BasicType : public Flatable {
    bool  vbool;
    std::int8_t vint8;
    std::uint8_t vuint8;
    std::int16_t vint16;
    std::uint16_t vuint16;
    std::int32_t  vint32;
    std::uint32_t vuint32;
    std::int64_t  vint64;
    std::uint64_t vuint64;
    float        f32;
    double       f64;

    std::string Group() const override {
        return "BasicType";
    }

    std::string Version() const override {
        return "BasicType";
    }
};


#endif // __TSSD_TEST_TYPES_H__
