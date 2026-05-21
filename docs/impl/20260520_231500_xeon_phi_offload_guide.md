# Xeon Phi 7120P Offload 编程指南

**文档日期**: 2026-05-20  
**适用硬件**: Intel Xeon Phi 7120P (KNC, 61 cores, 16GB GDDR5)  
**Host系统**: Rocky Linux 8.10 (kernel 4.18.0-553)  
**MPSS版本**: 3.8.6  
**编译器**: Intel ICC 16.0

---

## 1. 环境概述

### 1.1 硬件拓扑

```
Host: Rocky Linux 8.10, Xeon Gold 6252 x2, 192GB
    |
    | PCIe x16
    v
MIC0: Xeon Phi 7120P (KNC)
      - 61 cores @ 1.238 GHz
      - 244 hardware threads
      - 16GB GDDR5
      - OS: Linux 2.6.38.8+mpss3.8.6
```

### 1.2 关键发现

| 发现 | 状态 | 说明 |
|------|------|------|
| MPSS 3.8.6 COI 库 | ✅ 可用 | 补装 `mpss-core`, `mpss-offload`, `libscif0`, `libmicmgmt0` 后正常工作 |
| ICC OpenMP offload | ✅ 已验证 | `#pragma omp target` 可正常 offload 到 mic0 |
| Intel LEO offload | ✅ 已验证 | `#pragma offload target(mic)` 可正常工作 |
| `micnativeloadex` | ✅ 已验证 | 官方工具，自动处理 MIC 库依赖 |
| 手动 `-mmic` + SSH | ✅ 已验证 | 峰值测试基准，575 GFLOPS (FP64) |
| liboffloadmic | ❌ 不可行 | GCC 实验项目，与 ICC 不兼容，仅实现 emulator |

---

## 2. MPSS 环境配置

### 2.1 必需安装的 MPSS 包

从 `mpss-3.8.6-linux.tar` 中提取并安装：

```bash
# 核心包（必须）
sudo rpm -ivh --nodeps mpss-core-3.8.6-1.glibc2.12.x86_64.rpm
sudo rpm -ivh --nodeps mpss-daemon-3.8.6-4.el8.x86_64.rpm
sudo rpm -ivh --nodeps mpss-modules-4.18.0-553.*.x86_64-3.8.6-8.x86_64.rpm
sudo rpm -ivh --nodeps mpss-coi-3.8.6-1.glibc2.12.x86_64.rpm
sudo rpm -ivh --nodeps mpss-offload-3.8.6-1.glibc2.12.x86_64.rpm
sudo rpm -ivh --nodeps libscif0-3.8.6-1.glibc2.12.x86_64.rpm
sudo rpm -ivh --nodeps mpss-micmgmt-3.8.6-1.glibc2.12.x86_64.rpm
sudo rpm -ivh --nodeps glibc2.12pkg-libmicmgmt0-3.8.6-1.glibc2.12.x86_64.rpm

# 可选包
sudo rpm -ivh --nodeps mpss-myo-3.8.6-1.glibc2.12.x86_64.rpm
sudo rpm -ivh --nodeps mpss-sdk-k1om-3.8.6-1.x86_64.rpm
sudo rpm -ivh --nodeps mpss-boot-files-3.8.6-1.glibc2.12.x86_64.rpm
```

### 2.2 服务管理

```bash
# 启动 MPSS
sudo systemctl start mpss
sudo systemctl enable mpss

# 检查状态
systemctl status mpss
micctrl --status
micinfo
```

### 2.3 验证 COI 设备枚举

```bash
# COI 应返回 count=1
cat > /tmp/test_coi.c << 'EOF'
#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
typedef enum { COI_ISA_MIC = 2 } COI_ISA_TYPE;
int main() {
    void *h = dlopen("libcoi_host.so.0", RTLD_NOW);
    typedef int (*fn_t)(COI_ISA_TYPE, unsigned int*);
    fn_t fn = (fn_t)dlvsym(h, "COIEngineGetCount", "COI_1.0");
    unsigned int count = 0;
    int ret = fn(COI_ISA_MIC, &count);
    printf("ret=%d count=%u\n", ret, count);
    dlclose(h);
    return 0;
}
EOF
gcc -o /tmp/test_coi /tmp/test_coi.c -ldl
/tmp/test_coi
# 预期输出: ret=0 count=1
```

---

## 3. 编程模型选择

### 3.1 三种可用方案对比

