# ICC 16.0 MIC 编译器内联缺陷：TBB + Offload 不可行

## 结论

**TBB + `#pragma offload` / `__attribute__((target(mic)))` 混合编译在 KNC 上不可行。**

根本原因不是库路径或链接器配置，而是 **ICC 16.0 的 MIC 编译器在编译 `target(mic)` 代码时存在内联缺陷**。

---

## 复现步骤

### 测试 1: 自定义类在 target(mic) 中不内联

```cpp
#include <pthread.h>
class my_mutex {
    pthread_mutex_t impl;
public:
    my_mutex() {
        int error_code = pthread_mutex_init(&impl, NULL);
        if (error_code) __builtin_trap();
    }
    ~my_mutex() {
        pthread_mutex_destroy(&impl);
    }
};

__attribute__((target(mic))) int g() { my_mutex m; return 0; }
int main() { return g(); }
```

编译命令：
```bash
icpc -std=c++11 -qoffload -o test test.cpp
```

**结果：链接失败**
```
/tmp/icpcXXXX.o: In function `g()':
test.cpp:(.text+0x49): undefined reference to `my_mutex::my_mutex()'
test.cpp:(.text+0x52): undefined reference to `my_mutex::~my_mutex()'
```

### 测试 2: 同一类在 host 端正常内联

去掉 `__attribute__((target(mic)))`：
```cpp
int g() { my_mutex m; return 0; }  // 无 target(mic)
```

编译成功，目标文件中只有 `pthread_mutex_init`/`pthread_mutex_destroy` 引用，无 `my_mutex` 符号。

### 测试 3: TBB mutex 在 target(mic) 中同样失败

```cpp
#include <tbb/tbb.h>
__attribute__((target(mic))) int g() { tbb::mutex m; return 0; }
int main() { return g(); }
```

**结果：**
```
undefined reference to `tbb::mutex::mutex()'
undefined reference to `tbb::mutex::~mutex()'
```

即使添加 `-O2 -O3 -inline-forceinline` 也无效。

---

## 根因分析

### 编译器行为差异

| 场景 | 编译器 | 内联构造函数 | 结果 |
|------|--------|-------------|------|
| host-only | `mcpcom` (host) | ✅ 内联 | 成功 |
| `target(mic)` | `mcpcom` (MIC) | ❌ **不内联** | 链接失败 |

### 技术细节

1. **MIC 编译器模式**：当函数标记为 `__attribute__((target(mic)))` 时，ICC 使用 `mcpcom --offload_mode=2` 为 KNC (k1om) 生成代码。

2. **内联策略差异**：MIC 编译器在 `offload_mode=2` 下，**不会内联同一编译单元中定义的类构造函数和析构函数**，即使它们非常简单且定义在头文件中。

3. **TBB 的影响**：TBB 大量使用内联构造函数、析构函数和模板 functors。MIC 编译器为每个内联函数生成外部符号引用，导致链接阶段产生数百个 `undefined reference`。

4. **非库问题**：
   - `-ltbb` 正确传递给了 MIC 链接器 (`x86_64-k1om-linux-ld`)
   - `MIC_LIBRARY_PATH` 和 `LIBRARY_PATH` 配置正确
   - `libtbb.so` (MIC 版) 存在于 `/opt/intel/.../tbb/lib/mic/`
   - 但 `libtbb.so` 中不存在这些内联函数的符号（因为它们本应被内联）

---

## 已尝试但失败的方案

| 方案 | 描述 | 失败原因 |
|------|------|---------|
| A | `__attribute__((target(mic)))` + `-ltbb` | MIC 编译器不内联 TBB 构造函数 |
| B | `#pragma offload_attribute(push, target(mic))` + `#include <tbb>` | 同上 |
| C | 显式 `-offload-option,mic,ld,-ltbb` | 库已正确链接，但符号不存在 |
| D | `MIC_LIBRARY_PATH` / `SINK_LD_LIBRARY_PATH` | 非运行时问题 |
| E | `-O2 -O3 -inline-forceinline` | MIC 编译器忽略这些内联提示 |
| F | Functor 替代 lambda | functor 构造函数同样不被内联 |
| G | 纯 C 风格函数封装 TBB | 任何含构造函数的类都不被内联 |

---

## 可行替代方案

### 方案 1: Host TBB + MIC OpenMP（推荐）

```cpp
#include <tbb/tbb.h>

int main() {
    // Host 端用 TBB
    tbb::parallel_for(0, N, [](int i) { ... });
    
    // MIC 端用 OpenMP（无 TBB）
    #pragma offload target(mic) in(data) out(result)
    {
        #pragma omp parallel for
        for (int i = 0; i < N; i++) { ... }
    }
}
```

**状态：✅ 已验证可行**

### 方案 2: 纯 MIC TBB（micnativeloadex）

```bash
# MIC 端独立编译
icpc -mmic -tbb -ltbb -o mic_app mic_app.cpp
# 复制到 MIC 并运行
scp mic_app mic0:/tmp/
ssh mic0 /tmp/mic_app
# 或使用 micnativeloadex
micnativeloadex ./mic_app
```

**状态：✅ 已验证可行**

### 方案 3: Host TBB + `#pragma offload` 纯数据传递

```cpp
int result;
#pragma offload target(mic) out(result)
{
    // MIC 端不使用任何含构造函数的类
    // 只使用 POD 类型和基本运算
    result = compute_native();
}
```

**状态：✅ 已验证可行**

---

## 环境信息

- **ICC**: 16.0.0 (Build 20150815)
- **TBB**: 4.4.0 (Build 109)
- **MPSS**: 3.8.6
- **KNC 编译器**: `mcpcom --offload_mode=2`
- **MIC 链接器**: `x86_64-k1om-linux-ld`

---

## 参考

- Intel ICC 16.0 文档：`__attribute__((target(mic)))` 用于标记可在 MIC 上执行的函数
- TBB 4.4 是最后一个支持 KNC 的版本，后续版本移除 `intel64_lin_mic` 目录
- 该内联缺陷在 ICC 17+ 中是否修复未验证（KNC 在 ICC 17 后已被弃用）
