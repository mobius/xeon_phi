# Intel Xeon Phi 追加安装评估报告

> 在 3x NEC VE 1.0 方案基础上增加 1x Intel Xeon Phi  
> 评估时间: 2026-05-19

---

## 1. 可行性总览

| 维度 | 结论 | 说明 |
|------|------|------|
| **物理插槽** | ✅ 可行 | 4 个 x16 插槽刚好装 3 VE + 1 Phi |
| **功耗** | ⚠️ 紧张 | 整机 ~1550W，需 2000W+ 电源 |
| **散热** | ⚠️ 挑战 | 4 张加速卡热量巨大，需确认风道 |
| **驱动支持** | ⚠️ 社区补丁 | Intel 已停产，需 jjkeijser/ruribe17 社区驱动 |
| **软件栈兼容** | ❌ 冲突风险 | MPSS 与 VEOS 可能争夺内核资源 |
| **实际价值** | ❓ 存疑 | 编程模型完全不同，维护成本高 |

**总体结论**: **技术上可行，但性价比和实用性存疑。** 除非你已经持有这张卡且想物尽其用，否则不建议主动采购。

---

## 2. 物理可行性

### 2.1 插槽分配

```
CPU0 (NUMA0)                      CPU1 (NUMA1)
├─ 17:00.0  x16  VE #1 (新增)     ├─ ae:00.0  x16  VE #2 (新增)
├─ 3a:00.0  x16  (保留)           ├─ d7:00.0  x16  Xeon Phi (新增)
└─ 5d:00.0  x16  VE #0 (现有)     ├─ 80:00.0  x4   (不可用)
                                   └─ 85:00.0  x8   (已有设备)
```

**推荐分配**: Xeon Phi 装到 **d7:00.0 (CPU1 / NUMA1)**
- 与 VE 卡分散在不同 NUMA 节点，减少资源竞争
- CPU1 目前无加速卡，分担负载

### 2.2 功耗预算

| 组件 | 数量 | 单卡 TDP | 小计 |
|------|------|---------|------|
| NEC VE 1.0 | 3 | ~300W | **900W** |
| Intel Xeon Phi | 1 | ~225-300W | **~250W** |
| Xeon Gold 6252 | 2 | ~150W | 300W |
| 系统其余 | 1 | ~100W | 100W |
| **总计** | | | **~1550W** |

**电源要求**: **2000W+**（留 20% 余量）

⚠️ 如果当前电源 < 1600W，**必须升级电源**。

### 2.3 散热

- 4 张全高全长双宽加速卡 = 机箱内几乎塞满
- Xeon Phi 采用涡轮散热（排风向后），与 VE 卡方向一致
- 需确保机箱有足够进风空间，否则 thermal throttling

---

## 3. 软件可行性 — 关键发现

### 3.1 官方支持已终止

Intel 于 **2019 年**停止 Xeon Phi (KNC) 支持：
- 最后官方 MPSS 版本: **3.8.6**
- 官方支持的最后系统: **RHEL 7.4** (kernel 3.10.0-693)
- **完全不支持 RHEL 8 / Rocky 8**

### 3.2 社区驱动 — 重要发现 ✅

搜索发现**两个活跃社区项目**提供了 Rocky/Alma Linux 8.10 支持：

#### 项目 A: ruribe17 (AlmaLinux 8.10)

- 仓库: `Xeon-Phi-5110P-KNC-Drivers-Utilities-for-AlmaLinux-8.1`
- 提供 **预编译 RPM** 匹配内核 `4.18.0-553.x`
- 与你的系统内核 **4.18.0-553.124** 高度接近

```bash
# 预编译 RPM 示例
mpss-modules-4.18.0-553.40.1.el8_10.x86_64-3.8.6-8.x86_64.rpm
```

⚠️ 注意: 你的内核是 `.124` 后缀，预编译的 `.40` RPM **可能不直接匹配**，需要下载 SRPM 重新编译。

#### 项目 B: jjkeijser (CentOS/RHEL 8.2-8.5)

- 仓库: `jjkeijser/mpss`
- 长期维护的社区补丁
- 已验证内核: `4.18.0-193` 到 `4.18.0-348`
- 包含 `mic.ko` 内核模块 + `mpss-daemon` 用户态工具

