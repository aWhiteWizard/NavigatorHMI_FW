# NavigatorHMI_FW 硬件适配笔记

> **日期**: 2026-07-22
> **平台**: i.MX6ULL（正点原子 ALIENTEK EMMC 板）+ Buildroot rootfs + Qt 5.12.9 HMI
> **状态**: ✅ 已稳定运行，所有外设验证通过

---

## 1. 概述

本项目采用正点原子 ALIENTEK EMMC 开发板，搭载 NXP i.MX6ULL 处理器，运行 Linux kernel 4.1.15 NXP IMX + Buildroot 2019.02.6 rootfs，上层运行 Qt 5.12.9 嵌入式 HMI 应用。

硬件适配通过 **HWT 覆盖层**（`hwt/` 目录）管理，三个核心文件：

| 文件 | 路径 | 说明 |
|------|------|------|
| 设备树 | `hwt/linux/arch/arm/boot/dts/imx6ull-alientek-emmc.dts` | 引脚功能分配、LCD 时序、触摸、声卡等 |
| 内核配置 | `hwt/linux/arch/arm/configs/linux_hwt_defconfig` | 内核功能开关（4023 行） |
| U-Boot 板级头文件 | `hwt/uboot/include/configs/mx6ullevk.h` | U-Boot 环境定义 |

> **设计原则**：不直接修改厂商源码，所有自定义通过 HWT 覆盖层注入，保持与上游源码解耦。

---

## 2. LCD

### 2.1 硬件规格

| 参数 | 值 |
|------|------|
| 型号 | ATK-LCD-4.3-800x480 |
| 分辨率 | 800 × 480 |
| 色深 | RGB 24bit |
| 接口 | RGB 并行 |
| 背光 | PWM 背光（PWM1，GPIO1_IO08） |

### 2.2 DTS 时序配置

```dts
display0: display {
    bits-per-pixel = <24>;
    bus-width = <24>;

    display-timings {
        native-mode = <&timing0>;
        timing0: timing0 {
            clock-frequency = <31000000>;
            hactive = <800>;
            vactive = <480>;
            hfront-porch = <40>;
            hback-porch = <88>;
            hsync-len = <48>;
            vback-porch = <32>;
            vfront-porch = <13>;
            vsync-len = <3>;
            hsync-active = <0>;
            vsync-active = <0>;
            de-active = <1>;
            pixelclk-active = <0>;
        };
    };
};
```

| 时序参数 | 值 |
|---------|------|
| `clock-frequency` | 31 MHz |
| `hactive` × `vactive` | 800 × 480 |
| `hfront-porch` / `hback-porch` / `hsync-len` | 40 / 88 / 48 |
| `vfront-porch` / `vback-porch` / `vsync-len` | 13 / 32 / 3 |
| `hsync-active` / `vsync-active` | 0（低电平有效） |
| `de-active` | 1（高电平有效） |
| `pixelclk-active` | 0（下降沿采样） |

> **时序来源**：直接取自正点原子原版 dts 注释块，经目标机实测验证正确。

### 2.3 ⚠️ 踩坑记录：误配时序导致显示错位

**现象**：LCD 显示整体错位，图像撕裂，颜色异常。

**根因分析**：此前误用了 480×272 分辨率（4.3" 480×272 屏）的时序参数，具体差异：

| 参数 | 错误值（480×272） | 正确值（800×480） |
|------|------|------|
| `clock-frequency` | 9.2 MHz | 31 MHz |
| `bits-per-pixel` | 16 | 24 |
| `hactive` | 480 | 800 |
| `vactive` | 272 | 480 |

（`bus-width` 两种配置都是 24，无需变动）

此外，`&lcdif` 节点已移除 `pinctrl_lcdif_reset` 引用——因为 LCD 复位引脚（GPIO5_IO09）与 GT9147 触摸复位共线，复位控制权交给触摸驱动：

```dts
&lcdif {
    pinctrl-names = "default";
    /* 复位脚让给电容触摸（与正点原子原版一致，lcdif 不占复位）*/
    pinctrl-0 = <&pinctrl_lcdif_dat
                 &pinctrl_lcdif_ctrl>;
    /* ... */
};
```

