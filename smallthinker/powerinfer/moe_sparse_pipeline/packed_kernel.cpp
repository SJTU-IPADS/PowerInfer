#include "moe_sparse_pipeline/packed_kernel.hpp"

namespace moe_sparse_pipeline {

void gemv_q4_0_4x4_q8_0(int n, float * __restrict__ s, const void * __restrict__ vx, const void * __restrict__ vy, int nc) {
    const int qk = QK8_0;
    const int nb = n / qk;
    const int ncols_interleaved = 4;
    const int blocklen = 4;

    POWERINFER_ASSERT(n % qk == 0);
    POWERINFER_ASSERT(nc % ncols_interleaved == 0);

#if ! ((defined(_MSC_VER)) && ! defined(__clang__)) && defined(__aarch64__) && defined(__ARM_NEON) && defined(__ARM_FEATURE_DOTPROD)
    const block_q4_0x4 * b_ptr = (const block_q4_0x4 *) vx;

    for (int c = 0; c < nc; c += ncols_interleaved) {
        const block_q8_0 * a_ptr = (const block_q8_0 *) vy;
        float32x4_t acc = vdupq_n_f32(0);
        for (int b = 0; b < nb; b++) {
            int8x16_t b0 = vld1q_s8((const int8_t *) b_ptr->qs);
            int8x16_t b1 = vld1q_s8((const int8_t *) b_ptr->qs + 16);
            int8x16_t b2 = vld1q_s8((const int8_t *) b_ptr->qs + 32);
            int8x16_t b3 = vld1q_s8((const int8_t *) b_ptr->qs + 48);
            float16x4_t bd = vld1_f16((const __fp16 *) b_ptr->d);

            int8x16_t a0 = vld1q_s8(a_ptr->qs);
            int8x16_t a1 = vld1q_s8(a_ptr->qs + qk/2);
            float16x4_t ad = vld1_dup_f16((const __fp16 *) &a_ptr->d);

            int32x4_t ret = vdupq_n_s32(0);

            ret = vdotq_laneq_s32(ret, b0 << 4, a0, 0);
            ret = vdotq_laneq_s32(ret, b1 << 4, a0, 1);
            ret = vdotq_laneq_s32(ret, b2 << 4, a0, 2);
            ret = vdotq_laneq_s32(ret, b3 << 4, a0, 3);

            ret = vdotq_laneq_s32(ret, b0 & 0xf0U, a1, 0);
            ret = vdotq_laneq_s32(ret, b1 & 0xf0U, a1, 1);
            ret = vdotq_laneq_s32(ret, b2 & 0xf0U, a1, 2);
            ret = vdotq_laneq_s32(ret, b3 & 0xf0U, a1, 3);

            acc = vfmaq_f32(acc, vcvtq_n_f32_s32(ret, 4),
                            vmulq_f32(vcvt_f32_f16(ad), vcvt_f32_f16(bd)));
            a_ptr++;
            b_ptr++;
        }
        vst1q_f32(s, acc);
        s += ncols_interleaved;
    }
#else
    float sumf[4];
    int sumi;

    const block_q8_0 * a_ptr = (const block_q8_0 *) vy;
    for (int x = 0; x < nc / ncols_interleaved; x++) {
        const block_q4_0x4 * b_ptr = (const block_q4_0x4 *) vx + (x * nb);

        for (int j = 0; j < ncols_interleaved; j++) sumf[j] = 0.0;
        for (int l = 0; l < nb; l++) {
            for (int k = 0; k < (qk / (2 * blocklen)); k++) {
                for (int j = 0; j < ncols_interleaved; j++) {
                    sumi = 0;
                    for (int i = 0; i < blocklen; ++i) {
                        const int v0 = (int8_t) (b_ptr[l].qs[k * ncols_interleaved * blocklen + j * blocklen + i] << 4);
                        const int v1 = (int8_t) (b_ptr[l].qs[k * ncols_interleaved * blocklen + j * blocklen + i] & 0xF0);
                        sumi += ((v0 * a_ptr[l].qs[k * blocklen + i]) + (v1 * a_ptr[l].qs[k * blocklen + i + qk / 2])) >> 4;
                    }
                    sumf[j] += sumi * POWERINFER_FP16_TO_FP32(b_ptr[l].d[j]) * POWERINFER_FP16_TO_FP32(a_ptr[l].d);
                }
            }
        }
        for (int j = 0; j < ncols_interleaved; j++) s[x * ncols_interleaved + j] = sumf[j];
    }
#endif
}

auto make_block_q4_0x4(block_q4_0 * in, unsigned int blck_size_interleave) -> block_q4_0x4 {
    block_q4_0x4 out;

    for (int i = 0; i < 4; i++) {
        out.d[i] = in[i].d;
    }

    const int end = QK4_0 * 2 / blck_size_interleave;

    if (blck_size_interleave == 8) {
        const uint64_t xor_mask = 0x8888888888888888ULL;
        for (int i = 0; i < end; ++i) {
            int src_id = i % 4;
            int src_offset = (i / 4) * blck_size_interleave;
            int dst_offset = i * blck_size_interleave;

            uint64_t elems;
            // Using memcpy to avoid unaligned memory accesses
            memcpy(&elems, &in[src_id].qs[src_offset], sizeof(uint64_t));
            elems ^= xor_mask;
            memcpy(&out.qs[dst_offset], &elems, sizeof(uint64_t));
        }
    } else if (blck_size_interleave == 4) {
        const uint32_t xor_mask = 0x88888888;
        for (int i = 0; i < end; ++i) {
            int src_id = i % 4;
            int src_offset = (i / 4) * blck_size_interleave;
            int dst_offset = i * blck_size_interleave;

            uint32_t elems;
            memcpy(&elems, &in[src_id].qs[src_offset], sizeof(uint32_t));
            elems ^= xor_mask;
            memcpy(&out.qs[dst_offset], &elems, sizeof(uint32_t));
        }
    } else {
        POWERINFER_ASSERT(false);
    }

    return out;
}

int repack_q4_0_to_q4_0_4_bl(void * __restrict__ out_data, size_t ne0, size_t ne1, int interleave_block, const void * __restrict__ data, size_t data_size) {
    POWERINFER_ASSERT(interleave_block == 4 || interleave_block == 8);
    constexpr int nrows_interleaved = 4;

    block_q4_0x4 * dst = (block_q4_0x4 *)out_data;
    const block_q4_0 * src = (const block_q4_0 *)data;
    block_q4_0 dst_tmp[4];
    int nrow = ne1;
    int nblocks = ne0 / QK4_0;

    POWERINFER_ASSERT(data_size == nrow * nblocks * sizeof(block_q4_0));

    if (ne1 % nrows_interleaved != 0 || ne0 % 8 != 0) {
        return -1;
    }

    for (int b = 0; b < nrow; b += nrows_interleaved) {
        for (int64_t x = 0; x < nblocks; x++) {
            for (int i = 0; i < nrows_interleaved; i++) {
                dst_tmp[i] = src[x + i * nblocks];
            }
            *dst++ = make_block_q4_0x4(dst_tmp, interleave_block);
        }
        src += nrows_interleaved * nblocks;
    }
    return 0;
};

}
