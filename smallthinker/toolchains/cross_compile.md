## Cross Compile For multiple platforms

This document provides a cross-compilation tutorial for multiple platforms. The xxx.cmake files in the folder correspond to the compilation configurations for the target platforms. Some of the content can be flexibly adjusted (e.g., CLANG_VERSION, TARGET_PYTHON_VERSION, etc.), the configuration given in this folder is the configuration used by the author during testing.

### rk3588

```bash
mkdir -p /data/rk3588_sysroot
sshfs rk3588:/ /data/rk3588_sysroot -o follow_symlinks,kernel_cache

cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=toolchains/rk3588.cmake \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DGGML_OPENMP=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DPOWERINFER_NO_FFN_REPACK=ON \

cmake --build build --config RelWithDebInfo --target llama-cli llama-server -j32

rsync -avzP build/bin/llama-{cli,server} rk3588:/data/
```



### rk3576

```bash
mkdir -p /data/rk3576_sysroot
sshfs rk3576:/ /data/rk3576_sysroot -o follow_symlinks,kernel_cache

SYSROOT_PATH=/data/rk3576_sysroot cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=toolchains/rk3576.cmake \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DGGML_OPENMP=OFF \
    -DLLAMA_CURL=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DPOWERINFER_NO_FFN_REPACK=ON \

cmake --build build --config RelWithDebInfo --target llama-cli llama-server -j32

rsync -avzP build/bin/llama-cli rk3576:~
rsync -avzP build/bin/{llama-cli,llama-server} rk3576:~
```

### RasPi 5

```bash
mkdir -p /data/raspi_sysroot
sshfs raspi:/ /data/raspi_sysroot -o follow_symlinks,kernel_cache

SYSROOT_PATH=/data/raspi_sysroot cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=toolchains/raspi.cmake \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DGGML_OPENMP=OFF \
    -DLLAMA_CURL=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DPOWERINFER_NO_FFN_REPACK=ON \

cmake --build build --config RelWithDebInfo --target llama-cli llama-server -j32

rsync -avzP build/bin/llama-cli raspi:~
rsync -avzP build/bin/{llama-cli,llama-server} raspi:~
```

### RSK X5

```bash
mkdir -p /data/rskx5_sysroot
sshfs rskx5:/ /data/rskx5_sysroot -o follow_symlinks,kernel_cache

SYSROOT_PATH=/data/rskx5_sysroot cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=toolchains/rskx5.cmake \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DGGML_OPENMP=OFF \
    -DLLAMA_CURL=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DPOWERINFER_NO_FFN_REPACK=ON \

cmake --build build --config RelWithDebInfo --target llama-cli llama-server -j32

rsync -avzP build/bin/llama-cli rskx5:~
rsync -avzP build/bin/{llama-cli,llama-server} rskx5:~
```
### rk3566

```bash
mkdir -p /data/rk3566_sysroot
sshfs rk3566:/ /data/rk3566_sysroot -o follow_symlinks,kernel_cache

SYSROOT_PATH=/data/rk3566_sysroot cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=toolchains/rk3566.cmake \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DGGML_OPENMP=OFF \
    -DLLAMA_CURL=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DPOWERINFER_NO_FFN_REPACK=ON \

cmake --build build --config RelWithDebInfo --target llama-cli llama-server -j32

rsync -avzP build/bin/llama-cli rk3566:~
rsync -avzP build/bin/{llama-cli,llama-server} rk3566:~
```
