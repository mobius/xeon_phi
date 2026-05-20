# Xeon Phi 7120P 峰值性能测试实现记录

> 实现时间: 2026-05-20
> 目标: 使用 ICC + KNC intrinsics 编写 4 组峰值性能测试案例

---

## 1. 已交付文件

| 文件 | 说明 | 编译命令 |
|------|------|---------|
| `phi_stream_bench.c` | STREAM-like 内存带宽测试 (Copy/Scale/Add/Triad) | `icc -mmic -O3 -openmp` |
| `phi_peak_fp64.c` | FP64 FMA 峰值计算测试 | `icc -mmic -O3 -openmp` |
| `phi_peak_fp32.c` | FP32 FMA 峰值计算测试 | `icc -mmic -O3 -openmp` |
| `phi_peak_dgemm.c` | 分块矩阵乘法 DGEMM (2048×2048) | `icc -mmic -O3 -openmp` |
| `Makefile.peak` | Makefile 编译规则 | `make -f Makefile.peak` |
| `build_peak_tests.sh` | 一键编译脚本 | `bash build_peak_tests.sh` |

---

## 2. 各测试设计要点

### 2.1 phi_stream_bench.c

- **数据规模**: 64M doubles × 3 数组 = 1.5GB，远超 30.5MB L2 总缓存，确保测试内存带宽而非缓存
- **对齐**: 使用 `posix_memalign(64)` 保证 64 字节对齐，匹配 KNC SIMD 宽度
- **并行**: `#pragma omp parallel for` 自动使用 244 线程
- **测量**: 20 次迭代，丢弃第 1 次（warmup），取 best/average/max
- **输出**: MB/s 和 GB/s 两种单位，直接与理论 352 GB/s 对比

### 2.2 phi_peak_fp64.c

- **L2 驻留**: 工作集仅 2 × 8 doubles = 128 字节，完全在 L1/L2 内，消除内存瓶颈
- **KNC Intrinsics**: `__m512d` + `_mm512_fmadd_pd`，每次处理 8 个 double
- **依赖链消除**: 使用 16 个独立累加器 (`s0` ~ `s15`)，避免流水线停滞
- **FLOP 计算**: 每迭代每线程 = 16 FMA × 8 elements × 2 FLOP = 256 FLOP
- **防优化**: 最终将所有累加器归约并存储到数组，防止编译器删除死代码

### 2.3 phi_peak_fp32.c

- 与 FP64 结构完全一致，改用 `__m512` + `_mm512_fmadd_ps`
- 每指令处理 16 个 float（FP64 的 2 倍），理论峰值 2.416 TFLOPS

### 2.4 phi_peak_dgemm.c

- **矩阵规模**: 2048×2048，标准 HPC 测试规模
- **分块策略**: BLOCK=64，每个块约 64×64×8B×3 = 96KB，适配 512KB L2
- **向量化**: 内层 j 循环一次处理 32 列（4 个 `__m512d`），使用 `_mm512_fmadd_pd`
- **循环顺序**: i-k-j，k 维度在内层，j 向量化，A 的元素广播到 `__m512d`
- **OpenMP**: `#pragma omp parallel for collapse(2) schedule(dynamic)` 并行外层 i-j 块

---

## 3. 编译与部署流程

```bash
# 进入容器
podman exec -it centos7-phi-dev bash

# 设置环境
source /opt/mpss/3.8.6/environment-setup-k1om-mpss-linux
export PATH=/opt/intel/bin:$PATH

# 编译（方式一：脚本）
bash build_peak_tests.sh

# 编译（方式二：Makefile）
make -f Makefile.peak

# 部署到 Xeon Phi 卡
scp phi_stream_bench.mic mic0:/tmp/
scp phi_peak_fp64.mic mic0:/tmp/
scp phi_peak_fp32.mic mic0:/tmp/
scp phi_peak_dgemm.mic mic0:/tmp/

# 运行
ssh mic0 "cd /tmp && for f in phi_*.mic; do echo '=== Running' \$f '==='; ./\$f; done"
```

---

## 4. 预期性能结果

| 测试项 | 理论峰值 | 实际预期 | 判断标准 |
|--------|---------|---------|---------|
| STREAM Copy | 352 GB/s | ~300-340 GB/s | >250 GB/s 为通过 |
| FP64 FMA | 1.208 TFLOPS | ~1.0-1.15 TFLOPS | >800 GFLOPS 为通过 |
| FP32 FMA | 2.416 TFLOPS | ~2.0-2.3 TFLOPS | >1600 GFLOPS 为通过 |
| DGEMM 2048 | 1.208 TFLOPS | ~600-900 GFLOPS | >400 GFLOPS 为通过 |

---

## 5. 注意事项

1. **散热监控**: 7120P 为 300W 被动散热，满载测试时需监控温度
   ```bash
   ssh mic0 cat /sys/class/thermal/thermal_zone0/temp
   ```
   若 > 85000 (85°C)，会出现 thermal throttle，结果不具参考价值。

2. **线程数**: 代码使用 `omp_get_max_threads()` 自动获取线程数，在 7120P 上应为 244 (61×4)。若只想测物理核心性能，可设置 `OMP_NUM_THREADS=61`。

3. **编译器版本**: ICC 16.0 的 KNC intrinsics 语法与 AVX-512 类似但略有差异，已确认 `_mm512_fmadd_pd/ps` 可用。

4. **内存限制**: 16GB GDDR5 中约有 12GB+ 可用给用户空间，STREAM 测试的 1.5GB 完全安全。

---

## 6. 迭代记录

| 时间 | 修改内容 |
|------|---------|
| 2026-05-20 05:20 | 初始版本，完成 4 个测试文件 + Makefile + 编译脚本 |
| 2026-05-20 05:22 | 修复 phi_stream_bench.c: 静态大数组改为 posix_memalign 动态分配 |
| 2026-05-20 05:29 | 修复所有测试文件: `_mm_malloc` 改为 `posix_memalign`（k1om libc 兼容性）|
