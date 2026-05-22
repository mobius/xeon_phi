# Intel TBB on Xeon Phi 7120P (KNC) 使用指南

## 概述

本文档介绍如何在 Intel Xeon Phi 7120P (Knights Corner, KNC) 上使用 Intel Threading Building Blocks (TBB) 4.4。

> **重要前提**：TBB **不能**与 `#pragma offload` / `__attribute__((target(mic)))` 混合使用。MIC 编译器在 `target(mic)` 模式下存在内联缺陷，无法正确处理 TBB 的内联构造函数和模板 functor。
>
> 详见: `docs/research/20260522_020600_tbb_offload_inlining_bug.md`

---

## 两种使用模式

### 模式一：MIC 纯原生 TBB（推荐用于纯 MIC 计算）

将 TBB 代码编译为纯 MIC 二进制，通过 `micnativeloadex` 在 Xeon Phi 上直接运行。

**适用场景：**
- 计算任务可以完全在 MIC 上独立完成
- 需要利用 TBB 的任务调度、并行算法等高级特性
- 不需要频繁与 Host 交换数据

**编译命令：**
```bash
icpc -mmic -tbb -O2 -o myapp.mic myapp.cpp
```

**运行命令：**
```bash
# 方式 1: 使用 micnativeloadex（推荐）
SINK_LD_LIBRARY_PATH=/path/to/mic/libs micnativeloadex ./myapp.mic

# 方式 2: 复制到 MIC 后运行
scp myapp.mic mic0:/tmp/
ssh mic0 /tmp/myapp.mic
```

**MIC 运行时库路径：**
```bash
export SINK_LD_LIBRARY_PATH=/home/joey/Work/intel_phi/icc_mic_libs
export MIC_LD_LIBRARY_PATH=/home/joey/Work/intel_phi/icc_mic_libs
```

该目录应包含以下库：
- `libtbb.so.2`, `libtbbmalloc.so.2`
- `libiomp5.so`, `liboffload.so.5`
- `libimf.so`, `libsvml.so`, `libintlc.so.5`

---

### 模式二：Host TBB + MIC OpenMP 混合（推荐用于 Host-MIC 协作）

Host 端使用 TBB 进行数据预处理/后处理，MIC 端使用 OpenMP 进行大规模并行计算，通过 `#pragma offload` 传输数据。

**适用场景：**
- 需要 Host 和 MIC 协作完成计算
- 数据需要在 Host 和 MIC 之间传输
- MIC 端只需要简单的循环并行

**编译命令：**
```bash
icpc -qoffload -tbb -openmp -O2 -o myapp myapp.cpp
```

**运行命令：**
```bash
MIC_LD_LIBRARY_PATH=/path/to/mic/libs ./myapp
```

**关键规则：**
- `__attribute__((target(mic)))` 函数中**不能**使用 TBB
- MIC 端只能使用 OpenMP、原生 C/C++、Intel MKL

---

## 环境准备

### 1. 进入 ICC 开发容器

```bash
podman exec -it centos7-phi-dev bash
```

### 2. 加载 Intel 编译器环境

```bash
source /opt/intel/bin/compilervars.sh intel64
source /opt/intel/compilers_and_libraries_2016.0.109/linux/tbb/bin/tbbvars.sh intel64
```

### 3. 验证 TBB 安装

```bash
# Host 版 TBB
ls /opt/intel/compilers_and_libraries_2016.0.109/linux/tbb/lib/intel64/gcc4.4/libtbb.so*

# MIC 版 TBB
ls /opt/intel/compilers_and_libraries_2016.0.109/linux/tbb/lib/mic/libtbb.so*
```

### 4. 验证 MPSS 和 MIC 状态

```bash
micctrl --status
# 应显示: mic0: online
```

---

## ⚠️ 重要限制：ICC 16.0 MIC 编译器不支持 C++11 Lambda

ICC 16.0 的 MIC 编译器 (`-mmic`) **不支持**将 C++11 lambda 表达式作为 TBB 模板参数。编译会报错：

```
error: a template argument may not reference a local type
```

**解决方案：使用 Functor Class 替代 Lambda**

```cpp
// ❌ 错误：lambda 不被 MIC 编译器支持
tbb::parallel_for(
    tbb::blocked_range<size_t>(0, N),
    [&](const tbb::blocked_range<size_t>& r) {
        for (size_t i = r.begin(); i != r.end(); ++i) {
            c[i] = a[i] + b[i];
        }
    }
);

// ✅ 正确：使用 functor class
struct AddFunctor {
    const double* a;
    const double* b;
    double* c;

    AddFunctor(const double* a_, const double* b_, double* c_)
        : a(a_), b(b_), c(c_) {}

    void operator()(const tbb::blocked_range<size_t>& r) const {
        for (size_t i = r.begin(); i != r.end(); ++i) {
            c[i] = a[i] + b[i];
        }
    }
};

tbb::parallel_for(
    tbb::blocked_range<size_t>(0, N),
    AddFunctor(a, b, c)
);
```

