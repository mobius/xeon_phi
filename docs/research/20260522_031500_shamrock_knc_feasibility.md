# Shamrock 在 Xeon Phi 7120P (KNC) 上的可行性评估

> 评估日期: 2026-05-22
> 评估对象: [Shamrock](https://github.com/Shamrock-code/Shamrock) — 基于 SYCL 的高性能天体物理模拟框架
> 目标平台: Intel Xeon Phi 7120P (Knights Corner, KNC)

---

## 结论

**❌ 完全不可行**

Shamrock 无法在 Xeon Phi 7120P (KNC) 上运行。根本原因是 Shamrock 基于 **SYCL** 和 **AdaptiveCpp** 构建，而 KNC 架构从未获得 SYCL 支持，AdaptiveCpp 也没有 KNC 后端。

---

## Shamrock 技术栈分析

### 核心依赖

| 依赖 | Shamrock 要求版本 | KNC 支持情况 |
|------|------------------|-------------|
| **AdaptiveCpp** (SYCL 实现) | 最新版 | ❌ **不支持 KNC** |
| **LLVM/Clang** | 20 | ❌ **不支持 k1om 架构** |
| **OpenMP** | libomp-20-dev | ⚠️ 仅 host 端可用 |
| **CMake** | 3.x+ | ✅ 可安装 |
| **Boost** | 较新版本 | ✅ 可编译 |
| **Python** | 3.x+ | ✅ 可安装 |
| **MPI** | OpenMPI | ✅ 支持 KNC |

### Shamrock 架构特点

1. **SYCL-based**: 所有计算 kernel 使用 SYCL API (`sycl::queue`, `sycl::buffer`, `sycl::nd_range`)
2. **AdaptiveCpp backend**: 通过 AdaptiveCpp (原 hipSYCL) 将 SYCL 编译为 CUDA/OpenCL/oneAPI/OpenMP
3. **Kernel 执行模型**: Host 端管理 SYCL queue，Device 端执行 kernel
4. **Python 绑定**: 通过 pybind11 暴露 C++ API 到 Python

---

## KNC 平台约束

### 硬件/软件限制

| 项目 | KNC 7120P 现状 |
|------|---------------|
| 架构指令集 | x86_64 + KNC (k1om)，512-bit SIMD |
| 操作系统 | Linux 2.6.38，glibc ~2.14 |
| 编译器 | ICC 16.0 (最后一个支持 KNC 的版本) |
| SYCL Runtime | ❌ **不存在** |
| OpenCL Runtime | ⚠️ 1.2 支持，但需额外安装，已废弃 |
| LLVM 支持 | ❌ **从未支持 k1om** |
| oneAPI/Level Zero | ❌ KNC EOL 早于 oneAPI 发布 |
| CMake/Boost/Python | ✅ 可在 CentOS 7/容器内安装 |

### KNC 编程模型现状

KNC 支持的编程模型（已验证）：
- ✅ OpenMP (指令式并行)
- ✅ POSIX Threads (显式线程)
- ✅ Intel TBB 4.4 (任务并行，纯原生)
- ✅ Intel Cilk Plus (任务分治)
- ✅ Intel MKL (库自动并行)
- ✅ Intel MPI (分布式多进程)
- ❌ SYCL / AdaptiveCpp
- ❌ CUDA / ROCm / oneAPI

---

## 逐项可行性分析

### 1. AdaptiveCpp → ❌ 不可行

**AdaptiveCpp** 是 Shamrock 的核心依赖，负责将 SYCL 代码编译到目标后端。

**问题:**
- AdaptiveCpp 支持的后端: **CUDA, OpenCL, oneAPI/Level Zero, OpenMP (host)**
- **没有 KNC 后端**，也不可能有
- AdaptiveCpp 基于 LLVM，需要 Clang 编译器生成 device 代码
- Clang 从未支持 `k1om` 架构（`-target k1om-unknown-linux-gnu` 不存在）

**尝试编译的假设结果:**
```bash
# Shamrock 使用 AdaptiveCpp 的 syclcc 编译器
syclcc --backend=omp ...
# 或试图指定 KNC target
# AdaptiveCpp 无法生成 k1om 指令，编译会失败
```

### 2. SYCL 标准 → ❌ 不可行

**SYCL 1.2.1** 发布于 2017 年，**SYCL 2020** 发布于 2021 年，而 **KNC 已于 2017 年底 EOL**。

**问题:**
- KNC 没有 SYCL runtime 实现
- SYCL 的 `sycl::device_selector` 无法在 KNC 上找到设备
- SYCL 的内存模型 (`sycl::buffer`, `USM`) 需要底层 driver/runtime 支持
- KNC 上唯一接近的异构 API 是 **OpenCL 1.2**，但 Shamrock 不支持纯 OpenCL 后端（只通过 AdaptiveCpp 间接使用）

### 3. LLVM/Clang 20 → ❌ 不可行

Shamrock 要求 **LLVM 20 / Clang 20** 作为编译器基础。

**问题:**
- LLVM 从未支持 KNC (`k1om`) 架构
- Clang 没有 `-target k1om` 或 `-march=knc` 选项
- KNC 只能使用 Intel 专有编译器: `icc/icpc -mmic`
- 即使安装 LLVM 20 在 host 上，也无法生成 MIC 端代码

### 4. OpenMP Backend → ❌ 不可行

AdaptiveCpp 提供 **OpenMP backend**，将 SYCL kernel 转换为 OpenMP 并行循环在 CPU 上运行。

**问题:**
- OpenMP backend 只在 **host CPU** 上运行，不是在 MIC 设备上
- AdaptiveCpp 的 OpenMP backend 不支持 `#pragma omp target` (offload)
- 即使能编译，也只是在 Host Xeon Gold 上运行，不会利用 KNC
- KNC 的价值在于作为 offload 加速器，纯 host OpenMP 失去了意义

### 5. Python 绑定 → ⚠️ 技术上可行但无意义

Shamrock 的 Python 接口通过 pybind11 实现。

**问题:**
- Python 3.x 可以在 Rocky Linux 8.10 上运行
- pybind11 可以编译
- 但如果底层 C++ 无法编译（原因 1-4），Python 接口也无用武之地

### 6. MPI → ✅ 可行但不是瓶颈

Shamrock 使用 MPI 做分布式多节点并行。

**问题:**
- KNC 支持 Intel MPI（已验证）
- 但这只是 Shamrock 的一小部分
- 核心计算 kernel 仍依赖 SYCL，MPI 无法替代

---

## 如果非要移植，工作量估算

### 方案 A: 将 Shamrock 的 SYCL kernel 全部重写为 OpenMP offload

**工作量**: **巨大（相当于重写整个框架）**

- Shamrock 的 kernel 分布在数十个 `.hpp` 文件中，全部使用 `sycl::nd_item`, `sycl::item`, `sycl::group_barrier` 等 SYCL 特有 API
- 每个 kernel 需要手动转换为 `#pragma omp target` + `#pragma omp parallel for simd`
- SYCL 的内存管理 (`sycl::buffer`, `sycl::accessor`) 需要替换为 `#pragma offload` 数据拷贝
- 验证工作量和 bug 风险极高

### 方案 B: 编写 SYCL-to-KNC 翻译层

**工作量**: **极其巨大且不可行**

- 相当于为 KNC 实现一个 SYCL runtime
- SYCL 的 queue、event、buffer、accessor、kernel launch 等抽象层需要完整映射到 KNC 的 COI/SCIF 接口
- KNC 的编程模型与 GPU/SYCL 差异过大（KNC 是 x86 多核，SYCL 面向 SIMD/GPU）

### 方案 C: 纯 Host 运行（放弃 KNC）

**工作量**: 零，但失去 KNC 加速意义

- 在 Host Xeon Gold 6252 上编译运行 Shamrock
- KNC 不参与计算
- 这违背了在 KNC 上应用的初衷

---

## 与其他 KNC 项目的对比

| 项目 | 编程模型 | KNC 可行性 | 原因 |
|------|---------|-----------|------|
| **PhiBench** | OpenMP + Intrinsic | ✅ 可行 | 原生 OpenMP，已验证 |
| **STREAM** | OpenMP | ✅ 可行 | 标准 OpenMP 并行 |
| **GROMACS** | OpenMP + MPI | ✅ 可行 | 原生 C + OpenMP，有 MIC 补丁 |
| **LAMMPS** | OpenMP + MPI | ✅ 可行 | 原生 C++ + OpenMP |
| **Shamrock** | SYCL/AdaptiveCpp | ❌ 不可行 | SYCL runtime 不存在 |
| **PyTorch** | CUDA/oneAPI | ❌ 不可行 | 无 KNC 后端 |
| **TensorFlow** | CUDA/oneAPI | ❌ 不可行 | 无 KNC 后端 |

---

## 结论与建议

### 最终结论

**Shamrock 在 Xeon Phi 7120P (KNC) 上完全不可行。**

根本原因链：
1. Shamrock 依赖 SYCL → KNC 没有 SYCL runtime
2. Shamrock 依赖 AdaptiveCpp → AdaptiveCpp 没有 KNC 后端
3. Shamrock 依赖 Clang/LLVM → Clang/LLVM 不支持 k1om 架构
4. KNC 只能用 ICC 16.0 + `-mmic` → Shamrock 不兼容 ICC 编译

### 建议

1. **放弃在 KNC 上运行 Shamrock**：技术障碍是根本性的，不是配置问题
2. **如需天体物理模拟 + KNC**：寻找基于 OpenMP 或 MPI + OpenMP 的替代方案（如 GADGET-4, AREPO）
3. **如需使用 Shamrock**：在支持 SYCL 的平台上运行，如 Intel GPU (Xe), NVIDIA GPU (通过 AdaptiveCpp CUDA backend), AMD GPU (通过 ROCm)
4. **KNC 的最佳用途**：运行原生 OpenMP/TBB/MKL 的 C/C++ 代码，作为 offload 加速器辅助 Host

---

## 参考

- Shamrock GitHub: https://github.com/Shamrock-code/Shamrock
- Shamrock 文档: https://shamrock-code.github.io/Shamrock/
- AdaptiveCpp GitHub: https://github.com/AdaptiveCpp/AdaptiveCpp
- SYCL 2020 Spec: https://www.khronos.org/registry/SYCL/
- Intel Xeon Phi 文档: `docs/impl/20260520_231500_xeon_phi_offload_guide.md`
- KNC 已验证并行模型: `examples/mic_parallel/README.md`
