import gguf
from gguf.constants import GGMLQuantizationType
from gguf.gguf_writer import GGUFWriter
import torch
from pathlib import Path
import os
import struct
import numpy as np
import re


def load_activation_weights(models_base: Path):
    # TODO: might need a specification file to indicate which models to load.
    # But for now, let's assume it is a plain directory of activation_{0, ... , n_layers - 1}.pt
    *_, files = next(os.walk(models_base))
    activation_files = [f for f in files if re.match(r"activation_\d+.pt", f)]
    activation_files.sort()
    return [torch.load(models_base / f) for f in activation_files]


def append_gpu_idx(
    gguf: GGUFWriter,
    i_layer: int,
    activation: torch.Tensor,
    select_count: int,
    skip_bucket=False,
):
    _, indices = torch.topk(activation, k=int(select_count))
    gpu_idx = torch.zeros_like(activation)
    gpu_idx[indices] = 1
    gpu_idx = gpu_idx.numpy().astype(np.int32)
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
    if skip_bucket:
        return
    append_gpu_bucket(gguf, i_layer, indices)


def append_gpu_bucket(gguf: GGUFWriter, i_layer: int, indices: torch.Tensor):
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


def export_split(
    activations_path: str,
    output_path: str,
    solved_list: list[int],
    vram_capacity: int,
    for_moe=False,
):
    activations = load_activation_weights(Path(activations_path))
    gguf_out = GGUFWriter(
        output_path, "moe.gpu_idx" if for_moe else "generic.gpu_index"
    )
    for i, (activation, selected_count) in enumerate(zip(activations, solved_list)):
        # MoE models do not have remapping of neurons, so skip the bucket
        append_gpu_idx(gguf_out, i, activation, selected_count, skip_bucket=not for_moe)

    # set kvs
    gguf_out.add_block_count(len(activations))
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
