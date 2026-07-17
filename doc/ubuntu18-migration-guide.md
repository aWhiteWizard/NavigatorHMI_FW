# Ubuntu 18.04 迁移指南

> **日期**: 2026-07-17
> **目的**: 将编译环境从 Ubuntu 22.04 (kernel 6.6 + U-Boot 2020.04 + Buildroot 2025.05) 迁移到 Ubuntu 18.04 (kernel 4.1.15 NXP IMX + U-Boot IMX 4.1.15 + Buildroot 2019.02.6)
> **状态**: ✅ **已归档** — 系统完整启动，所有功能验证通过

---

## 1. 新 Docker 镜像

| 项目 | 原镜像 | 新镜像 |
|------|--------|--------|
| 镜像标签 | `v1.0` | `v1.0-ubuntu18` |
| 基础系统 | Ubuntu 22.04 | Ubuntu 18.04 |
| 交叉编译器 | gcc-arm-linux-gnueabihf 11.4.0 (apt) | gcc-linaro-4.9.4-2017.01-i686_arm-linux-gnueabihf (Linaro) |
| Linux 源码 | linux-6.6.144.tar.xz | linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek.tar.bz2 |
| U-Boot 源码 | u-boot-2020.04.tar.bz2 | uboot-imx-rel_imx_4.1.15_2.1.0_ga_alientek.tar.bz2 |
| Buildroot | 2025.05 | 2019.02.6 |

### 构建并推送镜像

```powershell
# 源码目录
D:\workspace\image_sources  # 包含所有源码压缩包

# 构建镜像
docker build --provenance=false -t swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/fw-builder-env:v1.0-ubuntu18 `
    -f ".devcontainer\Dockerfile.ubuntu18" `
    --build-context "sources=D:\workspace\image_sources" `
    "."

# 登录并推送
$SWR_AK="HPUAQUYWVPFHVHWJJGON"
$SWR_SK="G6vlkEzbjeG4ZspOgK7pYJPm5G5E7DtbbBvBK5HM"
$SWR_Region="cn-southwest-2"
$SWR_Domain="swr.$SWR_Region.myhuaweicloud.com"
$SWR_UserName="$SWR_Region@$SWR_AK"
$hmacsha256 = New-Object System.Security.Cryptography.HMACSHA256
$hmacsha256.Key = [Text.Encoding]::UTF8.GetBytes($SWR_SK)
$hash = $hmacsha256.ComputeHash([Text.Encoding]::UTF8.GetBytes($SWR_AK))
$SWR_Password = -join ($hash | ForEach-Object { "{0:x2}" -f $_ })
$SWR_Password | docker login -u $SWR_UserName --password-stdin $SWR_Domain
docker push swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/fw-builder-env:v1.0-ubuntu18
```

> **注意**: 华为云 SWR 不支持新版 manifest (provenance) 格式，必须加 `--provenance=false`

---

## 2. Dockerfile 要点 (`.devcontainer/Dockerfile.ubuntu18`)

```dockerfile
FROM ubuntu:18.04

# 启用 i386 架构（Linaro 4.9.4 是 32 位程序）
RUN dpkg --add-architecture i386 && apt-get update && apt-get install -y \
    libc6:i386 libstdc++6:i386 lib32z1

# Linaro 交叉编译器（预置在 sources 中）
COPY --from=sources /gcc-linaro-4.9.4-2017.01-i686_arm-linux-gnueabihf.tar.xz /tmp/
RUN tar -xJf /tmp/gcc-linaro-4.9.4-2017.01-i686_arm-linux-gnueabihf.tar.xz -C /opt/
ENV PATH=/opt/gcc-linaro-4.9.4-2017.01-i686_arm-linux-gnueabihf/bin:${PATH}

