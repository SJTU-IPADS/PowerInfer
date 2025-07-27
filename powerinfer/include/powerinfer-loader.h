#pragma once

#include <stdint.h>
#include <stddef.h>

#include "powerinfer-api.h"
#include "powerinfer-type.h"
#include "powerinfer-error.h"

extern "C" {

    /// The invalid id of model loader
    constexpr int POWERINFER_INVALID_MODEL_LOADER_ID = -1;

    /// The invalid id of kvcache
    constexpr int POWERINFER_INVALID_MODEL_CACHE_ID  = -1;

    /*!
     * @brief Create a model loader
     * @param[in] filename          The name of model file
     * @param[in] offload_to_disk   Whether to offload the model to disk
     * @note Please couple `offload_to_disk` and powerinfer-ssd operators, normal operator may incur undefined behaviours
     * @return The unique id of the model loader if success, otherwise POWERINFER_INVALID_MODEL_LOADER_ID.
     */
    POWERINFER_API int  powerinfer_loader_create_model_loader(const char *filename, bool offload_to_disk);

    /*!
     * @brief Give a hint about rotary embedding for a loader
     * @param[in] loader_id The unique id of the model loader
     * @param[in] meta              The metadata of rotary embedding
     * @param[in] num_rope_factor   The number of rope factor
     * @param[in] rope_factor_array The array of rope factor
     */
    POWERINFER_API struct PowerInferError powerinfer_loader_hint_rotary_embedding(const int loader_id, const PositionEmbeddingMetadata *meta,
            size_t num_rope_factor, const float *rope_factor_array);

    /*!
     * @brief Erase a model loader
     * @param[in] loader_id The unique id of the model loader
     * @note Using loader id of erased model loader incur undefined behaviours
     */
    POWERINFER_API struct PowerInferError powerinfer_loader_erase_model_loader(int loader_id);

    /*!
     * @brief Register tensor on the model loader
     * @param[in] loader_id         The unique id of the model loader
     * @param[in] tensor_name       The name of the tensor
     * @param[in] type              The type of tensor elements
     * @param[in] ne                The shape of the tensor
     * @param[in] nb                The size dimension of the tensor
     * @param[in] tensor_size       The total size of the tensor
     * @param[in] file_offset       The offset of the tensor in the model file
     * @param[in] data_ptr          The pointer to the buffer of tensor
     * @param[in] q4_convert        Convert q4 format as intel format: 0 -> not convert; 1 -> layout 1; 2 -> layout 2
     */
    POWERINFER_API struct PowerInferError powerinfer_loader_register_tensor(int loader_id, const char *tensor_name, enum powerinfer_type type,
        const int64_t ne[4], const size_t nb[4], size_t tensor_size, size_t file_offset, void *data_ptr, int q4_convert);

    /*!
     * @brief Load all tensors registered on the model loader
     * @param[in]  loader_id The unique id of the model loader
     * @note DO NOT call this function for twice on the same loader
     */
    POWERINFER_API struct PowerInferError powerinfer_loader_load_all_tensors(int loader_id);

    POWERINFER_API struct PowerInferError powerinfer_loader_load_expert_cache(int loader_id, void **up_exps, void **gate_exps, void **down_exps, int num_layer, int num_expert,
                                                      int num_embd, int num_ff);

    /*!
     * @brief Hint the input length
     * @param loader_id     The unique id of model loader
     * @param cache_id      The unique id of cache
     * @param input_length  The input length of this batch
     */
    POWERINFER_API struct PowerInferError powerinfer_loader_input_length_hint(int loader_id, int cache_id, int input_length);

    /*!
     * @brief Create a new kvcache
     * @param[in] loader_id: The unique id of model loader
     * @return The unique id of kvcache
     */
    POWERINFER_API int powerinfer_loader_create_kvcache(int loader_id);

    /*!
     * @brief Erase a kvcache
     * @param[in] loader_id: The unique id of model loader
     * @param[in] cache_id:  The unique id of kvcache
     */
    POWERINFER_API struct PowerInferError powerinfer_loader_erase_kvcache(int loader_id, int cache_id);

    /*!
     * @brief Init a kvcache
    * @brief Erase a kvcache
     * @param[in] loader_id: The unique id of model loader
     * @param[in] cache_id:  The unique id of kvcache
     * @param shape_array:   The shape of KVCache in each layer
     * @param num_layer:     The number of layer
     * @note Using an uninitialized kvcache may incur undefined behaviours
     * @note The length of shape_array should equal to `num_layer`
     */
    POWERINFER_API struct PowerInferError powerinfer_loader_init_kvcache(int loader_id, int cache_id, const PowerInferKVCacheShape *shape_array, const int *in_mem_attn_array, int num_in_mem_layer);

    /*!
     * @brief Clear a range of kvcache
     * @param loader_id The unique id of model loader
     * @param cache_id  The unique id of kvcache
     * @param start_pos The position after which the cache will be cleared
     * @note The start pos should be larger than 0. When start_pos equals 0, the whole kvcache will be cleared
     */
    POWERINFER_API struct PowerInferError powerinfer_loader_clear_kvcache(int loader_id, int cache_id, int start_pos);
}

