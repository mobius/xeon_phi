# ICC -mmic 快速参考

## 环境

| 组件 | 路径 |
|------|------|
| ICC | /opt/intel/bin/icc (16.0.0) |
| 容器 | centos7-phi-dev (CentOS 7) |
| Sysroot | /opt/mpss/3.8.6/sysroots/k1om-mpss-linux |
| Binutils | /opt/mpss/3.8.6/.../k1om-mpss-linux/ |

## 使用

```bash
# 进入容器
podman exec -it centos7-phi-dev bash

# 设置环境
source /opt/mpss/3.8.6/environment-setup-k1om-mpss-linux
export PATH=/opt/intel/bin:/opt/mpss/3.8.6/sysroots/x86_64-mpsssdk-linux/usr/bin/k1om-mpss-linux:$PATH

# intrinsics 编译
icc -std=c99 -mmic -O3 -c kernel.c -o kernel.o

# 链接
icc -std=c99 -mmic -O3 -lpthread kernel.o bench.c -o prog.mic

# 部署
scp prog.mic mic0:/tmp/ && ssh mic0 /tmp/prog.mic
```

## 性能对比

| 方法 | 带宽 | GFLOPS |
|------|------|--------|
| GCC x87 | 7.9 GB/s | 0.66 |
| GCC 内联汇编 | 26.1 GB/s | 2.18 |
| ICC intrinsics | 27.3 GB/s | 2.27 |
