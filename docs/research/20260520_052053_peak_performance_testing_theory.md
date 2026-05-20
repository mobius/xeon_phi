# Xeon Phi 7120P 峰值性能测试理论研究

> 研究时间: 2026-05-20
> 目标: 设计 ICC + KNC intrinsics 测试案例，验证 7120P 峰值 FP64/FP32/带宽性能

---

## 1. 7120P 理论峰值计算

### 1.1 FP64 (双精度浮点)

| 参数 | 数值 |
|------|------|
| 核心数 | 61 |
| 频率 | 1.238 GHz |
| SIMD 宽度 | 512-bit = 8 × double |
| 每周期 FMA | 2 个 (乘 + 加 合为一个 FMA，但计 2 FLOP) |
| **理论峰值** | 61 × 1.238 × 8 × 2 = **1.208 TFLOPS** |

### 1.2 FP32 (单精度浮点)

| 参数 | 数值 |
|------|------|
| SIMD 宽度 | 512-bit = 16 × float |
| **理论峰值** | 61 × 1.238 × 16 × 2 = **2.416 TFLOPS** |

### 1.3 内存带宽

| 参数 | 数值 |
|------|------|
| GDDR5 容量 | 16 GB |
| 理论带宽 | **~352 GB/s** |
| L2 缓存 | 30.5 MB (512 KB / 核心) |

---

## 2. 测试方法研究

### 2.1 内存带宽测试 (STREAM-like)

STREAM 是测试内存带宽的标准方法，包含四个核函数：

| 核函数 | 操作 | 字节/FLOP | 理论带宽上限 |
|--------|------|-----------|-------------|
| COPY | `a[i] = b[i]` | 16B / 0FLOP | 352 GB/s |
| SCALE | `a[i] = s * b[i]` | 16B / 1FLOP | 352 GB/s |
| ADD | `a[i] = b[i] + c[i]` | 24B / 1FLOP | 352 GB/s |
| TRIAD | `a[i] = b[i] + s * c[i]` | 24B / 2FLOP | 352 GB/s |

对于 Xeon Phi，数组必须 **64 字节对齐**，使用 `_mm_malloc` 或 `posix_memalign`。

### 2.2 计算峰值测试

要达到计算峰值，必须：

1. **数据在 L2 缓存内** - 避免内存带宽瓶颈
2. **连续 FMA 指令** - 填满流水线，无数据依赖
3. **使用 KNC 512-bit intrinsics** - `_mm512_fmadd_pd/_ps`
4. **OpenMP 并行 61 核心** - 每个核心独立计算

#### 依赖链消除技术

如果所有 FMA 都依赖前一个结果（如 `x = x * a + b`），流水线会停滞。需要展开循环，使用多个独立累加器：

```c
__m512d sum0 = _mm512_set1_pd(1.0);
__m512d sum1 = _mm512_set1_pd(1.0);
__m512d sum2 = _mm512_set1_pd(1.0);
__m512d sum3 = _mm512_set1_pd(1.0);

for(int i=0; i<N; i++) {
    sum0 = _mm512_fmadd_pd(sum0, a, b);
    sum1 = _mm512_fmadd_pd(sum1, a, b);
    sum2 = _mm512_fmadd_pd(sum2, a, b);
    sum3 = _mm512_fmadd_pd(sum3, a, b);
}
```

每组 sum 之间无依赖，可并行执行。

#### FMA 吞吐量计算

- 每个 `_mm512_fmadd_pd` = 8 doubles × 2 FLOP = 16 FLOP
- 每个 `_mm512_fmadd_ps` = 16 floats × 2 FLOP = 32 FLOP
- 每核心每周期可发射 1 条 FMA，所以需要足够展开才能打满

### 2.3 矩阵乘法 (DGEMM)

矩阵乘法是实际应用中最重要的计算密集型负载。

- 2048×2048 double 矩阵需 32MB，超过 L2
- 需要分块 (blocking) 策略，将工作集保持在 L2 内
- 分块大小约 64-128 较合适（512KB L2 可容纳 3 个 64×64 double 块 = 96KB）
- 使用 i-k-j 循环顺序，内层 j 向量化

---

## 3. KNC Intrinsics 参考

KNC (Knights Corner) 使用特殊的 512-bit SIMD，ICC 提供 intrinsics：

| 操作 | FP64 | FP32 |
|------|------|------|
| 类型 | `__m512d` | `__m512` |
| broadcast | `_mm512_set1_pd(x)` | `_mm512_set1_ps(x)` |
| load | `_mm512_load_pd(ptr)` | `_mm512_load_ps(ptr)` |
| store | `_mm512_store_pd(ptr, v)` | `_mm512_store_ps(ptr, v)` |
| FMA | `_mm512_fmadd_pd(a,b,c)` | `_mm512_fmadd_ps(a,b,c)` |
| add | `_mm512_add_pd(a,b)` | `_mm512_add_ps(a,b)` |
| mul | `_mm512_mul_pd(a,b)` | `_mm512_mul_ps(a,b)` |

编译：
```bash
icc -mmic -O3 -openmp -o test.mic test.c
```

---

## 4. 测试规模设计

| 测试项 | 数据规模 | 原因 |
|--------|---------|------|
| STREAM | 64M elements (512MB) | 远超 L2，测内存带宽 |
| FP64 峰值 | 小数组在 L2 内，循环 1M 次 | 避免内存瓶颈 |
| FP32 峰值 | 同上 | 同上 |
| DGEMM | 2048×2048 | 标准 HPC 测试规模 |

---

## 5. 预期结果

| 测试 | 理论峰值 | 实际预期 |
|------|---------|---------|
| STREAM Copy | 352 GB/s | ~300-340 GB/s |
| FP64 FMA | 1.208 TFLOPS | ~1.0-1.15 TFLOPS |
| FP32 FMA | 2.416 TFLOPS | ~2.0-2.3 TFLOPS |
| DGEMM 2048 | 1.208 TFLOPS | ~600-900 GFLOPS |

---

## 6. 参考文献

- Intel Xeon Phi Coprocessor Instruction Set Architecture Reference
- STREAM Benchmark: https://www.cs.virginia.edu/stream/
- ICC 16.0 KNC Intrinsics Guide
