# Intel Xeon Phi 7120P (Knights Corner) 开发环境

> Rocky Linux 8.10 + MPSS 3.8.6 + ICC 16.0 offload 环境，用于 Xeon Phi 7120P

## 硬件配置

| 组件 | 规格 |
|-----------|-------|
| Host | Xeon Gold 6252 x2, 192GB DDR4, Rocky Linux 8.10 |
| Phi | Xeon Phi 7120P (KNC), 16GB GDDR5, 61 核 @ 1.238 GHz |
| 线程 | 244 (4-way SMT) |
| FP64 峰值 | ~1.208 TFLOPS |
| FP32 峰值 | ~2.416 TFLOPS |
| 内存带宽 | ~352 GB/s |
| PCIe | Gen3 x16 |

## 环境状态

| 组件 | 版本 | 状态 |
|-----------|---------|--------|
| MPSS | 3.8.6 | ✅ 运行中 (`mpssd` 运行中, mic0 在线) |
| ICC | 16.0.0 | ✅ 可在 podman 容器中使用 |
| COI 库 | 3.8.6 | ✅ 设备枚举正常 (count=1) |
| OpenMP offload | ICC 16.0 | ✅ 已验证 (result=42, 向量加法正确) |
| Intel LEO offload | ICC 16.0 | ✅ 已验证 |
| micnativeloadex | 3.8.6 | ✅ 已验证 (561 GFLOPS FP64) |
| Intel TBB 4.4 | ICC 16.0 | ✅ 已安装并验证 (MIC 原生) |
| Intel MPI | 5.1 | ✅ 可用 (MIC 交叉编译已验证) |
| liboffloadmic | GCC 5.3.0 | ❌ 不可行 (仅模拟器, GCC 专用语法) |

## 已验证的编程模型

### 1. OpenMP Offload (推荐)

```c
#pragma omp target map(to: A[0:N], B[0:N]) map(from: C[0:N])
{
    for (int i = 0; i < N; i++) C[i] = A[i] + B[i];
}
```

在容器中编译，在 host 上运行:
```bash
podman exec centos7-phi-dev bash -c '
    source /opt/intel/bin/compilervars.sh intel64
    icc -qopenmp -qoffload=optional -o myapp myapp.c
'
podman cp centos7-phi-dev:/tmp/myapp ./myapp

export MIC_LD_LIBRARY_PATH="/path/to/intel64_lin_mic/libs"
export OFFLOAD_ENABLE_ORSL=0
./myapp
```

详见 [Xeon Phi Offload 指南](docs/impl/20260520_231500_xeon_phi_offload_guide.md)。

### 2. 手动 KNC Intrinsic (峰值性能)

```c
#include <immintrin.h>
__m512d va = _mm512_set1_pd(1.0);
__m512d vb = _mm512_set1_pd(2.0);
__m512d vc = _mm512_fmadd_pd(va, vb, vc);
```

编译并部署:
```bash
icc -std=c99 -mmic -O3 -openmp -o prog.mic prog.c
scp prog.mic mic0:/tmp/
ssh mic0 /tmp/prog.mic
```

### 3. micnativeloadex (官方工具)

```bash
micnativeloadex prog.mic -d 0 -t 60
```

### 4. MIC 原生并行模型 (6 种已验证)

全部使用 `-mmic` 编译并通过 `micnativeloadex` 在 mic0 上执行:

| 模型 | 文件 | 线程/进程数 | 结果 |
|-------|------|--------------|--------|
| POSIX Threads | `examples/mic_parallel/01_pthreads.c` | 244 | ✅ 0.53s, 通过 |
| OpenMP | `examples/mic_parallel/02_openmp.c` | 240 自动 | ✅ 0.26s, 通过 |
| Intel Cilk Plus | `examples/mic_parallel/03_cilkplus.cpp` | work-stealing | ✅ 通过 |
| Intel TBB | `examples/mic_parallel/04_tbb.cpp` | work-stealing | ✅ 通过 |
| Intel MKL | `examples/mic_parallel/05_mkl.cpp` | 240 自动 | ✅ 45.7 GFLOPS, 通过 |
| Intel MPI | `examples/mic_parallel/06_mpi.c` | 1+ 进程 | ✅ 单进程通过 |

编译说明详见 [examples/mic_parallel/README.md](examples/mic_parallel/README.md)。

### 5. Host TBB + MIC OpenMP 混合模式

Host 端使用 TBB 进行数据准备，MIC 端通过 `#pragma offload` 使用 OpenMP 进行计算:

