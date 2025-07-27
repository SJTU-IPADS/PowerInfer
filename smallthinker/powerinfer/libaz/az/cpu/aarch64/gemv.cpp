#include "gemv.hpp"

namespace az::cpu {

#define UNUSED AZ_UNUSED

void gemv_q4_0_4x4_q8_0(
    int n, float *AZ_RESTRICT s, size_t bs, const void *AZ_RESTRICT vx, const void *AZ_RESTRICT vy, int nr, int nc
) {
    const int qk = QK8_0;
    const int nb = n / qk;
    const int ncols_interleaved = 4;
    const int blocklen = 4;

    assert(n % qk == 0);
    assert(nc % ncols_interleaved == 0);

    UNUSED(s);
    UNUSED(bs);
    UNUSED(vx);
    UNUSED(vy);
    UNUSED(nr);
    UNUSED(nc);
    UNUSED(nb);
    UNUSED(ncols_interleaved);
    UNUSED(blocklen);

#if !((defined(_MSC_VER)) && !defined(__clang__)) && defined(__aarch64__) && defined(__ARM_NEON) &&                    \
    defined(__ARM_FEATURE_DOTPROD)

    const block_q4_0x4 *b_ptr = (const block_q4_0x4 *)vx;

    for (int c = 0; c < nc; c += ncols_interleaved) {
        const block_q8_0 *a_ptr = (const block_q8_0 *)vy;
        float32x4_t acc = vdupq_n_f32(0);
        for (int b = 0; b < nb; b++) {
            int8x16_t b0 = vld1q_s8((const int8_t *)b_ptr->qs);
            int8x16_t b1 = vld1q_s8((const int8_t *)b_ptr->qs + 16);
            int8x16_t b2 = vld1q_s8((const int8_t *)b_ptr->qs + 32);
            int8x16_t b3 = vld1q_s8((const int8_t *)b_ptr->qs + 48);
            float16x4_t bd = vld1_f16((const __fp16 *)b_ptr->d);

            int8x16_t a0 = vld1q_s8(a_ptr->qs);
            int8x16_t a1 = vld1q_s8(a_ptr->qs + qk / 2);
            float16x4_t ad = vld1_dup_f16((const __fp16 *)&a_ptr->d);

            int32x4_t ret = vdupq_n_s32(0);

            ret = vdotq_laneq_s32(ret, b0 << 4, a0, 0);
            ret = vdotq_laneq_s32(ret, b1 << 4, a0, 1);
            ret = vdotq_laneq_s32(ret, b2 << 4, a0, 2);
            ret = vdotq_laneq_s32(ret, b3 << 4, a0, 3);

            ret = vdotq_laneq_s32(ret, b0 & 0xf0U, a1, 0);
            ret = vdotq_laneq_s32(ret, b1 & 0xf0U, a1, 1);
            ret = vdotq_laneq_s32(ret, b2 & 0xf0U, a1, 2);
            ret = vdotq_laneq_s32(ret, b3 & 0xf0U, a1, 3);

            acc = vfmaq_f32(acc, vcvtq_n_f32_s32(ret, 4), vmulq_f32(vcvt_f32_f16(ad), vcvt_f32_f16(bd)));
            a_ptr++;
            b_ptr++;
        }
        vst1q_f32(s, acc);
        s += ncols_interleaved;
    }
    return;

#endif  // #if ! ((defined(_MSC_VER)) && ! defined(__clang__)) && defined(__aarch64__) && defined(__ARM_NEON) && defined(__ARM_FEATURE_DOTPROD)
    float sumf[4];
    int sumi;

    const block_q8_0 *a_ptr = (const block_q8_0 *)vy;
    for (int x = 0; x < nc / ncols_interleaved; x++) {
        const block_q4_0x4 *b_ptr = (const block_q4_0x4 *)vx + (x * nb);

        for (int j = 0; j < ncols_interleaved; j++)
            sumf[j] = 0.0;
        for (int l = 0; l < nb; l++) {
            for (int k = 0; k < (qk / (2 * blocklen)); k++) {
                for (int j = 0; j < ncols_interleaved; j++) {
                    sumi = 0;
                    for (int i = 0; i < blocklen; ++i) {
                        const int v0 = (int8_t)(b_ptr[l].qs[k * ncols_interleaved * blocklen + j * blocklen + i] << 4);
                        const int v1 =
                            (int8_t)(b_ptr[l].qs[k * ncols_interleaved * blocklen + j * blocklen + i] & 0xF0);
                        sumi +=
                            ((v0 * a_ptr[l].qs[k * blocklen + i]) + (v1 * a_ptr[l].qs[k * blocklen + i + qk / 2])) >> 4;
                    }
                    sumf[j] += sumi * AZ_FP16_TO_FP32(b_ptr[l].d[j]) * AZ_FP16_TO_FP32(a_ptr[l].d);
                }
            }
        }
        for (int j = 0; j < ncols_interleaved; j++)
            s[x * ncols_interleaved + j] = sumf[j];
    }
}

void gemv_q4_0_4x8_q8_0(
    int n, float *AZ_RESTRICT s, size_t bs, const void *AZ_RESTRICT vx, const void *AZ_RESTRICT vy, int nr, int nc
) {
    const int qk = QK8_0;
    const int nb = n / qk;
    const int ncols_interleaved = 4;
    const int blocklen = 8;

    assert(n % qk == 0);
    assert(nc % ncols_interleaved == 0);

    UNUSED(s);
    UNUSED(bs);
    UNUSED(vx);
    UNUSED(vy);
    UNUSED(nr);
    UNUSED(nc);
    UNUSED(nb);
    UNUSED(ncols_interleaved);
    UNUSED(blocklen);

#if !((defined(_MSC_VER)) && !defined(__clang__)) && defined(__aarch64__) && defined(__ARM_NEON) &&                    \
    defined(__ARM_FEATURE_DOTPROD)

    const block_q4_0x4 *b_ptr = (const block_q4_0x4 *)vx;

    for (int c = 0; c < nc; c += ncols_interleaved) {
        const block_q8_0 *a_ptr = (const block_q8_0 *)vy;
        float32x4_t acc = vdupq_n_f32(0);
        for (int b = 0; b < nb; b++) {
            int8x16_t b0 = vld1q_s8((const int8_t *)b_ptr->qs);
            int8x16_t b1 = vld1q_s8((const int8_t *)b_ptr->qs + 16);
            int8x16_t b2 = vld1q_s8((const int8_t *)b_ptr->qs + 32);
            int8x16_t b3 = vld1q_s8((const int8_t *)b_ptr->qs + 48);
            float16x4_t bd = vld1_f16((const __fp16 *)b_ptr->d);

            int8x16_t a0 = (int8x16_t)vld1q_dup_s64((const int64_t *)a_ptr->qs);
            int8x16_t a1 = (int8x16_t)vld1q_dup_s64((const int64_t *)a_ptr->qs + 1);
            int8x16_t a2 = (int8x16_t)vld1q_dup_s64((const int64_t *)a_ptr->qs + 2);
            int8x16_t a3 = (int8x16_t)vld1q_dup_s64((const int64_t *)a_ptr->qs + 3);
            float16x4_t ad = vld1_dup_f16((const __fp16 *)&a_ptr->d);

            int32x4_t ret0 = vdupq_n_s32(0);
            int32x4_t ret1 = vdupq_n_s32(0);

            ret0 = vdotq_s32(ret0, b0 << 4, a0);
            ret1 = vdotq_s32(ret1, b1 << 4, a0);
            ret0 = vdotq_s32(ret0, b2 << 4, a1);
            ret1 = vdotq_s32(ret1, b3 << 4, a1);

            ret0 = vdotq_s32(ret0, b0 & 0xf0U, a2);
            ret1 = vdotq_s32(ret1, b1 & 0xf0U, a2);
            ret0 = vdotq_s32(ret0, b2 & 0xf0U, a3);
            ret1 = vdotq_s32(ret1, b3 & 0xf0U, a3);

            int32x4_t ret = vpaddq_s32(ret0, ret1);

            acc = vfmaq_f32(acc, vcvtq_n_f32_s32(ret, 4), vmulq_f32(vcvt_f32_f16(ad), vcvt_f32_f16(bd)));
            a_ptr++;
            b_ptr++;
        }
        vst1q_f32(s, acc);
        s += ncols_interleaved;
    }
    return;

#endif  // #if ! ((defined(_MSC_VER)) && ! defined(__clang__)) && defined(__aarch64__) && defined(__ARM_NEON) && defined(__ARM_FEATURE_DOTPROD)
    float sumf[4];
    int sumi;

    const block_q8_0 *a_ptr = (const block_q8_0 *)vy;
    for (int x = 0; x < nc / ncols_interleaved; x++) {
        const block_q4_0x4 *b_ptr = (const block_q4_0x4 *)vx + (x * nb);

        for (int j = 0; j < ncols_interleaved; j++)
            sumf[j] = 0.0;
        for (int l = 0; l < nb; l++) {
            for (int k = 0; k < (qk / (2 * blocklen)); k++) {
                for (int j = 0; j < ncols_interleaved; j++) {
                    sumi = 0;
                    for (int i = 0; i < blocklen; ++i) {
                        const int v0 = (int8_t)(b_ptr[l].qs[k * ncols_interleaved * blocklen + j * blocklen + i] << 4);
                        const int v1 =
                            (int8_t)(b_ptr[l].qs[k * ncols_interleaved * blocklen + j * blocklen + i] & 0xF0);
                        sumi +=
                            ((v0 * a_ptr[l].qs[k * blocklen + i]) + (v1 * a_ptr[l].qs[k * blocklen + i + qk / 2])) >> 4;
                    }
                    sumf[j] += sumi * AZ_FP16_TO_FP32(b_ptr[l].d[j]) * AZ_FP16_TO_FP32(a_ptr[l].d);
                }
            }
        }
        for (int j = 0; j < ncols_interleaved; j++)
            s[x * ncols_interleaved + j] = sumf[j];
    }
}

void gemv_q4_0_8x8_q8_0(
    int n, float *AZ_RESTRICT s, size_t bs, const void *AZ_RESTRICT vx, const void *AZ_RESTRICT vy, int nr, int nc
) {
    const int qk = QK8_0;
    const int nb = n / qk;
    const int ncols_interleaved = 8;
    const int blocklen = 8;

    assert(n % qk == 0);
    assert(nc % ncols_interleaved == 0);

    UNUSED(s);
    UNUSED(bs);
    UNUSED(vx);
    UNUSED(vy);
    UNUSED(nr);
    UNUSED(nc);
    UNUSED(nb);
    UNUSED(ncols_interleaved);
    UNUSED(blocklen);

#if !((defined(_MSC_VER)) && !defined(__clang__)) && defined(__aarch64__)
#if defined(__ARM_FEATURE_SVE)

    const void *b_ptr = vx;
    const void *a_ptr = vy;
    float *res_ptr = s;

    __asm__ __volatile__("ptrue p0.b\n"
                         "add %x[b_ptr], %x[b_ptr], #0x10\n"
                         "1:"  // Column loop
                         "add x22, %x[a_ptr], #0x2\n"
                         "mov z31.b, #0x0\n"
                         "mov x21, %x[nb]\n"
                         "2:"  // Block loop
                         "ld1b { z30.b }, p0/Z, [%x[b_ptr]]\n"
                         "ld1b { z29.b }, p0/Z, [%x[b_ptr], #1, MUL VL]\n"
                         "mov z28.s, #0x0\n"
                         "mov z27.s, #0x0\n"
                         "ld1rd { z26.d }, p0/Z, [x22]\n"
                         "ld1b { z25.b }, p0/Z, [%x[b_ptr], #2, MUL VL]\n"
                         "sub x20, x22, #0x2\n"
                         "sub x21, x21, #0x1\n"
                         "ld1b { z24.b }, p0/Z, [%x[b_ptr], #3, MUL VL]\n"
                         "ld1rd { z23.d }, p0/Z, [x22, #8]\n"
                         "lsl z22.b, z30.b, #0x4\n"
                         "lsl z16.b, z29.b, #0x4\n"
                         "and z30.b, z30.b, #0xf0\n"
                         "and z29.b, z29.b, #0xf0\n"
                         "ld1rd { z21.d }, p0/Z, [x22, #16]\n"
                         "ld1rd { z20.d }, p0/Z, [x22, #24]\n"
                         "lsl z19.b, z25.b, #0x4\n"
                         "and z25.b, z25.b, #0xf0\n"
                         "ld1rh { z17.h }, p0/Z, [x20]\n"
                         "ld1h { z18.s }, p0/Z, [%x[b_ptr], #-1, MUL VL]\n"
                         "sdot z28.s, z22.b, z26.b\n"
                         "sdot z27.s, z16.b, z26.b\n"
                         "lsl z16.b, z24.b, #0x4\n"
                         "add x22, x22, #0x22\n"
                         "and z24.b, z24.b, #0xf0\n"
                         "add %x[b_ptr], %x[b_ptr], #0x90\n"
                         "fcvt z17.s, p0/m, z17.h\n"
                         "fcvt z18.s, p0/m, z18.h\n"
                         "sdot z28.s, z19.b, z23.b\n"
                         "sdot z27.s, z16.b, z23.b\n"
                         "fmul z18.s, z18.s, z17.s\n"
                         "sdot z28.s, z30.b, z21.b\n"
                         "sdot z27.s, z29.b, z21.b\n"
                         "sdot z28.s, z25.b, z20.b\n"
                         "sdot z27.s, z24.b, z20.b\n"
                         "uzp1 z17.s, z28.s, z27.s\n"
                         "uzp2 z16.s, z28.s, z27.s\n"
                         "add z17.s, z17.s, z16.s\n"
                         "asr z17.s, z17.s, #0x4\n"
                         "scvtf z17.s, p0/m, z17.s\n"
                         "fmla z31.s, p0/M, z17.s, z18.s\n"
                         "cbnz x21, 2b\n"
                         "sub %x[nc], %x[nc], #0x8\n"
                         "st1w { z31.s }, p0, [%x[res_ptr]]\n"
                         "add %x[res_ptr], %x[res_ptr], #0x20\n"
                         "cbnz %x[nc], 1b\n"
                         : [b_ptr] "+&r"(b_ptr), [res_ptr] "+&r"(res_ptr), [nc] "+&r"(nc)
                         : [a_ptr] "r"(a_ptr), [nb] "r"(nb)
                         : "memory",
                           "p0",
                           "x20",
                           "x21",
                           "x22",
                           "z16",
                           "z17",
                           "z18",
                           "z19",
                           "z20",
                           "z21",
                           "z22",
                           "z23",
                           "z24",
                           "z25",
                           "z26",
                           "z27",
                           "z28",
                           "z29",
                           "z30",
                           "z31");
    return;

#endif  // #if defined(__ARM_FEATURE_SVE)
#endif  // #if ! ((defined(_MSC_VER)) && ! defined(__clang__)) && defined(__aarch64__)
    {
        float sumf[8];
        int sumi;

        const block_q8_0 *a_ptr = (const block_q8_0 *)vy;
        for (int x = 0; x < nc / ncols_interleaved; x++) {
            const block_q4_0x8 *b_ptr = (const block_q4_0x8 *)vx + (x * nb);

            for (int j = 0; j < ncols_interleaved; j++)
                sumf[j] = 0.0;
            for (int l = 0; l < nb; l++) {
                for (int k = 0; k < (qk / (2 * blocklen)); k++) {
                    for (int j = 0; j < ncols_interleaved; j++) {
                        sumi = 0;
                        for (int i = 0; i < blocklen; ++i) {
                            const int v0 =
                                (int8_t)(b_ptr[l].qs[k * ncols_interleaved * blocklen + j * blocklen + i] << 4);
                            const int v1 =
                                (int8_t)(b_ptr[l].qs[k * ncols_interleaved * blocklen + j * blocklen + i] & 0xF0);
                            sumi += ((v0 * a_ptr[l].qs[k * blocklen + i]) +
                                     (v1 * a_ptr[l].qs[k * blocklen + i + qk / 2])) >>
                                    4;
                        }
                        sumf[j] += sumi * AZ_FP16_TO_FP32(b_ptr[l].d[j]) * AZ_FP16_TO_FP32(a_ptr[l].d);
                    }
                }
            }
            for (int j = 0; j < ncols_interleaved; j++)
                s[x * ncols_interleaved + j] = sumf[j];
        }
    }
}


template <>
void gemv<block_q4_0, 4, 4>(int n, float *s, size_t bs, const void *vx, const void *vy, int nr, int nc) {
    gemv_q4_0_4x4_q8_0(n, s, bs, vx, vy, nr, nc);
}

template <>
void gemv<block_q4_0, 8, 4>(int n, float *s, size_t bs, const void *vx, const void *vy, int nr, int nc) {
    gemv_q4_0_4x8_q8_0(n, s, bs, vx, vy, nr, nc);
}

template <>
void gemv<block_q4_0, 8, 8>(int n, float *s, size_t bs, const void *vx, const void *vy, int nr, int nc) {
    gemv_q4_0_8x8_q8_0(n, s, bs, vx, vy, nr, nc);
}

}  // namespace az::cpu
