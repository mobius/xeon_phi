# PhiBench 仓库分析与 7120P 预期结论

**仓库**: https://github.com/puckbee/PhiBench  
**分析日期**: 2026-05-21  
**目标硬件**: Intel Xeon Phi 7120P (KNC), 61 cores, 244 threads, 16GB GDDR5

---

## 1. 仓库概览

PhiBench 是一个专为 Xeon Phi (KNC) 设计的数据分析 benchmark suite，包含 8 个代表性工作负载：

| # | Workload | 来源 | 并行模型 | 关键优化 |
|---|----------|------|----------|----------|
| 1 | Collaborative Filtering (CF) | 自研 | OpenMP / Cilk | `#pragma simd reduction`, `_mm_malloc` 64B对齐 |
| 2 | Kmeans | 自研 | OpenMP / Cilk | 距离计算向量化 |
| 3 | Naive Bayes | 自研 | OpenMP / Cilk | `__declspec(target(mic))` offload |
| 4 | PageRank | 自研 | OpenMP / Cilk | 600M 大数组，图遍历 |
| 5 | SVM | 开源 | OpenMP / Cilk | 内存对齐版本 (`_aligned`) |
| 6 | PCA | 开源 (farbOpt) | OpenMP / MPI / Phi / CUDA | 多后端框架 |
| 7 | NL-PCA | 开源 (farbOpt) | OpenMP / MPI / Phi / CUDA | 神经网络 PCA |
| 8 | Sort (Bitonic) | 自研 | OpenMP + AVX-512 | **显式 KNC SIMD intrinsics** |

---

## 2. 代码实现深度分析

### 2.1 Sort (Bitonic Sort) — 最"KNC 原生"的 benchmark

**文件**: `sort_exp/program/phi_all_level3.cpp`

**关键特征**:
- 大量使用 KNC AVX-512 intrinsics:
  - `_mm512_min_epi32` / `_mm512_max_epi32`
  - `_mm512_shuffle_epi32` (lane 内重排)
  - `_mm512_permute4f128_epi32` (lane 间重排)
  - `_mm512_mask_min_epi32` / `_mm512_mask_max_epi32` (mask 操作)
  - `_mm512_loadunpacklo_epi32` / `_mm512_loadunpackhi_epi32` (非对齐加载)
- 模板化 bitonic merge network，支持 asc/desc 两种方向
- OpenMP 并行外层循环 (`#pragma omp parallel for`)
- `SEG_NUM` 控制分段数，适合 244 线程并行

**7120P 预期表现**: ⭐⭐⭐⭐⭐ **最佳**
- 这是唯一充分利用 KNC 512-bit SIMD 的 workload
- 整数 min/max/shuffle 是 KNC 的强项（每条指令处理 16 个 int32）
- 内存访问模式规则（merge sort 的顺序访问），GDDR5 带宽利用率高
- **潜在瓶颈**: shuffle/permute 指令吞吐量和 L2 cache 容量（512KB/core）

### 2.2 Collaborative Filtering — 编译器自动向量化

**文件**: `cf_exp/program/cf_openmp.c`

**关键特征**:
- `#pragma simd reduction(+:sum_numerator,sum_denominator_square_m,sum_denominator_square_n)`
- `_mm_malloc(sizeof(double)*USER_COUNT,64)` 64B 对齐内存
- 数据规模: USER_COUNT=384,546, ITEM_COUNT=1,019,318
- ITEM_BLOCK=3500 分块处理，适合 cache blocking
- OpenMP 外层并行 (`#pragma omp parallel for` 在 item pair 上)
- 内层 `#pragma simd` 在 user 维度上向量化

**7120P 预期表现**: ⭐⭐⭐⭐
- `#pragma simd` 会让 ICC 生成 AVX-512 浮点代码（FMA）
- 基于之前峰值测试，FP64 FMA 可达 575 GFLOPS，此 workload 有望达到 30-50% 峰值
- 分块策略 (ITEM_BLOCK=3500) 对 L2 cache 友好
- **潜在瓶颈**:
  - `sqrt()` 和除法无法向量化，成为串行瓶颈
  - k-similarity 插入排序（Top-K 维护）是 O(K) 的串行操作
  - 数据可能超过 16GB GDDR5（需确认是否 sparse）

