# RK3562 编译与部署指南（NavigatorHMI_FW，2026-08-14）

迅为 RK3562（Topeet 6.1 SDK）交叉编译 + 烧录部署全流程。

## 1. 环境

| 项 | 值 |
|---|---|
| 编译镜像 | `swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/rk3562-builder-env:v1.0-ubuntu20`（已推 SWR） |
| SDK | `D:\workspace\rk3562-sdk\rk3562-linux-6.1`（17.8GB，卷挂载 `/sdk`，不打进镜像） |
| 工具链 | SDK 自带 `prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu`（buildroot defconfig 相对路径自动解析） |
| 脚本 | `docker-build-rk3562.ps1`（入口）/ `docker-push-rk3562.ps1`（推镜像）/ `build-rk3562.sh`（容器内执行） |

首次构建镜像：
```powershell
# 凭据已固化 HKCU\Environment；dsh/新终端需重开或注入
.\docker-push-rk3562.ps1          # 构建并推 SWR（仅首次）
.\docker-push-rk3562.ps1 -SkipPush # 仅本地构建
```

## 2. 编译

```powershell
.\docker-build-rk3562.ps1                       # all = buildroot + kernel + uboot
.\docker-build-rk3562.ps1 -Target kernel        # 内核 Image + dtb + modules
.\docker-build-rk3562.ps1 -Target uboot         # U-Boot（含 SPL）
.\docker-build-rk3562.ps1 -Target buildroot     # rootfs + Qt 6.4.3（首次 1-2h）
.\docker-build-rk3562.ps1 -Target qt            # 用 buildroot host/bin/qmake 交叉编译应用
.\docker-build-rk3562.ps1 -Menuconfig linux     # 内核 menuconfig，退出保存 hwt/rk3562/kernel/.config
.\docker-build-rk3562.ps1 -Jobs 16 -Clean       # 16 线程 + 清产物
```

**Buildroot 配置**：`hwt/rk3562/buildroot/configs/rk3562_navihmi_defconfig`（自动复制进 SDK）——
去 chromium/Qt5/weston，启 **Qt 6.4.3**（eglfs + serialbus/serialport + svg + core5compat）、openssh、root 密码 `topeet`。
输出目录 = `buildroot/output/rk3562_navihmi/`（Rockchip 规则：O=defconfig 名）。

**产物**：`build\rk3562\` 下 `kernel/`（Image + dtb）、`uboot/`（u-boot.bin + spl）、`rootfs/`（rootfs.ext4）。

**SDK 自动修复**（`build-rk3562.sh` 每次编译前执行，Windows 解压缺陷）：
- 内核 dts include：`scripts/dtc/include-prefixes/dt-bindings` 真实目录复制（符号链接跨容器不稳定）
- 迅为包遗漏 `linux-event-codes.h`（补 uapi 副本）
- `media-bus-format.h` 相对 include 依赖（补 include-prefixes/uapi）
- 工具链 `liblto_plugin.so`（u-boot LTO 需要）

## 3. 部署打包（烧录镜像）

1. **组装 `Image/` 目录**（Rockchip 标准，打包脚本从 `tools/linux/Linux_Pack_Firmware/rockdev/` 读取）：
   - `parameter.txt`：分区表（`device/rockchip/.chips/rk3562/parameter-buildroot-fit.txt`，GPT：uboot/misc/boot/recovery/backup/rootfs:grow）
   - `MiniLoaderAll.bin`：u-boot 目录 `./make.sh loader` 生成（SPL+TPL+DDR bin 合并）
   - `uboot.img`：u-boot 产物（已生成，FIT 格式）
   - `boot.img`：内核 Image + dtb + resource.img（afptool 打包）
   - `rootfs.img`：buildroot `images/rootfs.ext4` 复制
2. **生成 update.img**：`rk3562-mkupdate.sh`（afptool -pack → rkImageMaker -RK3562 → update.img）
3. **烧录**：Windows 端 `RKDevTool`（`D:\资料\iTOP-3562开发板\02_开发工具\烧录工具`）——
   开发板进入 Loader/MaskROM 模式，按分区烧录或整包 update.img。

## 4. 已知限制

- buildroot Qt 6.4.3 仅 qt6base 有 dl 缓存，serialbus/serialport/svg/core5compat 首次编译需联网（443 放行，脚本已关证书验证）
- 本机代理 TUN 仅放行 443：镜像构建用 https 源 + `Acquire::https::Verify-Peer false`
- imx6ull 旧脚本（docker-build.ps1/docker-push.ps1）凭据已环境变量化，勿回填硬编码

## 5. 换电脑 / 迁移编译环境（方案 A：产物打包存档，2026-08-15 用户定）

**原则**：Buildroot 编译产物（工具链 + Qt 6.4.3 + rootfs，3~6GB）不与镜像绑定、不进 Docker，
打包存档到 D 盘；换电脑 = 拷贝 SDK + 解压产物包 → 增量编译（几分钟），**不需要重新编 2-3 小时**。

### 5.1 打包（当前电脑，编译环境完整时）

```powershell
# 产物目录（含工具链/目标包/rootfs/Qt 6.4.3，全部编译产物）
$src = "D:\workspace\rk3562-sdk\rk3562-linux-6.1\buildroot\output\rk3562_navihmi"
# 可选：dl 下载缓存一并打包（避免新电脑联网重下；约 2-3GB）
$dl  = "D:\workspace\rk3562-sdk\rk3562-linux-6.1\buildroot\dl"

# 产物打包（tar 需在容器内执行，Windows tar 不支持长路径/符号链接）
docker run --rm -v "D:\workspace\rk3562-sdk\rk3562-linux-6.1:/sdk" `
  swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/rk3562-builder-env:v1.0-ubuntu20 `
  bash -c "cd /sdk/buildroot && tar czf /sdk/../rk3562-br-output-20260815.tar.gz output/rk3562_navihmi"
# 产物包落在 D:\workspace\rk3562-sdk\ 下
```

> 产物包命名建议带日期：`rk3562-br-output-<YYYYMMDD>.tar.gz`，放 D 盘或传网盘/SWR。
> 换 Qt 版本时重新打一份新包即可（版本管理按需再做，V1.0 用 6.4.3 不动）。

### 5.2 新电脑恢复（换电脑后）

1. **装 Docker Desktop**，拉编译镜像：`docker pull swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/rk3562-builder-env:v1.0-ubuntu20`
2. **拷贝 SDK** 到同路径 `D:\workspace\rk3562-sdk\rk3562-linux-6.1\`（迅为网盘重新下载解压，或整盘拷贝）
3. **解压产物包**到 SDK 的 buildroot 下：
   ```powershell
   docker run --rm -v "D:\workspace\rk3562-sdk\rk3562-linux-6.1:/sdk" `
     swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/rk3562-builder-env:v1.0-ubuntu20 `
     bash -c "cd /sdk/buildroot && tar xzf /sdk/../rk3562-br-output-20260815.tar.gz"
   ```
4. **增量编译验证**（几分钟）：`.\docker-build-rk3562.ps1 -Target buildroot`
5. 以后编译直接 `docker-build-rk3562.ps1`，已编包全部跳过

### 5.3 注意

- SDK 必须挂载在容器 `/sdk`（buildroot defconfig 的相对工具链路径依赖此结构）
- `buildroot/output` 里的 `.config` 同时被恢复，无需重新合并 defconfig
- 换电脑前记得先跑一次完整 `-Target all` 确认产物是最新的
