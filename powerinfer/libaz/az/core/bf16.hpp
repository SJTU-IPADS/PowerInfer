#pragma once

#include <cstdint>
#include <cstring>

namespace az {

struct BFloat16 {
    uint16_t value = 0;

    static auto from_bits(uint16_t bits) -> BFloat16 {
        BFloat16 v;
        v.value = bits;
        return v;
    }

    BFloat16() = default;

    BFloat16(float x) {
        uint32_t bits;
        memcpy(&bits, &x, sizeof(bits));
        value = static_cast<uint16_t>(bits >> 16);
    }

    operator float() const {
        uint32_t bits = static_cast<uint32_t>(value) << 16;
        float x;
        memcpy(&x, &bits, sizeof(x));
        return x;
    }
};

}  // namespace az