# 源码压缩包
COPY --from=sources /linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek.tar.bz2 /root/source/
COPY --from=sources /uboot-imx-rel_imx_4.1.15_2.1.0_ga_alientek.tar.bz2 /root/source/
COPY --from=sources /buildroot-2019.02.6.tar.bz2 /root/source/
```

---

## 3. 构建脚本改动 (`build-linux-uboot.sh`)

### 3.1 源码路径变更

| 变量 | 旧值 | 新值 |
|------|------|------|
| KERNEL_SRC | `/tmp/linux-6.6.144` | `/tmp/linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek` |
| 内核解压命令 | `tar -xJf (xz)` | `tar -xjf (bz2)` |
| UBOOT_SRC | `/tmp/u-boot-2020.04` | `/tmp/uboot-imx-rel_imx_4.1.15_2.1.0_ga_alientek` |
| BR_VERSION | `2025.05` | `2019.02.6` |
| Buildroot 解压 | `tar -xzf (gz)` | `tar -xjf (bz2)` |

### 3.2 Linux defconfig 优先级

```
1. linux_hwt_defconfig (hwt 自定义)
2. imx_alientek_emmc_defconfig (正点原子官方)
3. imx_v7_defconfig (i.MX 通用)
```

### 3.3 U-Boot defconfig 优先级

```
1. uboot_hwt_defconfig (hwt 自定义)
2. mx6ull_alientek_emmc_defconfig (正点原子 eMMC)
3. mx6ull_14x14_evk_defconfig (NXP 官方 EVK)
```

---

## 4. HWT 覆盖层结构

### 4.1 最终文件结构

```
hwt/
├── buildroot/
│   ├── alientek_emmc_defconfig          # Buildroot 配置（只编译 rootfs，不编译内核/U-Boot）
│   │                                     含 BR2_TARGET_GENERIC_HOSTNAME 设置
│   └── rootfs-overlay/
│       └── etc/
│           ├── hostname                 # 系统主机名 (NavigatorHMI)
│           ├── hosts                    # hosts 映射
│           ├── issue                    # Linux 登录欢迎语
│           └── issue.net
├── linux/
│   └── arch/arm/
│       ├── boot/dts/imx6ull-alientek-emmc.dts   # 自定义设备树
│       └── configs/linux_hwt_defconfig           # 内核配置（4023 行）
└── uboot/
    ├── board/freescale/mx6ull_alientek_emmc/
    │   └── mx6ull_alientek_emmc.c                # U-Boot Board Name
    ├── configs/
    │   ├── .config
    │   └── uboot_hwt_defconfig                   # U-Boot 配置
    └── include/configs/mx6ullevk.h
```

### 4.2 DTS 路径

**旧路径** (kernel 6.6):
```
arch/arm/boot/dts/nxp/imx/imx6ull-14x14-evk.dts
```

**新路径** (kernel 4.1.15):
```
arch/arm/boot/dts/imx6ull-alientek-emmc.dts
```

### 4.3 U-Boot defconfig

基于正点原子 `mx6ull_alientek_emmc_defconfig`:

```kconfig
CONFIG_SYS_EXTRA_OPTIONS="IMX_CONFIG=board/freescale/mx6ull_alientek_emmc/imximage.cfg,MX6ULL_EVK_EMMC_REWORK"
CONFIG_ARM=y
CONFIG_ARCH_MX6=y
CONFIG_TARGET_MX6ULL_ALIENTEK_EMMC=y
CONFIG_CMD_GPIO=y
CONFIG_CMD_DHCP=y
CONFIG_CMD_PING=y
CONFIG_CMD_MMC=y
CONFIG_CMD_FAT=y
CONFIG_CMD_EXT2=y
CONFIG_CMD_EXT4=y
CONFIG_CMD_BOOTZ=y
CONFIG_HUSH_PARSER=y
CONFIG_SD_BOOT=y
CONFIG_ENV_IS_IN_MMC=y
CONFIG_ENV_SIZE=0x2000
CONFIG_ENV_OFFSET=0xC0000
CONFIG_FSL_USDHC=y
CONFIG_SYS_MMC_ENV_DEV=0
CONFIG_SYS_LOAD_ADDR=0x80800000
CONFIG_MMCROOT="/dev/mmcblk0p2"
```

---

## 5. 编译与烧录

### 5.1 一键编译

```powershell
.\docker-build.ps1 -target image -SkipLogin
```

编译流程：
1. `build_linux()` → 用预设源码编译 zImage + dtb
2. `build_uboot()` → 用预设源码编译 u-boot.imx
3. `build_app()` → 用 cmake 编译 NavigatorHMI_FW
4. `build_rootfs()` → Buildroot 编译 rootfs → 注入 zImage/dtb/u-boot → genimage 打包 sdcard.img

### 5.2 单独编译各组件

```powershell
.\docker-build.ps1 -target linux -SkipLogin    # 只编译内核
.\docker-build.ps1 -target uboot -SkipLogin    # 只编译 U-Boot
.\docker-build.ps1 -target app -SkipLogin      # 只编译应用
.\docker-build.ps1 -target rootfs -SkipLogin   # 只编译 rootfs
```

### 5.3 内核/U-Boot 配置

```powershell
.\docker-build.ps1 -Menuconfig linux -SkipLogin   # 内核 menuconfig
.\docker-build.ps1 -Menuconfig uboot -SkipLogin   # U-Boot menuconfig
```

修改后保存会自动写入 `hwt/linux/arch/arm/configs/linux_hwt_defconfig` 或 `hwt/uboot/configs/uboot_hwt_defconfig`。

### 5.4 烧录 SD 卡

用 **Win32 Disk Imager** 将 `build/rootfs/sdcard.img` 写入 SD 卡。

---

## 6. 关键差异说明

### 6.1 U-Boot 配置差异 (2020.04 → 4.1.15)

| 功能 | 2020.04 (旧) | 4.1.15 (新) | 说明 |
|------|-------------|-------------|------|
| OF_CONTROL | ✅ 支持 | ❌ 不支持 | 旧版用传统 IMX_CONFIG 方式 |
| DM_GPIO/DM_MMC | ✅ 支持 | ❌ 不支持 | 旧版无驱动模型 |
| CONFIG_BOOTCOMMAND | ✅ 可设置 | ❌ 不支持 | 旧版在头文件中定义 |
| CONFIG_DEFAULT_DEVICE_TREE | ✅ 支持 | ❌ 不支持 | 旧版 DTB 硬编码在 U-Boot 中 |

### 6.2 编译器差异

| 特性 | Ubuntu 22.04 + apt gcc | Ubuntu 18.04 + Linaro 4.9.4 |
|------|----------------------|------------------------------|
| GCC 版本 | 11.4.0 | 4.9.4 |
| 架构 | x86_64 | i686 (需 32 位库) |
| 兼容性 | 不支持旧版内核 | 编译 4.1.15 内核最佳 |
| C++ 标准 | C++17 | C++14 (CMakeLists.txt 已改) |

---

## 7. 自定义修改

### 7.1 修改 U-Boot Board Name

编辑 `hwt/uboot/board/freescale/mx6ull_alientek_emmc/mx6ull_alientek_emmc.c`，找到：

```c
puts("Board: MX6ULL ALIENTEK EMMC\n");
```

改为自定义名称，例如：

```c
puts("Board: NavigatorHMI i.MX6ULL\n");
```

### 7.2 修改 Linux 欢迎语

编辑 `hwt/buildroot/rootfs-overlay/etc/issue`：

```
Welcome to NavigatorHMI System
Kernel \r on \m
```

### 7.3 修改主机名

编辑 `hwt/buildroot/rootfs-overlay/etc/hostname`：

```
NavigatorHMI
```

> **注意**: 该文件必须使用 Unix (LF) 行尾，否则登录提示会显示乱码（如 `rHMI login:`）。
> Buildroot defconfig 中 `BR2_TARGET_GENERIC_HOSTNAME` 也需同步修改。

### 7.4 修改 rootfs 大小

编辑 `hwt/buildroot/alientek_emmc_defconfig`：

```
BR2_TARGET_ROOTFS_EXT2_SIZE=120M   # 改为需要的大小
```

---

## 8. 产物清单

编译完成后产出：

| 文件 | 路径 | 说明 |
|------|------|------|
| `sdcard.img` | `build/rootfs/` | 完整 SD 卡镜像（84MB） |
| `rootfs.tar` | `build/rootfs/` | 根文件系统打包 |
| `zImage` | `build/linux/` | Linux 内核 |
| `imx6ull-alientek-emmc.dtb` | `build/linux/` | 设备树 |
| `u-boot.imx` | `build/uboot/` | U-Boot 引导 |
| `u-boot.bin` | `build/uboot/` | U-Boot 原始二进制 |
| `NavigatorHMI_FW` | `build/bin/` | 应用可执行文件 |

---

## 9. 使用方式

```powershell
# 默认已改为 v1.0-ubuntu18 镜像
.\docker-build.ps1 -target image -SkipLogin