### 2.3 SVM — 内存对齐敏感

**文件**: `svm_exp/program/svm_openmp.cpp`, `svm_openmp_aligned.cpp`

**关键特征**:
- 提供 **aligned** 和 **non-aligned** 两个版本
- C++ 实现，OpenMP 并行
- 核函数计算密集

**7120P 预期表现**: ⭐⭐⭐⭐ (aligned) / ⭐⭐⭐ (non-aligned)
- Aligned 版本在 KNC 上性能显著更好（非对齐 AVX-512 有严重惩罚）
- 核函数如果是简单的点积，`#pragma simd` 可向量化到接近峰值
- **潜在瓶颈**: 如果核函数包含 exp/tanh 等 transcendental 函数，KNC 的 SVML 库吞吐量有限

### 2.4 PageRank — 内存带宽受限

**文件**: `pagerank_exp/program/pagerank_openmp.c`

**关键特征**:
- `#pragma omp parallel for` 在 edge loading 和 PageRank 迭代上
- `ARRAY_LENGTH = 600,000,000` (600M)
- float (REAL) 类型，内存占用约 2.4GB
- 稀疏矩阵表示（edge list），随机内存访问

**7120P 预期表现**: ⭐⭐⭐
- 数据量 2.4GB 完全 fits 16GB GDDR5
- 但图遍历是 **随机内存访问**，KNC 的 GDDR5 虽然带宽高（352 GB/s 理论），但延迟大
- 没有显式向量化，依赖编译器自动 vectorization
- **潜在瓶颈**:
  - 随机访问导致 L2 miss 率高
  - `do-while` 循环中的条件分支降低 SIMD 效率
  - 收敛判断 `check_result()` 需要 reduction，引入同步开销

### 2.5 Kmeans — 同步开销

**文件**: `kmeans_exp/program/kmeans_openmp.c`

**关键特征**:
- 纯 C 实现，距离计算内联函数
- 每轮迭代: (1) 分配点到最近中心 (2) 重新计算中心
- `MAX_ITERATIONS=100`, `CLUSTER_NUM=1000`

**7120P 预期表现**: ⭐⭐⭐
- 距离计算（减法+平方+累加）可向量化，但代码中没有 `#pragma simd`
- 每轮迭代结束需要全局同步来更新中心点坐标
- 244 线程争用 reduction/atomic 操作
- **潜在瓶颈**:
  - 缺乏显式向量化指令
  - 迭代间依赖（中心点更新后才能下一轮分配）
  - 1000 个 cluster 的 reduction 在 244 线程上扩展性有限

### 2.6 Naive Bayes — LEO offload 语法

**文件**: `naivebayes_exp/program/naive_bayes.c`

**关键特征**:
- 使用了 `__declspec(target(mic))` Intel LEO offload 语法
- `microtime()` 函数标记为 offload target
- 训练阶段: 统计每个属性的条件概率
- 分类阶段: 贝叶斯决策

**7120P 预期表现**: ⭐⭐⭐
- LEO offload 可以自动将代码 offload 到 MIC
- 但概率计算涉及大量条件分支（`if (rating != 0)`）
- 浮点除法和对数（如果用了 log）不是 KNC 强项
- **潜在瓶颈**: 分支预测失败、数据依赖链长

### 2.7 PCA / NL-PCA — farbOpt 多后端框架

**目录**: `pca_exp/program/farbOpt/`, `nlpca_exp/program/farbOpt/`

**关键特征**:
- farbOpt 框架支持多后端: CUDA, Phi (MIC), MPI+Phi, Multi-core, Accelerator
- 目录结构:
  - `pca_phi/` / `nlpca_phi/` — 纯 Phi 版本
  - `pca_mpi_phi/` / `nlpca_mpi_phi/` — MPI + Phi (多卡)
  - `pca_multi/` / `nlpca_multi/` — 多核 CPU 版本
  - `pca_acc/` / `nlpca_acc/` — Accelerator (OpenCL?) 版本
- `train.c` / `pred.c` — 训练和预测入口
- `genFunc.py` — 代码生成

