# Intel PSXE 2016 (icc -mmic) 评估 — 完成

> 时间: 2026-05-20 04:50
> 状态: 验证完成, ICC 16.0 可用

## 安装

- 包: parallel_studio_xe_2016.tgz (3.8 GB)
- License: parallel_studio.lic
- 容器: CentOS 7 (podman centos7-phi-dev)
- 安装: silent.cfg 静默安装 → /opt/intel/

## 验证结果

| 功能 | 状态 | 说明 |
|------|------|------|
| _mm512_* intrinsics | ✅ | 编译通过, 生成打包 FMA |
| 自动向量化 | ⚠️ | 需要 -restrict 标志验证 |
| 交叉编译 | ✅ | scp → ssh 部署 |
| 运行时 | ✅ | segfault 问题已解决 (posix_memalign) |

## 性能

SAXPY 32M (61 threads):

| 方法 | 带宽 | GFLOPS |
|------|------|--------|
| GCC x87 标量 | 7.9 GB/s | 0.66 |
| GCC 内联汇编 | 26.1 GB/s | 2.18 |
| ICC intrinsics | 27.3 GB/s | 2.27 |

温度: 41°C

## 使用方法

见 docs/icc-usage.md

## 限制

- ICC 需要 CentOS 7 容器 (glibc 2.17)
- 需要 source environment-setup-k1om-mpss-linux
- 需要 posix_memalign 替代 aligned_alloc (k1om libc 缺失)
