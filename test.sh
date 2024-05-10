#!/bin/bash

rm -rf build
CC=/opt/rocm/llvm/bin/clang CXX=/opt/rocm/llvm/bin/clang++ cmake -S . -B build -DLLAMA_HIPBLAS=ON -DAMDGPU_TARGETS=gfx1100
cmake --build build --config Release -j 24
./build/bin/main -m ./ReluLLaMA-7B/llama-7b-relu.q4.powerinfer.gguf -n 128 -p "Once upon a time" --ignore-eos --seed 0 --top-k 1 --reset-gpu-index --vram-budget 8