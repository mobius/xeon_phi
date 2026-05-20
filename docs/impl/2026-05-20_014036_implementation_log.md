# 实施日志 (更新)

> 时间: 2026-05-20 01:40
> 阶段: Phase 1-3 完成, Phase 4 向量化验证完成

## 环境

- Host: Rocky Linux 8.10, kernel 4.18.0-553.124
- Phi: Xeon Phi 7120P, mic0 online
- VE: 3x NEC VE 1.0 正常

## 工具链

| 编译器 | 路径 | 用途 |
|--------|------|------|
| SDK GCC 5.1.1 | /opt/mpss/.../k1om-mpss-linux-gcc | 推荐日常使用 |
| 自编译 GCC 5.1.1 | ~/Work/gcc-5.1.1-knc/install-container/bin/k1om-mpss-linux-gcc | apc-llc 源码编译 |
| ComposerXE compat | /usr/linux-k1om-4.7/bin/x86_64-k1om-linux-gcc | ICC 兼容适配 |

所有版本都支持内联汇编，都不支持通用 intrinsics。

## 性能基准

| 版本 | 耗时 | 带宽 | GFLOPS | 温度 |
|------|------|------|--------|------|
| 标量 x87 SAXPY | 0.102s | 7.90 GB/s | 0.66 | 48C |
| IMCI 内联汇编 SAXPY | 0.031s | 26.11 GB/s | 2.18 | 41C |
| 标量 MatMul 2048 | 7.04s | - | 2.44 | 48C |

IMCI 向量化提升 3.3x。

## 代码

- saxpy_kernel.c: IMCI 内联汇编 kernel
- saxpy_bench.c: pthread 基准测试
- matmul_bench.c: 矩阵乘法

## 已知限制

- 网络持久化 (重启需手动 ip link)
- 通用 _mm512_* intrinsics 不可用
- GCC 自动向量化不可用

## 下一步

- Intel PSXE 2016 (icc -mmic) 容器方案评估
- 循环展开优化内联汇编
