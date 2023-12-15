# PowerInfer: Fast Large Language Model Serving with a Consumer-grade GPU
---

*Demo* 🔥

https://github.com/hodlen/PowerInfer/assets/34213478/b782ccc8-0a2a-42b6-a6aa-07b2224a66f7

<sub>The demo is running with a single 24G 4090 GPU, the model is Falcon (ReLU)-40B, and the precision is FP16.</sub>

---
## Abstract

We introduce PowerInfer, a high-speed Large Language Model (LLM) inference engine on a personal computer (PC) 
equipped with a single consumer-grade GPU. The key underlying the design of PowerInfer is exploiting the high locality 
inherent in LLM inference, characterized by a power-law distribution in neuron activation. 
This distribution indicates that a small subset of neurons, termed hot neurons, are consistently activated 
across inputs, while the majority, cold neurons, vary based on specific inputs.
PowerInfer exploits such an insight to design a GPU-CPU hybrid inference engine:
hot-activated neurons are preloaded onto the GPU for fast access, while cold-activated neurons are computed 
on the CPU, thus significantly reducing GPU memory demands and CPU-GPU data transfers.
PowerInfer further integrates adaptive predictors and neuron-aware sparse operators,
optimizing the efficiency of neuron activation and computational sparsity.
Evaluation shows that PowerInfer attains an average token generation rate of 13.20 tokens/s, with a peak of 29.08 tokens/s, across various LLMs (including OPT-175B) on a single NVIDIA RTX 4090 GPU,
only 18\% lower than that achieved by a top-tier server-grade A100 GPU.
This significantly outperforms llama.cpp by up to 11.69x while retaining model accuracy.

## Feature
PowerInfer is a fast and easy-to-use inference engine for deploying LLM locally. Interestingly, we observe that in ReLU LLM, every neuron is an expert! And a small subset of neurons consistently contributes to the output.
PowerInfer is fast with:

- Exploiting the high locality in LLM infernece
- Neuron-aware hybrid CPU/GPU sparse operator
- Neuron granularity offloading

PowerInfer is flexible and easy to use with:

- Integration with popular [ReLU-sparse models](https://huggingface.co/SparseLLM)
- Low-latency serving locally with single consumer-grade GPU 

PowerInfer supports the following models:

- Falcon-40B model
- Llama family models

The SparseLLM Team is currently converting the Mistral-7B model to a sparser version. Stay tuned!



## Getting Started

- [Installation](##setup--installation)
- [Model Weights](##model-weights)
- [Supported Models](https://vllm.readthedocs.io/en/latest/models/supported_models.html)

## Setup & Installation
### Get the Code

```bash
git clone https://github.com/hodlen/PowerInfer
cd PowerInfer
```
### Build
In order to build PowerInfer you have two different options.

- Using `make`:
  - On Linux or MacOS:
    ```bash
      make
    ```
- Using `CMake`:
  - If you have one GPU:
  ```bash
    mkdir build
    cd build
    cmake .. -DLLAMA_CUBLAS=ON
    cmake --build . --config Release
  ```
  - If you just CPU:
  ```bash
    mkdir build
    cd build
    cmake .. 
    cmake --build . --config Release
  ```

## Model Weights

| Base Model | GGUF Format Link | Original Model |
|------------|------------------|----------------|
| LLaMA(ReLU)-2-7B   | [PowerInfer/ReluLLaMA-7B-PowerInfer-GGUF](https://huggingface.co/PowerInfer/ReluLLaMA-13B-PowerInfer-GGUF)    | [SparseLLM/ReluLLaMA-7B](https://huggingface.co/SparseLLM/ReluLLaMA-7B)     |
| LLaMA(ReLU)-2-13B    | [PowerInfer/ReluLLaMA-13B-PowerInfer-GGUF](https://huggingface.co/PowerInfer/ReluLLaMA-13B-PowerInfer-GGUF)   | [SparseLLM/ReluLLaMA-13B](https://huggingface.co/SparseLLM/ReluLLaMA-13B)  |
| Falcon(ReLU)-40B    | [PowerInfer/ReluFalcon-40B-PowerInfer-GGUF](https://huggingface.co/PowerInfer/ReluLLaMA-13B-PowerInfer-GGUF)    | [SparseLLM/ReluFalcon-40B](https://huggingface.co/SparseLLM/ReluFalcon-40B)      |

## Inference
- If you just have CPU:
```bash
  ./build/bin/main -m /PATH/TO/MODEL -n $(output_token_count) -t $(thread_num) -p $(prompt)
```
- If you have CPU with one consumer grade GPU:
```bash
  ./build/bin/main -m /PATH/TO/MODEL -n $(output_token_count) -t $(thread_num) -p $(prompt)
```


## Evaluation

![github-eval-4090](https://github.com/SJTU-IPADS/PowerInfer/assets/34213478/d700fa6c-77ba-462f-a2fc-3fd21c898f33)

![github-eval-2080ti-q4](https://github.com/SJTU-IPADS/PowerInfer/assets/34213478/0fc1bfc4-aafc-4e82-a865-bec0143aff1a)

PowerInfer achieves up to 11x and 8x speedup for FP16 and INT4 model!

## TODOs
We will release the code and data in the following order, please stay tuned!

- [x] Release core code of PowerInfer, supporting Llama-2, Falcon-40B.
- [ ] Release perplexity evaluation code
- [ ] Support Metal for Mac
- [ ] Release predictor training code 
- [ ] Support online split for FFN network
- [ ] Support Multi-GPU 



## Citation

If you find PowerInfer useful or relevant to your project and research, please kindly cite our paper:

```bibtex
Stay tuned!
```

## Acknowledgement
We are thankful for the easily modifiable operator library [ggml](https://github.com/ggerganov/ggml) and execution runtime provided by [llama.cpp](https://github.com/ggerganov/llama.cpp). We also extend our gratitude to [THUNLP](https://nlp.csai.tsinghua.edu.cn/) for their support of ReLU-based sparse models. We also appreciate the research of [DejaVu](https://proceedings.mlr.press/v202/liu23am.html), which inspires PowerInfer.
