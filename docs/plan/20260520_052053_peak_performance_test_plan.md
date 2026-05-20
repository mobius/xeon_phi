# Xeon Phi 7120P 峰值性能测试方案

> 计划时间: 2026-05-20
> 目标: 使用 ICC + KNC intrinsics 编写 4 组峰值性能测试案例

---

## 1. 测试架构

```
┌─────────────────────────────────────────┐
│         phi_peak_test_suite             │
├─────────────────────────────────────────┤
│  1. phi_stream_bench.c                  │
│     └── STREAM-like 内存带宽测试         │
│         (Copy/Scale/Add/Triad)          │
├─────────────────────────────────────────┤
│  2. phi_peak_fp64.c                     │
│     └── FP64 FMA 峰值计算测试            │
│         (L2-resident, 多累加器展开)       │
├─────────────────────────────────────────┤
│  3. phi_peak_fp32.c                     │
│     └── FP32 FMA 峰值计算测试            │
│         (L2-resident, 多累加器展开)       │
├─────────────────────────────────────────┤
│  4. phi_peak_dgemm.c                    │
│     └── 分块矩阵乘法 DGEMM               │
│         (2048x2048, L2 blocking)        │
├─────────────────────────────────────────┤
│  Makefile / build.sh                    │
│  编译: icc -mmic -O3 -openmp            │
└─────────────────────────────────────────┘
```

---

## 2. 文件清单与实现要点

### 2.1 phi_stream_bench.c

**目标**: 测量 GDDR5 实际内存带宽，验证是否接近 352 GB/s

**要点**:
- 数组大小: 64M doubles × 3 数组 = 1.5GB，远大于 L2
- 64 字节对齐分配
- OpenMP `#pragma omp parallel for` 并行 244 线程
- 测量 COPY / SCALE / ADD / TRIAD 四种模式
- 预热后多次运行取平均
- 计算: Bandwidth = (数据量 × 访问次数) / 时间

### 2.2 phi_peak_fp64.c

**目标**: 测量 FP64 计算峰值，验证是否接近 1.208 TFLOPS

**要点**:
- 数组驻留 L2 缓存内（约 256KB 工作集）
- KNC intrinsics: `__m512d`, `_mm512_fmadd_pd`
- 每核心展开 8-16 个独立累加器，消除依赖链
- 每个线程独立计算，无同步
- 运行足够长时间（>1 秒）保证测量精度
- 计算: GFLOPS = (N × ops_per_iter × 61) / 时间

### 2.3 phi_peak_fp32.c

**目标**: 测量 FP32 计算峰值，验证是否接近 2.416 TFLOPS

**要点**:
- 与 FP64 结构相同，使用 `__m512` / `_mm512_fmadd_ps`
- 每指令 16 floats vs 8 doubles，峰值翻倍

### 2.4 phi_peak_dgemm.c

**目标**: 实际计算密集型应用性能参考

**要点**:
- 矩阵规模 2048×2048
- 分块大小 64（适配 512KB L2）
- i-k-j 循环顺序
- 内层 j 循环使用 `_mm512_load_pd` / `_mm512_fmadd_pd` 向量化
- OpenMP 并行外层 i 循环

---

## 3. 编译与部署

```bash
# 进入容器
podman exec -it centos7-phi-dev bash
source /opt/mpss/3.8.6/environment-setup-k1om-mpss-linux
export PATH=/opt/intel/bin:$PATH

# 编译
icc -mmic -O3 -openmp -o phi_stream_bench.mic phi_stream_bench.c
icc -mmic -O3 -openmp -o phi_peak_fp64.mic phi_peak_fp64.c
icc -mmic -O3 -openmp -o phi_peak_fp32.mic phi_peak_fp32.c
icc -mmic -O3 -openmp -o phi_peak_dgemm.mic phi_peak_dgemm.c

# 部署运行
scp *.mic mic0:/tmp/
ssh mic0 "cd /tmp && for f in phi_*.mic; do echo \"=== Running \$f ===\"; ./\$f; done"
```

---

## 4. 验收标准

| 测试项 | 最低通过 | 理想目标 |
|--------|---------|---------|
| STREAM Copy | >250 GB/s | >320 GB/s |
| FP64 FMA | >800 GFLOPS | >1000 GFLOPS |
| FP32 FMA | >1600 GFLOPS | >2000 GFLOPS |
| DGEMM 2048 | >400 GFLOPS | >700 GFLOPS |

---

## 5. 风险与对策

| 风险 | 对策 |
|------|------|
| KNC intrinsics 在 ICC 16.0 中语法差异 | 参考 ICC 文档，使用 `_mm512_*` 系列 |
| 散热导致 thermal throttle | 每次测试后检查温度，温度>85°C暂停 |
| 内存分配失败 (16GB 限制) | 控制数组总大小 < 12GB |
| OpenMP 在 KNC 上行为异常 | 回退到 pthread 方案 |
