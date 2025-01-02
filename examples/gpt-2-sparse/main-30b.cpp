#include "ggml.h"
#include "ggml-alloc.h"
#include <regex>

#include "common.h"
#include "common-ggml.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include "ggml-cuda.h"

#if defined(_MSC_VER)
#pragma warning(disable: 4244 4267) // possible loss of data
#endif
typedef void (*offload_func_t)(struct ggml_tensor * tensor);
void opt_nop(struct ggml_tensor * tensor) { // don't offload by default
    (void) tensor;
}
// default hparams (GPT-2 117M)
struct gpt2_hparams {
    int32_t n_vocab = 50257;
    int32_t n_ctx   = 1024;
    int32_t n_embd  = 768;
    int32_t n_head  = 12;
    int32_t n_layer = 12;
    int32_t ftype   = 1;
    float   eps     = 1e-5f;
};

struct gpt2_layer {
    // normalization
    struct ggml_tensor * ln_1_g;
    struct ggml_tensor * ln_1_b;

    struct ggml_tensor * ln_2_g;
    struct ggml_tensor * ln_2_b;

    // attention
    // struct ggml_tensor * c_attn_attn_w;
    // struct ggml_tensor * c_attn_attn_b;

    struct ggml_tensor * c_attn_attn_q_w;
    struct ggml_tensor * c_attn_attn_q_b;

    struct ggml_tensor * c_attn_attn_k_w;
    struct ggml_tensor * c_attn_attn_k_b;

    struct ggml_tensor * c_attn_attn_v_w;
    struct ggml_tensor * c_attn_attn_v_b;

    struct ggml_tensor * c_attn_proj_w;
    struct ggml_tensor * c_attn_proj_b;

    // mlp
    struct ggml_tensor * c_mlp_fc_w;
    struct ggml_tensor * c_mlp_fc_b;

    struct ggml_tensor * c_mlp_proj_w;
    struct ggml_tensor * c_mlp_proj_b;

    struct ggml_tensor * gpu_idx;
    struct ggml_tensor * gpu_bucket;
    // gpu heat
    struct ggml_tensor * c_mlp_fc_w_gpu;
    struct ggml_tensor * c_mlp_proj_w_t;
    struct ggml_tensor * c_mlp_proj_w_gpu;

    //predictor
    struct ggml_tensor * mlp_pre_w1_w;
    struct ggml_tensor * mlp_pre_w2_w;
};

struct opt_file {
    // use FILE * so we don't have to re-open the file to mmap
    FILE * fp;
    size_t size;

    opt_file(const char * fname, const char * mode) {
        fp = std::fopen(fname, mode);
        if (fp == NULL) {
            throw std::runtime_error("opt_file fail\n");
		}
		seek(0, SEEK_END);
		size = tell();
		seek(0, SEEK_SET);
    }
	size_t tell() const {
#ifdef _WIN32
		__int64 ret = _ftelli64(fp);
#else
		long ret = std::ftell(fp);
#endif
		GGML_ASSERT(ret != -1); // this really shouldn't fail
		return (size_t) ret;
	}

	void seek(size_t offset, int whence) {
#ifdef _WIN32
		int ret = _fseeki64(fp, (__int64) offset, whence);
#else
		int ret = std::fseek(fp, (long) offset, whence);
#endif
		GGML_ASSERT(ret == 0); // same
	}

    ~opt_file() {
        if (fp) {
            std::fclose(fp);
        }
    }
};
#define _POSIX_MAPPED_FILES
#include <sys/types.h>
#include <sys/mman.h>

struct opt_mmap {
    void * addr;
    size_t size;

    opt_mmap(const opt_mmap &) = delete;

#ifdef _POSIX_MAPPED_FILES
    static constexpr bool SUPPORTED = true;

    opt_mmap(struct opt_file * file, size_t prefetch = (size_t) -1 /* -1 = max value */, bool numa = false) {
        size = file->size;
        int fd = fileno(file->fp);
        int flags = MAP_SHARED;
        // prefetch/readahead impairs performance on NUMA systems
        if (numa) { prefetch = 0; }
#ifdef __linux__
        if (prefetch) { flags |= MAP_POPULATE; }
#endif
        addr = mmap(NULL, file->size, PROT_READ, flags, fd, 0);
        if (addr == MAP_FAILED) {
            throw std::runtime_error("mmap failed\n");
        }

        if (prefetch > 0) {
            // Advise the kernel to preload the mapped memory
            if (madvise(addr, std::min(file->size, prefetch), MADV_WILLNEED)) {
                fprintf(stderr, "warning: madvise(.., MADV_WILLNEED) failed: %s\n",
                        strerror(errno));
            }
        }
        if (numa) {
            // advise the kernel not to use readahead
            // (because the next page might not belong on the same node)
            if (madvise(addr, file->size, MADV_RANDOM)) {
                fprintf(stderr, "warning: madvise(.., MADV_RANDOM) failed: %s\n",
                        strerror(errno));
            }
        }
    }

    ~opt_mmap() {
        munmap(addr, size);
    }
#else
    static constexpr bool SUPPORTED = false;

    opt_mmap(struct opt_file *, bool prefetch = true, bool numa = false) {
        (void) prefetch;
        (void) numa;

        throw std::runtime_error(std::string("mmap not supported"));
    }
#endif
};

struct gpt2_model {
    gpt2_hparams hparams;
    struct opt_file * file;
    struct opt_mmap * mapping;

    // normalization
    struct ggml_tensor * ln_f_g;
    struct ggml_tensor * ln_f_b;

    struct ggml_tensor * wte;     // position embedding
    struct ggml_tensor * wpe;     //    token embedding
    struct ggml_tensor * lm_head; // language model head

    std::vector<gpt2_layer> layers;

    // key + value memory
    struct ggml_tensor * memory_k;
    struct ggml_tensor * memory_v;

    //
    struct ggml_context * ctx;
    std::map<std::string, struct ggml_tensor **> tensors;
};

struct ggml_context * ctx0 = nullptr;
// std::vector<uint8_t> compute_buffer;
void *compute_buffer;

bool endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) {
        return false;
    }
    return str.substr(str.length() - suffix.length()) == suffix;
}


