#include "powerinfer-cpu-data.hpp"

#include <cstdint>
#include <cstring>

inline uint32_t compare_fp32x32_array(const float * __restrict__ array, const float value) {
    uint32_t result = 0;

#if defined(__AVX2__)
    // 将 value 广播到所有通道
    __m256 value_vec = _mm256_set1_ps(value);

    for (uint32_t i = 0; i < 32; i += 8) {
        __m256 v0    = _mm256_loadu_ps(&array[0]);
        __m256 v1    = _mm256_loadu_ps(&array[8]);
        __m256 v2    = _mm256_loadu_ps(&array[16]);
        __m256 v3    = _mm256_loadu_ps(&array[24]);

        __m256 cmp_result_0   = _mm256_cmp_ps(v0, value_vec, _CMP_GT_OQ);
        __m256 cmp_result_1   = _mm256_cmp_ps(v1, value_vec, _CMP_GT_OQ);
        __m256 cmp_result_2   = _mm256_cmp_ps(v2, value_vec, _CMP_GT_OQ);
        __m256 cmp_result_3   = _mm256_cmp_ps(v3, value_vec, _CMP_GT_OQ);

        result  =   static_cast<uint32_t>(_mm256_movemask_ps(cmp_result_0))        | 
                    static_cast<uint32_t>(_mm256_movemask_ps(cmp_result_1)) << 8U  |
                    static_cast<uint32_t>(_mm256_movemask_ps(cmp_result_2)) << 16U |
                    static_cast<uint32_t>(_mm256_movemask_ps(cmp_result_3)) << 24U;
    }
#elif defined(__ARM_NEON) 
    float32x4_t value_vec = vdupq_n_f32(value);

    for (uint32_t i = 0; i < 32; i += 4) {
        const float32x4_t v_val = vdupq_n_f32(value);

        // 分8组处理，每组4个元素
        uint8_t masks[8];
        for (int i = 0; i < 8; ++i) {
            float32x4_t data = vld1q_f32(&array[i*4]);
            uint32x4_t cmp = vcgtq_f32(data, v_val);
            
            // 提取4个比较结果的符号位
            uint32_t m0 = vgetq_lane_u32(cmp, 0) >> 31;
            uint32_t m1 = vgetq_lane_u32(cmp, 1) >> 31;
            uint32_t m2 = vgetq_lane_u32(cmp, 2) >> 31;
            uint32_t m3 = vgetq_lane_u32(cmp, 3) >> 31;
            
            // 合并为4bit掩码
            masks[i] = m0 | (m1 << 1) | (m2 << 2) | (m3 << 3);
        }

        // 合并最终32bit掩码
        result =    (masks[0]  | (masks[1] << 4))           |
                    ((masks[2] | (masks[3] << 4)) << 8)     |
                    ((masks[4] | (masks[5] << 4)) << 16)    |
                    ((masks[6] | (masks[7] << 4)) << 24);
    }
#else 
    for (uint32_t i = 0; i < 32; ++i) {
        if (array[i] > value) {
            result |= (1u << i);
        }
    }
#endif

    return result;
}