**7120P 预期表现**: ⭐⭐⭐⭐ (如果使用 `pca_phi` 优化版本)
- farbOpt 的 `*_phi` 版本应该包含 KNC 专用优化
- PCA 的核心是矩阵运算（协方差、特征分解），如果链接 MKL 会非常好
- NL-PCA 涉及神经网络前向/反向传播，如果有显式 SIMD 优化会不错
- **潜在瓶颈**:
  - 如果没有 MKL，纯 OpenMP 的矩阵运算效率可能不高
  - NL-PCA 的激活函数（sigmoid/tanh） transcendental 计算受限
  - 单卡 16GB 可能限制大规模矩阵（取决于输入维度）

---

## 3. 编译与部署分析

### 3.1 编译标志推断

从 `run_openmp.sh` 和 `run_cilk.sh` 推断：

```bash
# OpenMP 版本 (MIC 直接运行)
icc -mmic -O3 -openmp -o prog.mic prog_openmp.c

# Cilk 版本
icc -mmic -O3 -lcilkrts -o prog.mic prog_cilk.c

# Sort (C++ with AVX-512)
icpc -mmic -O3 -openmp -o sort.mic phi_all_level3.cpp
```

### 3.2 运行方式

仓库提供两种 MIC 运行方式：
1. **SSH 到 mic0 执行**: `run_on_mic.sh` → `ssh mic0 /tmp/prog.mic`
2. **VTune 分析**: `run_*_vtune_mic_openmp.sh` 使用 Intel VTune Amplifier

这与当前环境完全兼容：
- 容器内 ICC 编译 (`-mmic`)
- `scp`/`micnativeloadex` 部署到 mic0
- 环境变量: `MIC_LD_LIBRARY_PATH` 已配置

### 3.3 数据准备

| Workload | 数据需求 | 大小估计 | 备注 |
|----------|----------|----------|------|
| CF | `r1.train.raw`, `r1.test` | ~1GB+ | MovieLens 风格评分数据 |
| Kmeans | 命令行输入文件 | 可配置 | `# num rows= N num columns= M` |
| PageRank | Edge list | ~2.4GB | 600M array, float 类型 |
| Naive Bayes | 训练/分类数据集 | 可配置 | `linen propertyn classn` 格式 |
| SVM | LibSVM 格式 | 可配置 | 开源 SVM 输入 |
| PCA/NL-PCA | farbOpt 数据格式 | 可配置 | 需用 genFunc.py 生成 |
| Sort | 随机/文件输入 | 可配置 | 整数数组 |

**16GB GDDR5 限制**: PageRank (2.4GB) 和 CF (可能 >1GB) 需要确认是否 fit。如果 CF 的 rating 矩阵是 dense float (384K × 1M × 4B ≈ 1.47TB)，不可能 fit；但分块处理 (ITEM_BLOCK=3500) 暗示它是按块加载的，内存中同时存在的只有 2 个 block (~26MB)。

---

## 4. 预期性能排名（7120P 单卡）

基于代码分析 + 当前环境基准数据：

| 排名 | Workload | 预期效率 | 核心理由 |
|------|----------|----------|----------|
| 🥇 1 | **Sort** | 60-80% 峰值 | 显式 AVX-512 + 规则内存访问 |
| 🥈 2 | **CF** | 30-50% 峰值 | `#pragma simd` FMA + 分块 cache友好 |
| 🥈 3 | **SVM (aligned)** | 25-40% 峰值 | 对齐内存 + 可向量化核函数 |
| 🥉 4 | **PCA** | 20-35% 峰值 | farbOpt phi 版应含优化，但依赖 MKL |
| 🥉 5 | **NL-PCA** | 15-30% 峰值 | 神经网络计算，transcendental 受限 |
| 6 | **Kmeans** | 10-25% 峰值 | 无显式 SIMD，同步开销大 |
| 7 | **PageRank** | 10-20% 峰值 | 随机访问，L2 miss 高，无向量化 |
| 8 | **Naive Bayes** | 5-15% 峰值 | 分支密集，LEO offload 额外开销 |

> 注: "峰值"指对应数据类型的理论峰值。Sort 是 int32 操作，无标准 FLOPS 定义，效率以 "每周期处理元素数 / 理论最大值" 估算。