// load the model's weights from a file
bool gpt2_model_load(const std::string & fname, gpt2_model & model, gpt_vocab & vocab, gpt_params model_params) {
    printf("%s: loading model from '%s'\n", __func__, fname.c_str());
    model.file = new opt_file(fname.c_str(), "rb");
    printf("size %d\n", model.file->size);
    model.mapping = new opt_mmap(model.file, 0, false);

    auto fin = std::ifstream(fname, std::ios::binary);
    if (!fin) {
        fprintf(stderr, "%s: failed to open '%s'\n", __func__, fname.c_str());
        return false;
    }

    // verify magic
    {
        uint32_t magic;
        fin.read((char *) &magic, sizeof(magic));
        if (magic != GGML_FILE_MAGIC) {
            fprintf(stderr, "%s: invalid model file '%s' (bad magic)\n", __func__, fname.c_str());
            return false;
        }
    }

    // load hparams
    {
        auto & hparams = model.hparams;

        fin.read((char *) &hparams.n_vocab, sizeof(hparams.n_vocab));
        fin.read((char *) &hparams.n_ctx,   sizeof(hparams.n_ctx));
        fin.read((char *) &hparams.n_embd,  sizeof(hparams.n_embd));
        fin.read((char *) &hparams.n_head,  sizeof(hparams.n_head));
        fin.read((char *) &hparams.n_layer, sizeof(hparams.n_layer));
        fin.read((char *) &hparams.ftype,   sizeof(hparams.ftype));

        const int32_t qntvr = hparams.ftype / GGML_QNT_VERSION_FACTOR;

        printf("%s: n_vocab = %d\n", __func__, hparams.n_vocab);
        printf("%s: n_ctx   = %d\n", __func__, hparams.n_ctx);
        printf("%s: n_embd  = %d\n", __func__, hparams.n_embd);
        printf("%s: n_head  = %d\n", __func__, hparams.n_head);
        printf("%s: n_layer = %d\n", __func__, hparams.n_layer);
        printf("%s: ftype   = %d\n", __func__, hparams.ftype);
        printf("%s: qntvr   = %d\n", __func__, qntvr);

        hparams.ftype %= GGML_QNT_VERSION_FACTOR;
    }

    // load vocab
    {
        /* int32_t n_vocab = 0; */
        /* fin.read((char *) &n_vocab, sizeof(n_vocab)); */

        /* if (n_vocab != model.hparams.n_vocab) { */
        /*     fprintf(stderr, "%s: invalid model file '%s' (bad vocab size %d != %d)\n", */
        /*             __func__, fname.c_str(), n_vocab, model.hparams.n_vocab); */
        /*     return false; */
        /* } */
        int32_t n_vocab = model.hparams.n_vocab;

        std::string word;
        std::vector<char> buf(128);

        for (int i = 0; i < n_vocab; i++) {
            uint32_t len;
            fin.read((char *) &len, sizeof(len));

            buf.resize(len);
            fin.read((char *) buf.data(), len);
            word.assign(buf.data(), len);

            vocab.token_to_id[word] = i;
            vocab.id_to_token[i] = word;
        }
    }

    // for the big tensors, we have the option to store the data in 16-bit floats or quantized
    // in order to save memory and also to speed up the computation
    ggml_type wtype = ggml_ftype_to_ggml_type((ggml_ftype) (model.hparams.ftype));
    if (wtype == GGML_TYPE_COUNT) {
        fprintf(stderr, "%s: invalid model file '%s' (bad ftype value %d)\n",
                __func__, fname.c_str(), model.hparams.ftype);
        return false;
    }
    printf("wtype %d\n", wtype);

    auto & ctx = model.ctx;

    size_t ctx_size = 0;

    {
        const auto & hparams = model.hparams;

        const int n_embd  = hparams.n_embd;
        const int n_layer = hparams.n_layer;
        const int n_ctx   = hparams.n_ctx;
        const int n_vocab = hparams.n_vocab;

        ctx_size += n_embd*ggml_type_sizef(GGML_TYPE_F32); // ln_f_g
        ctx_size += n_embd*ggml_type_sizef(GGML_TYPE_F32); // ln_f_b

        ctx_size += n_vocab*n_embd*ggml_type_sizef(wtype);         // wte
        ctx_size +=   n_ctx*n_embd*ggml_type_sizef(GGML_TYPE_F32); // wpe
        ctx_size += n_vocab*n_embd*ggml_type_sizef(wtype);         // lm_head

        ctx_size += n_layer*(n_embd*ggml_type_sizef(GGML_TYPE_F32)); // ln_1_g
        ctx_size += n_layer*(n_embd*ggml_type_sizef(GGML_TYPE_F32)); // ln_1_b

        ctx_size += n_layer*(n_embd*ggml_type_sizef(GGML_TYPE_F32)); // ln_2_g
        ctx_size += n_layer*(n_embd*ggml_type_sizef(GGML_TYPE_F32)); // ln_2_b

        ctx_size += n_layer*(3*n_embd*n_embd*ggml_type_sizef(wtype));         // c_attn_attn_w
        ctx_size += n_layer*(       3*n_embd*ggml_type_sizef(GGML_TYPE_F32)); // c_attn_attn_b

        ctx_size += n_layer*(n_embd*n_embd*ggml_type_sizef(wtype));           // c_attn_proj_w
        ctx_size += n_layer*(       n_embd*ggml_type_sizef(GGML_TYPE_F32));   // c_attn_proj_b

        ctx_size += n_layer*(4*n_embd*n_embd*ggml_type_sizef(wtype));         // c_mlp_fc_w
        ctx_size += n_layer*(       4*n_embd*ggml_type_sizef(GGML_TYPE_F32)); // c_mlp_fc_b

        //need refactor
        ctx_size += n_layer*(4096*4*ggml_type_sizef(GGML_TYPE_I32));          // gpu_idx
        ctx_size += n_layer*(4096*4*ggml_type_sizef(GGML_TYPE_I32));          // gpu_bucket
        ctx_size += n_layer*(4096*n_embd*4*ggml_type_sizef(wtype));         // c_mlp_fc_w_h20
        ctx_size += n_layer*(4096*n_embd*4*ggml_type_sizef(wtype));
        //predictor
        ctx_size += n_layer*(4096*1024*ggml_type_sizef(GGML_TYPE_F32));          // pre_w
        ctx_size += n_layer*(4096*4*ggml_type_sizef(GGML_TYPE_F32));          // pre_b
        ctx_size += n_layer*(4096 * 4*1024*ggml_type_sizef(GGML_TYPE_F32));          // pre_w
        ctx_size += n_layer*(4096*ggml_type_sizef(GGML_TYPE_F32));          // pre_b

        ctx_size += n_layer*(4*n_embd*n_embd*ggml_type_sizef(wtype));         // c_mlp_proj_w

        ctx_size += n_layer*(4*n_embd*n_embd*ggml_type_sizef(wtype));         // c_mlp_proj_w
        ctx_size += n_layer*(         n_embd*ggml_type_sizef(GGML_TYPE_F32)); // c_mlp_proj_b
        ctx_size = 0;

        ctx_size += n_ctx*n_layer*n_embd*ggml_type_sizef(GGML_TYPE_F32); // memory_k
        ctx_size += n_ctx*n_layer*n_embd*ggml_type_sizef(GGML_TYPE_F32); // memory_v

        ctx_size += (6 + 12*n_layer)*51200; // object overhead

        printf("%s: ggml tensor size = %d bytes\n", __func__, (int) sizeof(ggml_tensor));
        printf("%s: ggml ctx size = %6.2f MB\n", __func__, ctx_size/(1024.0*1024.0));
    }

    // create the ggml context
    {
        struct ggml_init_params params = {
            /*.mem_size   =*/ ctx_size,
            /*.mem_buffer =*/ NULL,
            /*.no_alloc   =*/ false,
        };

        model.ctx = ggml_init(params);
        if (!model.ctx) {
            fprintf(stderr, "%s: ggml_init() failed\n", __func__);
            return false;
        }
    }
    int main_gpu = 0;
#if defined(GGML_USE_CUBLAS)
    fprintf(stderr, "%s: using CUDA for GPU acceleration\n", __func__);
    ggml_cuda_set_main_device(main_gpu);
#define OPT_BACKEND_OFFLOAD GGML_BACKEND_GPU
#define OPT_BACKEND_OFFLOAD_SPLIT GGML_BACKEND_GPU_SPLIT
#else
#define OPT_BACKEND_OFFLOAD GGML_BACKEND_CPU
#define OPT_BACKEND_OFFLOAD_SPLIT GGML_BACKEND_CPU
#endif
    

    // prepare memory for the weights
    {
        const auto & hparams = model.hparams;

        const int n_embd  = hparams.n_embd;
        const int n_layer = hparams.n_layer;
        const int n_ctx   = hparams.n_ctx;
        const int n_vocab = hparams.n_vocab;

        model.layers.resize(n_layer);

        // model.ln_f_g = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, n_embd);
        // model.ln_f_b = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, n_embd);
        // model.ln_f_g->backend = OPT_BACKEND_OFFLOAD;
        // model.ln_f_b->backend = OPT_BACKEND_OFFLOAD;

        // model.wte     = ggml_new_tensor_2d(ctx, wtype,         n_embd, n_vocab);
        // model.wpe     = ggml_new_tensor_2d(ctx, wtype, n_embd, n_ctx+2);
        // model.lm_head = ggml_new_tensor_2d(ctx, wtype,         n_embd, n_vocab);
        
        // model.lm_head->backend = OPT_BACKEND_OFFLOAD;

        // map by name
        model.tensors["output_norm.weight"] = &model.ln_f_g;
        model.tensors["output_norm.bias"] = &model.ln_f_b;

        model.tensors["tok_embeddings.weight"]     = &model.wte;
        model.tensors["pos_embeddings.weight"]     = &model.wpe;
        model.tensors["output.weight"] = &model.lm_head;

        for (int i = 0; i < n_layer; ++i) {
            auto & layer = model.layers[i];
            memset(&layer, 0, sizeof(gpt2_layer));

        //     layer.ln_1_g        = ggml_new_tensor_1d(ctx, GGML_TYPE_F32,   n_embd);
        //     layer.ln_1_b        = ggml_new_tensor_1d(ctx, GGML_TYPE_F32,   n_embd);

        //     layer.ln_2_g        = ggml_new_tensor_1d(ctx, GGML_TYPE_F32,   n_embd);
        //     layer.ln_2_b        = ggml_new_tensor_1d(ctx, GGML_TYPE_F32,   n_embd);

        //     // layer.c_attn_attn_w = ggml_new_tensor_2d(ctx, wtype,           n_embd, 3*n_embd);
        //     // layer.c_attn_attn_b = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 3*n_embd);
        //     layer.c_attn_attn_q_w = ggml_new_tensor_2d(ctx, wtype,         n_embd, n_embd);
        //     layer.c_attn_attn_q_b = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, n_embd);

        //     layer.c_attn_attn_k_w = ggml_new_tensor_2d(ctx, wtype,         n_embd, n_embd);
        //     layer.c_attn_attn_k_b = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, n_embd);

        //     layer.c_attn_attn_v_w = ggml_new_tensor_2d(ctx, wtype,         n_embd, n_embd);
        //     layer.c_attn_attn_v_b = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, n_embd);

        //     layer.c_attn_proj_w = ggml_new_tensor_2d(ctx, wtype,           n_embd, n_embd);
        //     layer.c_attn_proj_b = ggml_new_tensor_1d(ctx, GGML_TYPE_F32,   n_embd);

        //     layer.c_mlp_fc_w    = ggml_new_tensor_2d(ctx, wtype,           n_embd, 4*n_embd);
        //     layer.c_mlp_fc_b    = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 4*n_embd);

        //     // need refine
        //     layer.gpu_idx       = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, n_embd * 4);
        //     layer.gpu_bucket       = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, 2048*5);
        //     layer.c_mlp_fc_w_gpu = ggml_new_tensor_2d(ctx, wtype,         n_embd, 2048*5);

        //     layer.c_mlp_proj_w_t = ggml_new_tensor_2d(ctx, wtype,         n_embd, 4* n_embd);
        //     layer.c_mlp_proj_w  = ggml_new_tensor_2d(ctx, wtype,         4*n_embd, n_embd);
        //     layer.c_mlp_proj_b  = ggml_new_tensor_1d(ctx, GGML_TYPE_F32,   n_embd);

        //     layer.c_mlp_proj_w_gpu = ggml_new_tensor_2d(ctx, wtype,2048*5, n_embd);

        //     if (i <= 10) {
        //         layer.mlp_pre_w1_w = ggml_new_tensor_2d(ctx, wtype, n_embd, 192);
        //         layer.mlp_pre_w2_w = ggml_new_tensor_2d(ctx, wtype, 192, 4*n_embd);
        //     } else if (i <= 12) {
        //         layer.mlp_pre_w1_w = ggml_new_tensor_2d(ctx, wtype, n_embd, 288);
        //         layer.mlp_pre_w2_w = ggml_new_tensor_2d(ctx, wtype, 288, 4*n_embd);
        //     } else if (i <= 18) {
        //         layer.mlp_pre_w1_w = ggml_new_tensor_2d(ctx, wtype, n_embd, 512);
        //         layer.mlp_pre_w2_w = ggml_new_tensor_2d(ctx, wtype, 512, 4*n_embd);

        //     } else if (i <= 21) {
        //         layer.mlp_pre_w1_w = ggml_new_tensor_2d(ctx, wtype, n_embd, 768);
        //         layer.mlp_pre_w2_w = ggml_new_tensor_2d(ctx, wtype, 768, 4*n_embd);
        //     } else if (i <= 26) {
        //         layer.mlp_pre_w1_w = ggml_new_tensor_2d(ctx, wtype, n_embd, 1024);
        //         layer.mlp_pre_w2_w = ggml_new_tensor_2d(ctx, wtype, 1024, 4*n_embd);
        //     } else if (i <= 31) {
        //         layer.mlp_pre_w1_w = ggml_new_tensor_2d(ctx, wtype, n_embd, 1280);
        //         layer.mlp_pre_w2_w = ggml_new_tensor_2d(ctx, wtype, 1280, 4*n_embd);
        //     }

        //     layer.ln_1_g->backend = OPT_BACKEND_OFFLOAD;
        //     layer.ln_1_b->backend = OPT_BACKEND_OFFLOAD;
        //     layer.ln_2_g->backend = OPT_BACKEND_OFFLOAD;
        //     layer.ln_2_b->backend = OPT_BACKEND_OFFLOAD;
        //     layer.c_attn_attn_q_w->backend = OPT_BACKEND_OFFLOAD;
        //     layer.c_attn_attn_q_b->backend = OPT_BACKEND_OFFLOAD;
        //     layer.c_attn_attn_k_w->backend = OPT_BACKEND_OFFLOAD;
        //     layer.c_attn_attn_k_b->backend = OPT_BACKEND_OFFLOAD;
        //     layer.c_attn_attn_v_w->backend = OPT_BACKEND_OFFLOAD;
        //     layer.c_attn_attn_v_b->backend = OPT_BACKEND_OFFLOAD;
        //     layer.c_attn_proj_w->backend = OPT_BACKEND_OFFLOAD;
        //     layer.c_attn_proj_b->backend = OPT_BACKEND_OFFLOAD;
        //     layer.c_mlp_fc_b->backend = OPT_BACKEND_OFFLOAD;
        //     // layer.c_mlp_fc_w->backend = OPT_BACKEND_OFFLOAD;
        //     // layer.c_mlp_proj_w->backend = OPT_BACKEND_OFFLOAD;
        //     layer.c_mlp_proj_b->backend = OPT_BACKEND_OFFLOAD;

        //     layer.mlp_pre_w1_w->backend = OPT_BACKEND_OFFLOAD;
        //     layer.mlp_pre_w2_w->backend = OPT_BACKEND_OFFLOAD;
        //     layer.c_mlp_fc_w_gpu->backend = OPT_BACKEND_OFFLOAD;
        //     layer.c_mlp_proj_w_gpu->backend = OPT_BACKEND_OFFLOAD;
        //     layer.gpu_bucket->backend = OPT_BACKEND_OFFLOAD;
        //     // layer.c_mlp_proj_w_t->backend = OPT_BACKEND_OFFLOAD;

            // map by name
            model.tensors["layers." + std::to_string(i) + ".attention_norm.weight"]        = &layer.ln_1_g;
            model.tensors["layers." + std::to_string(i) + ".attention_norm.bias"]        = &layer.ln_1_b;

            model.tensors["layers." + std::to_string(i) + ".output_norm.weight"]        = &layer.ln_2_g;
            model.tensors["layers." + std::to_string(i) + ".output_norm.bias"]        = &layer.ln_2_b;

            model.tensors["layers." + std::to_string(i) + ".attention.wq.weight"] = &layer.c_attn_attn_q_w;
            model.tensors["layers." + std::to_string(i) + ".attention.wq.bias"] = &layer.c_attn_attn_q_b;

            model.tensors["layers." + std::to_string(i) + ".attention.wk.weight"] = &layer.c_attn_attn_k_w;
            model.tensors["layers." + std::to_string(i) + ".attention.wk.bias"] = &layer.c_attn_attn_k_b;

            model.tensors["layers." + std::to_string(i) + ".attention.wv.weight"] = &layer.c_attn_attn_v_w;
            model.tensors["layers." + std::to_string(i) + ".attention.wv.bias"] = &layer.c_attn_attn_v_b;

            model.tensors["layers." + std::to_string(i) + ".attention.wo.weight"] = &layer.c_attn_proj_w;
            model.tensors["layers." + std::to_string(i) + ".attention.wo.bias"] = &layer.c_attn_proj_b;

            model.tensors["layers." + std::to_string(i) + ".feed_forward.w1.weight"]    = &layer.c_mlp_fc_w;
            model.tensors["layers." + std::to_string(i) + ".feed_forward.w1.bias"]    = &layer.c_mlp_fc_b;

            model.tensors["layers." + std::to_string(i) + ".feed_forward.w2.weight"]  = &layer.c_mlp_proj_w;
            model.tensors["layers." + std::to_string(i) + ".feed_forward.w2.weight_transpose"]  = &layer.c_mlp_proj_w_t;
            model.tensors["layers." + std::to_string(i) + ".feed_forward.w2.bias"]  = &layer.c_mlp_proj_b;

            model.tensors["layers." + std::to_string(i) + ".gpu.weight"]    = &layer.gpu_idx;
            model.tensors["layers." + std::to_string(i) + ".gpu.bucket"]    = &layer.gpu_bucket;
            model.tensors["layers." + std::to_string(i) + ".feed_forward.w1.weight_h20"]    = &layer.c_mlp_fc_w_gpu;

            model.tensors["layers." + std::to_string(i) + ".feed_forward.w2.weight_h20"]    = &layer.c_mlp_proj_w_gpu;
            
            model.tensors["layers." + std::to_string(i) + ".fc1.weight"] = &layer.mlp_pre_w1_w;
            model.tensors["layers." + std::to_string(i) + ".fc2.weight"] = &layer.mlp_pre_w2_w;
        }
    }


    // key + value memory
    {
        const auto & hparams = model.hparams;

        const int n_embd  = hparams.n_embd;
        const int n_layer = hparams.n_layer;
        const int n_ctx   = hparams.n_ctx;

        const int n_mem      = n_layer*n_ctx;
        const int n_elements = n_embd*n_mem;

        model.memory_k = ggml_new_tensor_1d(ctx, GGML_TYPE_F16, n_elements);
        model.memory_v = ggml_new_tensor_1d(ctx, GGML_TYPE_F16, n_elements);
        #ifdef GGML_USE_CUBLAS
            // ggml_cuda_assign_buffers_no_scratch(model.memory_k); 
            // ggml_cuda_assign_buffers_no_scratch(model.memory_v); 
        #endif

        const size_t memory_size = ggml_nbytes(model.memory_k) + ggml_nbytes(model.memory_v);

        printf("%s: memory size = %8.2f MB, n_mem = %d\n", __func__, memory_size/1024.0/1024.0, n_mem);
    }
    ggml_set_no_alloc(ctx, true);
    // load weights
    {
        size_t total_size = 0;

        bool has_lm_head = false;
        const std::vector<std::string> to_gpu = {
                "output_norm.bias",
                "output_norm.weight",
                ".*attention.wq.weight",
                ".*attention.wq.bias",
                ".*attention.wk.weight",
                ".*attention.wk.bias",
                ".*attention.wv.weight",
                ".*attention.wv.bias",
                ".*attention.wo.weight",
                ".*attention.wo.weight_transpose",
                ".*attention.wo.bias",
                ".*feed_forward.w1.weight_h20",
                ".*feed_forward.w1.bias",
                ".*feed_forward.w2.weight_h20$",
                // ".*feed_forward.w2.weight_transpose",
                /* ".*feed_forward.w2.weight$", */
                // ".*feed_forward.w2.bias",
                ".*gpu.bucket",
                ".*attention_norm.weight",
                ".*attention_norm.bias",
                "layers.*output_norm.weight",
                "layers.*output_norm.bias",
                ".*fc1.weight",
                ".*fc2.weight",
                // ".*attention.*fc1.weight",
                // ".*attention.*fc1.bias",
                // ".*attention.*fc2.weight",
                // ".*attention.*fc2.bias",

                // "output.weight",
                
                // "model/h.*/attn/c_proj/w",
                // "model/h.*/mlp/c_fc/w",
                // "model/h.*/mlp/c_proj/w",
            };
            const std::vector<std::string> to_gpu_lv = {
                // ".*attention.wq.weight",
                // ".*attention.wq.bias",
                ".*attention.wk.weight",
                ".*attention.wk.bias",
                ".*attention.wv.weight",
                ".*attention.wv.bias",
                ".*attention.wo.weight",
                // ".*attention.wo.weight_transpose",
                ".*attention.wo.bias",
                ".*feed_forward.w1.weight_h20",
                ".*feed_forward.w1.bias",
                ".*feed_forward.w2.weight_h20$",
                // ".*feed_forward.w2.weight_transpose",
                /* ".*feed_forward.w2.weight$", */
                ".*feed_forward.w2.bias",
                ".*gpu.bucket",
                ".*attention_norm.weight",
                ".*attention_norm.bias",
                // "layers.*output_norm.weight",
                // "layers.*output_norm.bias",
                ".*fc1.weight",
                ".*fc2.weight",
                // ".*attention.*fc1.weight",
                // ".*attention.*fc1.bias",
                // ".*attention.*fc2.weight",
                // ".*attention.*fc2.bias",

                // "output.weight",
                
                // "model/h.*/attn/c_proj/w",
                // "model/h.*/mlp/c_fc/w",
                // "model/h.*/mlp/c_proj/w",
            };
            const std::vector<std::string> to_lock = {
                "tok_embeddings.weight",
                "pos_embeddings.weight",
                // "output_norm.bias",
                ".*attention.wq.weight",
                ".*attention.wq.bias",
                // ".*attention.wo.weight",
                // ".*attention.wo.weight_transpose",
                // ".*attention.wo.bias",
                ".*feed_forward.w1.weight",
                ".*feed_forward.w1.bias",
                ".*feed_forward.w2.weight_transpose",
                // ".*feed_forward.w2.weight",
                ".*feed_forward.w2.bias",
                ".*gpu.weight",
                ".*attention_norm.weight",
                ".*attention_norm.bias",
                ".*output_norm.weight",
                ".*output_norm.bias",
                ".*attention.*fc1.weight",
                ".*attention.*fc1.bias",
                ".*attention.*fc2.weight",
                ".*attention.*fc2.bias",
                // ".*w2.bias",
                // ".*w1.bias",
                "output.weight",
            };

        while (true) {
            int32_t n_dims;
            int32_t length;
            int32_t ttype;

            fin.read(reinterpret_cast<char *>(&n_dims), sizeof(n_dims));
            fin.read(reinterpret_cast<char *>(&length), sizeof(length));
            fin.read(reinterpret_cast<char *>(&ttype),  sizeof(ttype));

            if (fin.eof()) {
                break;
            }

            int32_t nelements = 1;
            int32_t ne[2] = { 1, 1 };
            int64_t new_ne[2];
            for (int i = 0; i < n_dims; ++i) {
                fin.read(reinterpret_cast<char *>(&ne[i]), sizeof(ne[i]));
                nelements *= ne[i];
                new_ne[i] = ne[i];
            }

            std::string name(length, 0);
            fin.read(&name[0], length);

            if (model.tensors.find(name) == model.tensors.end()) {
                fprintf(stderr, "%s: unknown tensor '%s' in model file\n", __func__, name.c_str());
                return false;
            }
            ggml_tensor ** ptr = model.tensors[name];
            // printf("name %s ptr %p\n", name.c_str(), *ptr);
            // int k;
            // scanf("%d", &k);
            *ptr = ggml_new_tensor(ctx, ggml_type(ttype), n_dims, (const int64_t *)&new_ne);

            auto tensor = (ggml_tensor *)*model.tensors[name];
            if (ggml_nelements(tensor) != nelements) {
                fprintf(stderr, "%s: tensor '%s' has wrong size in model file elements %d\n", __func__, name.c_str(), nelements);
                return false;
            }

            if (tensor->ne[0] != ne[0] || tensor->ne[1] != ne[1]) {
                fprintf(stderr, "%s: tensor '%s' has wrong shape in model file: got [%d, %d], expected [%d, %d]\n",
                        __func__, name.c_str(), (int) tensor->ne[0], (int) tensor->ne[1], ne[0], ne[1]);
                return false;
            }
            

            // for debugging
            if (1) {
                printf("%24s - [%5d, %5d], type = %6s, %6.2f MB, %9zu bytes\n", name.c_str(), ne[0], ne[1], ggml_type_name(ggml_type(ttype)), ggml_nbytes(tensor)/1024.0/1024.0, ggml_nbytes(tensor));
            }

            const size_t bpe = ggml_type_size(ggml_type(ttype));

            if ((nelements*bpe)/ggml_blck_size(tensor->type) != ggml_nbytes(tensor)) {
                fprintf(stderr, "%s: tensor '%s' has wrong size in model file: got %zu, expected %zu\n",
                        __func__, name.c_str(), ggml_nbytes(tensor), nelements*bpe);
                return false;
            }

            std::streampos offset = fin.tellg();
            // fin.read(reinterpret_cast<char *>(tensor->data), ggml_nbytes(tensor));
            fin.seekg(ggml_nbytes(tensor), std::ios::cur);
            tensor->data = model.mapping->addr + static_cast<std::streamoff>(offset);
            // if ( endsWith(name.c_str(), "weight_transpose")) {
            //     short *d = (short *)tensor->data;
            //     for (int i = 0; i < 10; i++) {
            //         printf("%d ", d[i+4096]);
            //     }
            // }
            // printf("\n");
            // if (endsWith(name.c_str(), "weight_h20")) {
            //     short *d = (short *)tensor->data;
            //     for (int i = 0; i < 10; i++) {
            //         printf("%d ", d[i]);

            //     }
            //     int k;
            //     scanf("%d", &k);
            // }

            // // GPT-2 models share the WTE tensor as the LM head
            // if (name == "model/wte" && has_lm_head == false) {
            //     memcpy(model.lm_head->data, tensor->data, ggml_nbytes(tensor));
            // }

            // if (name == "model/lm_head") {
            //     has_lm_head = true;
            // }
            if (model_params.low_vram == false) {
                for (const auto &s : to_gpu)
                {
                    // if (std::regex_search(name, std::regex(".*fc1.weight")) || std::regex_search(name, std::regex(".*fc2.weight")))
                    // {
                    //     std::regex pattern(R"(\d+)");
                    //     std::smatch match;
                    //     int layer_id = 0;
                    //     if (std::regex_search(name, match, pattern))
                    //     {
                    //         std::string digitStr = match.str();
                    //         int num = std::stoi(digitStr);
                    //         layer_id = num;
                    //     }
                    //     printf("layerid %d, ngpu_layers %d\n", layer_id, model_params.n_gpu_layers);
                    //     if (layer_id > model_params.n_gpu_layers)
                    //         break;
                    // }
                    if (std::regex_search(name, std::regex(s)))
                    {
                        tensor->backend = GGML_BACKEND_GPU;
                        break;
                    }
                }
            } else {
                for (const auto &s : to_gpu_lv)
                {
                    if (std::regex_search(name, std::regex(s)))
                    {
                        std::regex pattern(R"(\d+)");
                        std::smatch match;
                        int layer_id = 0;
                        if (std::regex_search(name, match, pattern))
                        {
                            std::string digitStr = match.str();
                            int num = std::stoi(digitStr);
                            layer_id = num;
                        }
                        // printf("layerid %d, ngpu_layers %d\n", layer_id, model_params.n_gpu_layers);
                        if (layer_id > model_params.n_gpu_layers)
                            break;
                        // printf("name %s\n", name.c_str());
                        tensor->backend = GGML_BACKEND_GPU;
                        break;
                    }
                }

            }
            if (tensor->backend == GGML_BACKEND_GPU) {
                #if defined(GGML_USE_CUBLAS)
                ggml_cuda_transform_tensor(tensor->data, tensor);
                #endif
            }
            for (const auto &s : to_lock)
            {
                if (std::regex_match(name, std::regex(s)))
                {
                    if(!mlock(tensor->data, ggml_nbytes(tensor))) {
                        // printf("mlock %s\n", name.c_str());
                    }
                    else {
                        printf("mlock failed %s\n", name.c_str());
                    }
                }
            }

            total_size += ggml_nbytes(tensor);
        }
        ggml_set_no_alloc(ctx, false);

        printf("%s: model size  = %8.2f MB\n", __func__, total_size/1024.0/1024.0);
    }
    printf("load finish\n");
    // int k;
    // scanf("%d", &k);

    fin.close();

    return true;
}

