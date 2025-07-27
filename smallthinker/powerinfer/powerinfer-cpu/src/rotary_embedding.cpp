
#include "powerinfer-cpu.hpp"
#include "powerinfer-type.hpp"

inline static void apply_token_rotary_embedding(
    const float * arr,
    float       * dst_arr,
    const float * cos_sin_ptr,
    int rot_offset, int embed_dim, bool is_neox) {

    int x_index;
    int y_index;

    const float cos_val = cos_sin_ptr[rot_offset * 2];
    const float sin_val = cos_sin_ptr[rot_offset * 2 + 1];

    if (is_neox) {
        x_index = rot_offset;
        y_index = embed_dim + rot_offset;
    } else {
        x_index = 2 * rot_offset;
        y_index = 2 * rot_offset + 1;
    }

    const float x = arr[x_index];
    const float y = arr[y_index];
    dst_arr[x_index] = x * cos_val - y * sin_val;
    dst_arr[y_index] = y * cos_val + x * sin_val;
}

PowerInferError powerinfer_host_rotary_embedding_fp32_impl(const HostComputeParam &param, const PositionEmbeddingMetadata &meta, const float *rope_cache_ptr,
    const float *key_ptr, const float *query_ptr, const int *position_ptr,
    float *dst_ptr, const int mode, const int num_token, const int num_head_q, const int num_head_kv,
    const int query_row_size, const int key_row_size) {

    const int ith               = param.ith;
    const int nth               = param.nth;

    const int head_dim = meta.rotary_dim;
    const int rot_dim  = meta.rotary_dim;

    float *dst_q_ptr = dst_ptr;
    float *dst_k_ptr = (float *)((char *)dst_ptr + num_token * query_row_size);

    const bool is_neox = (mode & 2);
    for (int token_id = 0; token_id < num_token; ++token_id) {
        const int32_t pos = position_ptr[token_id];

        const float *cache_ptr = rope_cache_ptr + pos * rot_dim;

        const int embed_dim = rot_dim / 2;

        const int nq      = num_head_q * embed_dim;
        const int avg_nq  = (nq + nth - 1) / nth;
        const int begin_q = avg_nq * ith;
        const int end_q   = begin_q + avg_nq < nq ? begin_q + avg_nq : nq;
        for (int i = begin_q; i < end_q; ++i) {
            const int head_idx   = i / embed_dim;
            const int rot_offset = i % embed_dim;
            const int64_t token_head = token_id * query_row_size + head_idx * head_dim * sizeof(float);
            apply_token_rotary_embedding(
                (const float *)((const char *)query_ptr + token_head),
                (float *)((char *)dst_q_ptr + token_head),
                cache_ptr, rot_offset, embed_dim, is_neox);
        }

        const int nk        = num_head_kv * embed_dim;
        const int avg_nk    = (nk + nth - 1) / nth;
        const int begin_k   = avg_nk * ith;
        const int end_k     = begin_k + avg_nk < nk ? begin_k + avg_nk : nk;
        for (int i = begin_k; i < end_k; ++i) {
            const int head_idx   = i / embed_dim;
            const int rot_offset = i % embed_dim;
            const int64_t token_head = token_id * key_row_size + head_idx * head_dim * sizeof(float);
            apply_token_rotary_embedding(
                (const float *)((const char *)key_ptr + token_head),
                (float *)((char *)dst_k_ptr + token_head),
                cache_ptr, rot_offset, embed_dim, is_neox);
        }
    }
    return {false, "Success"};
}