# Xeon Phi 7120P (KNC) 多线程并行方案大全

本文档系统介绍在 Intel Xeon Phi 7120P (Knights Corner, KNC) 上启动多线程/多进程完成工作的全部可行方案，并附带可编译运行的示例代码。

---

## 方案总览

| # | 方案 | 模型 | 编译选项 | 推荐度 | 验证状态 |
|---|------|------|---------|--------|---------|
| 1 | **POSIX Threads** | 底层显式线程 | `-lpthread` | ⭐⭐ | ✅ 已验证 |
| 2 | **OpenMP** | 指令式并行 | `-openmp` | ⭐⭐⭐⭐⭐ | ✅ 已验证 |
| 3 | **Intel Cilk Plus** | 任务分治 | 内置 | ⭐⭐⭐⭐ | ✅ 已验证 |
| 4 | **Intel TBB** | 任务/算法并行 | `-tbb` | ⭐⭐⭐⭐ | ✅ 已验证 |
| 5 | **Intel MKL 自动多线程** | 库级别自动 | `-lmkl_*` | ⭐⭐⭐ | ✅ 已验证 |
| 6 | **Intel MPI** | 分布式多进程 | `mpiicc` | ⭐⭐⭐ | ✅ 编译通过，单 rank 运行成功 |
| 7 | **OpenCL** | 异构 kernel | KNC OpenCL runtime | ⭐ | ❌ 容器中未安装 |

---

## 环境准备

```bash
# 进入 ICC 容器
podman exec -it centos7-phi-dev bash

# 加载环境
source /opt/intel/bin/compilervars.sh intel64
source /opt/intel/compilers_and_libraries_2016.0.109/linux/tbb/bin/tbbvars.sh intel64

# 确认 MIC 在线
micctrl --status
```

**MIC 运行时库路径**（用于 `micnativeloadex`）：
```bash
export SINK_LD_LIBRARY_PATH=/home/joey/Work/intel_phi/icc_mic_libs
export MIC_LD_LIBRARY_PATH=/home/joey/Work/intel_phi/icc_mic_libs
```

---

## 方案 1：POSIX Threads (pthreads)

### 特点
- 最底层 API，手动创建/同步线程
- 需要自己实现任务划分（chunking）
- KNC Linux 2.6.38 内核原生支持
- 无自动负载均衡

### 代码要点
```c
pthread_t threads[NUM_THREADS];
ThreadArgs args[NUM_THREADS];

for (int t = 0; t < NUM_THREADS; t++) {
    args[t].tid = t;
    pthread_create(&threads[t], NULL, worker, &args[t]);
}
for (int t = 0; t < NUM_THREADS; t++) {
    pthread_join(threads[t], NULL);
}
```

### 编译运行
```bash
make 01_pthreads.mic
make run-pthreads
```

### 适用场景
需要精确控制线程绑定、NUMA 感知的极致优化场景。

---

## 方案 2：OpenMP

### 特点
- 学习成本最低，加 `#pragma` 即可
- 自动线程管理，支持 static/dynamic/guided 调度
- KNC 244 线程时自动创建 244 个线程
- Offload 模式下 MIC 端可用（TBB 不行）

### 代码要点
```c
#pragma omp parallel for schedule(static)
for (int i = 0; i < N; i++) {
    c[i] = a[i] + b[i];
}

// 归约
#pragma omp parallel for reduction(+:sum) schedule(static)
for (int i = 0; i < N; i++) {
    sum += c[i];
}
```

### 编译运行
```bash
make 02_openmp.mic
make run-openmp
```

### 适用场景
科学计算循环并行，绝大多数场景首选。

---

## 方案 3：Intel Cilk Plus

### 特点
- ICC 内置扩展，无需额外库
- `_Cilk_for` 自动 work-stealing 调度
- `_Cilk_spawn` / `_Cilk_sync` 支持递归分治
- `reducer` 提供线程安全归约
- ICC 18 后弃用，但 ICC 16.0 + KNC 完全可用

### 代码要点
```cpp
// 并行循环
_Cilk_for (int i = 0; i < N; i++) {
    c[i] = a[i] + b[i];
}

// reducer 归约
cilk::reducer_opadd<double> sum(0.0);
_Cilk_for (int i = 0; i < N; i++) {
    *sum += c[i];
}
// sum.get_value() 获取结果
```

### 编译运行
```bash
make 03_cilkplus.mic
make run-cilkplus
```

### 适用场景
递归算法（快速排序、矩阵分治）、不规则并行负载。

---

## 方案 4：Intel TBB

### 特点
- C++ 模板库，任务级并行
- 支持任务图、流水线、并发容器
- 自动 work-stealing
- **限制：MIC 编译器不支持 C++11 lambda，必须用 functor**

### 代码要点
```cpp
struct AddFunctor {
    const double *a, *b;
    double* c;
    void operator()(const tbb::blocked_range<size_t>& r) const {
        for (size_t i = r.begin(); i != r.end(); ++i)
            c[i] = a[i] + b[i];
    }
};

tbb::parallel_for(
    tbb::blocked_range<size_t>(0, N),
    AddFunctor{a.data(), b.data(), c.data()}
);
```

### 编译运行
```bash
make 04_tbb.mic
make run-tbb
```

### 适用场景
复杂非规则并行、需要任务图调度的 C++ 应用。

---

## 方案 5：Intel MKL 自动多线程

