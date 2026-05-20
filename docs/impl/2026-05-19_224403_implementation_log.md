# 实施日志

> 时间: 2026-05-19 22:44
> 阶段: Phase 1-2 完成, Phase 3 进行中

## 环境快照

```
Host:    Rocky Linux 8.10
Kernel:  4.18.0-553.124.1.el8_10.x86_64
CPU:     2x Xeon Gold 6252
RAM:     62GB (768x 2MB hugepages 预分配)
PSU:     待确认

PCIe 设备:
  3b:00.0  NEC VE 1.0 (VE #0)
  5e:00.0  Intel Xeon Phi 7120P
  af:00.0  NEC VE 1.0 (VE #1)
  d8:00.0  NEC VE 1.0 (VE #2)

内核模块:
  mic     765952  0
  ve_drv  266240  24
  vp       20480  1 ve_drv
```

## Phase 1: 驱动安装

### 已安装 RPM

| 包名 | 版本 | 来源 |
|------|------|------|
| `mpss-modules` | 4.18.0-553.124.1.el8_10-3.8.6-8 | jjkeijser 社区 |
| `mpss-daemon` | 3.8.6-4.el8 | jjkeijser 社区 |

### 启动文件安装

**问题**: `/usr/share/mpss/boot/` 目录不存在。

**解决**: 从 `mpss-3.8.6-linux.tar` 提取 `mpss-boot-files` RPM。

## Phase 2: 卡初始化

### 遇到并解决的问题

| # | 问题 | 根因 | 解决 |
|---|------|------|------|
| 1 | bzImage not found | `mpss-boot-files` 未安装 | 从 tar 提取 RPM |
| 2 | mic0 接口 DOWN | Rocky 8 NM 与旧 ifup 不兼容 | `ip link set mic0 up` |
| 3 | SSH Permission denied | shadow 权限 000 | chmod 400 + updateramfs |
| 4 | 重启后网络丢失 | mpss.redhat 脚本失败 | 后续持久化 |

### 当前卡内状态

- 卡内核: Linux 2.6.38.8+mpss3.8.6 k1om
- 主机名: g4-mic0
- IP: 172.31.1.1
- 内存: 15513 MB
- 核心: 244 逻辑核心 (61 物理 x 4-way SMT)
- SSH: 密码 + 密钥双认证

## Phase 3: 进行中

- [x] 核心数确认: 244 逻辑核心
- [ ] 主频确认
- [ ] 温度传感器定位
- [ ] SCIF 功能测试
- [ ] VE 共存压力测试

## 已知遗留问题

1. 网络接口持久化: 重启后需手动 bring up mic0
2. 温度监控: 卡内无标准 thermal sysfs
3. 自动启动: BootOnStart 配置正确但需网络配合
