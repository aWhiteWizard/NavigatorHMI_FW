# RK3562 固件烧写指南（NavigatorHMI B-1，2026-08-16）

烧写 NavigatorHMI RK3562 固件的完整操作（USB + 网络双路径）。

## 0. 固件清单（`NavigatorHMI_FW\build\rk3562\firmware\`）

| 文件 | 大小 | 说明 |
|------|------|------|
| `idblock.img` | 316KB | Loader（u-boot `make.sh loader` 生成，含 SPL+DDR bin） |
| `u-boot.img` | 1.2MB | U-Boot 主镜像（FIT） |
| `boot.img` | 39MB | 内核（Image + resource.img，含 dtb/logo） |
| `resource.img` | 179KB | 内核资源（dtb/logo） |
| `rootfs.img` | 439MB | Buildroot rootfs（**含 qt-test 测试程序** /usr/bin/qt-test） |
| `parameter.txt` | 433B | 分区表（GPT：uboot/misc/boot/recovery/backup/rootfs:grow） |
| `update.img` | 506MB | **整包**（上述全部，afptool + rkImageMaker 生成） |

## 1. 前置准备

- **驱动**：Windows 安装 `DriverAssitant_v5.12`（`D:\资料\iTOP-3562开发板\02_开发工具\02_烧写工具及驱动\`）
- **烧写工具**：`RKDevTool_Release_v3.15\RKDevTool.exe`（同上目录）
- **串口**：USB 转 TTL 模块接板子 **DEBUG 串口**（原理图 14 页 UART(DEBUG)）——**注意电平 1.8V**，需 1.8V 兼容模块或电平转换
- **TFTP 服务器**（网络烧写用）：Docker 容器 `rk-tftpd` 已部署（端口 69/UDP，挂载 firmware 目录）——重启用：`docker start rk-tftpd`

## 2. USB 烧写（RKDevTool，首次安卓→Linux 推荐）

1. **板子进 Loader 模式**（三选一，08 手册 P29）：
   - 按住 **RECOVERY（恢复）键** + 按 **电源键** 上电 → 进 loader 后松开
   - 板子已有 u-boot：串口输入 `rockusb 0 mmc 0`
   - 系统内（串口/adb）：`reboot loader`
2. USB 线连接板子 OTG 口与电脑
3. 打开 RKDevTool → 确认"发现一个设备"（Loader 模式）
4. **配置烧写项**（linux.cfg 或手动）：parameter + idblock(MiniLoaderAll) + uboot + boot + rootfs（按 08 手册 4.2 节；分区对应 parameter.txt）
5. 点 **执行** → 等待完成（进度条）→ 板子自动重启进 buildroot

**整包方式**：RKDevTool 的"烧写 update.img"入口，或 Linux 端 `./upgrade_tool uf update.img`（09 手册 P64）。

## 3. 网络 TFTP 烧写（u-boot 下载写 eMMC）

> 前提：板子已有可联网 u-boot（出厂带系统即可）；电脑与板子同网段。

1. **确认 TFTP 服务器**（Windows 侧容器 rk-tftpd）：
   - `docker start rk-tftpd`（若未运行）
   - 电脑 IP：`ipconfig` 查（如 192.168.1.100）
2. **板子 u-boot 配置**（串口进 u-boot 控制台）：
   ```
   setenv ipaddr 192.168.1.50        # 板子 IP（与电脑同网段）
   setenv serverip 192.168.1.100     # 电脑/TFTP 服务器 IP
   saveenv
   ```
3. **下载 + 写分区**（各分区依次，偏移按 parameter.txt）：
   ```
   tftpboot 0x00200000 uboot.img      # 下载到内存
   mmc write 0x00200000 0x4000 0x1000 # 写 uboot 分区（偏移 0x4000，大小 0x1000 扇区）
   tftpboot 0x00200000 boot.img
   mmc write 0x00200000 0x8000 0x10000  # boot 分区
   tftpboot 0x00200000 rootfs.img
   mmc write 0x00200000 0x78000 0x...   # rootfs 分区（大小按实际）
   ```
   > **⚠ 分区偏移/大小以 parameter.txt 为准**；烧写前建议先烧 idblock（loader）确保可恢复。
4. `reset` 重启 → 串口观察进入新系统

> **实测确认点**：以上 u-boot 命令（mmc 偏移/大小）为手册流程推导，**B-1 网络刷入实测时在板子上确认后更新本文档**。

## 4. 验证（烧写后）

1. 串口看启动日志（u-boot → 内核 → rootfs 正常）
2. 系统内跑测试程序：`/usr/bin/qt-test`（eglfs 全屏显示 + 触摸变色）
3. 网络：`ping` 板子 IP 通

## 5. 常见问题

- **Loader 模式进不去**：换 USB 口/线，重装驱动，或 MaskROM 模式（按住 RECOVERY 不松直接插电）
- **TFTP 超时**：确认防火墙放行 69/UDP；serverip 与板子同网段
- **1.8V 串口乱码/无输出**：电平不匹配，需 1.8V 兼容串口模块