> **注意**：Host 端编译器 (`-qoffload` 的 host 部分) **支持** lambda，但为保持代码一致性，建议所有 TBB 代码统一使用 functor。

---

## 示例代码

### 示例 1: MIC 纯原生 parallel_for

**文件**: `example1_parallel_for.cpp`

展示如何在 MIC 上使用 TBB `parallel_for` 进行向量化计算。

```cpp
#include <tbb/tbb.h>
#include <vector>
#include <cmath>

// Functor for initialization
struct InitFunctor {
    double* a;
    double* b;
    size_t  N;

    InitFunctor(double* a_, double* b_, size_t N_)
        : a(a_), b(b_), N(N_) {}

    void operator()(const tbb::blocked_range<size_t>& r) const {
        for (size_t i = r.begin(); i != r.end(); ++i) {
            a[i] = static_cast<double>(i);
            b[i] = static_cast<double>(N - i);
        }
    }
};

// Functor for computation
struct ComputeFunctor {
    const double* a;
    const double* b;
    double*       c;

    ComputeFunctor(const double* a_, const double* b_, double* c_)
        : a(a_), b(b_), c(c_) {}

    void operator()(const tbb::blocked_range<size_t>& r) const {
        for (size_t i = r.begin(); i != r.end(); ++i) {
            c[i] = std::sqrt(a[i] * a[i] + b[i] * b[i]);
        }
    }
};

int main() {
    const size_t N = 100000000;
    std::vector<double> a(N), b(N), c(N);

    // TBB parallel_for 初始化
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, N),
        InitFunctor(a.data(), b.data(), N)
    );

    // TBB parallel_for 计算
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, N),
        ComputeFunctor(a.data(), b.data(), c.data())
    );

    return 0;
}
```

**编译运行：**
```bash
make example1_parallel_for.mic
SINK_LD_LIBRARY_PATH=/path/to/mic/libs micnativeloadex ./example1_parallel_for.mic
```

---

### 示例 2: MIC 纯原生 parallel_reduce

**文件**: `example2_parallel_reduce.cpp`

展示如何在 MIC 上使用 TBB `parallel_reduce` 进行自定义归约操作。

```cpp
#include <tbb/tbb.h>
#include <vector>
#include <cmath>

// 自定义归约体：计算向量的点积 + 最大值
struct DotMaxBody {
    const double* a;
    const double* b;
    double dot;
    double max_val;

    DotMaxBody(const double* a_, const double* b_)
        : a(a_), b(b_), dot(0.0), max_val(0.0) {}

    // 分裂构造函数 (required by TBB)
    DotMaxBody(DotMaxBody& other, tbb::split)
        : a(other.a), b(other.b), dot(0.0), max_val(0.0) {}

    void operator()(const tbb::blocked_range<size_t>& r) {
        double local_dot = dot;
        double local_max = max_val;
        for (size_t i = r.begin(); i != r.end(); ++i) {
            local_dot += a[i] * b[i];
            local_max = std::max(local_max, std::abs(a[i]));
        }
        dot = local_dot;
        max_val = local_max;
    }

    void join(const DotMaxBody& other) {
        dot += other.dot;
        max_val = std::max(max_val, other.max_val);
    }
};
```

**编译运行：**
```bash
make example2_parallel_reduce.mic
SINK_LD_LIBRARY_PATH=/path/to/mic/libs micnativeloadex ./example2_parallel_reduce.mic
```

---

### 示例 3: Host TBB + MIC OpenMP 混合

**文件**: `example3_host_tbb_mic_omp.cpp`

展示 Host-MIC 协作模式：
- **Host 端**：TBB 初始化数据
- **MIC 端**：OpenMP 并行计算（`#pragma offload` 传输数据）
- **Host 端**：验证结果

```cpp
#include <tbb/tbb.h>
#include <omp.h>

// MIC 端计算函数：只能用 OpenMP，不能用 TBB!
__attribute__((target(mic)))
void mic_compute(double* __restrict__ a,
                 double* __restrict__ b,
                 double* __restrict__ c,
                 size_t N)
{
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < N; ++i) {
        c[i] = std::sqrt(a[i] * a[i] + b[i] * b[i]);
    }
}

// Host 端 TBB Functor：初始化数据
struct HostInitFunctor {
    double* a;
    double* b;
    size_t  N;

    HostInitFunctor(double* a_, double* b_, size_t N_)
        : a(a_), b(b_), N(N_) {}

    void operator()(const tbb::blocked_range<size_t>& r) const {
        for (size_t i = r.begin(); i != r.end(); ++i) {
            a[i] = static_cast<double>(i);
            b[i] = static_cast<double>(N - i);
        }
    }
};

int main() {
    const size_t N = 100000000;
    double* a = new double[N];
    double* b = new double[N];
    double* c = new double[N];

    // Host 端：TBB 初始化
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, N),
        HostInitFunctor(a, b, N)
    );

    // Offload 到 MIC
    #pragma offload target(mic) in(a[0:N], b[0:N]) out(c[0:N])
    {
        mic_compute(a, b, c, N);
    }

    // Host 端：验证
    // ...

    delete[] a;
    delete[] b;
    delete[] c;
}
```