**已知工作项**:
- SSH 登录卡内 Linux (`ssh mic0`)
- SCIF 卸载通信
- OpenCL 卸载
- 多卡支持（需 patch）

**已知问题**:
- NetworkManager 不兼容（需 `network-scripts` 包）
- 多卡时 `micctrl` 可能重复列设备（有 patch 修复）

### 3.3 驱动安装预估步骤

```bash
# 1. 安装依赖
sudo yum install -y network-scripts rpm-build kernel-devel-$(uname -r)

# 2. 下载社区 SRPM
wget https://github.com/jjkeijser/mpss/releases/download/.../mpss-modules-3.8.6-7.src.rpm

# 3. 重新编译内核模块（匹配当前内核）
rpm -ivh mpss-modules-3.8.6-7.src.rpm
cd ~/rpmbuild/SPECS
# 修改 spec 文件中的内核版本为 $(uname -r)
rpmbuild -bb mpss-modules-3.8.6-rhel85.spec

# 4. 安装编译好的 RPM
sudo rpm -ivh ~/rpmbuild/RPMS/x86_64/mpss-modules-$(uname -r)-3.8.6-*.x86_64.rpm

# 5. 安装用户态工具
sudo rpm -ivh mpss-daemon-3.8.6-4.el8.x86_64.rpm

# 6. 启动服务
sudo systemctl start mpss

# 7. 检查状态
micctrl --status
```

**预估工作量**: 2-4 小时（熟悉 Linux 驱动编译的话）

---

## 4. 与 NEC VE 混用的挑战

### 4.1 内核资源竞争

| 资源 | VEOS (NEC) | MPSS (Intel) | 冲突风险 |
|------|-----------|-------------|---------|
| 内核模块 | `ve_drv.ko`, `vp.ko` | `mic.ko` | 低（不同子系统） |
| 巨页内存 | `ve-set-hugepages.service` | MPSS 也使用 hugepages | **中** |
| 设备文件 | `/dev/ve*`, `/dev/veslot*` | `/dev/mic*`, `/dev/scif*` | 无 |
| 网络接口 | 无 | `mic0`, `mic1`... | 无 |

### 4.2 内存竞争

Xeon Phi KNC 的卸载模型需要主机分配 **SCIF 缓冲区**：
- 默认可能占用数 GB 主机内存
- 加上 VEOS 的 hugepages（已配置）
- 当前 62GB 内存将更加紧张

**强烈建议**: 装 Phi 前先将主机内存升级到 **256GB**。

### 4.3 编程模型完全不同

| 维度 | NEC VE | Xeon Phi KNC |
|------|--------|-------------|
| 架构 | 专用向量处理器 (CISC) | 众核 x86 (MIC) |
| 核心数 | 1 向量核心 | 61~68 轻量核心 |
| 线程模型 | 向量流水线 | 独立 x86 线程 |
| 编程方式 | `ncc` 编译，`ve_exec` 加载 | `icc` 编译，`micnativeloadex` 或 SSH |
| 向量指令 | 256 x 256-bit 向量寄存器 | AVX-512 (512-bit) |
| 内存模型 | 48GB HBM2 (统一寻址) | 8-16GB GDDR5 (独立 Linux) |
| 卡内系统 | 无（裸机运行） | 完整 Linux (uOS) |
| 通信方式 | VHcall / AVEO / VEDA | SCIF / COI / OpenCL |
| 编译器 | ncc / nc++ / nfort | icc / icpc / ifort (或 gcc) |
| MPI | NEC MPI (`mpicc -vh`) | Intel MPI (`mpiicc -mmic`) |

**核心矛盾**:
- 你的代码不能同时跑在 VE 和 Phi 上
- 需要两套完全不同的编译脚本、两套优化策略
- 维护成本翻倍

### 4.4 典型使用场景对比

```bash
# NEC VE: 编译 + 执行（主机控制一切）
ncc -O3 -o prog prog.c
ve_exec -N 0 ./prog

# Xeon Phi: 编译 + 上传 + 执行（卡内有自己的 Linux）
icc -mmic -O3 -o prog.mic prog.c
scp prog.mic mic0:/tmp/
ssh mic0 /tmp/prog.mic

# 或 offload 方式
micnativeloadex ./prog -d 0
```