---

## 3. GT9147 触摸

### 3.1 硬件信息

| 参数 | 值 |
|------|------|
| 型号 | GT9147（汇顶科技） |
| 总线 | I2C2 |
| 设备地址 | 0x14 |
| 中断引脚 | GPIO1_IO09 |
| 复位引脚 | GPIO5_IO09（与 LCD 复位共线） |

### 3.2 DTS 节点

```dts
&i2c2 {
    clock_frequency = <100000>;
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_i2c2>;
    status = "okay";

    gt9147: gt9147@14 {
        compatible = "goodix,gt9147", "goodix,gt9xx";
        reg = <0x14>;
        pinctrl-names = "default";
        pinctrl-0 = <&pinctrl_gt9xx
                    &pinctrl_gt9xx_reset>;
        interrupt-parent = <&gpio1>;
        interrupts = <9 0>;
        reset-gpios = <&gpio5 9 GPIO_ACTIVE_LOW>;
        interrupt-gpios = <&gpio1 9 GPIO_ACTIVE_LOW>;
        status = "okay";
    };
};
```

### 3.3 pinctrl 配置

GT9147 使用两个 pin control 组，均在 `&iomuxc` 中定义（与 dts 中现有组同一约定）：

```dts
/* 中断引脚 —— GPIO1_IO09 */
pinctrl_gt9xx: gt9xxgrp {
    fsl,pins = <
        MX6UL_PAD_GPIO1_IO09__GPIO1_IO09    0x10B0
    >;
};

/* 复位引脚 —— GPIO5_IO09（SNVS 域引脚，组仍定义在 &iomuxc 内） */
pinctrl_gt9xx_reset: gt9xxresetgrp {
    fsl,pins = <
        MX6ULL_PAD_SNVS_TAMPER9__GPIO5_IO09  0x10B0
    >;
};
```

> **pinctrl 数值**：GT9147 中断脚用 `0x10B0`，而 FT5426 触控用 `0xF080`。两者电气特性不同，不可混用。

### 3.4 内核配置

```kconfig
CONFIG_TOUCHSCREEN_GOODIX=y
```

### 3.5 ⚠️ 兼容性说明

内核 `drivers/input/touchscreen/goodix.c` 的 `of_device_id` 表中没有 `"goodix,gt9147"` 条目，仅有 `"goodix,gt911"`、`"goodix,gt927"` 等。但已验证：Linux 4.1.15 的 `i2c-core` 中 `i2c_device_probe` **不强制 OF 匹配检查**，无条件调用 `driver->probe`（id 可为 NULL），驱动通过 dts 属性（interrupt/reset-gpios）完成初始化，因此 gt9147 节点能正常绑定工作。

### 3.6 冲突处理（关键）

GT9147 的 GPIO1_IO09 和 GPIO5_IO09 与板级默认功能存在冲突，做了两处修改：

**① lcdif 移除 pinctrl_lcdif_reset 引用**

本工程 hwt dts（源自 NXP EVK 风格）曾让 `&lcdif` 的 `pinctrl-0` 引用 `&pinctrl_lcdif_reset`，将 GPIO5_IO09 占为 LCD 复位输出；而**正点原子原版 dts 并不引用该组**（复位脚归触摸管）。移除后与原版一致，GPIO5_IO09 完全交给 GT9147 驱动控制。

```dts
&lcdif {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_lcdif_dat
                 &pinctrl_lcdif_ctrl>;       /* 不含 pinctrl_lcdif_reset */
    /* ... */
};
```

**② hog 组删除 GPIO1_IO09 的 SD1 RESET 占用**

`pinctrl_hog_1` 组中注释掉 `MX6UL_PAD_GPIO1_IO09` 的默认配置，确保 hog 组不抢占该引脚：

```dts
pinctrl_hog_1: hoggrp-1 {
    fsl,pins = <
        MX6UL_PAD_UART1_RTS_B__GPIO1_IO19    0x17059 /* SD1 CD */
        MX6UL_PAD_GPIO1_IO05__USDHC1_VSELECT  0x17059 /* SD1 VSELECT */
        /* Alientek 板上 GPIO1_IO09 是电容触摸中断，不是 SD1 RESET */
    >;
};
```

