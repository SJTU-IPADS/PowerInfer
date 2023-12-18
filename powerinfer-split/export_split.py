import argparse
import pickle
import torch
from pathlib import Path
import os
import struct
from typing import BinaryIO
import numpy as np

def load_activation_weights(models_base: Path):
    # TODO: might need a specification file to indicate which models to load.
    # But for now, let's assume it is a plain directory of models_{0, ... , n_layers - 1}.pt
    *_, files = next(os.walk(models_base))
    return [torch.load(models_base / f"activation_{i}.pt") for i in range(len(files))]


def write_file_header(fout: BinaryIO, n_tensors: int) -> None:
    POWERINFER_MAGIC = int.from_bytes(b"PWRI", "little")
    fout.write(struct.pack("<I", POWERINFER_MAGIC))
    fout.write(struct.pack("i", 1))  # file version
    # TODO: If we found we need more common parameters, we can add them here.
    fout.write(struct.pack("i", n_tensors))


def write_tensor_header(
    fout: BinaryIO, key: str, shape: tuple[int, ...], dtype: np.dtype
) -> None:
    _NUMPY_TYPE_TO_FTYPE: dict[str, int] = {"float32": 0, "float16": 1, "int32": 18}
    bkey = key.encode("utf-8")
    fout.write(
        struct.pack("iii", len(shape), len(bkey), _NUMPY_TYPE_TO_FTYPE[dtype.name])
    )
    fout.write(struct.pack("i" * len(shape), *shape))
    fout.write(bkey)
    # Aligns to 32 bytes
    fout.seek((fout.tell() + 31) & -32)

def append_gpu_idx(fout: BinaryIO, activation, select_count) -> None:
    values, indices = torch.topk(activation, k=int(select_count))
    gpu_idx = torch.zeros_like(activation)
    gpu_idx[indices] = 1
    gpu_idx = gpu_idx.numpy().astype(np.int32)
    weights = gpu_idx
    dims = gpu_idx.shape[::-1]
    key = "gpu_idx"
    print(
        f"{key} => {key} {weights.shape} {weights.dtype} {weights.nbytes/1024/1024} MiB"
    )
    write_tensor_header(fout, key, dims, np.dtype("int32"))
    weights.tofile(fout)

    indices = indices.numpy().astype(np.int32)
    weights = indices
    dims = weights.shape[::-1]
    key = "gpu_bucket"
    print(
        f"{key} => {key} {weights.shape} {weights.dtype} {weights.nbytes/1024/1024} MiB"
    )
    write_tensor_header(fout, key, dims, np.dtype("int32"))
    weights = np.sort(weights)
    weights.tofile(fout)

def export_split(activations_path: str, output_path: str, solved_list: list[int]):
    predictors = load_activation_weights(Path(activations_path)) # predictor => activation acount
    n_tensors = len(predictors) * 2 # gpu_idx and gpu_bucket
    with open(output_path, "wb") as fout:
        fout.truncate()
        write_file_header(fout, n_tensors=n_tensors)
        for i, activation in enumerate(predictors):
            print(f"appending gpu idx layer-{i}")
            append_gpu_idx(fout, activation, solved_list[i])

    print(f"exported GPU index to {output_path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("predictors_path", help="path to the MLP predictors")
    parser.add_argument(
        "output_path",
        help="path to the output GGML adapter",
        default="./gpu-index.bin",
    )
    parser.add_argument("solver", help="path to the solver")

    with open(parser.parse_args().solver, "rb") as f:
        loaded_lst = pickle.load(f)

    args = parser.parse_args()
    export_split(args.predictors_path, args.output_path, loaded_lst)
