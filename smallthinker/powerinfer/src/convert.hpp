#pragma once

#include <vector>
#include <cstring>
#include <cstdint>

#include "powerinfer-type.hpp"

inline void convert_intel_q4_format_1(uint8_t *cur_buffer_ptr, const size_t num_element) {
    const int num_block   = num_element / QK4_0;
    const int quant_size  = num_element / QR4_0;
    const int size        = quant_size + num_block * sizeof(uint16_t);

    std::vector<block_q4_0_ref> input_buffer(num_block);
    POWERINFER_ASSERT(size == num_block * sizeof(block_q4_0_ref));
    std::memcpy(input_buffer.data(), cur_buffer_ptr, size);

    uint8_t * __restrict__ output_quant = cur_buffer_ptr;
    uint16_t *__restrict__ output_scale = reinterpret_cast<uint16_t *>(cur_buffer_ptr + quant_size);

    for (int block_id = 0; block_id < num_block; ++block_id) {
        const int base_quant_id         = block_id * QK4_0 / QR4_0;
        const block_q4_0_ref cur_block  = input_buffer[block_id];
        const int scale_id              = block_id;

        output_scale[scale_id] = cur_block.d;
        std::memcpy(output_quant + base_quant_id, cur_block.qs, QK4_0 / QR4_0);
    }
}

inline void convert_intel_q4_format_2(uint8_t *cur_buffer_ptr, const size_t num_expert, const size_t num_row, const size_t num_element) {
    const int num_block   = num_element / QK4_0;
    const int quant_size  = num_element / 2;
    const int size        = quant_size + num_block * sizeof(uint16_t);

    const int num_expert_row    = num_row   / num_expert;
    const int num_row_block     = num_block / num_row;
    const int num_expert_block  = num_expert_row * num_row_block;

    constexpr int num_inner_block = QK4_0 / 32;

    std::vector<block_q4_0_ref> input_buffer(num_block);
    POWERINFER_ASSERT(size == num_block * sizeof(block_q4_0_ref));
    std::memcpy(input_buffer.data(), cur_buffer_ptr, size);

    uint8_t * __restrict__ output_quant = cur_buffer_ptr;
    uint16_t *__restrict__ output_scale = reinterpret_cast<uint16_t *>(cur_buffer_ptr + quant_size);

    for (int block_id = 0; block_id < num_block; ++block_id) {
        const int base_quant_id = block_id * QK4_0 / 2;

        const int expert_id       = block_id / num_expert_block;
        const int expert_block_id = block_id % num_expert_block;
        const int expert_row_id   = expert_block_id / num_row_block;
        const int row_block_id    = expert_block_id % num_row_block;
        const int scale_id        = expert_id * num_expert_block + row_block_id * num_expert_row + expert_row_id;

        const block_q4_0_ref block = input_buffer[block_id];

        uint8_t output_quant_tmp[QK4_0 / QR4_0]{0};
        for (int inner_block_id = 0; inner_block_id < num_inner_block; ++inner_block_id) {
            for (int inner_quant_id = 0; inner_quant_id < 16; ++inner_quant_id) {
                const int in_quant_id    = inner_block_id * 16 + inner_quant_id;
                const int out_quant_id_0 = inner_block_id * 16 + inner_quant_id / 2;
                const int out_quant_id_1 = inner_block_id * 16 + inner_quant_id / 2 + 8;

                if (in_quant_id % 2 == 0) {
                    output_quant_tmp[out_quant_id_0] |= block.qs[in_quant_id] & 0x0FU;
                    output_quant_tmp[out_quant_id_1] |= block.qs[in_quant_id] >>   4U;
                } else {
                    output_quant_tmp[out_quant_id_0] |= block.qs[in_quant_id] <<   4U;
                    output_quant_tmp[out_quant_id_1] |= block.qs[in_quant_id] & 0xF0U;
                }
            }
        }

        output_scale[scale_id]   = input_buffer[block_id].d;
        std::memcpy(output_quant + base_quant_id, output_quant_tmp, sizeof(output_quant_tmp));
    }
}