### 3.7 电阻触摸关闭

为确保电容触摸注册为 `event1`（避免电阻 TSC 抢先占用该编号），将 SOC 内置 TSC 禁用：

```dts
&tsc {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_tsc>;
    xnur-gpio = <&gpio1 3 GPIO_ACTIVE_LOW>;
    measure-delay-time = <0xffff>;
    pre-charge-time = <0xfff>;
    status = "disabled";  /* 使用电容触摸 GT9147，关闭电阻 TSC */
};
```

> 这样内核输入设备的 event 分配：`event0` = snvs-powerkey，`event1` = GT9147。rootfs 中 `/etc/profile.d/qt.sh` 的 `QT_QPA_GENERIC_PLUGINS=evdevtouch:/dev/input/event1` 即可正确定位。

---

## 4. 声卡

### 4.1 硬件说明

ALIENTEK EMMC 板上**没有焊接 WM8960 音频 codec**，因此声卡功能不可用。

### 4.2 DTS 禁用

两处设为 `status = "disabled"`：

```dts
/* sound 节点 */
sound {
    compatible = "fsl,imx6ul-evk-wm8960", "fsl,imx-audio-wm8960";
    model = "wm8960-audio";
    /* ... */
    status = "disabled";    /* 板子无 WM8960 codec，禁用声卡 */
};
```

```dts
/* I2C2 上的 codec 节点 */
codec: wm8960@1a {
    compatible = "wlf,wm8960";
    reg = <0x1a>;
    /* ... */
    status = "disabled";    /* 板子无 WM8960 codec */
};
```

### 4.3 ⚠️ 踩坑记录

如果不禁用上述节点，内核启动时会反复打印：

```
wm8960 1-001a: Failed to issue reset
```

后续执行 `aplay -l` 或 `arecord -l` 会提示：

```
No soundcards found...
```

虽然这不影响系统运行（声卡驱动 probe 失败后会跳过），但启动日志中的错误信息容易给调试带来干扰。禁用后日志干净。

---

## 5. MAC 地址

### 5.1 DTS 设置

板载双路以太网（FEC1 + FEC2），MAC 地址在 dts 中硬编码：

```dts
&fec1 {
    local-mac-address = [00 04 9f 01 02 03];
    /* ... */
};

&fec2 {
    local-mac-address = [00 04 9f 01 02 04];
    /* ... */
};
```

| 接口 | MAC 地址 | 说明 |
|-------|---------|------|
| FEC1 (eth0) | `00:04:9f:01:02:03` | 主网口 |
| FEC2 (eth1) | `00:04:9f:01:02:04` | 从网口 |

> `00:04:9f` 为 NXP（原 Freescale）OUI。

### 5.2 U-Boot 环境变量

U-Boot 中设置相同的 MAC 地址，确保 Pre-boot 阶段网络可用：

```
ethaddr=00:04:9f:01:02:03
eth1addr=00:04:9f:01:02:04
```

### 5.3 ⚠️ 量产注意事项

目前所有开发板共用同一组 MAC 地址。**量产时必须为每台设备分配唯一地址**，避免局域网内冲突。推荐方案：

- 使用芯片 UID（`/sys/fsl_uid`）生成 MAC
- 或在量产烧录时通过 U-Boot 脚本写 `env` 分区写入唯一地址

---

## 6. 部署方式

### 6.1 内核 / DTB 更新

SD 卡分区结构（FAT boot + ext4 rootfs）：

```
mmcblk0p1     FAT16       boot 分区（zImage + dtb）
mmcblk0p2     ext4        rootfs 分区
```

更新内核或设备树：

```bash
# 在目标机上挂载 boot 分区
mount /dev/mmcblk0p1 /boot

# 直接覆盖文件
cp zImage /boot/
cp imx6ull-alientek-emmc.dtb /boot/

# 重启
reboot
```

> 或在 Windows 下取出 SD 卡直接替换 FAT 分区中的文件——**最简单，最安全**。

