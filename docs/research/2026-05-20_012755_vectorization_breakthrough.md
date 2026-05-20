# Xeon Phi 7120P 向量化方案研究 (突破)

> 时间: 2026-05-20 01:27
> 状态: IMCI 内联汇编验证成功

## 突破

通过手写 IMCI 内联汇编实现 3.3x 带宽提升。

## 结果

| 版本 | 耗时 | 带宽 | GFLOPS | 温度 |
|------|------|------|--------|------|
| 标量 (x87) | 0.102s | 7.90 GB/s | 0.66 | 48C |
| IMCI 内联汇编 | 0.031s | 26.11 GB/s | 2.18 | 41C |

## 工具链

编译 apc-llc/gcc-5.1.1-knc 于 CentOS 7 容器:
- GCC 5.1.1, target=k1om-mpss-linux
- 参考: https://www.aidancrowther.com/project/xeonphi
- Dockerfile: https://www.aidancrowther.com/files/Dockerfile

## 限制与方案

- _mm512_* intrinsics 仍有 target mismatch (avx512fintrin.h 需 SSE)
- binutils 2.22 不支持 %xmm 寄存器
- 解决: 手写内联汇编, 纯 zmm + pd 指令

```c
__asm__ ("vbroadcastsd %[scalar], %%zmm1" : : [scalar] "m" (s) : "zmm1");
__asm__ __volatile__ (
    "vmovapd 0(%[aptr]), %%zmm0\n\t"
    "vfmadd213pd 0(%[bptr]), %%zmm1, %%zmm0\n\t"
    "vmovapd %%zmm0, 0(%[aptr])"
    : : [aptr] "r" (a+i), [bptr] "r" (b+i) : "zmm0", "memory");
```

## 后续

- 循环展开减少 asm volatile 开销
- 预取指令 (vprefetch)
- 矩阵乘法向量化
