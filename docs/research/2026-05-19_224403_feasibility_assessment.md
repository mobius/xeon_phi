# 可行性评估总结

> 时间: 2026-05-19 22:44
> 状态: 已完成 → 转入实施

## 背景

在现有 3x NEC VE 1.0 方案基础上，增加 1x Intel Xeon Phi 7120P (Knights Corner)。

## 关键发现

### 1. 硬件平台

| 组件 | 规格 |
|------|------|
| 主机 | 双路 Xeon Gold 6252, 62GB RAM |
| OS | Rocky Linux 8.10, kernel 4.18.0-553.124.1 |
| VE 卡 | 3x NEC Vector Engine 1.0 (3b:00.0, af:00.0, d8:00.0) |
| Phi 卡 | 1x Xeon Phi 7120P (5e:00.0), 61 核, 16GB GDDR5, 300W TDP 被动散热 |

### 2. 软件栈选择

- **MPSS 版本**: 3.8.6（Intel 最后官方版本）
- **驱动来源**: 社区维护 — jjkeijser/mpss 预编译 RPM
  - `mpss-modules-4.18.0-553.124.1.el8_10-3.8.6-8.x86_64`
  - `mpss-daemon-3.8.6-4.el8.x86_64`
- **启动文件**: 从官方 `mpss-3.8.6-linux.tar` 中提取 `mpss-boot-files-3.8.6-1.glibc2.12.x86_64.rpm`

### 3. 风险矩阵

| 风险 | 严重度 | 状态 |
|------|--------|------|
| 300W 被动散热 | 高 | 需持续监控 |
| 整机功耗 ~1600W | 高 | 需确认电源余量 |
| VE/Phi 内核模块冲突 | 中 | 已验证共存 |
| 内存压力 (62GB → 需 256GB) | 中 | 后续升级 |
| 编程模型完全不兼容 | 低 | 接受，分场景使用 |

### 4. 决策

**继续实施。** 卡已持有，硬件已插入，软件栈可行。

## 参考

- `Xeon_Phi_Addon_Assessment.md` — 完整评估报告
- `Xeon_Phi_7120P_Specific_Assessment.md` — 7120P 专项评估
