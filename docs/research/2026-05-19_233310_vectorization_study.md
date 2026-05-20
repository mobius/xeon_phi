# Xeon Phi 7120P 向量化方案研究

> 时间: 2026-05-19 23:33
> 状态: 研究完成 — 标量可用，自动向量化受限

## 结论

**GCC 5.1.1 交叉编译器无法有效自动向量化 KNC (k1om) 代码。** 当前标量性能约 2.4 GFLOPS FP64 (0.2% 峰值)，SAXPY 约 7.9 GB/s (2.2% 峰值)。

## 根因分析

### 1. KNC 指令集限制

| 指令类别 | KNC 支持 | 说明 |
|----------|---------|------|
| SSE/SSE2/SSE3 | ❌ | 不存在于 k1om |
| AVX/AVX2 | ❌ | 不存在于 k1om |
| AVX-512F | ✅ | IMCI (Initial Many Core Instructions) |
| AVX-512ER | ✅ | Exponential & Reciprocal |
| AVX-512PF | ✅ | Prefetch |
| AVX-512CD | ✅ | Conflict Detection |
| AVX-512BW/DQ/VL | ❌ | KNL+ 才支持 |
| x87 FPU | ✅ | 标量回退，极慢 |

### 2. GCC 5.1.1 编译器行为

- `-mavx512f` + `-ftree-vectorize`: 生成混合 SSE+AVX-512 代码 → 汇编错误 (SSE 在 k1om 不存在)
- `-mavx512f` + `-mno-sse`: 退化到 x87 标量浮点，无向量化
- `_mm512_*` intrinsics: `target specific option mismatch` — 需要 SSE 基础运行时
- 无 KNC 专用 `-march=knc` 选项（GCC 5.1 支持 `-march=knl` 而非 `knc`）

### 3. 编译结果对比

| 测试 | 选项 | 结果 |
|------|------|------|
| SAXPY 32M | `-O3` (标量) | 7.90 GB/s |
| SAXPY 32M | `-O3 -mavx512f -ftree-vectorize -mno-sse` | 8.07 GB/s (+2%) |
| MatMul 2K | `-O3` (标量) | 2.44 GFLOPS |
| MatMul 2K | `-O3 -mavx512f -ftree-vectorize -mno-sse` | 2.43 GFLOPS (相同) |

## 可行方案

### 方案 A: Intel Compiler (icc) ⭐ 推荐

Intel Parallel Studio XE 的 `icc -mmic` 对 KNC 有完整支持，包括：
- 自动向量化生成 IMCI 指令
- MKL for KNC (预优化 BLAS/LAPACK)
- Offload 编程模型

⚠️ 需要商业许可证（或教育版）。tar 包中无 icc。

### 方案 B: 手写内联汇编

直接编写 KNC IMCI 汇编指令（`vfmadd213pd`、`vbroadcastsd` 等）。

难度：高。需要熟悉 IMCI 指令集和 k1om ABI。

### 方案 C: 单独汇编文件

编写纯 .s 汇编文件，用 k1om 汇编器编译，C 代码调用。

### 方案 D: 接受标量性能

KNC 的 61 核心 × 标量性能 = 约 2-3 GFLOPS FP64，对于不规则访存任务仍有价值（x86 缓存的优势）。

对标 NEC VE 1.0: VE 的 ~1.3 TFLOPS 来自自动向量化，KNC 的 ~2.4 GFLOPS 来自标量 — 差距约 500x。

### 方案 E: 升级 GCC

从 `mpss-src-3.8.6.tar` 编译 GCC 5.1.1+mpss 源码（可能包含 KNC 补丁但未被我们的预编译版本使用）。

## 推荐

短期内接受标量性能用于开发验证。如需高性能计算，优先使用 3x NEC VE 卡（自动向量化 ~3.9 TFLOPS）。Xeon Phi 7120P 适合：I/O 密集型、不规则访存、x86 兼容性验证。

## 温度验证

- 温度节点: `/sys/class/micras/temp`
- 闲置: ~42°C
- 满载 (2.4 GFLOPS): ~48°C
- 阈值: 85°C 降频, 95°C 关机
- 散热状态: ✅ 充裕（被动散热在低负载下工作良好）