// build the computation graph
struct ggml_cgraph * gpt2_graph(
        const gpt2_model & model,
        struct ggml_allocr * allocr,
        const int n_past,
        const std::vector<gpt_vocab::id> & embd_inp) {
    const int N = embd_inp.size();

    const auto & hparams = model.hparams;

    const int n_embd  = hparams.n_embd;
    const int n_layer = hparams.n_layer;
    const int n_ctx   = hparams.n_ctx;
    const int n_head  = hparams.n_head;

    // since we are using ggml-alloc, this buffer only needs enough space to hold the ggml_tensor and ggml_cgraph structs, but not the tensor data
    static size_t buf_size = ggml_tensor_overhead()*GGML_MAX_NODES + ggml_graph_overhead();
    // static std::vector<uint8_t> buf(buf_size);
    static void * buf = ggml_cuda_host_malloc(buf_size);

    struct ggml_init_params params = {
        /*.mem_size   =*/ buf_size,
        /*.mem_buffer =*/ buf,
        /*.no_alloc   =*/ true, // the tensors will be allocated later by ggml_allocr_alloc_graph()
    };

    ctx0 = ggml_init(params);

    struct ggml_cgraph  * gf = ggml_new_graph(ctx0);

    struct ggml_tensor * embd = ggml_new_tensor_1d(ctx0, GGML_TYPE_I32, N);
    ggml_allocr_alloc(allocr, embd);

    // avoid writing to tensors if we are only measuring the memory usage
    if (!ggml_allocr_is_measure(allocr)) {
        memcpy(embd->data, embd_inp.data(), N*ggml_element_size(embd));
    }

    struct ggml_tensor * position = ggml_new_tensor_1d(ctx0, GGML_TYPE_I32, N);
    ggml_allocr_alloc(allocr, position);
    if (!ggml_allocr_is_measure(allocr)) {
        for (int i = 0; i < N; ++i) {
            ((int32_t *) position->data)[i] = n_past + i + 2;
        }
    }
    offload_func_t offload_func = opt_nop;
    offload_func_t offload_func_kq = opt_nop;
    offload_func_t offload_func_v = opt_nop;
    offload_func_t offload_func_nr = opt_nop;
    offload_func_t offload_debug = opt_nop;
#ifdef GGML_USE_CUBLAS
    offload_debug = ggml_cuda_assign_buffers_no_alloc;
    // offload_func = ggml_cuda_assign_buffers_no_alloc; 
    // offload_func_kq = ggml_cuda_assign_buffers_no_alloc; 
    // offload_func_v = ggml_cuda_assign_buffers_no_alloc; 
    // offload_func_nr = ggml_cuda_assign_buffers_no_alloc; 
#endif
    // offload_func_t offload_debug = ggml_cuda_assign_buffers_no_alloc;
    // int k; 
    // scanf("%d", &k); 

    struct ggml_tensor * KQ_scale = ggml_new_tensor_1d(ctx0, GGML_TYPE_F32, 1);
    ggml_allocr_alloc(allocr, KQ_scale);
    if (!ggml_allocr_is_measure(allocr)) {
        ggml_set_f32(KQ_scale, 1.0f/sqrtf(float(n_embd)/n_head));
    }

    // wte + wpe
    struct ggml_tensor * inpL =
        ggml_add(ctx0,
                ggml_get_rows(ctx0, model.wte, embd),
                ggml_get_rows(ctx0, model.wpe, position));
    ggml_set_name(inpL, "inpL_first");
    // offload_func(inpL);


    for (int il = 0; il < n_layer; ++il) {
        struct ggml_tensor * cur;

        // norm
        {
            // [ 768, N]
            cur = ggml_norm(ctx0, inpL, hparams.eps);
            offload_func(cur);

            // cur = ln_1_g*cur + ln_1_b
            // [ 768, N]
            cur = ggml_mul(ctx0,
                        cur,
                        model.layers[il].ln_1_g);
            offload_func(cur);
            ggml_set_name(cur, "ln_1_g");
            cur = ggml_add(ctx0,
                    cur,
                    model.layers[il].ln_1_b);
            ggml_set_name(cur, "ln_1_b");
            // offload_func(cur);
            
        }

        // attn
        // [2304, 768] - model.layers[il].c_attn_attn_w
        // [2304,   1] - model.layers[il].c_attn_attn_b
        // [ 768,   N] - cur (in)
        // [2304,   N] - cur (out)
        //
        // cur = attn_w*cur + attn_b
        // [2304, N]

        struct ggml_tensor *k_cpy = nullptr;
        struct ggml_tensor *v_cpy = nullptr;
        // self-attention
        {
            // struct ggml_tensor * Qcur = ggml_view_2d(ctx0, cur, n_embd, N, cur->nb[1], 0*sizeof(float)*n_embd);
            // struct ggml_tensor * Kcur = ggml_view_2d(ctx0, cur, n_embd, N, cur->nb[1], 1*sizeof(float)*n_embd);
            // struct ggml_tensor * Vcur = ggml_view_2d(ctx0, cur, n_embd, N, cur->nb[1], 2*sizeof(float)*n_embd);
            struct ggml_tensor * Qcur = ggml_mul_mat(ctx0, model.layers[il].c_attn_attn_q_w,cur);
            offload_func_kq(Qcur);
            Qcur = ggml_add(ctx0, Qcur, model.layers[il].c_attn_attn_q_b);
            offload_func_kq(Qcur);
            struct ggml_tensor * Kcur = ggml_mul_mat(ctx0, model.layers[il].c_attn_attn_k_w,cur);
            offload_func_kq(Kcur);
            Kcur = ggml_add(ctx0, Kcur, model.layers[il].c_attn_attn_k_b);
            offload_func_kq(Kcur);
            struct ggml_tensor * Vcur = ggml_mul_mat(ctx0, model.layers[il].c_attn_attn_v_w,cur);
            offload_func_v(Vcur);
            Vcur = ggml_add(ctx0, Vcur, model.layers[il].c_attn_attn_v_b);
            offload_func_v(Vcur);

            Vcur = ggml_transpose(ctx0, ggml_reshape_2d(ctx0, Vcur, n_embd, N));
            offload_func_v(Vcur);


            // store key and value to memory
            if (N >= 1) {
                struct ggml_tensor * k = ggml_view_1d(ctx0, model.memory_k, N*n_embd, (ggml_element_size(model.memory_k)*n_embd)*(il*n_ctx + n_past));
                offload_func_kq(k);
                // struct ggml_tensor * v = ggml_view_1d(ctx0, model.memory_v, N*n_embd, (ggml_element_size(model.memory_v)*n_embd)*(il*n_ctx + n_past));

                struct ggml_tensor * v = ggml_view_2d(ctx0, model.memory_v, N, n_embd,
                        (   n_ctx)*ggml_element_size(model.memory_v),
                        (il*n_ctx)*ggml_element_size(model.memory_v)*n_embd+ n_past*ggml_element_size(model.memory_v));

                offload_func_v(v);
                k_cpy = ggml_cpy(ctx0, Kcur, k);
                offload_func_kq(k_cpy);
                ggml_set_name(k_cpy, "k_cpy");
                v_cpy = ggml_cpy(ctx0, Vcur, v);
                offload_func_v(v_cpy);
                ggml_set_name(v_cpy, "v_cpy");
                // ggml_build_forward_expand(gf, ggml_cpy(ctx0, Kcur, k));
                // ggml_build_forward_expand(gf, ggml_cpy(ctx0, Vcur, v));
            }

            // Q = Qcur.contiguous().view(n_embd/n_head, n_head, N).permute(0, 2, 1, 3)
            // [64, N, 12]
            Qcur = ggml_reshape_3d(ctx0, Qcur, n_embd/n_head, n_head, N);
            offload_func_kq(Qcur);
             struct ggml_tensor * Q =
                ggml_permute(ctx0,
                        Qcur,
                        0, 2, 1, 3);
            ggml_set_name(Q, "Q");
            offload_func_kq(Q);


            // K = Kmem.view(n_embd/n_head, n_head, n_past + N).permute(0, 2, 1, 3)
            // [64, n_past + N, 12]
            // struct ggml_tensor * K =
            //     ggml_permute(ctx0,
            //             ggml_reshape_3d(ctx0,
            //                 ggml_view_1d(ctx0, model.memory_k, (n_past + N)*n_embd, il*n_ctx*ggml_element_size(model.memory_k)*n_embd),
            //                 n_embd/n_head, n_head, n_past + N),
            //             0, 2, 1, 3);
            
            struct ggml_tensor * K =
                ggml_view_3d(ctx0, model.memory_k,
                        128, n_past + N, n_head,
                        ggml_element_size(model.memory_k)*n_embd,
                        ggml_element_size(model.memory_k)*128,
                        ggml_element_size(model.memory_k)*n_embd*n_ctx*il);
            K->src[1] = k_cpy;
            offload_func_kq(K);

            // GG: flash attention
            //struct ggml_tensor * V =
            //    ggml_cpy(ctx0,
            //            ggml_permute(ctx0,
            //                ggml_reshape_3d(ctx0,
            //                    ggml_view_1d(ctx0, model.memory_v, (n_past + N)*n_embd, il*n_ctx*ggml_element_size(model.memory_v)*n_embd),
            //                    n_embd/n_head, n_head, n_past + N),
            //                1, 2, 0, 3),
            //            ggml_new_tensor_3d(ctx0, GGML_TYPE_F32, n_past + N, n_embd/n_head, n_head));

            //struct ggml_tensor * KQV = ggml_flash_attn(ctx0, Q, K, V, true);

            // K * Q
            // [n_past + N, N, 12]
            struct ggml_tensor * KQ = ggml_mul_mat(ctx0, K, Q);
            offload_func_kq(KQ);

            // KQ_scaled = KQ / sqrt(n_embd/n_head)
            // [n_past + N, N, 12]
            struct ggml_tensor * KQ_scaled =
                ggml_scale(ctx0,
                        KQ,
                        KQ_scale);
            offload_func_kq(KQ_scaled);

            // KQ_masked = mask_past(KQ_scaled)
            // [n_past + N, N, 12]
            struct ggml_tensor * KQ_masked = ggml_diag_mask_inf(ctx0, KQ_scaled, n_past);
            offload_func_kq(KQ_masked);

            // KQ = soft_max(KQ_masked)
            // [n_past + N, N, 12]
            struct ggml_tensor * KQ_soft_max = ggml_soft_max(ctx0, KQ_masked);
            offload_func_v(KQ_soft_max);

            // V_trans = Vmem.view(n_embd/n_head, n_head, n_past + N).permute(1, 2, 0, 3).contiguous()
            // [n_past + N, 64, 12]

            struct ggml_tensor * V =
                ggml_view_3d(ctx0, model.memory_v,
                        n_past + N, 128, n_head,
                        n_ctx*ggml_element_size(model.memory_v),
                        n_ctx*ggml_element_size(model.memory_v)*128,
                        n_ctx*ggml_element_size(model.memory_k)*n_embd*il);
            V->src[1] = v_cpy;
            offload_func_v(V);

            // KQV = transpose(V) * KQ_soft_max
            // [64, N, 12]
            struct ggml_tensor * KQV = ggml_mul_mat(ctx0, V, KQ_soft_max);
            offload_func_v(KQV);

            // KQV_merged = KQV.permute(0, 2, 1, 3)
            // [64, 12, N]
            struct ggml_tensor * KQV_merged = ggml_permute(ctx0, KQV, 0, 2, 1, 3);
            offload_func_v(KQV_merged);

            // cur = KQV_merged.contiguous().view(n_embd, N)
            // [768, N]
            cur = ggml_cpy(ctx0,
                    KQV_merged,
                    ggml_new_tensor_2d(ctx0, GGML_TYPE_F32, n_embd, N));
            ggml_set_name(cur, "KQV_merge_cont");
            offload_func_v(cur);
        }

        // projection
        // [ 768, 768] - model.layers[il].c_attn_proj_w
        // [ 768,   1] - model.layers[il].c_attn_proj_b
        // [ 768,   N] - cur (in)
        // [ 768,   N] - cur (out)
        //
        // cur = proj_w*cur + proj_b
        // [768, N]
        {
            cur = ggml_mul_mat(ctx0,
                    model.layers[il].c_attn_proj_w,
                    cur);
            ggml_set_name(cur, "attn_proj");
            offload_func(cur);

            cur = ggml_add(ctx0,
                    cur,
                    model.layers[il].c_attn_proj_b);
            ggml_set_name(cur, "attn_bias");
            offload_func(cur);
        }

        // add the input
        cur = ggml_add(ctx0, cur, inpL);
        offload_func(cur);
        ggml_set_name(cur, "after attn");

        struct ggml_tensor * inpFF = cur;

        // feed-forward network
        {
            ggml_tensor *idx = nullptr;
            ggml_tensor *idx_g = nullptr;
            ggml_tensor *cur_c = nullptr;
            
            // norm
            {
                cur = ggml_norm(ctx0, inpFF, hparams.eps);
                offload_func(cur);
                ggml_set_name(cur, "norm_FFN");
                // cur = ln_2_g*cur + ln_2_b
                // [ 768, N]
                cur = ggml_mul(ctx0,
                            cur,
                            model.layers[il].ln_2_g);
                offload_func(cur);
                ggml_set_name(cur, "norm_FFN_g");
                cur = ggml_add(ctx0,
                        cur, 
                        model.layers[il].ln_2_b);
                // offload_func(cur);
                // ggml_set_name(cur, "norm_FFN_w");
                // cur_c = ggml_dup(ctx0, cur);
            }
            // if (N == 1)
            if (1)
            {
                idx = ggml_mul_mat(ctx0,
                                   model.layers[il].mlp_pre_w1_w,
                                   inpFF);
                offload_func(idx);
                ggml_set_name(idx, "mlp_pre_w1");
                idx = ggml_relu(ctx0, idx);
                offload_func(idx);
                ggml_set_name(idx, "relu_pre");
                idx = ggml_mul_mat(ctx0,
                                   model.layers[il].mlp_pre_w2_w,
                                   idx);
                ggml_set_name(idx, "mlp_pre_w2");
                // offload_func(idx);
                // idx = ggml_sigmoid(ctx0, idx);
                // offload_func(idx);
                // idx_g = idx;
                // idx = ggml_dup(ctx0, idx_g);
                // ggml_set_name(idx, "idx_cpu_dup");
            }

            // fully connected
            // [3072, 768] - model.layers[il].c_mlp_fc_w
            // [3072,   1] - model.layers[il].c_mlp_fc_b
            // [ 768,   N] - cur (in)
            // [3072,   N] - cur (out)
            //
            // cur = fc_w*cur + fc_b
            // [3072, N]
            if (N >= 80)
            // if (0)
            {
                cur = ggml_mul_mat(ctx0,
                                   model.layers[il].c_mlp_fc_w,
                                   cur);
                offload_debug(cur);
                offload_func(cur);
                ggml_set_name(cur, "up_ffn");
                cur = ggml_add(ctx0,
                    cur,
                    model.layers[il].c_mlp_fc_b);
                offload_debug(cur);
                offload_func(cur);
            }
            else 
            {
                // cur = ggml_mul_mat(ctx0,
                //                    model.layers[il].c_mlp_fc_w,
                //                    cur);
                // offload_func(cur);
                // cur = ggml_add(ctx0,
                //     cur,
                //     model.layers[il].c_mlp_fc_b);
                // offload_func(cur);

                
                struct ggml_tensor *tmp = ggml_mul_mat_special(ctx0,
                model.layers[il].c_mlp_fc_w_gpu,
                cur,
                idx,
                model.layers[il].gpu_bucket);
                ggml_set_name(tmp, "mlp_up_gpu");
                offload_func(tmp);
                offload_debug(tmp);
                cur = ggml_mul_mat_idx(ctx0,
                                       model.layers[il].c_mlp_fc_w,
                                       cur,
                                       idx,
                                       model.layers[il].gpu_idx);
                ggml_set_name(cur, "mlp_up_cpu");
                cur = ggml_add_idx(ctx0,
                    cur,
                    model.layers[il].c_mlp_fc_b,
                    idx);
                ggml_set_name(tmp, "mlp_up_bias");
                offload_debug(tmp);
                offload_func(tmp);

            cur = ggml_add(ctx0, cur, tmp);
            ggml_set_name(cur, "mlp_up_mix");
            offload_func(cur);

                // cur = tmp;

            }

            

            // GELU activation
            // [3072, N]
            cur = ggml_relu(ctx0, cur);
            // cur_c = cur;
            // offload_func(cur);
            cur_c = cur->backend==GGML_BACKEND_CPU? cur : ggml_dup(ctx0, cur);

            // projection
            // [ 768, 3072] - model.layers[il].c_mlp_proj_w
            // [ 768,    1] - model.layers[il].c_mlp_proj_b
            // [3072,    N] - cur (in)
            // [ 768,    N] - cur (out)
            //
            // cur = proj_w*cur + proj_b
            // [768, N]
            if (N >= 80) {
            // if (0) { 
                // cur = ggml_mul_mat(ctx0,
                //                    model.layers[il].c_mlp_proj_w,
                //                    cur);
                cur = ggml_axpy(ctx0,
                                   model.layers[il].c_mlp_proj_w_t,
                                   cur,
                                   NULL,
                                   NULL);
                offload_debug(cur);
                offload_func(cur);
                ggml_set_name(cur, "down_ffn");

                cur = ggml_add(ctx0,
                               cur,
                               model.layers[il].c_mlp_proj_b);
                offload_func(cur);
                offload_debug(cur);
            }
            else {
                // cur = ggml_mul_mat(ctx0,
                //                    model.layers[il].c_mlp_proj_w,
                //                    cur);
                // offload_func(cur);
                
                // cur = ggml_axpy(ctx0, 
                // model.layers[il].c_mlp_proj_w_t,
                // cur,
                // NULL,
                // NULL);
                // offload_func(cur);


                // struct ggml_tensor *tmp = ggml_mul_mat_idx(ctx0, 
                // model.layers[il].c_mlp_proj_w_gpu,
                // cur,
                // model.layers[il].gpu_bucket,
                // NULL);
                struct ggml_tensor *tmp = ggml_axpy(ctx0, 
                    model.layers[il].c_mlp_proj_w_gpu,
                    cur,
                    idx,
                    model.layers[il].gpu_bucket);
                ggml_set_name(tmp, "axpy");
                offload_func(tmp);
                offload_debug(tmp);
                cur = ggml_axpy(ctx0, 
                model.layers[il].c_mlp_proj_w_t,
                cur_c,
                idx,
                model.layers[il].gpu_idx);

                cur = ggml_add(ctx0, cur, tmp);
                offload_func(cur);

                cur = ggml_add(ctx0, cur, model.layers[il].c_mlp_proj_b);
                offload_func(cur);
                
                // tmp = ggml_add(ctx0,
                //                tmp,
                //                model.layers[il].c_mlp_proj_b);
                // offload_func(tmp);
                // offload_debug(tmp);

                // cur = tmp;
            }
            
        }

        // input for next layer
        inpL = ggml_add(ctx0, cur, inpFF);
        offload_func(inpL);
    }

    // norm
    {
        // [ 768, N]
        inpL = ggml_norm(ctx0, inpL, hparams.eps);
        offload_func_nr(inpL);

        // inpL = ln_f_g*inpL + ln_f_b
        // [ 768, N]
        inpL = ggml_mul(ctx0,
                    inpL,
                    model.ln_f_g);
        offload_func_nr(inpL);
        inpL = ggml_add(ctx0,
                inpL,
                model.ln_f_b);
        ggml_set_name(inpL, "before");
        offload_func_nr(inpL);
    }

    // inpL = WTE * inpL
    // [ 768, 50257] - model.lm_head
    // [ 768, N]     - inpL
    inpL = ggml_mul_mat(ctx0, model.lm_head, inpL);
    ggml_set_name(inpL, "last_layer");
// offload_func(inpL);

    // logits -> probs
    //inpL = ggml_soft_max(ctx0, inpL);

    ggml_build_forward_expand(gf, inpL);

    ggml_free(ctx0);

    return gf;
}

