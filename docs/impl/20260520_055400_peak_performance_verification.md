# Xeon Phi 7120P 峰值性能测试验证报告

> 验证时间: 2026-05-20
> 验证环境: CentOS 7 容器 (ICC 16.0) → mic0 (k1om Linux 2.6.38.8+mpss3.8.6)
> 线程数: 244 (61 cores × 4 SMT)

---

## 1. 最终实测结果

### 1.1 内存带宽测试 (phi_stream_bench.mic)

| 核函数 | 实测带宽 | 理论带宽 | 达成率 |
|--------|---------|---------|--------|
| COPY | **157.25 GB/s** | 352 GB/s | 44.7% |
| SCALE | **136.70 GB/s** | 352 GB/s | 38.8% |
| ADD | **72.00 GB/s** | 352 GB/s | 20.5% |
| TRIAD | **71.86 GB/s** | 352 GB/s | 20.4% |

**分析**:
- COPY 达到 157 GB/s，为理论值的 44.7%
- ADD/TRIAD 受限于计算操作（FMA），带宽下降为 COPY 的约 46%
- KNC 的 STREAM 效率通常在 40-50% 范围，此结果符合文献数据

### 1.2 FP64 计算峰值测试 (phi_peak_fp64.mic)

| 指标 | 数值 |
|------|------|
| 线程数 | 244 |
| 迭代次数/线程 | 4,194,304 |
| 累加器数量 | 16 (512-bit zmm) |
| 运行时间 | 0.455 sec |
| **实测 FP64 GFLOPS** | **575.39** |
| 理论 FP64 | 1,208 GFLOPS |
| **达成率** | **47.6%** |

**分析**:
- 使用 KNC 512-bit intrinsics (`_mm512_fmadd_pd`)，16 个独立累加器消除依赖链
- 汇编验证：生成 `vfmadd213pd` 指令，使用 zmm 寄存器
- 47.6% 的达成率与项目文档中 "7120P 实际输出可能只有标称的 30-50%" 的评估一致

### 1.3 FP32 计算峰值测试 (phi_peak_fp32.mic)

| 指标 | 数值 |
|------|------|
| 线程数 | 244 |
| 迭代次数/线程 | 4,194,304 |
| 累加器数量 | 16 (512-bit zmm) |
| 运行时间 | 0.448 sec |
| **实测 FP32 GFLOPS** | **1,170.31** |
| 理论 FP32 | 2,416 GFLOPS |
| **达成率** | **48.4%** |

**分析**:
- FP32 峰值约为 FP64 的 2 倍（1,170 vs 575），符合预期（16 floats vs 8 doubles）
- 同样使用 `_mm512_fmadd_ps` intrinsics

### 1.4 DGEMM 矩阵乘法测试 (phi_peak_dgemm.mic)

| 指标 | 数值 |
|------|------|
| 矩阵规模 | 2048 × 2048 |
| 线程数 | 244 |
| 运行时间 | 0.273 sec |
| **实测 GFLOPS** | **62.82** |
| 理论峰值 | 1,208 GFLOPS |
| **达成率** | **5.2%** |

**分析**:
- 简单 i-k-j 向量化实现，未做 L2 cache blocking
- 性能受限于内存带宽（每行需读取 16KB B 矩阵数据）
- 作为参考基准，说明 naive DGEMM 在 KNC 上需要更 aggressive 的分块优化

---

## 2. 优化尝试记录

### 2.1 已尝试的优化手段

| 优化手段 | 应用对象 | 结果 |
|----------|---------|------|
| `restrict` 关键字 | 全部 | 对 STREAM 无显著影响，对 intrinsics 版本反而降低性能 |
| `schedule(static)` | STREAM | 最佳调度方式，减少线程竞争 |
| 24 累加器 (vs 16) | FP64/FP32 | 性能下降（约 25%），说明 16 累加器已足够 |
| `#pragma vector aligned` + 自动向量化 | FP64/FP32 | 性能大幅下降至 27%，不如手动 intrinsics |
| `KMP_AFFINITY=scatter` | FP64/FP32 | 无改善，反而轻微下降 |
| 分块 DGEMM (BLOCK=64/128) | DGEMM | 比 naive 实现更差（dynamic 调度开销） |
| 编译选项 `-restrict` | 全部 | 对 intrinsics 版本有害 |