struct PowerInferLoaderHandle {
private:
    int loader_id_ = POWERINFER_INVALID_MODEL_LOADER_ID;

    bool offload_  = false;

public:
    PowerInferLoaderHandle() = default;

    PowerInferLoaderHandle(const char *filename, bool offload_to_disk) {
        loader_id_ = powerinfer_loader_create_model_loader(filename, offload_to_disk);
        offload_   = offload_to_disk;
    }

    ~PowerInferLoaderHandle() noexcept {
        if (loader_id_ != POWERINFER_INVALID_MODEL_LOADER_ID) { powerinfer_loader_erase_model_loader(loader_id_); }
    }

    PowerInferLoaderHandle(const PowerInferLoaderHandle &other) = delete;

    PowerInferLoaderHandle &operator= (const PowerInferLoaderHandle &other) = delete;

    PowerInferLoaderHandle(PowerInferLoaderHandle &&other) noexcept : loader_id_(other.loader_id_), offload_(other.offload_) {
        other.loader_id_ = POWERINFER_INVALID_MODEL_LOADER_ID;
    }

    PowerInferLoaderHandle &operator= (PowerInferLoaderHandle &&other) noexcept {
        if (loader_id_ != POWERINFER_INVALID_MODEL_LOADER_ID) { powerinfer_loader_erase_model_loader(loader_id_); }

        loader_id_ = other.loader_id_;
        offload_   = other.offload_;
        other.loader_id_ = POWERINFER_INVALID_MODEL_LOADER_ID;
        return *this;
    }

public:
    void load(const char *filename, bool offload_to_disk) {
        loader_id_ = powerinfer_loader_create_model_loader(filename, offload_to_disk);
        offload_   = offload_to_disk;
    }

    PowerInferError hint(const PositionEmbeddingMetadata *meta, const size_t num_rope_factor, const float *rope_factor_array) const {
        return powerinfer_loader_hint_rotary_embedding(loader_id_, meta, num_rope_factor, rope_factor_array);
    }

    bool valid()              const { return loader_id_ != POWERINFER_INVALID_MODEL_LOADER_ID; }

    int  get_loader_id()      const { return loader_id_; }

    bool is_offload_to_disk() const { return offload_; }
};

struct PowerInferCacheHandle {
private:
    int loader_id_;

    int cache_id_;

public:
    PowerInferCacheHandle(): loader_id_(POWERINFER_INVALID_MODEL_LOADER_ID), cache_id_(POWERINFER_INVALID_MODEL_CACHE_ID) {}

    ~PowerInferCacheHandle() noexcept {
        if (cache_id_ != POWERINFER_INVALID_MODEL_CACHE_ID) { powerinfer_loader_erase_kvcache(loader_id_, cache_id_); }
    }

public:
    PowerInferError init_kvcache(int loader_id, const PowerInferKVCacheShape *shape_array, const int *in_mem_attn_array, const int num_in_mem_layer) {
        fprintf(stderr, "init kvcache %d\n", num_in_mem_layer);
        if (cache_id_ != POWERINFER_INVALID_MODEL_CACHE_ID) { return { true, "Redundant initialization of KVCache" }; }

        const int cache_id = powerinfer_loader_create_kvcache(loader_id);
        loader_id_ = loader_id;
        cache_id_  = cache_id;

        return powerinfer_loader_init_kvcache(loader_id, cache_id, shape_array, in_mem_attn_array, num_in_mem_layer);
    }

    PowerInferError clear_kvcache(const int start_pos)  {
        return powerinfer_loader_clear_kvcache(loader_id_, cache_id_, start_pos);
    }

public:
    bool valid()        const { return loader_id_ != POWERINFER_INVALID_MODEL_LOADER_ID && cache_id_ != POWERINFER_INVALID_MODEL_CACHE_ID; }

    int get_loader_id() const { return loader_id_; }

    int get_cache_id() const  { return cache_id_;  }
};