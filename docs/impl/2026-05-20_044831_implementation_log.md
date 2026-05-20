# 实施日志 (ICC 更新)

> 时间: 2026-05-20 04:50
> 阶段: Phase 5 — ICC -mmic 验证完成

## 环境

- Host: Rocky 8.10, podman 4.9.4
- 容器: CentOS 7 (centos7-phi-dev), ICC 16.0
- Phi: Xeon Phi 7120P, mic0 online, 41°C
- VE: 3x NEC VE 1.0 正常

## 工具链矩阵

| 编译器 | 路径 | 内联汇编 | intrinsics | 自动向量化 |
|--------|------|---------|-----------|-----------|
| SDK GCC 5.1.1 | /opt/mpss/.../k1om-mpss-linux-gcc | ✅ | ❌ | ❌ |
| 自编 GCC 5.1.1 | ~/Work/gcc-5.1.1-knc/install-container/bin/ | ✅ | ❌ | ❌ |
| ComposerXE compat | /usr/linux-k1om-4.7/bin/ | ✅ | ❌ | ❌ |
| **ICC 16.0** | **/opt/intel/bin/icc** | ✅ | **✅** | ⚠️ |

## 性能基准 (SAXPY 32M, 61 threads)

| 版本 | 耗时 | 带宽 | GFLOPS | 温度 |
|------|------|------|--------|------|
| GCC x87 标量 | 0.102s | 7.90 GB/s | 0.66 | 48C |
| GCC 内联汇编 | 0.031s | 26.11 GB/s | 2.18 | 41C |
| ICC intrinsics | 0.030s | 27.27 GB/s | 2.27 | 41C |

## 代码

- saxpy_kernel.c: GCC 内联汇编版本
- saxpy_bench.c: 基准测试 (posix_memalign)
- docs/icc-usage.md: ICC 使用指南

## 限制

- 网络持久化 (重启需手动 ip link)
- 需要 posix_memalign 替代 aligned_alloc

## 下一步

- ICC 自动向量化验证 (-restrict)
- MKL for KNC
- 多卡扩展