| 方案 | 编程粒度 | 适用场景 | 复杂度 |
|------|---------|---------|--------|
| **OpenMP offload** | 声明式，编译器自动处理 | 通用并行计算，快速移植 | 低 |
| **Intel LEO `#pragma offload`** | 显式数据映射 | 需要精确控制数据传输时 | 中 |
| **手动 KNC intrinsic** | 指令级控制 | 极致性能优化，内核开发 | 高 |

### 3.2 推荐选择

- **快速开发/原型验证** → OpenMP offload
- **已有 Cilk/Intel 代码迁移** → Intel LEO
- **峰值性能优化** → 手动 KNC intrinsic (`_mm512_*`)

---

## 4. OpenMP Offload 使用说明

### 4.1 编译环境

ICC 16.0 安装在 podman 容器 `centos7-phi-dev` 中：

```bash
# 进入容器编译
podman exec -it centos7-phi-dev bash
source /opt/intel/bin/compilervars.sh intel64

# 编译 OpenMP offload 程序
icc -qopenmp -qoffload=optional -o myapp myapp.c
```

编译选项说明：
| 选项 | 含义 |
|------|------|
| `-qopenmp` | 启用 OpenMP |
| `-qoffload=optional` | 启用 offload，若 MIC 不可用则回退到 CPU |
| `-qoffload=mandatory` | 强制 offload，MIC 不可用时报错 |

### 4.2 基本语法

```c
#include <omp.h>

int main() {
    // 查询设备数量
    int num_dev = omp_get_num_devices();
    printf("Devices: %d\n", num_dev);
    
    int result = 0;
    
    // 简单 offload
    #pragma omp target map(tofrom: result)
    {
        result = 42;
    }
    
    // 数组运算 offload
    double a[1024], b[1024], c[1024];
    // ... 初始化 a, b ...
    
    #pragma omp target map(to: a[0:1024], b[0:1024]) \
                        map(from: c[0:1024])
    {
        for (int i = 0; i < 1024; i++) {
            c[i] = a[i] + b[i];
        }
    }
}
```

### 4.3 运行程序

```bash
# 设置 MIC 库路径（必须）
# 指向 HOST 端的 MIC 库目录，liboffload.so.5 会自动复制到 mic0
export MIC_LD_LIBRARY_PATH="/opt/intel/compilers_and_libraries_2016.0.109/linux/compiler/lib/intel64_lin_mic"
export OFFLOAD_ENABLE_ORSL=0

# 运行
./myapp
```

**关键**: `MIC_LD_LIBRARY_PATH` 必须指向 **host 端**的 MIC 库（`intel64_lin_mic/`），而非 mic0 上的路径。ICC 运行时会自动将所需库复制到 coprocessor。

### 4.4 环境变量速查

| 变量 | 作用 |
|------|------|
| `MIC_LD_LIBRARY_PATH` | Host 端 MIC 库搜索路径，运行时自动复制到 mic0 |
| `OFFLOAD_ENABLE_ORSL=0` | 禁用 ORSL（Offload Resource Scheduling Layer），避免初始化问题 |
| `OFFLOAD_DEBUG=3` | 启用 offload 调试输出 |

---

## 5. Intel LEO (`#pragma offload`) 使用说明

### 5.1 编译

```bash
icc -qoffload=optional -o myapp myapp.c
```

### 5.2 语法

```c
int result = 0;

#pragma offload target(mic:0) out(result)
{
    result = 42;
}

// 数组传输
#pragma offload target(mic:0) \
    in(a[0:1024]: alloc_if(1) free_if(0)) \
    in(b[0:1024]: alloc_if(1) free_if(0)) \
    out(c[0:1024]: alloc_if(0) free_if(1))
{
    for (int i = 0; i < 1024; i++) {
        c[i] = a[i] + b[i];
    }
}
```

### 5.3 数据子句

| 子句 | 作用 |
|------|------|
| `in(...)` | 只传 host → mic |
| `out(...)` | 只传 mic → host |
| `inout(...)` | 双向传输 |
| `alloc_if(1)` | 首次在 mic 上分配内存 |
| `free_if(1)` | 结束后释放 mic 内存 |
| `nocopy(...)` | 声明已在 mic 上的数据 |

---

## 6. `micnativeloadex` 使用说明

用于直接在 MIC 上运行预编译的 k1om 二进制文件。

### 6.1 编译 k1om 二进制

```bash
# 在容器中
icc -mmic -O3 -o myapp.mic myapp.c
```

### 6.2 运行