---

## 5. Xeon Phi 型号建议

如果你**确定**要装，优先选择以下型号：

| 型号 | 代际 | 核心 | 内存 | TDP | 建议 |
|------|------|------|------|-----|------|
| **5110P** | KNC | 60 | 8GB | 225W | ✅ 最常见，社区支持最好 |
| **7120P** | KNC | 61 | 16GB | 300W | ✅ 内存大，但功耗高 |
| 7120X | KNC | 61 | 16GB | 300W | ⚠️ 需特殊散热 |
| 31S1P | KNC | 57 | 8GB | 215W | ✅ 被动散热版 |
| 7210 | KNL | 64 | 16GB | 245W | ❌ KNL 驱动更难找 |
| 7250 | KNL | 68 | 16GB | 245W | ❌ KNL 驱动更难找 |

**强烈推荐 KNC 系列（5110P/7120P）**，因为：
- 社区补丁成熟（jjkeijser 项目）
- KNL 的 MPSS 4.x 几乎没有 RHEL 8 支持

---

## 6. 如果坚持要装 — 完整 Checklist

### 硬件
- [ ] 确认机箱有 1 个空闲双宽插槽（在 ae/d7/17/3a 中）
- [ ] 确认电源 ≥ 2000W 或确认有冗余电源
- [ ] 确认机箱散热能承受 +250W
- [ ] 升级主机内存到 **256GB**

### BIOS
- [ ] Above 4G Decoding: Enabled
- [ ] SR-IOV: Enabled
- [ ] 目标插槽确认为 x16 模式

### 软件
- [ ] 安装 `kernel-devel-$(uname -r)`
- [ ] 安装 `rpm-build`, `network-scripts`
- [ ] 下载 jjkeijser 的 SRPM
- [ ] 重新编译 `mpss-modules` 匹配当前内核
- [ ] 安装 `mpss-daemon`
- [ ] 启动 `mpss` 服务
- [ ] 验证 `micctrl --status` 显示 `online`
- [ ] 测试 `ssh mic0`
- [ ] 测试卸载程序 (`micnativeloadex` 或 SCIF)

### 与 VE 共存验证
- [ ] 确认 `ve_drv` 模块仍能正常加载
- [ ] 确认 `vecmd state get` 显示 VE 卡 ONLINE
- [ ] 确认 `/dev/ve0` 存在且可访问
- [ ] 同时运行 VE 程序和 Phi 程序，观察稳定性

---

## 7. 最终建议

### 如果你已经持有这张 Xeon Phi 卡

> **可以尝试，但做好踩坑准备。**

- 社区驱动在 Rocky 8.10 上有成功先例
- 你的内核版本 4.18.0-553.124 需要重新编译驱动（不是即插即用）
- 装完后你同时维护两套完全不同的 HPC 软件栈

### 如果你还没有这张卡，正考虑购买

> **强烈不建议。**

理由:
1. **性能**: KNC 的 FP64 峰值 ~1.1 TFLOPS，远低于 VE 1.0 的 ~1.3 TFLOPS
2. **生态**: Intel 已彻底放弃，社区维护力度有限
3. **功耗**: 225-300W 换来 1.1 TFLOPS，能效比差
4. **复杂度**: 两套软件栈的维护成本远超单卡价值
5. **内存**: 8-16GB GDDR5 vs VE 的 48GB HBM2

### 替代方案

如果预算允许，**第四张卡选择 NEC VE 1.0**：
- 同构系统，软件栈统一
- 4 张 VE = ~5.2 TFLOPS FP64
- 无需额外学习成本
- 社区和支持生态一致

---

## 附录: 社区驱动资源

| 项目 | 地址 | 说明 |
|------|------|------|
| jjkeijser/mpss | github.com/jjkeijser/mpss | 最成熟的 KNC 社区补丁，支持 RHEL 8 |
| ruribe17/AlmaLinux-8.1 | github.com/ruribe17/Xeon-Phi-5110P-KNC-Drivers-Utilities-for-AlmaLinux-8.1 | 预编译 RPM，Alma/Rocky 8.10 |
| Keijser 文档 | jjkeijser.github.io/mpss | 详细安装指南 |
| Aurora Forum | www.hpc.nec/forums | NEC 社区论坛 |