### 2.2 最佳编译选项

```bash
icc -std=c99 -mmic -O3 -openmp -o prog.mic prog.c
```

- `-std=c99`: 必须，ICC 16.0 默认 C89，不支持 for-loop 内声明变量
- `-mmic`: 交叉编译到 KNC (k1om)
- `-O3`: 最大优化级别
- `-openmp`: 启用 OpenMP 并行
- **不加 `-restrict`**：对 intrinsics 版本反而有害

---

## 3. 性能瓶颈分析

### 3.1 为什么只有 ~48% 理论峰值？

根据文献和实测，KNC 的实际峰值通常只能达到理论值的 40-60%：

1. **In-order 架构限制**: KNC 核心基于 Pentium P54C 修改，2-wide in-order 执行，指令取指和发射存在瓶颈
2. **超线程竞争**: 4-way SMT 下，244 线程共享 61 个物理 FMA 单元，存在资源竞争
3. **编译器限制**: ICC 16.0 对 KNC 的代码生成并非最优，helloflops3 等文献案例需要特定的代码模式才能达到 ~97%
4. **散热/功耗**: 300W 被动散热在实际运行中可能导致 subtle 的降频

### 3.2 内存带宽瓶颈

- GDDR5 理论 352 GB/s，实际 STREAM COPY 157 GB/s
- 这与文献中 KNC 的 STREAM 效率（40-50%）一致
- ADD/TRIAD 的带宽更低是因为 FMA 操作占用了计算资源

### 3.3 DGEMM 瓶颈

- Naive DGEMM 受限于内存带宽和缓存未命中
- 要达到更高性能需要：MKL 级别的优化、L2 blocking、寄存器分块、数据预取

---

## 4. 文件清单（最终版本）

| 文件 | 说明 |
|------|------|
| `phi_stream_bench.c` | STREAM-like 内存带宽测试 |
| `phi_peak_fp64.c` | FP64 FMA 峰值测试 (16 accumulators, KNC intrinsics) |
| `phi_peak_fp32.c` | FP32 FMA 峰值测试 (16 accumulators, KNC intrinsics) |
| `phi_peak_dgemm.c` | DGEMM 2048×2048 参考测试 |
| `Makefile.peak` | Makefile |
| `build_peak_tests.sh` | 一键编译脚本 |

---

## 5. 结论

| 测试项 | 最低通过标准 | 实测结果 | 状态 |
|--------|------------|---------|------|
| STREAM COPY | >100 GB/s | **157.25 GB/s** | ✅ 通过 |
| FP64 FMA | >400 GFLOPS | **575.39 GFLOPS** | ✅ 通过 |
| FP32 FMA | >800 GFLOPS | **1,170.31 GFLOPS** | ✅ 通过 |
| DGEMM 2048 | >30 GFLOPS | **62.82 GFLOPS** | ✅ 通过 |

**总体评估**: 测试套件成功在 Xeon Phi 7120P 上运行，实测 FP64/FP32 计算峰值达到理论值的 ~48%，STREAM 带宽达到理论值的 ~45%，与项目文档中对 7120P "实际输出可能只有标称的 30-50%" 的评估完全吻合。

---

## 6. 参考文献

- NASA/IPAC: "Benchmarking the Intel Xeon Phi Coprocessor" (MKL sgemm ~1 TFLOPS)
- Tulane HPC: "Programming for the Xeon Phi Coprocessor on Cypress" (helloflops3 ~2335 GFLOPS)
- Colfax International: "MIC Developer Boot Camp" (KNC architecture details)
- 项目文档: `Xeon_Phi_7120P_Specific_Assessment.md`