```bash
# 方法1: micnativeloadex（自动复制依赖库）
micnativeloadex myapp.mic -d 0 -t 60

# 方法2: 指定超时和环境变量
micnativeloadex myapp.mic -d 0 -t 10 -e "OMP_NUM_THREADS=244"

# 方法3: 仅列出依赖（不执行）
micnativeloadex myapp.mic -d 0 -l
```

### 6.3 手动 SCP + SSH 运行

```bash
# 复制到 mic0
scp myapp.mic mic0:/tmp/

# 复制依赖库（如有）
scp /opt/intel/compilers_and_libraries_2016.0.109/linux/compiler/lib/intel64_lin_mic/libiomp5.so mic0:/tmp/

# 运行
ssh mic0 "LD_LIBRARY_PATH=/tmp /tmp/myapp.mic"
```

---

## 7. 手动 KNC Intrinsic 编程

### 7.1 编译

```bash
icc -std=c99 -mmic -O3 -openmp -o myapp.mic myapp.c
```

### 7.2 核心语法

```c
#include <immintrin.h>

// 512-bit 双精度浮点向量
__m512d va = _mm512_set1_pd(1.0);
__m512d vb = _mm512_set1_pd(2.0);
__m512d vc = _mm512_fmadd_pd(va, vb, vc);  // c = a*b + c
```

### 7.3 关键编译标志

| 标志 | 作用 |
|------|------|
| `-mmic` | 编译 KNC 目标代码 |
| `-O3` | 最高优化级别 |
| `-openmp` | 启用 OpenMP 并行 |
| `-fno-alias` | 假设无指针别名，帮助向量化 |

---

## 8. 峰值性能参考

在 Xeon Phi 7120P 上验证的基准：

| 测试 | 实测性能 | 理论峰值 | 效率 |
|------|---------|---------|------|
| FP64 FMA | 575 GFLOPS | 1,208 GFLOPS | 47.6% |
| FP32 FMA | 1,170 GFLOPS | 2,416 GFLOPS | 48.4% |
| STREAM Copy | 157 GB/s | 352 GB/s | 44.7% |
| DGEMM 2048 | 63 GFLOPS | 1,208 GFLOPS | 5.2% |

---

## 9. 故障排查

### 9.1 "Error enumerating coprocessors"

**原因**: MPSS 安装不完整  
**解决**: 安装 `mpss-core`, `mpss-offload`, `libscif0`, `libmicmgmt0`

### 9.2 "cannot start process on the device (error code 19)"

**原因**: mic0 上缺少 ICC 运行时库  
**解决**: 设置 `MIC_LD_LIBRARY_PATH` 指向 host 端的 `intel64_lin_mic/` 库目录

### 9.3 "libiomp5.so: cannot open shared object file"

**原因**: host 端缺少 ICC 运行时库  
**解决**: 从容器中复制到 host `/usr/lib64/` 或设置 `LD_LIBRARY_PATH`

### 9.4 容器内无法发现 MIC 设备

**原因**: 容器没有 `/dev/mic*` 设备访问权限  
**解决**: 在 host 上编译和运行 offload 程序（推荐），或重新创建特权容器：

```bash
podman run --privileged \
    --device /dev/mic0 \
    --device /dev/mic/ctrl \
    --device /dev/mic/scif \
    -v /sys/class/mic:/sys/class/mic \
    -v /tmp:/tmp \
    centos:7
```

---

## 10. 文件清单

| 文件 | 说明 |
|------|------|
| `phi_peak_fp64.c` | FP64 FMA 峰值测试 |
| `phi_peak_fp32.c` | FP32 FMA 峰值测试 |
| `phi_stream_bench.c` | STREAM 带宽测试 |
| `phi_peak_dgemm.c` | DGEMM 基准测试 |
| `saxpy_bench.c` / `saxpy_kernel.c` | SAXPY 基准 |
| `matmul_bench.c` | 矩阵乘法基准 |
| `docs/research/20260520_071200_liboffloadmic_assessment.md` | liboffloadmic 评估 |
| `docs/research/20260520_214838_liboffloadmic_detailed_assessment.md` | 详细评估 |
| `docs/impl/20260520_231500_xeon_phi_offload_guide.md` | 本指南 |

---

## 11. 参考资料

- Intel MPSS 3.8.6 文档: `/opt/mpss/3.8.6/docs/`
- COI API 文档: `mpss-coi-doc-3.8.6-1.glibc2.12.x86_64.rpm`
- SCIF 用户指南: `mpss-3.8.6/docs/SCIF_UserGuide.pdf`
- Intel ICC 16.0 文档: `/opt/intel/documentation_2016/`
