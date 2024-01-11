# Token generation performance troubleshooting

## Verifying that the model is running on the Nvidia GPU with cuBLAS

Make sure to set `-DLLAMA_CUBLAS=ON` when configuring CMake according to [README](../README.md#build), and purge the previous `build` directory before reconfiguring and recompiling.

When PowerInfer utilizes GPU, it will output diagnostic information that shows whether cuBLAS is offloading work to the GPU. Look for these lines:

```shell
llm_load_sparse_model_tensors: using CUDA for GPU acceleration
llm_load_sparse_model_tensors: mem required  = 16825.94 MB
llm_load_sparse_model_tensors: VRAM used: 10183.80 MB
```

If you see these lines, then the GPU is being used and model tensors are being loaded into VRAM.

## Verifying that FFN split is working

Ideally PowerInfer should be able to utilize full GPU memory or the VRAM budget you set. It first tries to offload dense layers to VRAM (attention, predictor, etc.), then it tries to split hot neurons of the FFN into VRAM if there is still space left.

If can look for this line to see how much FFN has been split and offloaded:

```shell
llm_load_gpu_split: offloaded 12577.50 MiB of FFN weights to GPU
```

If you find that the VRAM usage is much lower than expected, then FFN split is likely not working. Splitting FFN requires solving the neuron placement via `powerinfer` Python module and loading the generated GPU index file, shown in the following lines.

Solving (the result is cached, so this only happens once):
```shell
invoking powerinfer Python module to generate gpu split for 12738.39 MiB of VRAM
solver args: Namespace(activation='/nvme2/huggingface/PowerInfer/ReluLLaMA-13B-PowerInfer-GGUF/activation', neuron=13824, capacity=429432, layer=40, vram_capacity=13357166592, batch=256, threshold=0, output='/nvme2/huggingf
ace/PowerInfer/ReluLLaMA-13B-PowerInfer-GGUF/llama-13b-relu.powerinfer.gguf.generated.gpuidx')
...
exported GPU index to /nvme2/huggingface/PowerInfer/ReluLLaMA-13B-PowerInfer-GGUF/llama-13b-relu.powerinfer.gguf.generated.gpuidx
```

Loading generated or cached GPU index:
```shell
llama_model_loader: loaded meta data with 3 key-value pairs and 80 tensors from /nvme2/huggingface/PowerInfer/ReluLLaMA-13B-PowerInfer-GGUF/llama-13b-relu.powerinfer.gguf.generated.gpuidx (version GGUF V3 (latest))
llama_model_loader: - tensor    0:                    blk.0.gpu_idx i32      [ 13824,     1,     1,     1 ]
...
apply_tensors_to_base_model: applying gpu_idx adapter from '/nvme2/huggingface/PowerInfer/ReluLLaMA-13B-PowerInfer-GGUF/llama-13b-relu.powerinfer.gguf.generated.gpuidx' - please wait ...
```

If you don't any of see these lines, then FFN split is not working. It can be caused by:

- `powerinfer` Python module is not installed or not activated if you are using a virtual environment
- There is no `activation` directory in the model directory which contains the activation files for solving FFN split

Please refer to [Setup and Installation](../README.md#setup-and-installation) for more information on runtime dependencies and [Model Weights](../README.md#model-weights) for more information on model weights.

# Example of runtime flags effect on inference speed benchmark

Please refer to [Evaluation](../README.md#evaluation) for more information on token generation benchmark on Linux.

For Windows, we have tested [PowerInfer/ReluLLaMA-13B-PowerInfer-GGUF](https://huggingface.co/PowerInfer/ReluLLaMA-13B-PowerInfer-GGUF) on a machine with the following specs:

GPU: Nvidia RTX 2080Ti (11GB VRAM)
CPU: Intel i7-13700
RAM: 64GB DDR4 3200MHz

Run command: `.\build\bin\Release\main.exe -m path\to\model -n 64 -p "Once upon a time" [additional benchmark flags]`

Result:

| command | tokens/second (higher is better) |
| - | - |
| [no additional flags] | 4.05 |
| -t 8 | 4.27 |

# CPU affinity and hybrid architecture (big.LITTLE)

PowerInfer achieves the best performance when it is running on all CPU performance cores (P cores). On a hybrid architecture such as Intel 12th/13th Gen (Alder Lake), we recommend setting `-t --threads` with the available number of P cores. 

Windows sometimes are not able to schedule threads on P cores. If you find the token generation speed is unstable, or the utilization of P cores is low, you can try to set CPU affinity manually with `Start-Process` in PowerShell like this exmaple on 12th Gen Core i7 (8 P cores):

```ps
Start-Process -FilePath path\to\main.exe -ArgumentList "-m", "path\to\model", "-t", "8", "-n", "128", "-p", "`"Once upon a time`"" -NoNewWindow -PassThru -Wait | ForEach-Object { $_.ProcessorAffinity = 0x5555 }
```

It works like `taskset` on Linux and sets CPU affinity to P cores only (0x5555 is a bit mask for CPU0,2,4,6,8,10,12,14). Please refer to the docs of [Start-Process](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.management/start-process?view=powershell-7.4) for more details.
