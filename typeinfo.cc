#include <meta>
#include <iostream>
#include <print>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <list>
#include <set>

#include "tssd.h"
#include "typeinfo.h"
#include "flat.h"


void
TypeInfo::parse(std::shared_ptr<TypeInfo> parent)
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

TError
stringOper::save(const std::byte *src, TBuffer &buf) const
{
    buf.append(node_.tssd_type_);
    auto pstr = (const std::string *)src;

    buf.appendSize(pstr->size()); //sizet

    buf.append((const std::byte*)pstr->c_str(), pstr->size());
    return OK;
}

TError 
stringOper::dump(TBuffer &buf, std::byte *dest) const
{
    if (auto ret = CheckTType(buf))
        return ret;
    auto size = buf.dumpSize();
    if (size < 0) {
        return size;
    }

    auto pstr = (std::string *)dest;

    const char *ptr = (const char *)buf.dump(size);
    if (!ptr) {
        return ERR_INSUFFICIENT_DATA;
    }

    pstr->assign(ptr, size);

    return OK;
}

TError 
objectOper::save(const std::byte *src, TBuffer &buf) const
{
    buf.append(node_.tssd_type_);   //T
    std::size_t pos = buf.appendSize(0);   //sizet reserve
    buf.appendSize(children_.size()); //sizea
    
    for (auto &it : children_) {
        if (auto ret = it->save(&src[it->node_.offset_],  buf)) 
            return ret;
    }

    buf.updateSize(pos, buf.size() - pos - 2);
    return OK;
}

TError 
objectOper::dump(TBuffer &buf, std::byte *dest) const 
{
    if (auto ret = CheckTType(buf))
        return ret;
    auto sizet = buf.dumpSize();
    if (sizet < 0 || buf.size() < 1 + 2 + sizet) {
        return ERR_INSUFFICIENT_DATA;
    }

    //sizea
    if (buf.dumpSize() != children_.size()) {
        return ERR_FORMAT_ERROR;
    }
    
    for (auto &it : children_) {
        if (auto ret = it->dump(buf, &dest[it->node_.offset_])) {
            return ret;
        }
    }
    return OK;
}

//array
TError arrayOper::save(const std::byte *src, TBuffer &buf) const
{
    buf.append(node_.tssd_type_);   //T
    std::size_t pos = buf.appendSize(0);   //sizet reserve

    auto real_size = node_.size_;
    auto addr = src;
    auto &node = children_[0]->node_;
    if (node_.local_type_ == TType::Tvector) {  //for vector, we convert to vector<byte> to calc the real size
        auto *p = (std::vector<std::byte>*)src;
        real_size = p->size()/node.size_;
        addr = p->data();
    }
    buf.appendSize(real_size);
    
    for (int i=0; i<real_size; ++i) {
        if (auto ret = children_[0]->save(&addr[node.size_ * i],  buf)) 
            return ret;
    }

    buf.updateSize(pos, buf.size() - pos - 2);
    return OK;
}

TError arrayOper::dump(TBuffer &buf, std::byte *dest) const 
{
    if (auto ret = CheckTType(buf))
        return ret;
    auto sizet = buf.dumpSize();
    if (sizet < 0 || buf.size() < 1 + 2 + sizet) {
        return ERR_INSUFFICIENT_DATA;
    }

    //sizea
    auto sizea = buf.dumpSize();
    if (sizea < 0) return ERR_FORMAT_ERROR;

    auto addr = dest;
    auto &node = children_[0]->node_;
    //static array need check node_.size, but dyname array(vector) need skip
    if (node_.local_type_ == TType::Tarray) {
        if (sizea != node_.size_)
            return ERR_FORMAT_ERROR;
    } else { //for vector we need reserve capacity first
        auto *p = (std::vector<std::byte>*)dest;
        p->reserve(node.size_ * sizea);
        p->resize(node.size_ * sizea);
        addr = p->data();
    }
    
    for (int i=0; i<sizea; ++i) {
        if (auto ret = children_[0]->dump(buf, &addr[node.size_ * i])) {
            return ret;
        }
    }
    return OK;
}