```cpp
// Host: TBB
 tbb::parallel_for(...);

// MIC: OpenMP (TBB 不能在 offload 块中使用)
#pragma offload target(mic) in(a[0:N]) out(c[0:N])
{
    #pragma omp parallel for
    for (int i = 0; i < N; i++) c[i] = ...;
}
```

详见 [examples/tbb_mic/README.md](examples/tbb_mic/README.md)。

## 峰值性能基准测试

| 测试 | 实测值 | 理论值 | 效率 |
|------|----------|--------|------------|
| FP64 FMA | **575 GFLOPS** | 1,208 GFLOPS | 47.6% |
| FP32 FMA | **1,170 GFLOPS** | 2,416 GFLOPS | 48.4% |
| STREAM Copy | **157 GB/s** | 352 GB/s | 44.7% |
| DGEMM 2048 | **63 GFLOPS** | 1,208 GFLOPS | 5.2% |
| MKL DGEMM 2000 (MIC 原生) | **45.7 GFLOPS** | 1,208 GFLOPS | 3.8% |

来源: `phi_peak_fp64.c`, `phi_peak_fp32.c`, `phi_stream_bench.c`, `phi_peak_dgemm.c`, `examples/mic_parallel/05_mkl.cpp`

## 项目结构

```
├── README.md                          # 本文件
├── README_cn.md                       # 中文版本
├── .gitignore                         # 排除 *.mic 二进制文件, mpss 压缩包
│
├── tests/
│   ├── perf/                            # 峰值性能基准测试
│   │   ├── phi_peak_fp64.c              # FP64 FMA 峰值基准测试
│   │   ├── phi_peak_fp32.c              # FP32 FMA 峰值基准测试
│   │   ├── phi_peak_dgemm.c             # DGEMM 基线基准测试
│   │   ├── phi_stream_bench.c           # STREAM 带宽基准测试
│   │   ├── Makefile.peak                # 构建自动化
│   │   ├── build_peak_tests.sh          # 构建脚本
│   │   └── README.md                    # 基准测试指南
│   │
│   ├── legacy/                          # 早期实验 (历史)
│   │   ├── saxpy_bench.c                # 使用 pthreads 的 SAXPY
│   │   ├── saxpy_kernel.c               # SAXPY 内核
│   │   ├── matmul_bench.c               # 使用 pthreads 的矩阵乘法
│   │   └── README.md                    # 历史说明
│   │
├── docs/
│   ├── research/
│   │   ├── 20260520_214838_liboffloadmic_detailed_assessment.md
│   │   ├── 20260520_071200_liboffloadmic_assessment.md
│   │   ├── 20260520_052053_peak_performance_testing_theory.md
│   │   ├── ESC4000G4_7120P_Final_Assessment.md
│   │   ├── Xeon_Phi_7120P_Specific_Assessment.md
│   │   ├── Xeon_Phi_Addon_Assessment.md
│   │   └── 20260522_020600_tbb_offload_inlining_bug.md  # TBB+offload 不可行根因
│   ├── impl/
│   │   ├── 20260520_231500_xeon_phi_offload_guide.md    # ⭐ 主指南
│   │   ├── 20260520_052053_peak_performance_implementation.md
│   │   └── 20260520_055400_peak_performance_verification.md
│   └── plan/
│       └── 20260520_052053_peak_performance_test_plan.md
│
├── examples/                          # 已验证的可运行示例
│   ├── mic_parallel/                  # 6 种 MIC 并行编程模型
│   │   ├── 01_pthreads.c              # POSIX Threads
│   │   ├── 02_openmp.c                # OpenMP
│   │   ├── 03_cilkplus.cpp            # Intel Cilk Plus
│   │   ├── 04_tbb.cpp                 # Intel TBB
│   │   ├── 05_mkl.cpp                 # Intel MKL 自动多线程
│   │   ├── 06_mpi.c                   # Intel MPI
│   │   ├── Makefile                   # 构建并运行所有示例
│   │   └── README.md                  # 6 种模型的使用指南
│   └── tbb_mic/                       # MIC 上的 TBB 使用指南
│       ├── example1_parallel_for.cpp
│       ├── example2_parallel_reduce.cpp
│       ├── example3_host_tbb_mic_omp.cpp
│       ├── Makefile
│       └── README.md
│
├── tests/                             # 最佳实践验证测试
│   ├── orsl_multi_proc/               # ORSL 多进程 offload 测试
│   ├── icc_gcc_compat/                # ICC/GCC 混合编译测试
│   ├── openmp_dual_lib/               # OpenMP 双库冲突演示
│   ├── cpp_cross_abi/                 # C++ 跨编译器异常测试
│   ├── mic_ldpath_verify/             # MIC 库路径验证
│   └── Makefile                       # 构建并运行所有测试
│
├── psxe_install/                      # ICC 16.0 安装程序 (gitignored)
├── mpss-3.8.6-linux.tar               # MPSS 3.8.6 发行版 (gitignored)
└── mpss-src-3.8.6.tar                 # MPSS 3.8.6 源码 (gitignored)
```

