# LCD 显示问题排查（ARM 嵌入式 + Qt linuxfb）

## 排查树

```
显示异常
├── 画面错位/拉伸/花屏/局部显示
│   └── dts 时序与实际屏幕不匹配（分辨率、像素时钟、porch）
│       → 对照厂商 dts 注释块中的官方时序修改
├── 颜色不对（偏色/红蓝互换）
│   └── 像素格式不匹配
│       1. 先查 bpp：dts bits-per-pixel 用厂商验证值（正点原子=24，勿自作主张改 16）
│       2. bpp 正确仍偏色 → RGB/BGR 通道交换 → RGB 色块法确认后调像素格式
├── 背光亮但全黑/全白
│   └── 时序完全不对或 lcdif/pinctrl/背光 PWM 未使能
└── 有显示但触摸不灵
    └── evdev 设备号不对：cat /proc/bus/input/devices 查触摸屏 event 号，
        改 /etc/profile.d/qt.sh 的 QT_QPA_GENERIC_PLUGINS=evdevtouch:/dev/input/eventN
```

## 识别屏幕型号

**U-Boot 启动串口打印**会显示屏幕型号（厂商 U-Boot 从 LCD 板载 EEPROM/ID 读取）：
`Display: ATK-LCD-4.3-800x480 (800x480)`

注意：U-Boot 能自动识别 ≠ 内核 dts 配置正确。内核显示用 dts 时序，两者独立。

## 获取官方时序

芯片厂商内核源码的原始 dts 里通常有**全部屏型号的注释块**，直接照搬：

```bash
tar -xjf <kernel-src>.tar.bz2 -C /tmp <dts 相对路径>
grep -n 'hactive' <dts>   # 定位所有 display 节点
```

正点原子 ATK-LCD-4.3-800x480 官方值（mxsfb, DE 模式）：

```
bits-per-pixel = <24>;  bus-width = <24>;
clock-frequency = <31000000>;
hactive=<800>; vactive=<480>;
hfront-porch=<40>; hback-porch=<88>; hsync-len=<48>;
vfront-porch=<13>; vback-porch=<32>; vsync-len=<3>;
hsync-active=<0>; vsync-active=<0>; de-active=<1>; pixelclk-active=<0>;
```

## RGB 色块诊断法

在 Qt 应用里放三个纯色块（#FF0000/#00FF00/#0000FF）：
- 显示 红/绿/蓝 → 像素格式正确
- 红↔蓝互换 → RGB565/888 通道序问题（BGR），调 dts 像素格式或 Qt 端处理
- 整体偏色/发暗 → bpp 不匹配（如 fb 16bpp vs Qt 32bpp 渲染假设）

## 电容触摸接入（以 GT9147 为例）

### dts 节点

厂商原版 dts 通常自带触摸节点（默认 status="disable" 并注释需要时改 okay），
照搬其 compatible/地址/复位中断脚/pinctrl 配置值：

| 芯片 | I2C 地址 | pinctrl 配置值 |
|------|---------|---------------|
| GT9147 | 0x14 | 0x10B0 |
| FT5426 | 0x38 | 0xF080 |

以 i.MX6ULL 正点原子板 GT9147 为例（照原版 dts，注意在 **I2C2** 上）：

```dts
&i2c2 {
    gt9147: gt9147@14 {
        compatible = "goodix,gt9147", "goodix,gt9xx";
        reg = <0x14>;
        pinctrl-names = "default";
        pinctrl-0 = <&pinctrl_gt9xx &pinctrl_gt9xx_reset>;
        interrupt-parent = <&gpio1>;
        interrupts = <9 0>;
        reset-gpios = <&gpio5 9 GPIO_ACTIVE_LOW>;
        interrupt-gpios = <&gpio1 9 GPIO_ACTIVE_LOW>;
        status = "okay";
    };
};
```

## goodix 驱动缺少 gt9147 匹配项

内核 4.1.15 自带 `goodix.c` 的 `of_device_id` 表没有 `gt9147`（仅有 `gt911`/`gt912`/`gt927` 等）。i2c 设备绑定走 `i2c_device_match` 三层检查（OF→ACPI→ID table），三者全败 → 驱动不绑定 → probe 从未调用 → 无任何内核日志。**不能简单认为"i2c probe 不强制 OF 匹配"——匹配失败发生在总线层，probe 根本不会被调用。**

**修复方法**：在 `goodix.c` 的 `of_device_id` 表中，`gt911` 条目后追加：

```c
{ .compatible = "goodix,gt9147", },
{ .compatible = "goodix,gt9xx", },
```

补丁文件放到 HWT 覆盖层（`hwt/linux/drivers/input/touchscreen/goodix.c`），随内核一起编译。

### 引脚冲突排查

`grep` 目标引脚在整个 dts 的占用。两类典型冲突：