### 特点
- BLAS/LAPACK/FFT/Vector Math 自动并行
- 通过 `MKL_NUM_THREADS` 控制线程数
- 无需修改业务代码，库内部并行
- KNC 上 MKL 11.3.0 支持良好

### 代码要点
```cpp
// 查询 MKL 线程数
int mkl_threads = mkl_get_max_threads();

// DGEMM 内部自动并行
cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
            M, N, K, alpha, A, lda, B, ldb, beta, C, ldc);
```

### 编译运行
```bash
make 05_mkl.mic
make run-mkl
```

### MKL 链接模式对比

| 模式 | 链接选项 | 线程行为 |
|------|---------|---------|
| Threaded | `-lmkl_intel_lp64 -lmkl_intel_thread -lmkl_core -liomp5` | MKL 自动多线程 |
| Sequential | `-lmkl_intel_lp64 -lmkl_sequential -lmkl_core` | MKL 单线程 |

**KNC 推荐**：如果上层已用 OpenMP/TBB，MKL 用 sequential 模式避免线程层级冲突。

---

## 方案 6：Intel MPI

### 特点
- 多进程模型（区别于其他方案的多线程）
- 可跨节点、跨卡片扩展
- Host + MIC 协同：Host 上一些 rank，MIC 上一些 rank
- 每张 KNC 卡可运行多个 MPI rank

### 代码要点
```c
MPI_Init(&argc, &argv);
MPI_Comm_rank(MPI_COMM_WORLD, &rank);
MPI_Comm_size(MPI_COMM_WORLD, &size);

// 每个 rank 处理自己的 chunk
compute_my_chunk(rank, size);

// 全局归约
MPI_Reduce(&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
MPI_Finalize();
```

### 编译运行
```bash
make 06_mpi.mic

# 方式 1：Host 启动 MIC 进程
mpirun -host mic0 -np 4 ./06_mpi.mic

# 方式 2：复制到 MIC 后运行
scp 06_mpi.mic mic0:/tmp/
ssh mic0 "LD_LIBRARY_PATH=/tmp/mic_libs mpirun -np 4 /tmp/06_mpi.mic"
```

### 适用场景
多卡并行、大规模集群、已有 MPI 应用迁移。

### ⚠️ 注意事项
MIC 端需要 MPI runtime 库（`libmpi.so` 等）。已通过 `SINK_LD_LIBRARY_PATH` 提供。

多 rank 运行需要额外配置：
- Host 需安装 Intel MPI 的 `mpirun`
- Host 到 MIC0 的 SSH 免密登录
- 或配置 Intel MPI 的 OFI/SCIF 网络层

单 rank 验证：
```bash
SINK_LD_LIBRARY_PATH=/path/to/libs micnativeloadex ./06_mpi.mic
```

---

## 方案 7：OpenCL

### 状态：❌ 未安装

KNC 硬件支持 OpenCL 1.2，但当前容器和 MPSS 3.8.6 中未预装 OpenCL runtime/SDK。

如需使用，需额外安装 Intel OpenCL SDK for KNC，通常随 MPSS 或 Intel SDK 单独提供。

### 特点（理论）
- 写 kernel 在 MIC 上执行
- 性能通常不如原生 OpenMP/TBB
- Intel 后续转向 SYCL/oneAPI

---

## 方案对比与选择

```
你需要在 MIC 上并行？
│
├─ 代码是 C，主要是循环？
│   └─ 用 OpenMP ────────────► 最简单
│
├─ 代码是 C++，需要任务调度？
│   ├─ 需要任务图/流水线？ ──► TBB
│   └─ 需要递归分治？ ────────► Cilk Plus
│
├─ 做矩阵/FFT/向量运算？
│   └─ 用 MKL ───────────────► 库自己并行
│
├─ 多卡/多节点？
│   └─ 用 MPI ───────────────► Host+MIC 协同
│
├─ 极致控制线程行为？
│   └─ 用 pthreads ──────────► 最底层
│
└─ 已有 OpenCL 代码？
    └─ 需额外安装 OpenCL SDK
```

---

## 关键限制汇总

| 方案 | `target(mic)` 函数中 | `#pragma offload` 块中 |
|------|---------------------|------------------------|
| OpenMP | ✅ | ✅ |
| pthreads | ✅ | ✅ |
| Cilk Plus | ⚠️ 未验证 | ❌ 不建议 |
| TBB | ❌ **不可用** | ❌ **不可用** |
| MPI | ✅（独立进程） | N/A |
| MKL | ✅ | ✅ |

**结论**：Offload 模式下 MIC 端只能用 **OpenMP/pthreads/MKL**，不能用 TBB/Cilk Plus。

---

## Makefile 速查

```bash
# 编译全部
make all

# 单独编译/运行
make 01_pthreads.mic && make run-pthreads
make 02_openmp.mic   && make run-openmp
make 03_cilkplus.mic && make run-cilkplus
make 04_tbb.mic      && make run-tbb
make 05_mkl.mic      && make run-mkl
make 06_mpi.mic      && make run-mpi

# 运行全部（除 MPI）
make run-all

# 清理
make clean
```

---

## 参考

- `docs/research/20260522_020600_tbb_offload_inlining_bug.md` — TBB + Offload 不可行根因
- `examples/tbb_mic/README.md` — TBB 使用指南
- Intel ICC 16.0 + MPSS 3.8.6 文档
