import argparse
import pickle
import gguf
from gguf.constants import GGMLQuantizationType
from gguf.gguf_writer import GGUFWriter
import torch
from pathlib import Path
import os
import struct
import numpy as np
import re

def load_activation_weights(models_base: Path, i_layer: int, i_expert: int):
    res_tensor_list  = []

    if i_expert <= 0:
        for i in range(i_layer):
            filename = f"activation_{i}.pt"
            new_tensor = torch.load(models_base / filename)
            res_tensor_list.append(new_tensor)
    else:
        for i in range(i_layer):
            tensor_cat = []
            for e in range(i_expert):
                filename = f"activation_{i}_{e}.pt"
                new_tensor = torch.load(models_base / filename)
                tensor_cat.append(new_tensor)
            res_tensor_list.append(torch.cat(tensor_cat))

    return res_tensor_list


def append_gpu_idx(gguf: GGUFWriter, i_layer: int, i_expert: int, activation, select_count) -> None:
    _, indices = torch.topk(activation, k=int(select_count))

    if len(activation.shape) > 1:
        raise Exception(f'activation of multiple row has not been supported. (now: {activation.shape})')

    gpu_idx = torch.zeros_like(activation)
    gpu_idx[indices] = 1
    gpu_idx = gpu_idx.numpy().astype(np.int32)

    if i_expert > 0:
        expert_indice_col = int(activation.shape[0] / i_expert)
        for e in range(i_expert):
            expert_range_low  = e * expert_indice_col
            expert_range_high = expert_range_low + expert_indice_col
            expert_gpu_idx = gpu_idx[expert_range_low: expert_range_high]

            key = f"blk.{i_layer}.{e}.gpu_idx" 
            print(
                f"{key} => {key} {expert_gpu_idx.shape} {expert_gpu_idx.dtype} {expert_gpu_idx.nbytes/1024/1024} MiB"
            )
            gguf.add_tensor(
                name=key,
                tensor=expert_gpu_idx,
                raw_shape=expert_gpu_idx.shape[::-1],
                raw_dtype=GGMLQuantizationType.I32,
            )

            expert_indices = torch.masked_select(
                indices,
                (indices >= expert_range_low) & (indices < expert_range_high)
            )
            expert_indices -= expert_range_low
            expert_indices =  expert_indices.numpy().astype(np.int32)
            gpu_bucket     =  np.sort(expert_indices)

            key = f"blk.{i_layer}.{e}.gpu_bucket"
            print(
                f"{key} => {key} {gpu_bucket.shape} {gpu_bucket.dtype} {gpu_bucket.nbytes/1024/1024} MiB"
            )
            gguf.add_tensor(
                name=key,
                tensor=gpu_bucket,
                raw_shape=gpu_bucket.shape[::-1],
                raw_dtype=GGMLQuantizationType.I32,
            )
    else:
        key = f"blk.{i_layer}.gpu_idx"
        print(
            f"{key} => {key} {gpu_idx.shape} {gpu_idx.dtype} {gpu_idx.nbytes/1024/1024} MiB"
        )
        gguf.add_tensor(
            name=key,
            tensor=gpu_idx,
            raw_shape=gpu_idx.shape[::-1],
            raw_dtype=GGMLQuantizationType.I32,
        )

        indices = indices.numpy().astype(np.int32)
        gpu_bucket = np.sort(indices)
        key = f"blk.{i_layer}.gpu_bucket"
        print(
            f"{key} => {key} {gpu_bucket.shape} {gpu_bucket.dtype} {gpu_bucket.nbytes/1024/1024} MiB"
        )
        gguf.add_tensor(
            name=key,
            tensor=gpu_bucket,
            raw_shape=gpu_bucket.shape[::-1],
            raw_dtype=GGMLQuantizationType.I32,
        )

def export_split(activations_path: str, output_path: str, solved_list: list[int], vram_capacity: int, i_layer: int, i_expert: int):
    predictors = load_activation_weights(Path(activations_path), i_layer, i_expert) # predictor => activation acount
    gguf_out = GGUFWriter(output_path, "generic.gpu_index")
    for i, (activation, selected_count) in enumerate(zip(predictors, solved_list)):
        append_gpu_idx(gguf_out, i, i_expert, activation, selected_count)

    # set kvs
    gguf_out.add_block_count(len(predictors))
    # TODO: better to save the actual capacity that split neurons require
    gguf_out.add_uint64(gguf.Keys.Split.VRAM_CAPACITY, vram_capacity)

    gguf_out.write_header_to_file()
    gguf_out.write_kv_data_to_file()
    gguf_out.write_tensors_to_file()
    gguf_out.close()

    # post-process: write another unique file header to distinguish from the origianl GGUF file
    with open(output_path, "r+b") as fout:
        POWERINFER_MAGIC = int.from_bytes(b"PWRI", "little")
        fout.write(struct.pack("<I", POWERINFER_MAGIC))
        fout.write(struct.pack("<I", 3))

    print(f"exported GPU index to {output_path}")

