# liboffloadmic 方案评估报告

> 评估时间: 2026-05-20
> 项目: https://github.com/apc-llc/liboffloadmic
> 环境: CentOS 7 容器 (ICC 16.0) + MPSS 3.8.6 + mic0 (KNC)

---

## 1. 方案概述

`liboffloadmic` 是从 GCC 5.3.0 源码树中提取的 `libgomp` + `liboffloadmic` 独立版本，提供 **CUDA-like API** (`micMalloc`, `micMemcpy`, `micLaunchKernel`) 用于 Xeon Phi KNC 设备编程。

### 编程模型

| 组件 | 技术 |
|------|------|
| 主机运行时 | `libmicrt.so` (CUDA-like API wrapper) |
| 主机插件 | `libgomp-plugin-intelmic.so` |
| 设备运行时 | `liboffloadmic_target.so` + `libgomp` |
| 并行框架 | TBB (`tbb::parallel_for`) |
| 底层通信 | COI (Coprocessor Offload Infrastructure) / MYO |

---

## 2. 验证过程

### 2.1 构建阶段

```bash
git clone https://github.com/apc-llc/liboffloadmic.git
cd liboffloadmic
git submodule init && git submodule update
make target=native
```

**已构建成功的库**:
| 库 | 架构 | 状态 |
|----|------|------|
| `libgomp.so` | host (x86-64) + mic (k1om) | ✅ |
| `liboffloadmic_host.so` | host (x86-64) | ✅ |
| `liboffloadmic_target.so` | mic (k1om) | ✅ |
| `libmicrt.so` | host (x86-64) | ✅ |
| `libcoi_host.so` | host (emulator) | ✅ |
| `libmyo-client.so` | host (emulator) | ✅ |
| TBB for MIC | mic (k1om) | ❌ (ICC 链接器路径问题) |

**环境适配修改**:
- `Makefile.inc`: MIC_PREFIX 从 `/usr/linux-k1om-4.7/...` 改为 MPSS 3.8.6 路径
- `LOAD_ICC_MODULE` / `LOAD_GCC_MODULE`: 改为直接 export PATH
- `tests/saxpy/kernel_simple.cpp`: 移除 TBB 依赖，改用 OpenMP

### 2.2 运行时测试

#### Native 模式 (mic0)

```bash
./test
# 输出: /tmp/offload_xxx/offload_target_main: cannot execute binary file
# 原因: emulator COI 在本地 fork/exec 运行 k1om 二进制，x86-64 无法执行
```

#### Emulation 模式

```bash
export OFFLOAD_EMUL_RUN=""
./test
# 输出: 同样错误
# 原因: offload_target_main 始终是 k1om 架构
```

---

## 3. 核心问题分析

### 3.1 根本原因: COI 实现缺失

liboffloadmic 项目 **只实现了 COI emulator**，没有真正的 Intel COI host 实现：

```
liboffloadmic/runtime/emulator/coi_host.cpp  ← 唯一的 COI 实现
liboffloadmic/runtime/emulator/coi_device.cpp
```

**Emulator 的工作方式** (`coi_host.cpp:972`):
```cpp
pid = vfork();
if (pid == 0) {
    execvpe(run_argv[0], run_argv, envp);  // 本地执行 k1om 二进制
}
```

Emulator 将 k1om 二进制写入 `/tmp/offload_xxx/offload_target_main`，然后通过 `vfork+execvpe` 在 **本地主机上执行**。这在 x86-64 主机上必然失败，因为 k1om 二进制无法直接执行。

### 3.2 主机缺少真正的 COI 库

Intel MPSS 3.8.6 **没有提供 host 端的 `libcoi_host.so`**：

```bash
find /opt/mpss/3.8.6 -name "libcoi_host.so*"   # 无结果
find /usr/lib64 -name "libcoi_host.so*"        # 无结果
rpm -ql mpss-daemon | grep coi                 # 无 host 库
```

MPSS 3.8.6 只在 k1om sysroot 中提供了 COI 头文件和设备端库，host 端缺少关键的 offload 运行时库。

### 3.3 offload_target_main 的架构问题

`offload_target_main` 只在 `build_mic`（k1om 交叉编译）中生成：

```
liboffloadmic/build_mic/ofldbegin.o    ← k1om
liboffloadmic/build_mic/ofldend.o      ← k1om
liboffloadmic/build_mic/plugin/offload_target_main  ← k1om (不存在于 build_host)
```

Host 端构建没有生成 x86-64 版本的 `offload_target_main`，因此 emulation 模式也无法工作。

---

## 4. 可行性结论

| 维度 | 评估 | 说明 |
|------|------|------|
| 构建可行性 | ⚠️ 部分可行 | 需要修改 Makefile 适配 MPSS 3.8.6 路径 |
| TBB 编译 | ❌ 不可行 | ICC 链接器路径与 MPSS 3.8.6 不兼容 |
| Native Offload | ❌ 不可行 | 缺少 Intel COI host 库 |
| Emulation Offload | ❌ 不可行 | Emulator 设计为本地执行 k1om 二进制，x86-64 不支持 |
| 整体可行性 | ❌ 不推荐 | 在当前 MPSS 3.8.6 环境中无法工作 |

### 为什么不可行？

1. **MPSS 3.8.6 缺少 host 端 COI 库**: Intel 在 MPSS 3.5+ 后改变了软件栈，host 端 offload 库不再作为独立 SO 分发
2. **liboffloadmic 的 emulator 是半成品**: 它假设有一个外部工具（如 QEMU）能运行 k1om 二进制，但当前环境没有这样的工具
3. **项目维护状态**: 最后更新约 2016 年，基于 GCC 5.3.0，与 MPSS 3.8.6 (GCC 5.1.1) 存在兼容性问题

---

## 5. 替代建议

| 替代方案 | 说明 |
|---------|------|
| **原生编译 + SSH** | `icc -mmic` 编译，scp 到 mic0，ssh 执行（当前项目已采用） |
| **OpenMP Offload (ICC)** | ICC 的 `#pragma offload` 指令，但需要 MPSS 的完整 COI 支持 |
| **Intel MKL / IPP** | 使用 Intel 优化库，通过 host 端 API 调用 |
| **SYCL / DPC++** | 不适用，KNC 不支持现代 oneAPI |
| **手动 SCIF 通信** | 直接调用 SCIF API 进行 host-device 数据传输 |

---

## 6. 参考文献

- liboffloadmic GitHub: https://github.com/apc-llc/liboffloadmic
- Intel MPSS 3.8.6 Documentation
- COI Emulator 源码分析: `liboffloadmic/runtime/emulator/coi_host.cpp`