1. **厂商板上 LCD 复位与触摸复位共线** → `lcdif` 节点不应引用 `lcdif_reset` pinctrl，与原版对齐
2. **NXP EVK 遗留占用**（如 GPIO1_IO09 被当 SD 复位）→ 删除

### event 号规划

关闭不用的输入设备节点（如 SOC 电阻 TSC `&tsc disabled`），
保证目标触摸设备落到 Qt 环境变量指定的 `/dev/input/eventN`。

### 内核开关

```
CONFIG_TOUCHSCREEN_GOODIX=y
```

## 触控硬件故障诊断决策树

当所有软件修复无效（dts 正确、驱动已绑定、中断注册成功）但触摸仍无响应时，按以下流程判断是否为硬件故障：

### Step 0: 确认芯片在线

```bash
i2c_scan /dev/i2c-1   # 扫描 I2C2 总线，确认 0x14 有应答
```

若芯片地址消失或间歇性丢失 → 供电/焊接/虚焊问题。

### Step 1: 写入官方配置表 + 校验和

汇顶芯片上电后需 host 写入配置表（含校验和）才能正常工作。若驱动未配表或校验和错误，芯片会 ACK I2C 但不报触摸。

```bash
# 读状态寄存器（0x814E），正常应返回 0x80（数据就绪）或 0x00（空闲）
i2cread /dev/i2c-1 0x14 0x814E 1

# 读坐标区（0x8140 起 32 字节），触摸时应非零
i2cread /dev/i2c-1 0x14 0x8140 32
```

写入正确配置表后仍无效 → 进入下一步。

### Step 2: ISR 加调试打印看 GSTID

在 goodix 驱动的 ISR 中插打印，查看 `GSTID` 寄存器（0x814E）：

```c
// 在 goodix_ts_irq_handler() 中临时添加
u8 status;
goodix_i2c_read(ts->client, GOODIX_READ_COORDS_ADDR, &status, 1);
printk("GSTID=0x%02x
", status);
```

- 触摸时 GSTID 从 0x00 → 非零（通常 0x80=数据就绪，0x01=有触点）→ 正常
- 触摸时 GSTID 始终 0x00 → 芯片检测到中断但内部无坐标数据 → **传感器失效**

### Step 3: 中断计数是否在涨？

```bash
cat /proc/interrupts | grep gt9xx
```

- **中断计数涨 + GSTID=0x00** → 芯片 IRQ 脚正常，但触摸传感器本身坏了（碎裂/压坏/内部断线）
- **中断计数不涨** → 中断通路问题（GPIO 配置/硬件连线/电平不匹配）

### Step 4: 冷启动后芯片 ID 是否稳定？

读芯片 ID 寄存器（GT9147 的 0x8140-0x8143 应返回 "9147"）：

```bash
# 连续冷启动 3 次，每次读芯片 ID
i2cread /dev/i2c-1 0x14 0x8140 4
```

- 每次返回相同 ID（如 `91 47 xx xx`）→ 芯片正常
- ID 随机变化（如一次 `9147`、一次 `FFFF`、一次 `0000`）→ **内部 flash 损坏**，芯片已不可靠

### Step 5: 原厂系统验证

把触摸屏接到原厂开发板（如正点原子出厂系统）测试：

- 原厂系统同样失败 → **硬件故障定案**（换屏）
- 原厂系统正常 → 回到软件排查（驱动版本/配置表/dts 时序）

### 判断口诀

> **ISR 风暴（IRQ 百万级）→ 换驱动；ISR 正常但 GSTID 始终零 → 硬件问题。**

- ISR 风暴：驱动缺配置表或中断触发模式错误（如 level 配成 edge），IRQ 无限重入
- GSTID 始终零：中断通路正常，但传感层无数据上报，通常是物理损坏

### ⚠️ GPIO active-low 陷阱

汇顶驱动中 `reset-gpios` 标记为 `GPIO_ACTIVE_LOW`，GPIO 子系统的语义是：

```c
gpiod_set_value(rst, 0) = deassert（释放复位，即拉高）   // 不是 assert！
gpiod_set_value(rst, 1) = assert（拉低复位）            // 才是拉低
```

这是反复踩坑的点：`GPIO_ACTIVE_LOW` 翻转了 `gpiod_set_value` 的物理电平含义。写 `1` 才是拉低复位，写 `0` 是释放复位。

## 无声卡时清理报错

板子无 codec（如 WM8960）时，把 dts 的 sound 节点和 codec 节点都 `status="disabled"`，消除 probe 报错。

## Qt 侧配合

- 应用 `showFullScreen()` 而非 `resize(w,h)`，自适应 fb 实际分辨率
- 查看 fb 实际参数：`cat /sys/class/graphics/fb0/modes`、`fbset -s`、
  `cat /sys/class/graphics/fb0/bits_per_pixel`
- dts 改动后需重编 dtb 并烧录/替换 boot 分区中的 dtb 才生效
