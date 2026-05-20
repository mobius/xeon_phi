# 实施日志

> 时间: 2026-05-19 23:14
> 阶段: Phase 1-3 完成, Phase 4 待进行

## 环境快照

Host: Rocky Linux 8.10, kernel 4.18.0-553.124.1.el8_10.x86_64
CPU: 2x Xeon Gold 6252, RAM 62GB (768x 2MB hugepages)

PCIe:
- 3b:00.0  NEC VE 1.0 (VE #0)
- 5e:00.0  Intel Xeon Phi 7120P
- af:00.0  NEC VE 1.0 (VE #1)
- d8:00.0  NEC VE 1.0 (VE #2)

内核模块: mic, ve_drv, vp

## Phase 1: 驱动安装 - 完成

| 包 | 版本 | 来源 |
|----|------|------|
| mpss-modules | 4.18.0-553.124.1.el8_10-3.8.6-8 | jjkeijser |
| mpss-daemon | 3.8.6-4.el8 | jjkeijser |
| mpss-boot-files | 3.8.6-1 | 官方 tar 提取 |

## Phase 2: 卡初始化 - 完成

问题及解决：
1. bzImage not found → 从 tar 提取 boot-files RPM
2. mic0 接口 DOWN → 手动 ip link set mic0 up
3. SSH Permission denied → chmod 400 shadow + updateramfs

卡内状态:
- 内核: Linux 2.6.38.8+mpss3.8.6 k1om
- IP: 172.31.1.1, 主机名: g4-mic0
- 内存: 15513 MB
- 核心: 244 逻辑 (61 物理 x 4 SMT), 1238 MHz
- SSH: 密码 + 密钥双认证

## Phase 3: 环境验证 - 完成

- 核心数: 244 逻辑核心
- 主频: 1238.094 MHz
- 工具链: GCC 5.1.1 交叉编译器
- Hello World: 编译运行通过

工具链路径:
- gcc: /usr/linux-k1om-4.7/bin/x86_64-k1om-linux-gcc
- sysroot: /opt/mpss/3.8.6/sysroots/k1om-mpss-linux/

验证命令:
```
/usr/linux-k1om-4.7/bin/x86_64-k1om-linux-gcc \
    --sysroot=/opt/mpss/3.8.6/sysroots/k1om-mpss-linux \
    -o hello_phi hello_phi.c
scp hello_phi mic0:/tmp/ && ssh mic0 /tmp/hello_phi
```

## Phase 4: 待进行

- 温度传感器定位
- SCIF 通信测试
- 计算性能基准
- VE 卡共存验证

## 遗留问题

1. 网络接口持久化 (重启需手动 bring up)
2. 温度监控方式待确认
3. BootOnStart 自动启动待修复
