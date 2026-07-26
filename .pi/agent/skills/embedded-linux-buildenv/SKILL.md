---
name: embedded-linux-buildenv
description: 搭建 ARM 嵌入式 Linux 交叉编译环境（Docker 镜像双机同步、内核/U-Boot/Buildroot/Qt 一站式构建、hwt 覆盖层模式）。当为新芯片/新开发板搭建编译环境、交叉编译 Qt、定制 Buildroot rootfs，或排查目标机 LCD 显示错位/颜色异常、iconv_open failed、触摸无响应等问题时使用。
---

# 嵌入式 Linux 编译环境搭建

一套在 i.MX6ULL（正点原子）项目上完整验证过的环境搭建方法论，适用于其他 ARM 芯片/开发板。

## 核心架构模式

```
Windows 宿主机                          Docker 编译容器
─────────────────                      ─────────────────
image_sources/ (源码tarballs)  ──COPY──►  /root/source/（镜像内，推 SWR）
项目仓库/hwt/ (自定义覆盖层)   ──挂载──►  /workspace/hwt/
项目仓库/build/ (编译产物缓存) ──挂载──►  /workspace/build/
```

**五条核心原则**（换芯片时同样适用）：

1. **内核/U-Boot 用芯片厂商移植版，不追新**（i.MX6ULL 用 NXP 4.1.15 而非 mainline 6.6）
2. **Docker 基础镜像的"年龄"匹配芯片年龄**（老芯片 → Ubuntu 18.04 + 老 Linaro 工具链；32 位工具链需 `dpkg --add-architecture i386` + libc6:i386/libstdc++6:i386/lib32z1）
3. **一切自定义走 hwt 覆盖层**：构建时把 `hwt/` 下的 defconfig/dts/板级文件 cp 覆盖到解压后的纯净源码，绝不直接改原始源码包
4. **构建脚本 Target 制**：`linux/uboot/app/qt/rootfs/image/menuconfig_*` 分目标编译；产物落到挂载卷 `build/` 持久化；`/tmp` 只做容器内临时编译（IO 快 5~10 倍）
5. **镜像即环境**：源码 tarballs + 工具链全部打进镜像推 SWR，第二台机器 `docker pull` 即同步，唯一需要 git 同步的只有 hwt/ 和脚本

## 新芯片移植清单

1. 收集厂商 SDK：kernel/uboot/buildroot 源码 tarballs + 厂商交叉工具链
2. 选 Docker 基础镜像（按工具链要求：i686 工具链→加 i386 库；python2 需求→≤18.04）
3. 写 Dockerfile（COPY tarballs 到 /root/source/、装工具链、apt 装 bison/flex/libssl-dev/bc/lzop/libncurses5-dev/cpio/rsync/pkg-config/mtools/dosfstools）
4. 建 hwt 覆盖层：厂商 defconfig 为底做 hwt 版、dts 按板子改、rootfs-overlay 放 hostname/issue/profile.d
5. 写统一构建脚本（解压→覆盖→编译→收集产物→打包 rootfs/镜像）
6. 如需 GUI：交叉编译 Qt（见 references/qt-cross-compile.md）
7. Buildroot 集成与软件包选型（见 references/buildroot-pitfalls.md）
8. 上板验证：显示/触摸/字体/网络逐项过（见 references/lcd-display-debug.md）

## 专题参考（按需加载）

| 文件 | 内容 |
|------|------|
| [references/qt-cross-compile.md](references/qt-cross-compile.md) | Qt 交叉编译：mkspec 定制、configure 参数模板、staging 缓存、CMake 对接、rootfs 注入 |
| [references/buildroot-pitfalls.md](references/buildroot-pitfalls.md) | Buildroot 坑：gconv/iconv 缺失、外部工具链静默失效、DL 缓存、版本匹配 |
| [references/windows-docker-pitfalls.md](references/windows-docker-pitfalls.md) | Windows+Docker 坑：代理、路径转换、编码、权限、长编译管理 |
| [references/lcd-display-debug.md](references/lcd-display-debug.md) | LCD 显示排查树：错位/颜色/触摸/字体，dts 时序与像素格式 |
| [references/board-bringup.md](references/board-bringup.md) | 目标机启动问题速查：随机 MAC、env bad CRC、无害启动噪音 |

## 最常见四个坑（速记）

1. **Qt 报 `iconv_open failed`** → Buildroot 没装 gconv 模块，post-build 脚本从 STAGING_DIR 补拷
2. **LCD 画面错位/颜色不对** → dts 时序/bpp 必须匹配实际屏（U-Boot 启动打印有屏型号），bpp 用厂商验证值
3. **defconfig 里 BR2_TOOLCHAIN_EXTERNAL 不生效** → 缺 `BR2_TOOLCHAIN_EXTERNAL_CUSTOM=y` 会静默回退内部工具链，用 rootfs 里 libc/libstdc++ 版本号反查
4. **厂商原版 dts 是最好的参考** → 屏时序/触摸节点都在注释块里，先查源码再改配置
