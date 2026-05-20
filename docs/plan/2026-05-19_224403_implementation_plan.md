# 实施计划

> 时间: 2026-05-19 22:44
> 状态: 进行中

## 阶段划分

### Phase 1 — 驱动安装 ✅

- [x] 安装社区内核模块 (`mpss-modules`)
- [x] 安装用户态守护进程 (`mpss-daemon`)
- [x] 安装启动文件 (`mpss-boot-files`)
- [x] 启动 `mpss.service`
- [x] 加载 `mic.ko` 内核模块

### Phase 2 — 卡初始化 ✅

- [x] PCIe 识别 (5e:00.0)
- [x] `micctrl --status` 显示 `ready`
- [x] 修复缺少 `bzImage-knightscorner` 问题
- [x] `micctrl --boot mic0` → `online`
- [x] 修复网络接口未激活 (手动 `ip link set mic0 up`)
- [x] 修复 `/etc/shadow` 权限 → SSH 密码登录
- [x] SSH 登录验证 (`ssh mic0`)

### Phase 3 — 卡环境验证 ⬅ 当前位置

- [ ] 核心数确认 (预期 244 逻辑核心, 61 物理核心)
- [ ] 内存确认 (预期 ~15.5 GB)
- [ ] 温度监控建立
- [ ] 卡内 gcc/工具链确认

### Phase 4 — 功能验证

- [ ] SCIF 通信测试
- [ ] 简单计算卸载测试 (native execution)
- [ ] 温度压力测试 (持续负载 10min)
- [ ] VE 卡共存验证 (同时运行 Phi + VE 程序)

### Phase 5 — 优化与收尾

- [ ] 设置开机自启 (`BootOnStart`)
- [ ] 网络接口持久化
- [ ] 温度告警脚本
- [ ] 编译工具链安装 (icc 或 gcc cross-compile)