// evaluate the transformer
//
//   - model:     the model
//   - allocr:    ggml_allocr to use to allocate the compute buffer
//   - n_threads: number of threads to use
//   - n_past:    the context size so far
//   - embd_inp:  the embeddings of the tokens in the context
//   - embd_w:    the predicted logits for the next token
//
bool gpt2_eval(
        const gpt2_model & model,
        struct ggml_allocr * allocr,
        const int n_threads,
        const int n_past,
        const std::vector<gpt_vocab::id> & embd_inp,
              std::vector<float>         & embd_w) {
    const int N = embd_inp.size();

    const auto & hparams = model.hparams;

    const int n_vocab = hparams.n_vocab;

    // reset the allocator to free all the memory allocated during the previous inference
    ggml_allocr_reset(allocr);
    struct ggml_cgraph * gf = gpt2_graph(model, allocr, n_past, embd_inp);

    // allocate tensors
    ggml_allocr_alloc_graph(allocr, gf);

#ifdef GGML_USE_CUBLAS
    for (int i = 0; i < gf->n_leafs; i++) {
        ggml_tensor * node = gf->leafs[i];
        if (node->backend == GGML_BACKEND_GPU && node->extra == NULL) {
            // ggml_cuda_assign_scratch_offset(node, (char*)node->data - (char *) compute_buffer.data());
            ggml_cuda_assign_scratch_offset(node, (char*)node->data - (char *) compute_buffer);
        }
    }

    for (int i = 0; i < gf->n_nodes; i++) {
        ggml_tensor * node = gf->nodes[i];
        if (node->backend == GGML_BACKEND_GPU && node->extra == NULL) {
            ggml_cuda_assign_scratch_offset(node, (char*)node->data - (char *) compute_buffer);
        }
    }
#endif



    // run the computation
    struct ggml_cplan plan = ggml_graph_plan(gf, n_threads);
    static std::vector<uint8_t> work_buffer;
    work_buffer.resize(plan.work_size);
    plan.work_data = work_buffer.data();
    ggml_graph_compute(gf, &plan);

    //if (n_past%100 == 0) {
    //    ggml_graph_print   (gf);
    //    ggml_graph_dump_dot(&gf, NULL, "gpt-2.dot");
    //}

    // in this case, the output tensor is the last one in the graph
    struct ggml_tensor * inpL = gf->nodes[gf->n_nodes - 1];

    //embd_w.resize(n_vocab*N);
    //memcpy(embd_w.data(), ggml_get_data(inpL), sizeof(float)*n_vocab*N);

    // return result just for the last token
    embd_w.resize(n_vocab);
    memcpy(embd_w.data(), (float *) ggml_get_data(inpL) + (n_vocab*(N-1)), sizeof(float)*n_vocab);

    return true;
}

