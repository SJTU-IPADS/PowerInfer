## Intro
- SmallThinker ([SmallThinker-21BA3B-Instruct](https://huggingface.co/PowerInfer/SmallThinker-21BA3B-Instruct) and [SmallThinker-4BA0.6B-Instruct](https://huggingface.co/PowerInfer/SmallThinker-4BA0.6B-Instruct)) is a family of on-device native Mixture-of-Experts (MoE) language models specially designed for local deployment, co-developed by the IPADS and School of AI at Shanghai Jiao Tong University and Zenergize AI. Designed from the ground up for resource-constrained environments, SmallThinker brings powerful, private, and low-latency AI directly to your personal devices, without relying on the cloud.

- This inference framework is specifically optimized for sparse model inference to achieve faster speeds, leveraging the router's pre-selection mechanism to enable efficient inference even in memory-constrained scenarios.

## Demo


https://github.com/user-attachments/assets/cefd466e-3b1f-47a9-8dc3-f1cf5119045e


## Speed
### SmallThinker 21B 
| Model                            | Memory(GiB)         | i9 14900 | 1+13 8ge4 | rk3588 (16G) | Raspberry PI 5 |
|--------------------------------------|---------------------|----------|-----------|--------------|----------------|
| SmallThinker 21B+sparse              | 11.47               | 30.19    | 23.03     | 10.84        | 6.61           |
| SmallThinker 21B+sparse +limited memory | limit 8G         | 20.30     | 15.50        | 8.56     | -              |
| Qwen3 30B A3B                        | 16.20               | 33.52    | 20.18     | 9.07         | -              |
| Qwen3 30B A3Blimited memory          | limit 8G            | 10.11     | 0.18         | 6.32     | -              |
| Gemma 3n E2B                         | 1G, theoretically   | 36.88    | 27.06     | 12.50        | 6.66           |
| Gemma 3n E4B                         | 2G, theoretically   | 21.93    | 16.58     | 7.37         | 4.01           |

### SmallThinker 4B 
| Model                                         | Memory(GiB)         | i9 14900 | 1+13 8gen4 | rk3588 (16G) | rk3576 | Raspberry PI 5 | RDK X5 | rk3566 |
|-----------------------------------------------|---------------------|----------|------------|--------------|--------|----------------|--------|--------|
| SmallThinker 4B+sparse ffn +sparse lm_head    | 2.24                | 108.17   | 78.99      | 39.76        | 15.10  | 28.77          | 7.23   | 6.33   |
| SmallThinker 4B+sparse ffn +sparse lm_head+limited memory | limit 1G           | 29.99    | 20.91      | 15.04        | 2.60   | 0.75           | 0.67   | 0.74   |
| Qwen3 0.6B                                    | 0.6                 | 148.56   | 94.91      | 45.93        | 15.29  | 27.44          | 13.32  | 9.76   |
| Qwen3 1.7B                                    | 1.3                 | 62.24    | 41.00      | 20.29        | 6.09   | 11.08          | 6.35   | 4.15   |
| Qwen3 1.7B limited memory                     | limit 1G            | 2.66     | 1.09       | 1.00         | 0.47   | -              | -      | 0.11   |
| Gemma3n E2B                                   | 1G, theoretically   | 36.88    | 27.06      | 12.50        | 3.80   | 6.66           | 3.46   | 2.45   |



Note：i9 14900、1+13 8ge4 use 4 threads，others use the number of threads that  can achieve the maximum speed 

## Setup
1. cd smallthinker before compiling
```bash
cd smallthinker
```
2. install clang-21 and mold：

```bash
sudo apt install clang-21 mold
```
3. init submodule：

```bash
git submodule update --init --recursive
```

## Convert Model
```bash
python3 convert_hf_to_gguf.py /path/to/safetensors_model --outtype f16 --outfile /path/to/gguf_fp16 --transpose-down all

./build_x86/bin/llama-quantize --pure /path/to/gguf_fp16  /path/to/gguf_q4_0 Q4_0  8
```
Note:lm_head sparsity is not included. If needed, please merge model_lm_head.pt into the safetensors file before executing the above commands, or directly download the GGUF file we provide.
## x86 Compile

```bash
cmake -S . -B build \
    -DCMAKE_C_COMPILER=clang-21 \
    -DCMAKE_CXX_COMPILER=clang++-21 \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DGGML_OPENMP=OFF \
    -DLLAMA_CURL=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DAZ_ENABLE_PERFETTO=OFF \
    -DPOWERINFER_NO_FFN_REPACK=ON \
    -DPOWERINFER_WITH_TRACING=OFF \
    -DGGML_CPU_AARCH64=OFF  

cmake --build build --config RelWithDebInfo --target llama-cli -j32
```

## Android NDK (Qualcomm 8 Elite)
1. Need to manually compile and install libaio into the NDK.：
```bash
cd powerinfer/third_part/libaio
export TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64
export TARGET=aarch64-linux-android
export HOST=$TARGET
export API=34
export AR=$TOOLCHAIN/bin/llvm-ar
export CC=$TOOLCHAIN/bin/$TARGET$API-clang
export AS=$CC
export CXX=$TOOLCHAIN/bin/$TARGET$API-clang++
export LD=$TOOLCHAIN/bin/ld
export RANLIB=$TOOLCHAIN/bin/llvm-ranlib
export STRIP=$TOOLCHAIN/bin/llvm-strip
make prefix=$NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr install
```
2. liburing is the same
```bash
cd powerinfer/third_part/liburing
export TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64
export TARGET=aarch64-linux-android
export HOST=$TARGET
export API=34
export AR=$TOOLCHAIN/bin/llvm-ar
export CC=$TOOLCHAIN/bin/$TARGET$API-clang
export AS=$CC
export CXX=$TOOLCHAIN/bin/$TARGET$API-clang++
export LD=$TOOLCHAIN/bin/ld
export RANLIB=$TOOLCHAIN/bin/llvm-ranlib
export STRIP=$TOOLCHAIN/bin/llvm-strip
./configure --prefix=$NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr
make install
```

```bash
cmake -S . -B build_a \
    -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-34 \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBUILD_SHARED_LIBS=OFF \
    -DGGML_OPENMP=OFF \
    -DLLAMA_CURL=OFF \
    -DAZ_ENABLE_PERFETTO=ON \
    -DPOWERINFER_NO_FFN_REPACK=ON \
    -DDISABLE_ARM_FEATURE_CHECK=ON \
    -DCMAKE_C_FLAGS="-march=armv8.6-a -D__USE_GNU -Ofast -flto" \
    -DCMAKE_CXX_FLAGS="-march=armv8.6-a -D__USE_GNU -Ofast -flto"

    cmake --build build_a --config RelWithDebInfo --target llama-cli -j32
```
Other platforms (such as rk3588) compile commands refer to toolchains/cross_compile.md

## Run(need to use a sparse model with Q4_0 quantization, and a maximum of 8 threads)
### Normal Run
```bash
./llama-cli -m /path/to/gguf_q4_0 -no-cnv --temp 0.6 --top-k 20 --top-p 0.95 --samplers "temperature;top_k;top_p" -p "<|im_start|>system\nYou are a helpful assistant.<|im_end|>\n<|im_start|>user\nCalculate the integral of f(x) = sin(x) from 0 to 3pi/4.<|im_end|>\n<|im_start|>assistant" -t 4 -n 256
```
### Memory-Efficient Run 
#### Prepare：
1. generate expert bundle
```bash
GENERATE_EXPERT_BUNDLE=/path/to/bundle ./llama-cli -m /path/to/gguf_q4_0 --temp 0.6 --top-p 0.95 --top-k 20 --samplers "penalties;temperature;top_k;top_p" -t 4 -n 128  -no-cnv
```
2. remove moe weights in the gguf file(use when run in termux)
```bash
python get_no_moe_weights_ffn.py /path/to/gguf_q4_0 /path/to/no_moe_gguf_q4_0
``` 
3. Modify the value on line 22 (max_n_cached_matrices) of the file (powerinfer/moe_sparse_pipeline/moe_sparse_pipeline/config.hpp) according to the actual memory of your own machine, here are some recommended configuration for SmallThinker:

#### Run the Memory-Efficient Version：
- 21B model under 8GB limit: max_n_cached_matrices = 3 * 64 * 32
- 4B model under 1GB limit: max_n_cached_matrices = 3 * 32 * 8
```bash
EXPERT_BUNDLE_PATH=/path/to/bundle ./llama-cli -m /path/to/no_moe_gguf_q4_0 --no-cnv --temp 0.6 --top-k 20 --top-p 0.95 --samplers "temperature;top_k;top_p" -p "<|im_start|>system\nYou are a helpful assistant.<|im_end|>\n<|im_start|>user\nCalculate the integral of f(x) = sin(x) from 0 to 3pi/4.<|im_end|>\n<|im_start|>assistant" -t 4 -n 256 -ub 4
```
### Note: 
1. The models use a sparse lm_head which may lead to some loss in precision. If you want to disable it, change the condition at src/llama-model.cpp:7580 to false.But the speed is slower.
2. It may require root privileges when running in Termux when run the Memory-Efficient Version.


## Acknowledgements

We would like to thank the following projects:
- [llama.cpp](https://github.com/ggml-org/llama.cpp)