**编译运行：**
```bash
make example3_host_tbb_mic_omp
MIC_LD_LIBRARY_PATH=/path/to/mic/libs ./example3_host_tbb_mic_omp
```

---

## Makefile 使用

```bash
# 编译所有示例
make all

# 运行 MIC 原生示例
make run-mic

# 运行 Host 混合示例
make run-host

# 运行所有示例
make run-all

# 清理
make clean
```

---

## TBB on KNC 的最佳实践

### 1. 线程数控制

KNC 有 61 核 × 4 线程 = 244 线程，但 TBB 自动线程池可能创建过多线程。

```cpp
// 显式设置 TBB 线程数
tbb::task_scheduler_init init(120);  // 使用 120 线程（避免超订）
```

### 2. 分区器选择

KNC 的 SIMD 宽度为 512-bit (8x double)，选择合适的分区器：

```cpp
// 自动分区器（默认，适合大多数场景）
tbb::parallel_for(tbb::blocked_range<size_t>(0, N), body);

// 简单分区器（适合负载均匀的情况）
tbb::parallel_for(tbb::blocked_range<size_t>(0, N), body,
                  tbb::simple_partitioner());

// 静态分区器（最小调度开销）
tbb::parallel_for(tbb::blocked_range<size_t>(0, N), body,
                  tbb::static_partitioner());
```

### 3. 向量化提示

帮助 ICC 生成 AVX-512 代码：

```cpp
struct VecFunctor {
    const double* a;
    const double* b;
    double* c;

    void operator()(const tbb::blocked_range<size_t>& r) const {
        #pragma ivdep          // 告诉编译器无数据依赖
        #pragma vector always  // 强制向量化
        for (size_t i = r.begin(); i != r.end(); ++i) {
            c[i] = a[i] + b[i];
        }
    }
};
```

### 4. 内存对齐

KNC 的 SIMD 加载/存储需要 64 字节对齐：

```cpp
// 使用 TBB 的 cache_aligned_allocator
tbb::cache_aligned_allocator<double> alloc;
double* a = alloc.allocate(N);
```

### 5. 避免在 MIC 上使用 TBB + Offload 混合

```cpp
// ❌ 错误：TBB 不能在 target(mic) 函数中使用
__attribute__((target(mic)))
void bad_function() {
    tbb::mutex m;              // 链接失败！
    tbb::parallel_for(...);    // 链接失败！
}

// ❌ 错误：TBB 不能在 #pragma offload 块中使用
#pragma offload target(mic)
{
    tbb::parallel_for(...);    // 编译失败！
}
```

---

## 故障排除

### 问题 1: `libtbb.so.2: cannot open shared object file`

**原因**: MIC 运行时找不到 TBB 库。

**解决**:
```bash
export SINK_LD_LIBRARY_PATH=/home/joey/Work/intel_phi/icc_mic_libs
micnativeloadex ./myapp.mic
```

### 问题 2: `undefined reference to tbb::mutex::mutex()`

**原因**: 试图在 `target(mic)` 函数或 `#pragma offload` 块中使用 TBB。

**解决**: 使用本文档介绍的两种正确模式之一。

### 问题 3: `a template argument may not reference a local type`

**原因**: 在 `-mmic` 编译中使用了 C++11 lambda。

**解决**: 将 lambda 替换为 functor class。详见本文档 "重要限制" 章节。

### 问题 4: `base of array section must be pointer or array type`

**原因**: `#pragma offload` 的 `in/out` 子句中使用了 `std::vector`。

**解决**: 使用原生指针：
```cpp
// ❌ 错误
std::vector<double> a(N);
#pragma offload target(mic) in(a[0:N])

// ✅ 正确
double* a = new double[N];
#pragma offload target(mic) in(a[0:N])
```

### 问题 5: MIC 程序运行极慢

**原因**: 可能使用了过多线程导致超订。

**解决**:
```cpp
tbb::task_scheduler_init init(120);  // 限制线程数
```

### 问题 6: `micnativeloadex` 提示 `COI_PROCESS_ERROR`

**原因**: MPSS 服务未启动或 mic0 不在线。

**解决**:
```bash
sudo systemctl status mpss
sudo micctrl --boot
micctrl --status  # 确认 mic0: online
```

---

## 参考

- [TBB 4.4 Documentation](https://www.intel.com/content/www/us/en/developer/articles/tool/oneapi-threading-building-blocks-documentation.html)
- [Intel Xeon Phi Coprocessor Developer's Guide](https://www.intel.com/content/www/us/en/developer/articles/guide/intel-xeon-phi-coprocessor-developers-guide.html)
- `docs/research/20260522_020600_tbb_offload_inlining_bug.md` — TBB + Offload 不可行根因分析
- `docs/impl/20260521_075300_icc_bundled_libraries.md` — ICC 16.0 捆绑库清单