int main(int argc, char ** argv) {
    ggml_time_init();

    const int64_t t_main_start_us = ggml_time_us();

    gpt_params params;
    params.model = "models/gpt-2-117M/ggml-model.bin";

    if (gpt_params_parse(argc, argv, params) == false) {
        return 1;
    }

    if (params.seed == LLAMA_DEFAULT_SEED) {
        params.seed = time(NULL);
    }

    printf("%s: seed = %d\n", __func__, params.seed);

    std::mt19937 rng(params.seed);
    if (params.prompt.empty()) {
        params.prompt = gpt_random_prompt(rng);
    }

    int64_t t_load_us = 0;

    gpt_vocab vocab;
    gpt2_model model;

    // load the model
    {
        const int64_t t_start_us = ggml_time_us();

        if (!gpt2_model_load(params.model, model, vocab, params)) {
            fprintf(stderr, "%s: failed to load model from '%s'\n", __func__, params.model.c_str());
            return 1;
        }

        t_load_us = ggml_time_us() - t_start_us;

        test_gpt_tokenizer(vocab, "hello world");
    }
    printf("load finish\n");

    // keep this buffer alive while evaluating the model

    struct ggml_allocr * allocr = NULL;
    // allocate the compute buffer
    {
        allocr = ggml_allocr_new_measure(GGML_MEM_ALIGN);

        // create the worst case graph for memory usage estimation
        int n_tokens = std::min(model.hparams.n_ctx, params.n_batch);
        int n_past = model.hparams.n_ctx - n_tokens;
        struct ggml_cgraph * gf = gpt2_graph(model, allocr, n_past, std::vector<gpt_vocab::id>(n_tokens, 0));

        // compute the required memory
        size_t mem_size = ggml_allocr_alloc_graph(allocr, gf) + GGML_MEM_ALIGN;

        // recreate the allocator with the required memory
        ggml_allocr_free(allocr);
        // compute_buffer.resize(mem_size);
        compute_buffer = ggml_cuda_host_malloc(mem_size);
        // allocr = ggml_allocr_new(compute_buffer.data(), mem_size, GGML_MEM_ALIGN);
        allocr = ggml_allocr_new(compute_buffer, mem_size, GGML_MEM_ALIGN);

        fprintf(stderr, "%s: compute buffer size: %.2f MB\n", __func__, mem_size/1024.0/1024.0);
    }

    int n_past = 0;

    int64_t t_sample_us  = 0;
    int64_t t_predict_us = 0;

    std::vector<float> logits;

    // tokenize the prompt
    std::vector<gpt_vocab::id> embd_inp = ::gpt_tokenize(vocab, params.prompt);

    params.n_predict = std::min(params.n_predict, model.hparams.n_ctx - (int) embd_inp.size());

    printf("%s: prompt: '%s'\n", __func__, params.prompt.c_str());
    printf("%s: number of tokens in prompt = %zu, first 8 tokens: ", __func__, embd_inp.size());
    for (int i = 0; i < std::min(8, (int) embd_inp.size()); i++) {
        printf("%d ", embd_inp[i]);
    }
    printf("\n\n");

    // submit the input prompt token-by-token
    // this reduces the memory usage during inference, at the cost of a bit of speed at the beginning
    std::vector<gpt_vocab::id> embd;

    int cnt = 0;
    for (size_t i = embd.size(); i < embd_inp.size() + params.n_predict; i++) {
        // predict
        if (embd.size() > 0) {
            const int64_t t_start_us = ggml_time_us();

            if (!gpt2_eval(model, allocr, params.n_threads, n_past, embd, logits)) {
                printf("Failed to predict\n");
                return 1;
            }
            cnt += 1;

            if (cnt > 0)
                t_predict_us += ggml_time_us() - t_start_us;
        }

        n_past += embd.size();
        embd.clear();

        if (i >= embd_inp.size()) {
            // sample next token
            llama_sampling_params & sparams = params.sparams;
            const int   top_k = sparams.top_k;
            const float top_p = sparams.top_p;
            const float temp  = sparams.temp;

            const int n_vocab = model.hparams.n_vocab;

            gpt_vocab::id id = 0;

            {
                const int64_t t_start_sample_us = ggml_time_us();

                id = gpt_sample_top_k_top_p(vocab, logits.data() + (logits.size() - n_vocab), top_k, top_p, temp, rng);

                t_sample_us += ggml_time_us() - t_start_sample_us;
            }

            // add it to the context
            embd.push_back(id);
        } else {
            // if here, it means we are still processing the input prompt
            for (size_t k = i; k < embd_inp.size(); k++) {
                embd.push_back(embd_inp[k]);
                if (int32_t(embd.size()) >= params.n_batch) {
                    break;
                }
            }
            i += embd.size() - 1;
        }

        // display text
        for (auto id : embd) {
            printf("%s", vocab.id_to_token[id].c_str());
        }
        fflush(stdout);

        // end of text token
        if (embd.back() == 50256) {
            break;
        }
    }

    // report timing
    {
        const int64_t t_main_end_us = ggml_time_us();

        printf("\n\n");
        printf("%s:     load time = %8.2f ms\n", __func__, t_load_us/1000.0f);
        printf("%s:   sample time = %8.2f ms\n", __func__, t_sample_us/1000.0f);
        printf("%s:  predict time = %8.2f ms / %.2f ms per token\n", __func__, t_predict_us/1000.0f, t_predict_us/1000.0f/(cnt));
        printf("%s:    total time = %8.2f ms\n", __func__, (t_main_end_us - t_main_start_us)/1000.0f);
    }

    ggml_free(model.ctx);

    return 0;
}
