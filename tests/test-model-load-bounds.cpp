#include "ggml.h"
#include "llama.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

static const char * const TEST_MODEL = "test-model-load-bounds.gguf";

static void check(bool condition, const char * message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

static void write_u64(std::vector<uint8_t> & data, size_t offset, uint64_t value) {
    check(offset <= data.size() && sizeof(value) <= data.size() - offset, "GGUF tensor offset field is outside the file");
    for (size_t i = 0; i < sizeof(value); ++i) {
        data[offset + i] = uint8_t(value >> (i * 8));
    }
}

static void patch_tensor_offset(const std::string & path, const char * tensor_name, uint64_t offset) {
    std::ifstream input(path, std::ios::binary);
    check(bool(input), "failed to open generated GGUF model");
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();

    const size_t name_size = std::strlen(tensor_name);
    size_t name_pos = std::string::npos;
    for (size_t i = sizeof(uint32_t) * 4; i + name_size <= data.size(); ++i) {
        if (std::memcmp(data.data() + i, tensor_name, name_size) == 0) {
            name_pos = i;
            break;
        }
    }
    check(name_pos != std::string::npos, "failed to locate tensor name in generated GGUF model");

    const size_t dims_pos = name_pos + name_size;
    check(dims_pos <= data.size() && sizeof(uint32_t) <= data.size() - dims_pos, "truncated GGUF tensor metadata");
    uint32_t n_dims = 0;
    std::memcpy(&n_dims, data.data() + dims_pos, sizeof(n_dims));

    const size_t offset_pos = dims_pos + sizeof(n_dims) + sizeof(uint64_t) * n_dims + sizeof(uint32_t);
    write_u64(data, offset_pos, offset);

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    check(bool(output), "failed to rewrite generated GGUF model");
    output.write(reinterpret_cast<const char *>(data.data()), data.size());
    check(bool(output), "failed to write generated GGUF model");
}

static void write_test_model(const std::string & path) {
    gguf_context * gguf = gguf_init_empty();
    check(gguf != nullptr, "failed to create GGUF context");

    gguf_set_val_str(gguf, "general.architecture", "llama");
    gguf_set_val_u32(gguf, "llama.context_length", 1);
    gguf_set_val_u32(gguf, "llama.embedding_length", 1);
    gguf_set_val_u32(gguf, "llama.feed_forward_length", 1);
    gguf_set_val_u32(gguf, "llama.attention.head_count", 1);
    gguf_set_val_u32(gguf, "llama.block_count", 1);
    gguf_set_val_f32(gguf, "llama.attention.layer_norm_rms_epsilon", 1e-5f);
    gguf_set_val_str(gguf, "tokenizer.ggml.model", "llama");
    gguf_set_val_u32(gguf, "tokenizer.ggml.bos_token_id", 0);
    gguf_set_val_u32(gguf, "tokenizer.ggml.eos_token_id", 0);
    gguf_set_val_u32(gguf, "tokenizer.ggml.unknown_token_id", 0);

    const char * tokens[] = { "<0x0A>" };
    const float scores[] = { 0.0f };
    gguf_set_arr_str(gguf, "tokenizer.ggml.tokens", tokens, 1);
    gguf_set_arr_data(gguf, "tokenizer.ggml.scores", GGUF_TYPE_FLOAT32, scores, 1);

    ggml_init_params params = {
        /*.mem_size   =*/ 16 * 1024,
        /*.mem_buffer =*/ nullptr,
        /*.no_alloc   =*/ false,
    };
    ggml_context * ctx = ggml_init(params);
    check(ctx != nullptr, "failed to create GGML context");

    const char * names[] = {
        "token_embd.weight",
        "output_norm.weight",
        "output.weight",
        "blk.0.attn_norm.weight",
        "blk.0.attn_q.weight",
        "blk.0.attn_k.weight",
        "blk.0.attn_v.weight",
        "blk.0.attn_output.weight",
        "blk.0.ffn_norm.weight",
        "blk.0.ffn_gate.weight",
        "blk.0.ffn_down.weight",
        "blk.0.ffn_up.weight",
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        const char * name = names[i];
        ggml_tensor * tensor = i == 1 || i == 3 || i == 8
            ? ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 1)
            : ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 1, 1);
        ggml_set_name(tensor, name);
        *static_cast<float *>(tensor->data) = 1.0f;
        gguf_add_tensor(gguf, tensor);
    }

    gguf_write_to_file(gguf, path.c_str(), false);
    ggml_free(ctx);
    gguf_free(gguf);
}

static bool load_model(const std::string & path, bool use_mmap) {
    llama_model_params params = llama_model_default_params();
    params.use_mmap = use_mmap;

    llama_model * model = llama_load_model_from_file(path.c_str(), params);
    if (model == nullptr) {
        return false;
    }

    llama_free_model(model);
    return true;
}

static size_t get_data_offset(const std::string & path) {
    gguf_init_params params = {
        /*.no_alloc = */ true,
        /*.ctx      = */ nullptr,
    };
    gguf_context * gguf = gguf_init_from_file(path.c_str(), params);
    check(gguf != nullptr, "failed to parse generated GGUF model");
    const size_t offset = gguf_get_data_offset(gguf);
    gguf_free(gguf);
    return offset;
}

static void check_bounds_rejected(const std::string & path, uint64_t tensor_offset) {
    patch_tensor_offset(path, "output_norm.weight", tensor_offset);
    check(!load_model(path, true), "mmap model loading accepted a tensor outside the file bounds");
    check(!load_model(path, false), "non-mmap model loading accepted a tensor outside the file bounds");
}

int main() {
    llama_backend_init(false);

    int result = 0;
    try {
        write_test_model(TEST_MODEL);
        check(load_model(TEST_MODEL, true), "mmap model loading rejected a valid model");
        check(load_model(TEST_MODEL, false), "non-mmap model loading rejected a valid model");

        std::ifstream input(TEST_MODEL, std::ios::binary | std::ios::ate);
        check(bool(input), "failed to open generated GGUF model for size query");
        const std::streamoff file_size = input.tellg();
        check(file_size >= 0, "failed to determine generated GGUF model size");
        const size_t data_offset = get_data_offset(TEST_MODEL);
        check(static_cast<size_t>(file_size) >= data_offset + 4, "generated GGUF model has no tensor data");

        check_bounds_rejected(TEST_MODEL, static_cast<uint64_t>(static_cast<size_t>(file_size) - data_offset));
        check_bounds_rejected(TEST_MODEL, static_cast<uint64_t>(static_cast<size_t>(file_size) - data_offset - 2));
        check_bounds_rejected(TEST_MODEL, std::numeric_limits<uint64_t>::max());
    } catch (const std::exception & error) {
        std::fprintf(stderr, "test-model-load-bounds: %s\n", error.what());
        result = 1;
    }

    std::remove(TEST_MODEL);
    llama_backend_free();
    return result;
}
