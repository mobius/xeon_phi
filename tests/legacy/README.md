# Legacy / Early Experiments

此目录存放项目早期的实验性代码，用于验证 Xeon Phi 7120P 的基本功能和交叉编译流程。

> **状态**: 历史代码，不再维护。仅作参考。

## 文件说明

| 文件 | 描述 | 状态 |
|------|------|------|
| `saxpy_bench.c` | SAXPY (Single-precision A*X Plus Y) 基准测试，使用 pthreads | 历史 |
| `saxpy_kernel.c` | SAXPY 内核函数，供 saxpy_bench 调用 | 历史 |
| `matmul_bench.c` | 矩阵乘法基准测试，使用 pthreads | 历史 |

## 技术细节

### saxpy_bench.c

- **并行方式**: POSIX Threads (`pthread_create`/`pthread_join`)
- **线程数**: 硬编码为 8 (未利用 KNC 的 244 线程)
- **编译**: `icc -mmic -O2 -o saxpy_bench saxpy_bench.c -lpthread`
- **备注**: 早期验证 pthreads 在 MIC 上可用性的代码

### matmul_bench.c

- **并行方式**: POSIX Threads
- **线程数**: 硬编码为 8
- **算法**: 基础三重循环矩阵乘法 (O(n³))
- **编译**: `icc -mmic -O2 -o matmul_bench matmul_bench.c -lpthread`
- **备注**: 早期验证 MIC 计算能力的代码

## 与现代代码的关系

这些早期代码已被以下正式测试/示例取代：

| 旧代码 | 替代方案 | 说明 |
|--------|---------|------|
| `saxpy_bench.c` (pthreads, 8线程) | `tests/perf/phi_peak_fp64.c` | 使用 OpenMP + AVX-512 达到 244 线程峰值 |
| `matmul_bench.c` (pthreads, 8线程) | `tests/perf/phi_peak_dgemm.c` | 使用 OpenMP + 向量化，效率更高 |
| 手动 pthreads 管理 | `examples/mic_parallel/01_pthreads.c` | 规范的 244 线程 pthreads 示例 |

## 保留原因

1. **历史记录**: 展示项目从早期实验到完整测试套件的演进
2. **参考对比**: 对比 pthreads vs OpenMP、8线程 vs 244 线程的性能差异
3. **教学价值**: 最简单的 MIC 交叉编译示例，无需复杂环境
