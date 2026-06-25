#ifndef __TYPEINFO_H__
#define __TYPEINFO_H__

#include <vector>
#include <span>

#include "tssd.h"

class TBuffer : public std::vector<std::byte> {
    size_t current = 0;
    void grow(const std::size_t require=1) {
        if (size() +  require > capacity()) {
            reserve(capacity()*2);
        }
    }

    void append(const std::span<std::byte> content) {
        grow(content.size());
        append_range(content);
    }

public:

    TBuffer(std::size_t size=2048) {
        reserve(std::max(size, size_t(1)));  //make sure we reserve 1 byte at least
    }

    TBuffer& clear() {
        std::vector<std::byte>::clear();
        current = 0;
        return *this;
    }

    TBuffer& reset() {
        return this->clear();
    }

    void print() const {

        std::cout<<"TBuffer print:[";
        for (int i=0; i<size(); ++i)
            std::cout << (int)(*this)[i] << '\t';
        std::println("]");
    }

    void append(const TType t) {
        grow();
        emplace_back(std::byte(t));
    }

    //append two bytes size, return the pos of it
    //you may update it at buf[pos]
    std::size_t appendSize(const size_t size) {
        std::uint16_t size2 = std::uint16_t(size);
        auto pos = this->size();
        append((std::byte*)&size2, sizeof(size2));
        return pos;
    }

    void updateSize(const size_t pos, const size_t size) {
        std::uint16_t *ptr = (std::uint16_t *)&(*this)[pos];
        *ptr = (std::uint16_t)size;
    }

    void append(const std::byte *ptr, std::size_t size) {
        auto span = std::span<std::byte>((std::byte*)ptr, size);
        append(span);
    }

    std::int16_t dumpSize() {
        std::int16_t size(0);
        if (auto ret = dump(sizeof(size), (std::byte *)&size))
            return ret;
        return size;
    }

    TError dump(std::size_t size, std::byte *dest) {
        if (this->size() < size + current) return ERR_INSUFFICIENT_DATA;
        memcpy(dest, &(*this)[current], size);
        current +=  size;
        return OK;
    }

    std::byte *dump(std::size_t size) {
        if (this->size() < size + current ) return nullptr;
        auto ret = &(*this)[current];
        current += size;
        return ret;
    }
};

class Oper {
public:
    virtual ~Oper() {}
    virtual TError save(const std::byte *src, TBuffer &buf) const = 0;
    virtual TError dump(TBuffer &buf, std::byte *dest) const = 0;
};

struct TypeInfo : public Oper {

    struct Node {
        char const* type_ = nullptr;
        char const* name_ = nullptr;
        std::ptrdiff_t offset_ = 0;
        std::ptrdiff_t total_offset_ = 0;
        std::size_t size_ = 0;

        //TODO: support reflerence
        bool is_number_ = false;  //is_arithmetic_type
        bool is_float_ = false;
        bool is_signed_ = true;

        TType tssd_type_ = TType::Tobject;
        TType local_type_ = tssd_type_;
        std::shared_ptr<TypeInfo> parent_;
        constexpr Node(char const *type, char const *name, ptrdiff_t offset, std::size_t size, TType local_type) 
            : name_(name), type_(type), offset_(offset), size_(size), local_type_(local_type) {}
        
        constexpr Node(char const *type, char const *name, ptrdiff_t offset, std::size_t size, bool is_number, bool is_float, bool is_signed) 
            : name_(name), type_(type), offset_(offset), size_(size), is_number_(is_number), is_float_(is_float), is_signed_(is_signed) {}
    };

    Node node_;
    std::vector<std::shared_ptr<TypeInfo>> children_;

    constexpr TypeInfo(char const *type,
            char const *name,
            std::ptrdiff_t offset, std::size_t size, bool is_number=false, bool is_float=false, bool is_signed=false) : 
            node_(type, name, offset, size, is_number, is_float, is_signed) {}   

