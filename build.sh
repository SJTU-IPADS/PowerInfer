#!/bin/bash
#rm -rf build
cmake -S . -B build -DLLAMA_CUBLAS=ON -DLLAMA_GGML_PERF=ON #-DLLAMA_RUN_WARMUP=OFF
cmake --build build --config Release

# if DLLAMA_GGML_PERF=ON
# -> avg exec time of operator in prefill & decode stage.
# -> ratio of active neurons located in the CPU
