#include "az/core/buf.hpp"

#include "az/core/aligned_alloc.hpp"

namespace az {

Buf::Buf(Buf &&other) {
    std::swap(data, other.data);
    std::swap(size, other.size);
}

Buf &Buf::operator=(Buf &&other) {
    std::swap(data, other.data);
    std::swap(size, other.size);
    return *this;
}

AlignedBuf::AlignedBuf(size_t size) : Buf(size) {
    data = aligned_alloc(alignment, size);
    AZ_DEBUG_ASSERT(data);
}

AlignedBuf::~AlignedBuf() {
    if (data) {
        aligned_free(data);
        data = nullptr;
        size = 0;
    }
}

}  // namespace az
