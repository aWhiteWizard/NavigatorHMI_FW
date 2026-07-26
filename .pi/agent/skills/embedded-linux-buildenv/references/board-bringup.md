# 目标机启动问题速查

本文收集板子 bring-up 阶段串口日志的常见问题与定性。

## 随机 MAC 地址

**现象**：`fec: Invalid MAC address 00:00... Using random MAC address`

**根因**：U-Boot 环境变量无 ethaddr（env bad CRC 走默认）且未烧 MAC fuse。

**修复优先级**：

1. **dts `&fec` 节点加 `local-mac-address`**（内核确定性 MAC，推荐）
2. **U-Boot `CONFIG_EXTRA_ENV_SETTINGS` 加默认 ethaddr/eth1addr**（U-Boot 网络操作用）
3. 或板子上 `setenv`+`saveenv`

**注意**：`local-mac-address` 会让所有板同 MAC，量产须每机唯一。

## U-Boot 环境变量 bad CRC

**现象**：`*** Warning - bad CRC, using default environment`

**含义**：env 分区无有效数据（首次启动正常），`saveenv` 后消失；但走默认环境期间自定义 env（panel/MAC 等）不生效。

## U-Boot 屏型号与内核显示无关

**现象**：U-Boot 打印的 `Display: xxx` 来自其 LCD 检测（可能走默认项报错型号）；内核显示只用 dts 时序，两者独立，U-Boot 报错型号不影响系统显示。

## 无害启动噪音速查表

| 打印内容 | 含义 |
|----------|------|
| Unable to read file boot.scr | 无 boot 脚本，正常 |
| EXT3/EXT2 couldn't mount 随后 EXT4 mounted | 按序尝试，正常 |
| QSPI unrecognized JEDEC id | 板子未贴 QSPI |
| Duplicate name in lcdif renamed display#1 | EVK 继承节点重名，无害 |
| mxsfb failed to find mxc display driver 随后 initialized | 正常 |
| devpts bogus options | busybox 经典无害 |
| libstdc++ no version information | 版本不一致警告，无害 |
| EXT4 re-mounted Opts data=ordered | 正常 |

## 无声卡报错

`wm8960 Failed to issue reset / No soundcards found` → 确认板子是否贴 codec，无则 dts 禁用 sound+codec 节点（详见 lcd-display-debug.md 同名小节）。

## SSH 密码过期与 shadow 修复

**现象**：SSH 提示 `WARNING: Your password has expired.` 修改密码后仍循环。

**根因**：`/etc/shadow` 第三字段（密码最后修改日）为 `0`（epoch 1970），SSHD 判定过期。

**修复**：

```bash
DAYS=$(expr $(date +%s) / 86400)
awk -F: -v d="$DAYS" 'BEGIN{OFS=":"} $1=="root"{$3=d}1' /etc/shadow > /tmp/s && mv /tmp/s /etc/shadow
```

| 字段 | 含义 | 错误值 | 正确值 |
|------|------|--------|--------|
| shadow 第3字段 | 密码最后修改日(epoch days) | `0` | 当前天数(如19987) |

**应急**(串口可用)：`passwd -d root` → `echo 'PermitEmptyPasswords yes' >> /etc/ssh/sshd_config` → 空密码登录 → `passwd` → `sed -i '/PermitEmptyPasswords/d' /etc/ssh/sshd_config`

**应急**(串口不可用)：U-Boot `init=/bin/sh` → `mount -o remount,rw /` → 重建 shadow → `sync` → `exec /sbin/init`

## 重烧镜像后 SSH host key 变更

现象：`WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!`——重烧 sdcard.img 后 openssh 首次启动生成新密钥对，Windows 客户端记录与板子不匹配。

Windows 修复命令：

```powershell
ssh-keygen -R 192.168.1.125
```

原则：任何需持久化的板级配置（网络、SSH 密钥等）必须写进 HWT rootfs-overlay，随镜像一起烧录；手工在板上修改重烧即丢。
