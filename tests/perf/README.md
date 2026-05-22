# Xeon Phi 7120P Peak Performance Benchmark Suite

Xeon Phi 7120P (KNC) 峰值性能基准测试套件。

## 测试项目

| 文件 | 测试内容 | 理论峰值 | 实测记录 |
|------|---------|---------|---------|
| `phi_peak_fp64.c` | FP64 FMA 峰值 (AVX-512) | 1,208 GFLOPS | **575 GFLOPS** (47.6%) |
| `phi_peak_fp32.c` | FP32 FMA 峰值 (AVX-512) | 2,416 GFLOPS | **1,170 GFLOPS** (48.4%) |
| `phi_stream_bench.c` | STREAM 内存带宽 | 352 GB/s | **157 GB/s** (44.7%) |
| `phi_peak_dgemm.c` | DGEMM (OpenMP, naive) | 1,208 GFLOPS | **63 GFLOPS** (5.2%) |

## 环境要求

- Intel ICC 16.0 (with `-mmic` cross-compiler)
- MPSS 3.8.6 (mic0 online)
- 可选: `SINK_LD_LIBRARY_PATH` 指向 MIC 运行时库

## 构建

### 方式 1: Makefile

```bash
cd tests/perf
make -f Makefile.peak
```

### 方式 2: 构建脚本

```bash
cd tests/perf
./build_peak_tests.sh
```

### 方式 3: 手动编译

```bash
cd tests/perf

# FP64 峰值
icc -std=c99 -mmic -O3 -openmp -o phi_peak_fp64.mic phi_peak_fp64.c

# FP32 峰值
icc -std=c99 -mmic -O3 -openmp -o phi_peak_fp32.mic phi_peak_fp32.c

# STREAM 带宽
icc -std=c99 -mmic -O3 -openmp -o phi_stream_bench.mic phi_stream_bench.c

# DGEMM
icc -std=c99 -mmic -O3 -openmp -o phi_peak_dgemm.mic phi_peak_dgemm.c
```

> **注意**: 编译需在 ICC 容器内执行，或确保 host 已安装 ICC 16.0。

## 运行

### 方式 1: micnativeloadex (推荐)

```bash
export SINK_LD_LIBRARY_PATH=/path/to/mic/libs
micnativeloadex ./phi_peak_fp64.mic
micnativeloadex ./phi_peak_fp32.mic
micnativeloadex ./phi_stream_bench.mic
micnativeloadex ./phi_peak_dgemm.mic
```

### 方式 2: SSH 到 mic0

```bash
scp phi_*.mic mic0:/tmp/
ssh mic0 "cd /tmp && for f in phi_*.mic; do echo === Running \$f ===; ./\$f; done"
```

## 输出示例

```
=== phi_peak_fp64.mic ===
Threads: 244
Triad GFLOPS: 575.23

=== phi_stream_bench.mic ===
STREAM Copy: 157.3 GB/s
STREAM Scale: 156.8 GB/s
STREAM Add:   158.1 GB/s
STREAM Triad: 157.5 GB/s
```

## 文件说明

- `phi_peak_fp64.c` — 使用 `_mm512_fmadd_pd` 的 FP64 FMA 峰值测试
- `phi_peak_fp32.c` — 使用 `_mm512_fmadd_ps` 的 FP32 FMA 峰值测试
- `phi_stream_bench.c` — STREAM 基准测试 (Copy/Scale/Add/Triad)
- `phi_peak_dgemm.c` — 基础 DGEMM 实现，用于对比 MKL 性能
- `Makefile.peak` — Makefile 构建规则
- `build_peak_tests.sh` — 一键构建脚本

## 参考

- `docs/impl/20260520_052053_peak_performance_implementation.md`
- `docs/impl/20260520_055400_peak_performance_verification.md`
- `docs/research/20260520_052053_peak_performance_testing_theory.md`
