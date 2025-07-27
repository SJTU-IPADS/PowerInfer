#include "az/core/fp16.h"

float az_fp16_to_fp32_table[1 << 16];

void az_precompute_fp16_to_fp32_table(void) {
    for (int s = 0; s < (1 << 16); s++) {
        union {
            uint16_t s;
            az_fp16_t f;
        } cvt;

        cvt.s = s;
        az_fp16_to_fp32_table[s] = AZ_COMPUTE_FP16_TO_FP32(cvt.f);
    }
}
