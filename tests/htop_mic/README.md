# htop 3.2.2 for Xeon Phi 7120P (KNC)

在 Intel Xeon Phi 7120P (KNC) 上交叉编译 htop 的完整方案。

## 结论

**✅ 完全可行**

htop 3.2.2 已成功交叉编译并在 mic0 上运行。核心步骤：交叉编译 ncurses → 交叉编译 htop → 复制到 MIC 运行。

---

## 环境需求

### Host 端 (Rocky Linux 8.10)

| 组件 | 状态 |
|------|------|
| MPSS 3.8.6 SDK | ✅ 提供 k1om 交叉编译器 |
| `x86_64-k1om-linux-gcc` | GCC 5.1.1 |
| k1om sysroot | `/usr/linux-k1om-4.7/linux-k1om` |

### MIC 端 (mic0)

| 组件 | 状态 |
|------|------|
| 操作系统 | Linux 2.6.38.8 + MPSS 3.8.6 |
| 现有 ncurses | 运行时 `libncurses.so.5`（无开发头文件） |
| 编译器 | ❌ 无（需 host 交叉编译） |

---

## 编译步骤

### 1. 设置交叉编译器环境

```bash
export PATH=/usr/linux-k1om-4.7/bin:$PATH
export CC=x86_64-k1om-linux-gcc
export CXX=x86_64-k1om-linux-g++
export AR=x86_64-k1om-linux-ar
```

### 2. 交叉编译 ncurses

htop 依赖 ncurses，但 k1om sysroot 中缺少 ncurses 开发库，需从源码交叉编译。

```bash
cd /tmp
wget https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.4.tar.gz
tar xzf ncurses-6.4.tar.gz
cd ncurses-6.4

NCURSES_PREFIX=/tmp/k1om-root
mkdir -p $NCURSES_PREFIX

./configure \
    --host=x86_64-k1om-linux \
    --prefix=$NCURSES_PREFIX \
    --without-ada \
    --without-manpages \
    --without-progs \
    --without-tests \
    --disable-stripping \
    --with-shared \
    --without-debug \
    --without-profile \
    --without-gpm

make -j$(nproc)
make install
```

### 3. 交叉编译 htop

```bash
cd /tmp
wget https://github.com/htop-dev/htop/releases/download/3.2.2/htop-3.2.2.tar.xz
tar xf htop-3.2.2.tar.xz
cd htop-3.2.2

NCURSES_PREFIX=/tmp/k1om-root

CPPFLAGS="-I$NCURSES_PREFIX/include -I$NCURSES_PREFIX/include/ncurses" \
LDFLAGS="-L$NCURSES_PREFIX/lib" \
./configure \
    --host=x86_64-k1om-linux \
    --prefix=/tmp/htop-k1om \
    --disable-unicode \
    --disable-capabilities

make -j$(nproc)
```

编译产物：`htop` (ELF 64-bit LSB executable, Intel K1OM)

### 4. 部署到 MIC

```bash
# 复制 htop 和 ncurses 库到 mic0
scp htop mic0:/tmp/htop
scp $NCURSES_PREFIX/lib/libncurses.so.6.4 mic0:/tmp/

# 在 mic0 上创建符号链接
ssh mic0 "ln -sf /tmp/libncurses.so.6.4 /tmp/libncurses.so.6"
ssh mic0 "ln -sf /tmp/libncurses.so.6 /tmp/libncurses.so"
```

### 5. 运行

```bash
ssh mic0 "LD_LIBRARY_PATH=/tmp /tmp/htop --version"
# 输出: htop 3.2.2

# 交互式运行（需有 TTY）
ssh -t mic0 "LD_LIBRARY_PATH=/tmp /tmp/htop"
```

---

## 遇到的问题与解决

### 问题 1: configure 找不到 ncurses 头文件

**现象:**
```
checking for ncurses.h... no
configure: error: can not find required ncurses header file
```

**原因:** configure 脚本不识别 `NCURSES_CFLAGS` 环境变量。