---

## 5. 与当前基准对比

当前已验证的 7120P 峰值测试：

| Test | 实测 | 理论 | 效率 |
|------|------|------|------|
| FP64 FMA | 575 GFLOPS | 1,208 GFLOPS | 47.6% |
| FP32 FMA | 1,170 GFLOPS | 2,416 GFLOPS | 48.4% |
| STREAM Copy | 157 GB/s | 352 GB/s | 44.7% |
| DGEMM 2048 | 63 GFLOPS | 1,208 GFLOPS | 5.2% |

**预期对比**:
- **Sort** 应接近 STREAM 带宽效率 (~40-45%)，因为 shuffle/permute 是内存/寄存器混合操作
- **CF** 的 float 内积计算可能接近 FP32 FMA 的 30-40%（~350-470 GFLOPS）
- **PageRank** 受限于随机访问带宽，可能远低于 STREAM（~50-80 GB/s 有效带宽）
- **SVM** 如果核函数是简单点积，可能接近 FP64 FMA 的 20-30%

---

## 6. 实验建议

### 6.1 编译建议

```bash
# 通用 OpenMP MIC 编译
icc -mmic -O3 -openmp -vec-threshold0 -o prog.mic prog.c

# Sort (C++ with AVX-512)
icpc -mmic -O3 -openmp -o sort.mic phi_all_level3.cpp

# SVM aligned 版本 (推荐)
icpc -mmic -O3 -openmp -o svm.mic svm_openmp_aligned.cpp

# CF (启用报告查看向量化情况)
icc -mmic -O3 -openmp -qopt-report=5 -qopt-report-phase=vec -o cf.mic cf_openmp.c
```

### 6.2 运行建议

```bash
# 部署到 mic0
micnativeloadex ./sort.mic -d 0 -e "KMP_AFFINITY=compact"

# 或通过 SSH
scp sort.mic mic0:/tmp/
ssh mic0 "export KMP_AFFINITY=compact; export OMP_NUM_THREADS=244; /tmp/sort.mic"
```

### 6.3 调优方向

| Workload | 调优优先级 |
|----------|-----------|
| Sort | 已高度优化，关注 `SEG_NUM` 与线程数匹配 |
| CF | 考虑将 `sqrt` 和除法移出内循环，或用近似算法 |
| PageRank | 尝试 CSR 格式 + 预取，减少随机访问 |
| Kmeans | 添加 `#pragma simd` 到距离计算循环 |
| SVM | 始终使用 aligned 版本，避免非对齐内存 |
| PCA/NL-PCA | 优先使用 `*_phi` 目录中的实现 |

---

## 7. 风险与限制

1. **GDDR5 容量**: 16GB 对 CF 的 full dense 矩阵不够，但分块处理应该可以 work
2. **单核性能**: KNC 1.238 GHz 频率低，串行部分（如 CF 的 Top-K 插入）会成为显著瓶颈
3. **VTune 依赖**: 仓库很多脚本依赖 VTune (`run_*_vtune_mic_*.sh`)，当前环境可能未安装
4. **MPI 版本**: `pca_mpi_phi` / `nlpca_mpi_phi` 需要 MPI 环境，单卡无需启用
5. **数据缺失**: 仓库似乎不包含输入数据集，需要自行准备或从原始来源获取

---

## 8. 总结

PhiBench 是一个**针对 KNC 设计良好**的 benchmark suite，特别是 **Sort (Bitonic)** 和 **CF** 两个 workload 充分利用了 KNC 的 AVX-512 和 OpenMP 能力。

在当前 7120P 环境下：
- **Sort 预期表现最佳**（显式 SIMD + 高并行）
- **CF 和 SVM (aligned) 次之**（编译器自动向量化 + 对齐内存）
- **PageRank 和 Naive Bayes 预期最差**（随机访问/分支密集）
- **PCA/NL-PCA 取决于 farbOpt phi 版本的实现质量**

所有 workload 都可以在当前 ICC 16.0 + MPSS 3.8.6 环境中编译运行，运行方式（`micnativeloadex` 或 SSH）与现有工具链完全兼容。
