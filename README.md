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
│   └── Dockerfile                    # 编译镜像定义（内含 Linux/U-Boot 源码压缩包）
├── .github/workflows/
│   └── build-project.yml             # GitHub Actions CI 配置
├── hwt/
│   ├── linux/
│   │   ├── arch/arm/configs/         # Linux 内核配置（linux_hwt_defconfig）
│   │   └── arch/arm/boot/dts/        # 自定义 dts
│   └── uboot/
│       ├── configs/                  # U-Boot 配置（uboot_hwt_defconfig）
│       └── board/freescale/mx6ullevk/    # 自定义板级配置（imximage.cfg）
├── src/                              # NavigatorHMI_FW 应用源码
├── cmake/
│   └── arm-linux-gnueabihf-toolchain.cmake
├── buildroot/                        # Buildroot 自定义配置（可选）
├── cmake/                            # CMake 交叉编译工具链
├── docker-push.ps1                   # 构建镜像并推送到华为云 SWR
├── docker-build.ps1                  # 编译脚本（Windows PowerShell）
├── build-linux-uboot.sh              # 容器内编译脚本
├── BUILD_GUIDE.md                    # 详细编译指南
└── CMakeLists.txt
```

## 首次使用：构建镜像并推送到华为云 SWR

```powershell
.\docker-push.ps1
```

> 将 `tarballs/` 下的源码压缩包打包进 Docker 镜像，推送到华为云 SWR。
> 后续编译直接使用该镜像，不再需要本地 `tarballs/` 目录。

## 编译命令

### 编译全部（Linux Kernel + U-Boot + 应用 + Rootfs）

```powershell
.\docker-build.ps1
```

### 选择编译目标

```powershell
.\docker-build.ps1                      # 全部编译（内核 + U-Boot + 应用 + rootfs）
.\docker-build.ps1 -Target linux        # 只编译 Linux Kernel 6.6
.\docker-build.ps1 -Target uboot        # 只编译 U-Boot 2020.04
.\docker-build.ps1 -Target app          # 只编译应用
.\docker-build.ps1 -Target linux+app    # 编译 Linux + 应用（跳过 U-Boot）
.\docker-build.ps1 -Target rootfs       # 编译应用 + Buildroot Rootfs（应用自动打包进 rootfs）
.\docker-build.ps1 -Target image        # 编译应用 + Rootfs + 完整 SD 卡镜像 (sdcard.img)
```

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
- Linux 配置保存到 `hwt/linux/arch/arm/configs/linux_hwt_defconfig`
- U-Boot 配置保存到 `hwt/uboot/configs/uboot_hwt_defconfig`
- 修改后直接生效，无需手动复制

> **注意**：Menuconfig 为交互式界面，需要终端支持（PowerShell 直接运行即可）。

## CI 构建（GitHub Actions）

项目配置了 GitHub Actions CI，提交代码后可在 Actions 页面手动触发构建：

1. 进入 GitHub 仓库 → **Actions** → **Build NavigatorHMI_FW (ARM)**
2. 点击 **Run workflow**
3. 选择目标（同本地 `-Target` 参数）和构建类型
4. 等待构建完成，自动上传编译产物（linux/uboot/rootfs 按需上传）

CI 使用与本地相同的 Docker 编译镜像，保证构建环境一致。

## 编译产物

```
build/
├── linux/
│   ├── zImage                      # Linux 6.6.144 内核镜像
│   ├── imx6ull-14x14-evk.dtb       # 设备树文件
│   ├── lib/modules/                # 内核模块
│   └── bin/
│       └── NavigatorHMI_FW         # 应用可执行文件
├── uboot/
│   └── u-boot-dtb.imx              # U-Boot i.MX 格式（带 imx header，推荐使用）
├── rootfs/
│   ├── rootfs.tar                  # Rootfs 压缩包（已含 NavigatorHMI_FW 应用）
│   └── sdcard.img                  # 完整 SD 卡镜像（image 目标）
└── buildroot/
    └── dl/                         # Buildroot 下载缓存（自动复用）
```

## 编译流程说明

`build-linux-uboot.sh` 在容器内根据 `-Target` 选择性执行：

| 目标 | 执行顺序 |
|------|---------|
| `all` | build_linux → build_uboot → build_app → collect_artifacts → build_rootfs |
| `linux` | build_linux |
| `uboot` | build_uboot |
| `app` | build_app → collect_artifacts |
| `linux+app` | build_linux → build_app → collect_artifacts |
| `rootfs` | build_app → build_rootfs（应用自动打包进 rootfs） |
| `image` | build_app → build_rootfs → build_image（生成 sdcard.img） |

各阶段功能：
1. **解压源码** — 从镜像内 `/root/source/` 解压源码到 `/tmp/`
2. **应用 HWT 覆盖** — 从挂载的 `/workspace/hwt/` 将自定义配置/补丁覆盖到源码
3. **交叉编译** — 使用 `arm-linux-gnueabihf-` 工具链编译
4. **收集产物** — 应用自动复制到 `build/linux/bin/`
5. **构建 Rootfs** — 应用自动打包进 rootfs-overlay 的 `/usr/bin/` 目录
