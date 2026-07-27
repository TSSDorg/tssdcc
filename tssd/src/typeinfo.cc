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
TypeInfo::MakeTypes()
{
    node_.root_->node_.types_.push_back(std::byte(node_.tssd_type_));
    if (node_.tssd_type_ == TType::Tobject) {
        int size = children_.size();
        auto sp = std::span((std::byte*)&size, sizeof(size));
        node_.root_->node_.types_.append_range(sp);
    }
    /*
    switch (node_.tssd_type_) {
        case TType::Tobject:
            int size = children_.size();
            auto sp = std::span((std::byte*)&size, sizeof(size));
            node_.root_->node_.types_.append_range(sp);
            break;
        case TType::Tarraym:
            break;
        case TType::Ttime:
            break;
        default:
            break;
    }*/
    for (auto &it : children_) {
        it->MakeTypes();
    }
}

void
TypeInfo::UpdateMergedArray(std::shared_ptr<TypeInfo> child)
{
    if (child->node_.tssd_type_ != TType::Tarray) return;
    if (child->children_.size() != 1) return;
    if (!child->children_[0]->node_.is_number_) return;
    child->node_.tssd_type_ = TType::Tarraym;
}

void
TypeInfo::parse(std::shared_ptr<TypeInfo> parent)
{
    node_.root_ = parent->node_.root_;
    for (auto &it : children_) {
        it->node_.parent_ = parent;
        it->node_.root_ = node_.root_;
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
                case TType::Tarray:      //it'a static array
                case TType::Tvector:     //dynamic array
                    it->node_.tssd_type_ = TType::Tarray;
                    UpdateMergedArray(it);
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
stringOper::save(const std::byte *src, Buffer &buf) const
{
    buf.append(node_.tssd_type_);
    auto pstr = (const std::string *)src;

    buf.appendSize4(pstr->size()); //sizet

    buf.append((const std::byte*)pstr->c_str(), pstr->size());
    return OK;
}

TError
stringOper::dump(Buffer &buf, std::byte *dest) const
{
    if (auto ret = CheckTType(buf))
        return ret;
    auto size = buf.dumpSize4();
    if (size <= 0 ) {
        return size;
    }

    auto pstr = (std::string *)dest;

    Bytes bs(size);

    if (auto ret = buf.dump(size, &bs[0]))
        return ret;

    pstr->assign(bs.begin(), bs.end());

    return OK;
}

TError
objectOper::save(const std::byte *src, Buffer &buf) const
{
    buf.append(node_.tssd_type_);   //T
    int index(0), offset(0);
    buf.ftell(index, offset);
    std::size_t pos = buf.appendSize4(0);   //sizet reserve
    buf.appendSize2(children_.size()); //sizea

    for (auto &it : children_) {
        if (auto ret = it->save(&src[it->node_.offset_],  buf))
            return ret;
    }

    buf.updateSize(index, offset, buf.size() - pos);
    return OK;
}

TError
objectOper::dump(Buffer &buf, std::byte *dest) const
{
    int sizet = CheckDumpTS(buf);
    if (sizet<0) return sizet;

    //sizea
    if (buf.dumpSize2() != children_.size()) {
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
TError arrayOper::save(const std::byte *src, Buffer &buf) const
{
    buf.append(node_.tssd_type_);   //T
    auto &child = children_[0]->node_;
    if (node_.tssd_type_ == TType::Tarraym) {
        buf.append(child.tssd_type_);
        auto sizet = child.size_ * node_.size_ + TSSD_SIZEA_LENGTH;
        buf.appendSize4(child.size_ * node_.size_ + TSSD_SIZEA_LENGTH);
        buf.appendSize2(node_.size_);
        buf.append(src, child.size_ * node_.size_);
        return OK;
    }
    int index(0), offset(0);
    buf.ftell(index, offset);
    std::size_t pos = buf.appendSize4(0);   //sizet reserve

    auto real_size = node_.size_;
    auto addr = src;
    buf.appendSize2(real_size);

    for (int i=0; i<real_size; ++i) {
        if (auto ret = children_[0]->save(&addr[child.size_ * i],  buf))
            return ret;
    }

    buf.updateSize(index, offset, buf.size() - pos);
    return OK;
}


TError arrayOper::dump(Buffer &buf, std::byte *dest) const
{
    std::int8_t t(0);
    if (auto ret = buf.dump(sizeof(t), (std::byte*)&t))
            return ret;

    auto &child = children_[0]->node_;
    if (t == (std::int8_t)TType::Tarraym) {
        std::int8_t t2(0);
        if (auto ret = buf.dump(sizeof(t2), (std::byte*)&t2))
            return ret;

        if (t2 != (std::int8_t)child.tssd_type_) {
            return ERR_FORMAT_ERROR;
        }
    } else if ( t != (std::int8_t)TType::Tarray)
        return ERR_FORMAT_ERROR;

    auto sizet = buf.dumpSize4();
    if (sizet < 0 || buf.size() < sizet) {
        return ERR_INSUFFICIENT_DATA;
    }
    //sizea
    auto sizea = buf.dumpSize2();
    if (sizea != node_.size_)
        return ERR_FORMAT_ERROR;

    if (t == (std::int8_t)TType::Tarraym) {
        if (sizet != child.size_ * sizea + TSSD_SIZEA_LENGTH)
            return ERR_FORMAT_ERROR;
        return buf.dump(sizet-TSSD_SIZEA_LENGTH, dest);
    }

    for (int i=0; i<sizea; ++i) {
        if (auto ret = children_[0]->dump(buf, &dest[child.size_ * i])) {
            return ret;
        }
    }
    return OK;
}
