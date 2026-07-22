#include <cstring>
#include <memory>
#include <span>

#include "buffer.h"


Buffer& Buffer::append(const std::span<std::byte> bs)
{
    if (bs.empty()) return *this;
	int written = 0;
    while (written < bs.size())
	{
		if (windex_ == fragments_.size()) {
			auto fra = std::make_shared<Fragment>(MTU);
            if (!heads_.empty()) {
                memcpy(&fra->data[0], &heads_[0], heads_.size());
                woffset_ += heads_.size();
            }
			fragments_.emplace_back(fra);
		}

		auto fra = fragments_[windex_];

		if ( woffset_ + bs.size() - written <= avail(windex_) ) {
			memcpy(&fra->data[woffset_], &bs[written], bs.size() - written);
			//woffset_ += bs.size() - written;
			size_ += bs.size() - written;
            updateOffset(windex_, woffset_, bs.size() - written);
			return *this;
		}

		int fill = avail(windex_) - woffset_;
		memcpy(&fra->data[woffset_], &bs[written], fill);
        updateOffset(windex_, woffset_, bs.size() - written);
		size_ += fill;
		written += fill;
	}
    return *this;
}


TError Buffer::dump(std::size_t size, std::byte *dest) {
    if (size > size_) return ERR_INSUFFICIENT_DATA;

    int read = 0;
    while (read < size)
    {
        if ( offset_ + size - read <= avail(index_)) {
            memcpy(dest, &fragments_[index_]->data[offset_], size - read);
            updateOffset(index_, offset_, size - read);
            size_ -= size - read;
            return OK;
        }

        int remain = avail(index_) - offset_;
        memcpy(dest, &fragments_[index_]->data[offset_], remain);
        size_ -= remain;
        updateOffset(index_, offset_, remain);
        read += remain;
    }
    return OK;
}


Buffer& Buffer::append(const std::byte bt)
{
	return this->append(&bt, 1);
}

Buffer& Buffer::append(const TType bt)
{
    return append(std::byte(bt));
}

int Buffer::dumpSize2()
{
    return dumpSize<std::int16_t>();
}

int Buffer::dumpSize4()
{
    return dumpSize<std::int32_t>();
}