    constexpr TypeInfo(char const *type,
            char const *name,
            std::ptrdiff_t offset,
            std::size_t size,
            TType ttype,
            std::vector<std::shared_ptr<TypeInfo>> ch) : 
            node_(type, name, offset, size, ttype), 
            children_(ch) {}

    TError save(const std::byte *src, TBuffer &buf) const override {
        buf.append(node_.tssd_type_);
        buf.append(src, node_.size_);
        return OK;
    }

    inline TError CheckTType(TBuffer &buf, std::int8_t type= 0) const {
        std::int8_t t(0);
        if (auto ret = buf.dump(sizeof(t), (std::byte*)&t)) 
            return ret;
        type = type != 0 ? type : (std::int8_t)node_.tssd_type_;
        if ( t != type)
            return ERR_FORMAT_ERROR;
        
        return OK;
    }

    TError dump(TBuffer &buf, std::byte *dest) const override {

        if (auto ret = CheckTType(buf))
            return ret;
     
        return buf.dump(node_.size_, dest);
    }

    //set tssd_type, total offset, save, dump by the reflect type
    void parse(std::shared_ptr<TypeInfo> parent) 
    {
        for (auto &it : children_) {
            it->node_.parent_ = parent;
            it->node_.total_offset_ = parent->node_.total_offset_ + it->node_.offset_;
            if (it->node_.is_number_) {
                if (it->node_.is_float_)
                    it->node_.tssd_type_ = (it->node_.size_ == 4) ? TType::Tfloat32 : TType::Tfloat64;
                else {
                    switch(it->node_.size_) {
                        case 1:
                            it->node_.tssd_type_ = (it->node_.is_signed_) ? TType::Tint8 : TType::Tuint8;
                            break;
                        case 2:
                            it->node_.tssd_type_ = (it->node_.is_signed_) ? TType::Tint16 : TType::Tuint16;
                            break;
                        case 4:
                            it->node_.tssd_type_ = (it->node_.is_signed_) ? TType::Tint32 : TType::Tuint32;
                            break; 
                        case 8:
                            it->node_.tssd_type_ = (it->node_.is_signed_) ? TType::Tint64 : TType::Tuint64;
                            break;                                                          
                    }
                }
                it->node_.local_type_ = it->node_.tssd_type_;
            } else {
                switch(it->node_.local_type_) {
                    case TType::Tstring:
                        it->node_.tssd_type_ = TType::Tstring;
                        break;
                    case TType::Tarray:     //it'a static array
                    case TType::Tvector:     //dynamic array
                        it->node_.tssd_type_ = TType::Tarray;
                        it->parse(it);
                        break;
                    case TType::Tdict:
                        it->node_.tssd_type_ = TType::Tdict;
                        it->parse(it);
                        break;
                    default:
                        it->parse(it);  //Tobject is the default, just walk throuth children
                }
            }
        }
    }

    template <typename T> 
    constexpr static std::shared_ptr<TypeInfo> parse2(std::ptrdiff_t offset=0, const char *name = "");

public:
    constexpr TypeInfo(
        const char *type,
        const char *name,
        std::vector<std::shared_ptr<TypeInfo>> ch) : 
        node_(type, name, 0, 0, TType::Tobject), 
        children_(ch) {}

    void print() const {
        std::println("result type:{} name:{} offset:{} size:{} total_offset:{}", node_.type_, node_.name_?node_.name_:"annonymous", node_.offset_, node_.size_, node_.total_offset_);
        for (auto &it : children_) 
            it->print();
    }

    template <typename T> 
    static auto Create() {
        static_assert(std::meta::is_class_type(^^T));
        auto ti = parse2<T>();
        ti->parse(ti);
        return ti;
    }

    TError MarshalTo(const void *obj, TBuffer &buf) const {
        return save((const std::byte*)obj, buf);
    }

    TError UnmarshalTo(TBuffer &buf, void *obj) const {
        return dump(buf, (std::byte *)obj);
    }
};

#endif