<!--
 * @Author: aWhiteWizard www.123518341@qq.com
 * @Date: 2026-07-09 23:19:00
 * @LastEditors: aWhiteWizard www.123518341@qq.com
 * @LastEditTime: 2026-07-09 23:53:27
 * @FilePath: \NavigatorHMI_FW\README.md
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
-->

# NavigatorHMI_FW
Embedded device code for NavigatorHMI (i.MX6ULL ARM)

## 环境要求

- [Docker Desktop](https://www.docker.com/products/docker-desktop/)（Windows）
- PowerShell 5.1+

## 项目结构

```
NavigatorHMI_FW/
├── .devcontainer/
│   └── Dockerfile              # 编译镜像定义（内含 Linux/U-Boot 源码压缩包）
├── tarballs/                   # 内核/U-Boot 源码压缩包（已打包进镜像，.gitignore 忽略）
│   ├── linux-6.6.144.tar.xz
│   └── u-boot-2020.04.tar.bz2
├── hwt/
│   ├── linux/
│   │   ├── arch/arm/configs/        # Linux 内核配置（menuconfig 导出）
│   │   └── arch/arm/boot/dts/       # 自定义 dts（任意目录层级，自动扫描）
│   └── uboot/
│       ├── configs/                 # U-Boot 配置（menuconfig 导出）
│       └── arch/arm/dts/            # 自定义 dts（编译时覆盖）
├── src/                        # NavigatorHMI_FW 应用源码
├── cmake/
│   └── arm-linux-gnueabihf-toolchain.cmake
├── buildroot/                   # Buildroot 自定义配置（可选）
├── docker-push.ps1            # 构建镜像并推送到华为云 SWR
├── docker-build.ps1           # 编译脚本
├── build-linux-uboot.sh       # 容器内编译脚本（内核+uboot+应用+rootfs）
└── CMakeLists.txt
```

## 首次使用：构建镜像并推送到华为云 SWR

```powershell
.\docker-push.ps1
```

> 将 `tarballs/` 下的源码压缩包打包进 Docker 镜像，推送到华为云 SWR。
> 后续编译直接使用该镜像，不再需要本地 `tarballs/` 目录。

## 编译命令

### 编译全部（Linux Kernel + U-Boot + 应用）

```powershell
.\docker-build.ps1
```

### 选择编译目标

```powershell
.\docker-build.ps1 -Target linux       # 只编译 Linux Kernel 6.6
.\docker-build.ps1 -Target uboot       # 只编译 U-Boot 2020.04
.\docker-build.ps1 -Target app         # 只编译应用
.\docker-build.ps1 -Target linux+app   # 编译 Linux + 应用（跳过 U-Boot）
.\docker-build.ps1 -Target rootfs     # 编译 Buildroot Rootfs
```

> dts 文件放在 `hwt/linux/arch/arm/boot/dts/` 下，编译时自动只编这些 dtb。

### 其他选项

```powershell
.\docker-build.ps1 -BuildType Release   # Release 模式
.\docker-build.ps1 -Clean               # 清理后重新编译
.\docker-build.ps1 -Jobs 8              # 8 线程并行编译
.\docker-build.ps1 -SkipLogin           # 跳过 SWR 登录（已登录时使用）
.\docker-build.ps1 -Help                # 查看完整帮助信息
```

### 交互式配置（Menuconfig）

进入 Linux 或 U-Boot 的 menuconfig 图形化配置界面，修改后自动保存到 `hwt/` 目录：

```powershell
.\docker-build.ps1 -Menuconfig linux   # Linux Kernel menuconfig
.\docker-build.ps1 -Menuconfig uboot   # U-Boot menuconfig
```

**工作流程**：解压源码 → 应用 `hwt/` 补丁 → 加载现有配置 → 进入 menuconfig → **退出时自动保存**
- Linux 配置保存到 `hwt/linux/hwt_defconfig`
- U-Boot 配置保存到 `hwt/uboot/hwt_defconfig`
- 下次编译时脚本会自动检测并使用这些自定义配置

> **注意**：Menuconfig 为交互式界面，需要终端支持（PowerShell 直接运行即可）。```

## 编译产物

```
build/
├── linux/
│   ├── zImage                      # Linux 6.6.144 内核镜像
│   ├── *.dtb                       # 设备树文件
│   ├── lib/modules/                # 内核模块
│   └── bin/
│       └── NavigatorHMI_FW         # 应用可执行文件
├── uboot/
│   ├── u-boot.bin                  # U-Boot 镜像
│   ├── u-boot.imx                  # U-Boot i.MX 格式（推荐使用）
│   └── u-boot-dtb.imx              # U-Boot + DTB 镜像
└── rootfs/
    ├── rootfs.tar                  # Rootfs 压缩包
    ├── rootfs.ext2/ext4            # Rootfs 分区镜像
    └── sdcard.img                  # 完整 SD 卡镜像
```

## 编译流程说明

`build-linux-uboot.sh` 在容器内根据 `-Target` 选择性执行：

| 目标 | 编译内容 | 版本 |
|------|---------|------|
| `linux` | Linux Kernel | 6.6.144 LTS |
| `uboot` | U-Boot | 2020.04 |
| `app` | NavigatorHMI_FW | CMake 交叉编译 |
| `rootfs` | Buildroot 根文件系统 | 2025.05 (imx6ullevk) |

1. **解压源码** — 从镜像内 `/root/source/` 解压源码到 `/tmp/`
2. **应用 HWT 覆盖** — 从挂载的 `/workspace/hwt/` 将自定义配置/补丁覆盖到源码
3. **交叉编译** — 使用 `arm-linux-gnueabihf-` 工具链编译
4. **收集产物** — 输出到 `/workspace/build/`