# 或手动指定镜像
.\docker-build.ps1 -target image -DockerImage swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/fw-builder-env:v1.0-ubuntu18 -SkipLogin
```

---

## 10. 已知问题与排查

### 10.1 `"CONFIG_CMD_DHCP" redefined` 警告

U-Boot 编译时会大量出现此类 warning：

```
include/configs/mx6ull_alientek_emmc.h:322:0: warning: "CONFIG_CMD_DHCP" redefined
```

**原因**: 正点原子移植的 U-Boot 正处于从 `#define`（头文件）向 Kconfig（defconfig）过渡的时期，`CONFIG_CMD_DHCP`、`CONFIG_CMD_PING`、`CONFIG_SYS_HUSH_PARSER` 等在两个地方同时定义。

**影响**: ❌ 无 — 只是警告，定义的值一致，编译产物正确。

### 10.2 `login: rHMI` 登录提示乱码

**原因**: `/etc/hostname` 文件使用了 Windows CRLF (`\r\n`) 行尾。Linux `getty` 读到 `\r`（回车符）时光标回到行首，覆盖了前面的字符，只看到后半段。

**修复**: 确保 `rootfs-overlay/etc/` 下所有配置文件均为 Unix LF 行尾。

### 10.3 CMake `Permission denied` (feature_tests.cxx)

```
CMake Error: file failed to open for writing (Permission denied):
  /workspace/build/CMakeFiles/feature_tests.cxx
```

**原因**: 使用 `-Clean` 参数后，Windows 重建的 `build/` 目录挂载到 Docker 容器中权限异常。

**解决**: 不加 `-Clean` 参数直接编译即可。如果必须 clean，手动删除 `build/` 目录后在容器内 `mkdir -p /workspace/build`。

### 10.4 `libstdc++.so.6` 版本警告

```
/lib/arm-linux-gnueabihf/libstdc++.so.6: warning: GLIBCXX_... not found
```

**原因**: Buildroot 2019.02.6 使用的工具链与 Linaro 4.9.4 的 libstdc++ 版本有细微差异。

**影响**: ❌ 无 — 应用正常运行，仅在 `ldd` 或 `ldconfig` 时显示。

---

> *文档版本: v1.0 — 2026-07-17 归档*