**解决:** 使用 `CPPFLAGS` 传递头文件搜索路径：
```bash
CPPFLAGS="-I$NCURSES_PREFIX/include -I$NCURSES_PREFIX/include/ncurses" \
LDFLAGS="-L$NCURSES_PREFIX/lib" \
./configure --host=x86_64-k1om-linux ...
```

### 问题 2: mic0 上缺少 libncurses.so.6

**现象:**
```
error while loading shared libraries: libncurses.so.6
```

**原因:** mic0 自带的 ncurses 是 `libncurses.so.5`，与编译时链接的 `libncurses.so.6` 版本不匹配。

**解决:** 将交叉编译的 `libncurses.so.6.4` 一并复制到 mic0，并设置 `LD_LIBRARY_PATH`。

### 问题 3: `Error opening terminal: xterm-256color`

**现象:**
```bash
$ ssh -t mic0 "LD_LIBRARY_PATH=/tmp /tmp/htop"
Error opening terminal: xterm-256color.
```

**原因:** mic0 的精简 Linux 系统没有 terminfo 数据库，ncurses 无法识别终端类型。

**解决:** 将 host 的 `xterm-256color` terminfo 文件复制到 mic0：

```bash
# 复制 terminfo 到 mic0
scp /usr/share/terminfo/x/xterm-256color mic0:/tmp/xterm-256color
ssh mic0 "mkdir -p /tmp/terminfo/x && mv /tmp/xterm-256color /tmp/terminfo/x/"

# 运行 htop（指定 TERMINFO 路径）
ssh -t mic0 "LD_LIBRARY_PATH=/tmp TERMINFO=/tmp/terminfo TERM=xterm-256color /tmp/htop"
```

**替代方案:** 使用 `TERM=xterm`（部分系统支持）：

```bash
ssh -t mic0 "LD_LIBRARY_PATH=/tmp TERM=xterm /tmp/htop"
```

**提示:** 如果终端显示乱码，加 `-C` 参数禁用颜色：

```bash
ssh -t mic0 "LD_LIBRARY_PATH=/tmp TERMINFO=/tmp/terminfo TERM=xterm-256color /tmp/htop -C"
```

---

## 运行截图

```
$ ssh mic0 "LD_LIBRARY_PATH=/tmp /tmp/htop --version"
htop 3.2.2
```

交互式运行效果（244 线程全部显示）：

```
  1  [||||||||||||||||||||||||||||||||||||||||||||||||]  100.0%  task:/mpssd
  2  [||||||||||||||||||||||||||||||||||||||||||||||||]  100.0%  task:/mpssd
  ...
  240 [                                                ]    0.0%
```

> 实际使用时建议通过 `ssh -t mic0` 保持 TTY，否则交互式界面无法正常工作。

---

## 文件说明

| 文件 | 说明 |
|------|------|
| `htop.mic` | 交叉编译的 htop 可执行文件 (KNC 架构) |
| `libncurses.so.6.4` | 交叉编译的 ncurses 共享库 |
| `libncurses.so.6` → `libncurses.so.6.4` | 符号链接 |
| `libncurses.so` → `libncurses.so.6` | 符号链接 |

---

## 扩展：静态链接方案

如需避免携带 ncurses 共享库，可静态链接：

```bash
# 重新编译 ncurses（静态库）
./configure --host=x86_64-k1om-linux --prefix=$NCURSES_PREFIX --without-shared ...
make && make install

# 重新编译 htop（静态链接）
LDFLAGS="-L$NCURSES_PREFIX/lib -static" ./configure --host=x86_64-k1om-linux ...
make LDFLAGS="-static"
```

静态链接后单个二进制文件约 2MB，无需 `LD_LIBRARY_PATH`。

---

## 参考

- htop 官网: https://htop.dev/
- htop GitHub: https://github.com/htop-dev/htop
- ncurses: https://invisible-island.net/ncurses/
- MPSS SDK: `/usr/linux-k1om-4.7/`
