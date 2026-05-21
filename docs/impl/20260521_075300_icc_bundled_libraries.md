# ICC 16.0 容器内捆绑库清单

**时间**: 2026-05-21 07:53:00 -0400
**容器**: centos7-phi-dev
**总安装大小**: 8.4 GB

---

## 核心编译器

| 组件 | 版本 | 说明 |
|------|------|------|
| icc / icpc | 16.0.0 (20150815) | C/C++ 编译器 |
| ifort | 16.0.0 (20150815) | Fortran 编译器 |
| Cilk Plus | 内嵌 | `libcilkrts.so`，MIC 版本可用 |
| OpenMP | 内嵌 | `libiomp5.so`，MIC 版本可用 |

## 数学与性能库

### 1. MKL 11.3.0 — Math Kernel Library
- **Host 库**: BLAS、LAPACK、ScaLAPACK、FFT、VML、BLACS
- **MIC 库**: 完整 (`intel64_lin_mic/`，30 个库文件)
- **线程后端**: Intel Thread (`libmkl_intel_thread`) + TBB Thread (`libmkl_tbb_thread`)
- **Fortran 接口**: `ilp64` / `lp64` 双 ABI
- **关键发现**: MIC 上支持 MKL TBB 线程后端，可配合 TBB 4.4 使用

### 2. TBB 4.4 — Threading Building Blocks
- **Host 库**: `libtbb.so.2`, `libtbbmalloc.so.2`, `libtbb_preview.so.2`
- **MIC 库**: 完整 (`intel64_lin_mic/`，含 debug/preview/malloc 变体)
- **环境脚本**: `/opt/intel/.../tbb/bin/tbbvars.sh`
- **C++11 支持**: 需加 `-std=c++11`

### 3. IPP 9.0.0 — Integrated Performance Primitives
- **Host 库**: 信号/图像/向量处理原语
- **MIC 库**: 8 个静态库 (`libipp*.a`)，无动态链接版本
- **版本**: 9.0.0 (r47849)

### 4. DAAL — Data Analytics Acceleration Library
- **Host 库**: `libdaal_core.so` (~69MB), `libdaal_thread.so`, `libdaal_sequential.so`
- **MIC 库**: ❌ **无** (`intel64_lin_mic/` 目录不存在)
- **说明**: DAAL 的 KNC 支持不完整，MIC 端不可用

## 并行与通信

### Intel MPI
- **版本**: 捆绑于 PSXE 2016
- **MIC 支持**: ✅ 有 `mic/` 子目录（bin/include/lib）
- **编译器 wrapper**: `mpiicc`（基于 ICC 16.0）

## 性能分析工具

| 工具 | 版本 | 用途 | MIC 支持 |
|------|------|------|----------|
| VTune Amplifier | 2016.1.0.424694 | Hotspot/微架构分析 | ✅ |
| Advisor | 2016.1.0.423501 | 向量化建议 | ✅ |
| Inspector | 2016.1.0.423441 | 内存/线程错误检测 | ✅ |
| ITAC | 9.1.1.017 | MPI Trace Analyzer | ✅ |
| GDB (Intel) | 2016 | 增强调试器 | ✅ |

## MIC (KNC) 支持矩阵

| 库 | Host | MIC | 说明 |
|----|------|-----|------|
| Compiler Runtime | ✅ | ✅ | `liboffload`, `libiomp5`, `libcilkrts` |
| Fortran Runtime | ✅ | ✅ | `libifcore`, `libifcoremt` |
| MKL | ✅ | ✅ | 完整，含 BLACS/ScaLAPACK |
| TBB | ✅ | ✅ | 刚安装，完整 |
| IPP | ✅ | ✅ (静态) | 仅静态库 `.a` |
| DAAL | ✅ | ❌ | MIC 不支持 |
| MPI | ✅ | ✅ | `mic/` 目录存在 |

## 编译使用示例

```bash
source /opt/intel/bin/compilervars.sh intel64
source /opt/intel/compilers_and_libraries_2016.0.109/linux/tbb/bin/tbbvars.sh intel64

# Host 端使用 MKL + TBB
icc -mkl -tbb -o app app.c

# MIC 端使用 MKL
icc -mmic -mkl -o app.mic app.c

# MIC 端使用 MKL + TBB
icc -mmic -mkl -tbb -o app.mic app.c
```

## 关键限制

1. **DAAL 无 MIC 版**: 数据分析算法库只能在 host 端使用
2. **IPP MIC 只有静态库**: MIC 上需静态链接 (`-lippcore -lippvm ...`)
3. **工具链版本锁定**: 所有组件均为 2016 年版本，无更新通道