### 6.2 U-Boot 更新

如需更新 U-Boot，需 `dd` 写入 SD 卡偏移位置：

```bash
# bs=1k seek=1 跳过第一个 1KB（分区表/头信息）
dd if=u-boot.imx of=/dev/sdX bs=1k seek=1 conv=fsync
```

> **本次硬件适配涉及环境变量新增（ethaddr/eth1addr），U-Boot 二进制不需要重新烧录**。dts 中 local-mac-address 优先级高于 U-Boot 环境变量，内核启动后自动生效。

---

## 7. 经验总结

以下是在本项目中积累的硬件适配经验，供后续维护参考。

### 7.1 厂商 SDK 是最好的文档

正点原子提供的 linux-imx 4.1.15 和 uboot-imx 源码中包含了完整的板级支持：dts、defconfig、头文件、驱动配置。遇到任何硬件适配问题，**首先查阅厂商原版 dts 和内核 config**，比 NXP 官方 EVK 的配置更贴近实际硬件。

### 7.2 修改前 grep 查引脚冲突

在改动 dts 或添加新外设前，务必全文搜索目标引脚是否已被占用。例如 GPIO1_IO09 同时被以下功能引用：

- SD1 RESET（NXP EVK dts 遗留，本板无此连接）
- GT9147 触摸中断（本项目使用）

提前 grep 可避免踩坑：

```bash
grep -rn 'GPIO1_IO09\|gpio1 9\|GPIO1.09' arch/arm/boot/dts/
```

### 7.3 LCD 时序参数速查表

| 分辨率 | clock-frequency | bpp | bus-width | 对应 LCD 型号 |
|--------|----------------|-----|-----------|--------------|
| 800×480 | 31 MHz | 24 | 24 | ATK-LCD-4.3-800x480（本项目） |
| 480×272 | 9.2 MHz | 16 | 16 | ATK-4.3'-480x272（旧屏，不适用） |
| 1024×600 | 51 MHz | 24 | 24 | ATK-LCD-7-1024x600（未验证） |

### 7.4 触摸驱动 pinctrl 参考

| 触控芯片 | pinctrl 数值 | compatible |
|---------|-------------|------------|
| GT9147 | `0x10B0` | `goodix,gt9147` / `goodix,gt9xx` |
| FT5426 | `0xF080` | `edt,edt-ft5406`（FT5x06 系列） |

### 7.5 event 分配速查

```bash
# 在目标机上查看输入设备
cat /proc/bus/input/devices
```

预期分配：

| event | 设备 | 备注 |
|-------|------|------|
| `event0` | gpio-keys / 按键 | 系统按键 |
| `event1` | GT9147 触摸 | Qt 触摸事件来源 |
| `event2` | 其他（如鼠标） | 可选 |

> Qt 环境变量设置（已在 `/etc/profile.d/qt.sh` 中配好）：`export QT_QPA_GENERIC_PLUGINS=evdevtouch:/dev/input/event1`

### 7.6 内核配置确认命令

每次修改 dts 后，编译前检查相关内核配置是否开启：

```bash
grep -E "CONFIG_TOUCHSCREEN_GOODIX|CONFIG_FB_IMX" hwt/linux/arch/arm/configs/linux_hwt_defconfig
```

### 7.7 验证清单（在目标板上检查）

| 检查项 | 命令 | 预期结果 |
|--------|------|---------|
| LCD 显示 | 观察屏幕 | 正点原子默认图标 + Qt 界面正常 |
| 触摸输入 | evtest /dev/input/event1 | 触摸时上报 ABS_MT events |
| 网口 MAC | ifconfig eth0 / eth1 | HWaddr 显示 00:04:9f:01:02:03 / 04 |
| 无 WM8960 报错 | dmesg | grep -i wm8960 | 无输出 |
| 触摸 event 号 | cat /proc/bus/input/devices | GT9147 对应 event1 |
| Qt 触摸工作 | 启动 HMI 应用 | 触摸按下有反应，位置准确 |

---

*文档版本: v1.0 — 2026-07-22*

---

