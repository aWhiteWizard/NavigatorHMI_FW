# NavigatorHMI_FW 构建指南

## 项目结构

```
NavigatorHMI_FW/
├── .devcontainer/
│   └── Dockerfile              # Docker 编译镜像定义
├── tarballs/
│   ├── linux-5.4.234.tar.gz      # Linux Kernel 源码压缩包
│   └── u-boot-2020.04.tar.bz2 # U-Boot 源码压缩包
├── hwt/
│   ├── linux/                    # Linux 补丁/配置/设备树覆盖
│   └── uboot/                    # U-Boot 补丁/配置覆盖
├── src/                          # NavigatorHMI_FW 应用源码
├── cmake/
│   └── arm-linux-gnueabihf-toolchain.cmake
├── docker-build.ps1            # 编译脚本（应用+内核+uboot）
├── docker-push.ps1             # 构建镜像并推送到华为云 SWR
└── CMakeLists.txt
```

---

## 工作流程

### 1. 首次使用：构建镜像并推送到华为云 SWR

```powershell
.\docker-push.ps1
```

该脚本会：
1. 登录华为云 SWR
2. 使用 `.devcontainer/Dockerfile` 构建镜像
   - 将 `source/` 下的 Linux/U-Boot 压缩包复制到镜像内的 `/root/source/`
   - 将 `hwt/linux` 和 `hwt/uboot` 复制到镜像内的 `/root/hwt/`
   - 安装交叉编译工具链
   - 创建 `/usr/local/bin/build-all.sh` 编译脚本
3. 推送到华为云 SWR：`swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/fw-builder-env:v1.0`

> **提示**：如果后续修改了 `source/` 或 `hwt/` 内容，需要重新执行 `docker-push.ps1` 更新镜像。

### 2. 日常编译

```powershell
# 全部编译（Linux + U-Boot + 应用）
.\docker-build.ps1

# 只编译 Linux Kernel
.\docker-build.ps1 -Target linux

# 只编译 U-Boot
.\docker-build.ps1 -Target uboot

# 只编译应用
.\docker-build.ps1 -Target app

# 编译 Linux Kernel + 应用（跳过 U-Boot）
.\docker-build.ps1 -Target linux+app

# Release 模式
.\docker-build.ps1 -BuildType Release

# 清理后重新编译
.\docker-build.ps1 -Clean

# 8 线程并行编译
.\docker-build.ps1 -Jobs 8

# 跳过 SWR 登录（如果已登录）
.\docker-build.ps1 -SkipLogin
```

### 3. 交互式配置（Menuconfig）

进入 Linux 或 U-Boot 的 menuconfig 图形化配置界面：

```powershell
# Linux Kernel menuconfig（交互式）
.\docker-build.ps1 -Menuconfig linux

# U-Boot menuconfig（交互式）
.\docker-build.ps1 -Menuconfig uboot
```

执行流程：
1. 解压源码 → 应用 `hwt/` 补丁 → 加载现有配置
2. 进入 menuconfig 界面供修改
3. **退出时自动保存**：
   - Linux 配置 → `hwt/linux/hwt_defconfig`
   - U-Boot 配置 → `hwt/uboot/hwt_defconfig`
4. 下次编译时脚本会优先检测并使用这些自定义配置

> **注意**：Menuconfig 需要交互式终端，PowerShell 中直接运行即可。

---

## 编译产物

编译完成后，产物位于 `build/` 目录：

```
build/
├── linux/
│   ├── zImage                 # Linux 内核镜像
│   ├── *.dtb                  # 设备树文件
│   ├── lib/modules/           # 内核模块
│   └── bin/
│       └── NavigatorHMI_FW    # 应用可执行文件（已部署到内核 /bin）
├── uboot/
│   ├── u-boot.bin             # U-Boot 镜像
│   ├── u-boot.imx             # U-Boot i.MX 格式镜像
│   └── SPL                    # SPL（如生成）
└── bin/                       # CMake 编译中间产物，可忽略
```

---

## 编译流程说明

编译时（`docker-build.ps1`）的执行流程：

所有编译在一个 `docker run` 内完成，顺序执行：

1. **编译 Linux Kernel 4.1.15**
   - 解压镜像内 `/root/source/linux-imx-4.1.15.tar.bz2` 到 `/tmp/`
   - 从挂载的 `/workspace/hwt/linux/` 覆盖解压后的内核源码
   - 配置并编译 `zImage` + `dtbs` + `modules`
   - 产物暂存到 `/workspace/build/linux/`

2. **编译 U-Boot 2016.03**
   - 解压镜像内 `/root/source/uboot-imx-2016.03.tar.bz2` 到 `/tmp/`
   - 从挂载的 `/workspace/hwt/uboot/` 覆盖解压后的 U-Boot 源码
   - 配置并编译 U-Boot
   - 产物输出到 `/workspace/build/uboot/`

3. **编译 NavigatorHMI_FW 应用**
   - CMake 配置 + `make` 编译
   - 可执行文件复制到内核的 `/bin` 目录

### 最终产物结构

```
build/linux/
├── zImage                 # 内核镜像
├── *.dtb                  # 设备树
├── lib/modules/           # 内核模块
└── bin/
    └── NavigatorHMI_FW    # 应用可执行文件
```

1. **编译 Linux Kernel 4.1.15**
   - 解压 `/root/source/linux-imx-4.1.15.tar.bz2` 到 `/tmp/`
   - 将 `/root/hwt/linux/` 的内容覆盖到内核源码目录
   - 使用 `hwt_defconfig` 或 `imx_v7_defconfig` 配置内核
   - 编译 `zImage`、`dtbs`、`modules`
   - 产物输出到 `/workspace/build/linux/`

2. **编译 U-Boot 2016.03**
   - 解压 `/root/source/uboot-imx-2016.03.tar.bz2` 到 `/tmp/`
   - 将 `/root/hwt/uboot/` 的内容覆盖到 U-Boot 源码目录
   - 使用 `hwt_defconfig` 或 `mx6ull_14x14_evk_defconfig` 配置
   - 编译 U-Boot
   - 产物输出到 `/workspace/build/uboot/`
