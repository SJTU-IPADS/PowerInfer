from __future__ import annotations

from typing import Sequence

from .constants import MODEL_ARCH, MODEL_TENSOR, MODEL_TENSORS, TENSOR_NAMES


class TensorNameMap:
    mappings_cfg: dict[MODEL_TENSOR, tuple[str, ...]] = {
        # Token embeddings
        MODEL_TENSOR.TOKEN_EMBD: (
            "gpt_neox.embed_in",                         # gptneox
            "transformer.wte",                           # gpt2 gpt-j mpt refact qwen
            "transformer.word_embeddings",               # falcon
            "word_embeddings",                           # bloom
            "model.embed_tokens",                        # llama-hf
            "tok_embeddings",                            # llama-pth
            "embeddings.word_embeddings",                # bert
            "language_model.embedding.word_embeddings",  # persimmon
        ),

        # Token type embeddings
        MODEL_TENSOR.TOKEN_TYPES: (
            "embeddings.token_type_embeddings",  # bert
        ),

        # Normalization of token embeddings
        MODEL_TENSOR.TOKEN_EMBD_NORM: (
            "word_embeddings_layernorm",  # bloom
        ),

        # Position embeddings
        MODEL_TENSOR.POS_EMBD: (
            "transformer.wpe",                 # gpt2
            "embeddings.position_embeddings",  # bert
        ),

        # Output
        MODEL_TENSOR.OUTPUT: (
            "embed_out",                 # gptneox
            "lm_head",                   # gpt2 mpt falcon llama-hf baichuan qwen
            "output",                    # llama-pth bloom
            "word_embeddings_for_head",  # persimmon
        ),

        # Output norm
        MODEL_TENSOR.OUTPUT_NORM: (
            "gpt_neox.final_layer_norm",               # gptneox
            "transformer.ln_f",                        # gpt2 gpt-j falcon
            "model.norm",                              # llama-hf baichuan
            "norm",                                    # llama-pth
            "embeddings.LayerNorm",                    # bert
            "transformer.norm_f",                      # mpt
            "ln_f",                                    # refact bloom qwen
            "language_model.encoder.final_layernorm",  # persimmon
        ),

        # Rope frequencies
        MODEL_TENSOR.ROPE_FREQS: (
            "rope.freqs",  # llama-pth
        ),
    }

    block_mappings_cfg: dict[MODEL_TENSOR, tuple[str, ...]] = {
        # Attention norm
        MODEL_TENSOR.ATTN_NORM: (
            "gpt_neox.layers.{bid}.input_layernorm",                # gptneox
            "transformer.h.{bid}.ln_1",                             # gpt2 gpt-j refact qwen
            "transformer.blocks.{bid}.norm_1",                      # mpt
            "transformer.h.{bid}.input_layernorm",                  # falcon7b
            "h.{bid}.input_layernorm",                              # bloom
            "transformer.h.{bid}.ln_mlp",                           # falcon40b
            "model.layers.{bid}.input_layernorm",                   # llama-hf
            "layers.{bid}.attention_norm",                          # llama-pth
            "encoder.layer.{bid}.attention.output.LayerNorm",       # bert
            "language_model.encoder.layers.{bid}.input_layernorm",  # persimmon
            "model.layers.{bid}.ln1",                               # yi
        ),

        # Attention norm 2
        MODEL_TENSOR.ATTN_NORM_2: (
            "transformer.h.{bid}.ln_attn",  # falcon40b
        ),

        # Attention query-key-value
        MODEL_TENSOR.ATTN_QKV: (
            "gpt_neox.layers.{bid}.attention.query_key_value",                     # gptneox
            "transformer.h.{bid}.attn.c_attn",                                     # gpt2 qwen
            "transformer.blocks.{bid}.attn.Wqkv",                                  # mpt
            "transformer.h.{bid}.self_attention.query_key_value",                  # falcon
            "h.{bid}.self_attention.query_key_value",                              # bloom
            "language_model.encoder.layers.{bid}.self_attention.query_key_value",  # persimmon
        ),

        # Attention query
        MODEL_TENSOR.ATTN_Q: (
            "model.layers.{bid}.self_attn.q_proj",       # llama-hf
            "layers.{bid}.attention.wq",                 # llama-pth
            "encoder.layer.{bid}.attention.self.query",  # bert
            "transformer.h.{bid}.attn.q_proj",           # gpt-j
        ),

        # Attention key
        MODEL_TENSOR.ATTN_K: (
            "model.layers.{bid}.self_attn.k_proj",     # llama-hf
            "layers.{bid}.attention.wk",               # llama-pth
            "encoder.layer.{bid}.attention.self.key",  # bert
            "transformer.h.{bid}.attn.k_proj",         # gpt-j
        ),

        # Attention value
        MODEL_TENSOR.ATTN_V: (
            "model.layers.{bid}.self_attn.v_proj",       # llama-hf
            "layers.{bid}.attention.wv",                 # llama-pth
            "encoder.layer.{bid}.attention.self.value",  # bert
            "transformer.h.{bid}.attn.v_proj",           # gpt-j
        ),

        # Attention output
        MODEL_TENSOR.ATTN_OUT: (
            "gpt_neox.layers.{bid}.attention.dense",                     # gptneox
            "transformer.h.{bid}.attn.c_proj",                           # gpt2 refact qwen
            "transformer.blocks.{bid}.attn.out_proj",                    # mpt
            "transformer.h.{bid}.self_attention.dense",                  # falcon
            "h.{bid}.self_attention.dense",                              # bloom
            "model.layers.{bid}.self_attn.o_proj",                       # llama-hf
            "layers.{bid}.attention.wo",                                 # llama-pth
            "encoder.layer.{bid}.attention.output.dense",                # bert
            "transformer.h.{bid}.attn.out_proj",                         # gpt-j
            "language_model.encoder.layers.{bid}.self_attention.dense",  # persimmon
        ),

        # Rotary embeddings
        MODEL_TENSOR.ATTN_ROT_EMBD: (
            "model.layers.{bid}.self_attn.rotary_emb.inv_freq",   # llama-hf
            "layers.{bid}.attention.inner_attention.rope.freqs",  # llama-pth
        ),

        # Feed-forward norm
        MODEL_TENSOR.FFN_NORM: (
            "gpt_neox.layers.{bid}.post_attention_layernorm",                # gptneox
            "transformer.h.{bid}.ln_2",                                      # gpt2 refact qwen
            "h.{bid}.post_attention_layernorm",                              # bloom
            "transformer.blocks.{bid}.norm_2",                               # mpt
            "model.layers.{bid}.post_attention_layernorm",                   # llama-hf
            "layers.{bid}.ffn_norm",                                         # llama-pth
            "encoder.layer.{bid}.output.LayerNorm",                          # bert
            "language_model.encoder.layers.{bid}.post_attention_layernorm",  # persimmon
            "model.layers.{bid}.ln2",                                        # yi
        ),

        MODEL_TENSOR.FFN_GATE_INP: (
            "layers.{bid}.feed_forward.gate",           # mixtral
            "model.layers.{bid}.block_sparse_moe.gate", # mixtral
        ),

        # Feed-forward up
        MODEL_TENSOR.FFN_UP: (
            "gpt_neox.layers.{bid}.mlp.dense_h_to_4h",                # gptneox
            "transformer.h.{bid}.mlp.c_fc",                           # gpt2
            "transformer.blocks.{bid}.ffn.up_proj",                   # mpt
            "transformer.h.{bid}.mlp.dense_h_to_4h",                  # falcon
            "h.{bid}.mlp.dense_h_to_4h",                              # bloom
            "model.layers.{bid}.mlp.up_proj",                         # llama-hf refact
            "layers.{bid}.feed_forward.w3",                           # llama-pth
            "encoder.layer.{bid}.intermediate.dense",                 # bert
            "transformer.h.{bid}.mlp.fc_in",                          # gpt-j
            "language_model.encoder.layers.{bid}.mlp.dense_h_to_4h",  # persimmon
            "transformer.h.{bid}.mlp.w1",                             # qwen
        ),

        MODEL_TENSOR.FFN_UP_EXP: (
            "layers.{bid}.feed_forward.experts.{xid}.w3",           # mixtral
            "model.layers.{bid}.block_sparse_moe.experts.{xid}.w3", # mixtral
        ),

        # Feed-forward gate
        MODEL_TENSOR.FFN_GATE: (
            "model.layers.{bid}.mlp.gate_proj",           # llama-hf refact
            "layers.{bid}.feed_forward.w1",               # llama-pth
            "transformer.h.{bid}.mlp.w2",                 # qwen
        ),

        MODEL_TENSOR.FFN_GATE_EXP: (
            "layers.{bid}.feed_forward.experts.{xid}.w1",           # mixtral
            "model.layers.{bid}.block_sparse_moe.experts.{xid}.w1", # mixtral
        ),

        # Feed-forward down
        MODEL_TENSOR.FFN_DOWN: (
            "gpt_neox.layers.{bid}.mlp.dense_4h_to_h",                # gptneox
            "transformer.h.{bid}.mlp.c_proj",                         # gpt2 refact qwen
            "transformer.blocks.{bid}.ffn.down_proj",                 # mpt
            "transformer.h.{bid}.mlp.dense_4h_to_h",                  # falcon
            "h.{bid}.mlp.dense_4h_to_h",                              # bloom
            "model.layers.{bid}.mlp.down_proj",                       # llama-hf
            "layers.{bid}.feed_forward.w2",                           # llama-pth
            "encoder.layer.{bid}.output.dense",                       # bert
            "transformer.h.{bid}.mlp.fc_out",                         # gpt-j
            "language_model.encoder.layers.{bid}.mlp.dense_4h_to_h",  # persimmon
        ),

        MODEL_TENSOR.FFN_DOWN_EXP: (
            "layers.{bid}.feed_forward.experts.{xid}.w2",           # mixtral
            "model.layers.{bid}.block_sparse_moe.experts.{xid}.w2", # mixtral
        ),

        MODEL_TENSOR.ATTN_Q_NORM: (
            "language_model.encoder.layers.{bid}.self_attention.q_layernorm",
        ),

        MODEL_TENSOR.ATTN_K_NORM: (
            "language_model.encoder.layers.{bid}.self_attention.k_layernorm",
        ),

        MODEL_TENSOR.ROPE_FREQS: (
            "language_model.encoder.layers.{bid}.self_attention.rotary_emb.inv_freq",  # persimmon
        ),
    }

    mapping: dict[str, tuple[MODEL_TENSOR, str]]

    def __init__(self, arch: MODEL_ARCH, n_blocks: int):
        self.mapping = {}
        for tensor, keys in self.mappings_cfg.items():
            if tensor not in MODEL_TENSORS[arch]:
                continue
            tensor_name = TENSOR_NAMES[tensor]
            self.mapping[tensor_name] = (tensor, tensor_name)
            for key in keys:
                self.mapping[key] = (tensor, tensor_name)
        for bid in range(n_blocks):
            for tensor, keys in self.block_mappings_cfg.items():
                if tensor not in MODEL_TENSORS[arch]:
                    continue
                # TODO: make this configurable
                n_experts = 8
                for xid in range(n_experts):
                    tensor_name = TENSOR_NAMES[tensor].format(bid = bid, xid = xid)
                    self.mapping[tensor_name] = (tensor, tensor_name)
                    for key in keys:
                        key = key.format(bid = bid, xid = xid)
                        self.mapping[key] = (tensor, tensor_name)

    def get_type_and_name(self, key: str, try_suffixes: Sequence[str] = ()) -> tuple[MODEL_TENSOR, str] | None:
        result = self.mapping.get(key)
        if result is not None:
            return result
        for suffix in try_suffixes:
            if key.endswith(suffix):
                result = self.mapping.get(key[:-len(suffix)])
                if result is not None:
                    return result[0], result[1] + suffix
        return None

    def get_name(self, key: str, try_suffixes: Sequence[str] = ()) -> str | None:
        result = self.get_type_and_name(key, try_suffixes = try_suffixes)
        if result is None:
            return None
        return result[1]

    def get_type(self, key: str, try_suffixes: Sequence[str] = ()) -> MODEL_TENSOR | None:
        result = self.get_type_and_name(key, try_suffixes = try_suffixes)
        if result is None:
            return None
        return result[0]

    def __getitem__(self, key: str) -> str:
        try:
            return self.mapping[key][1]
        except KeyError:
            raise KeyError(key)

    def __contains__(self, key: str) -> bool:
        return key in self.mapping

    def __repr__(self) -> str:
        return repr(self.mapping)


def get_tensor_name_map(arch: MODEL_ARCH, n_blocks: int) -> TensorNameMap:
    return TensorNameMap(arch, n_blocks)