## 最佳实践测试套件

用于 ICC/GCC 共存和 offload 行为验证的可复现测试:

```bash
cd tests
make all    # 构建所有测试 (通过 podman 使用 ICC)
make test   # 运行所有测试
```

| 测试 | 验证内容 |
|------|------------------|
| `orsl_multi_proc` | 单卡场景下多进程 offload 无需 ORSL 即可工作 |
| `icc_gcc_compat` | GCC OpenMP 目标文件可通过 `libiomp5.so` 正确链接到 ICC 二进制文件 |
| `openmp_dual_lib` | 同时链接 `libiomp5.so` 和 `libgomp.so` 会导致运行时冲突 |
| `cpp_cross_abi` | GCC 代码抛出的 C++ 异常可被 ICC 代码捕获 |
| `mic_ldpath_verify` | Offload 需要 `MIC_LD_LIBRARY_PATH` 指向 host 可见的 ICC MIC 库 |


## 容器环境

提供预配置好的 ICC 16.0 + TBB 4.4 容器:

```bash
cd container
./export-import.sh import   # 从 tar.gz 导入
./export-import.sh run      # 启动开发环境
```

或从零构建 (需要 PSXE 2016 安装程序):

```bash
cd container
./build.sh
```

详见 [container/README.md](container/README.md)。

## 关键发现

1. **MPSS 3.8.6 可在 Rocky 8.10 上运行** — 但除基础包外，还需要安装 `mpss-core`、`mpss-offload`、`libscif0`、`libmicmgmt0`。
2. **COI 库功能正常** — 完成 MPSS 完整安装后，设备枚举工作正常。
3. **OpenMP offload 是推荐路径** — ICC 16.0 支持 `#pragma omp target` 完整 MIC offload。
4. **liboffloadmic 不可行** — 该项目仅实现模拟器，硬编码 GCC 专用语法，无原生 COI 选项。
5. **单卡场景不需要 ORSL** — `OFFLOAD_ENABLE_ORSL=1` 对单张 7120P 无收益。
6. **切勿同时链接两个 OpenMP 库** — 仅使用 `libiomp5.so`; 它提供 GCC ABI 兼容性。
7. **容器 ICC 需要 MIC 库复制到 host** — host 无法看到 podman 内的 `/opt/intel`; 需将 `intel64_lin_mic/` 库复制到 host 并设置 `MIC_LD_LIBRARY_PATH`。
8. **TBB + `#pragma offload` 在 KNC 上不可行** — ICC 16.0 MIC 编译器无法在 `target(mic)` 代码中内联类构造/析构函数，导致 TBB 符号链接错误。TBB 只能通过纯原生 (`-mmic`) 二进制在 MIC 上运行，不能在 offload 块中使用。
9. **ICC 16.0 MIC 编译器不支持 C++11 lambda** — 使用 `-mmic` 编译 TBB 或 Cilk Plus 代码时，使用 functor 类替代 `[&](...){...}`。

## 速查表

| 任务 | 命令 |
|------|---------|
| 检查 MPSS 状态 | `systemctl status mpss` |
| 检查 Phi 设备 | `micinfo` / `micctrl --status` |
| 进入构建容器 | `podman exec -it centos7-phi-dev bash` |
| 设置 ICC 环境 | `source /opt/intel/bin/compilervars.sh intel64` |
| MIC 编译 | `icc -mmic -O3 -openmp ...` |
| OpenMP offload 编译 | `icc -qopenmp -qoffload=optional ...` |
| 通过 micnativeloadex 运行 | `micnativeloadex prog.mic -d 0 -t 60` |
| 通过 SSH 在 mic0 上运行 | `ssh mic0 /tmp/prog.mic` |
| 运行 MIC 并行示例 | `cd examples/mic_parallel && make all && make run-all` |
| 运行 TBB 示例 | `cd examples/tbb_mic && make all && make run-mic` |

## 许可证

项目特定代码按原样提供，用于研究和基准测试目的。
Intel 工具 (ICC, MPSS) 受各自许可证约束。
