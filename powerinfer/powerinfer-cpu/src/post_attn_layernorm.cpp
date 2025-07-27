#include <cstring>
#include <cmath>

#include "powerinfer-cpu.hpp"

PowerInferError powerinfer_host_post_attn_layernorm_impl(HostComputeParam param,
    const float *inpSA_data, const float *attn_out_data, const float *weight_data, const float *bias_data,
    float *dst_data, float *residual_data,
    int ne00, int nrows, float eps) {

    const int ith               = param.ith;
    const int nth               = param.nth;

    for (int i01 = ith; i01 < nrows; i01 += nth) {
        const float * attn_out_row  = attn_out_data + i01 * ne00;
        const float * inpSA_row     = inpSA_data    + i01 * ne00;
        float * y                   = dst_data      + i01 * ne00;
        float * residual            = residual_data + i01 * ne00;

        for (int i00 = 0; i00 < ne00; i00++) { residual[i00] = attn_out_row[i00] + inpSA_row[i00]; }

        float sum = 0.0;
        for (int i00 = 0; i00 < ne00; i00++) { sum += residual[i00] * residual[i00]; }

        const float mean  = sum   / static_cast<float>(ne00);
        const float scale = 1.0f  / std::sqrt(mean + eps);

        std::memcpy(y, residual, ne00 * sizeof(float));

        for (int i00 = 0; i00 < ne00; i00++) { y[i00] *= scale; }
        if (weight_data) {
            for (int i00 = 0; i00 < ne00; i00++) { y[i00] *= weight_data[i00]; }
        }
        if (bias_data) {
            for (int i00 = 0; i00 < ne00; i00++) { y[i00] += bias_data[i00]; }
        }
    }

    return { false, "Success" };
}