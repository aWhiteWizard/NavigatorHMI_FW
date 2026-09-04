#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
D1 OTA 固件打包脚本（2026-08-30 批 2-5）——Docker 构建后产出 .fw（NHFW 格式，与 PC FwPackageBuilder 互读）。
字节级布局以 PC 侧 FwPackageBuilder.cs 为单一事实源（本脚本与之严格一致）：

  Header 128B：magic "NHFW"(4) + version(16, ASCII 右补空格) + timestamp(8, LE) +
                component_count(4, LE) + sha256(64, payload 整体 SHA256 hex 小写, 右补空格)
  Component Table：每项 184B——name(32) + type(16) + target(48) + size(8, LE) + sha256(64, 组件 hex 小写) + version(16)
  Payload：各组件二进制按表序拼接（偏移由表序+size 推导，无 offset 字段）

O-D D-3（2026-09）rootfs 文件级组件：--rootfs-files <dir> 打包目录为「文件级 rootfs」组件（type=rootfs），
payload = 连续文件段（对齐 FW otaupdater installRootfsFiles 解析）：
  循环到 payload 尾：
    4B LE 路径长度 + 路径 UTF8（相对 <dir> 根的安装相对路径，如 usr/sbin/start_runtime.sh）
    8B LE 内容长度 + 内容
（目录递归收集全部常规文件；符号链接/特殊文件拒绝——FW 端同白名单：禁绝对路径与 ..）

用法：
  pack_fw.py --version 1.1.0 --out /path/to \
      --app /usr/bin/navigatorhmi-fw  \
      [--rootfs-files /path/to/file-tree-dir] [--kernel /path/to/boot.img] [--rootfs /path/to/rootfs.img]
  # 调试 OTA（2026-09-04 用户规范）：版本恒 v1.1.0 + --name-ts <本地时刻 YYYYMMDDHHMMSS>
  #   → NavigatorHMI_v1.1.0_7inch_20260904185622.fw（例：Windows 侧 `Get-Date -Format yyyyMMddHHmmss` 取值，
  #     容器内 date 为 UTC 会差 8 小时——时刻由调用方给本地时间）
组件类型（用户 2026-08-30 分组）：app / rootfs（文件级或整镜像）/ kernel；U-Boot 不打包。
kernel 分区写需 backup 分区兜底——O-D D-3 板端勘察 backup 32MB 装不下 boot 64MB → 本轮不产 kernel 包。
"""
import argparse
import hashlib
import os
import struct
import time

MAGIC = b"NHFW"
HEADER_SIZE = 128
ENTRY_SIZE = 184
ALLOWED_TYPES = ("app", "kernel", "rootfs")


def fixed(s: str, length: int) -> bytes:
    """ASCII 定长右补空格；非 ASCII/超长抛错（与 C# FixedField 一致）。"""
    b = s.encode("ascii")  # 非 ASCII 抛 UnicodeEncodeError
    if len(b) > length:
        raise ValueError(f"字段超长: {s!r} ({len(b)} > {length})")
    return b.ljust(length, b" ")


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def collect_file_entries(root: str):
    """递归收集目录常规文件 → [(rel_posix_path, bytes)]。符号链接/特殊文件拒绝（对齐 FW 白名单）。"""
    entries = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for fn in sorted(filenames):
            full = os.path.join(dirpath, fn)
            if os.path.islink(full):
                raise ValueError(f"rootfs-files 不支持符号链接: {full}")
            rel = os.path.relpath(full, root).replace("\\", "/")
            if rel.startswith("/") or ".." in rel.split("/"):
                raise ValueError(f"rootfs-files 路径非法: {rel}")
            with open(full, "rb") as f:
                entries.append((rel, f.read()))
    return entries


def build_rootfs_files_payload(entries):
    """文件段 payload：4B 路径长+路径 + 8B 内容长+内容（循环）。"""
    out = bytearray()
    for rel, content in entries:
        pb = rel.encode("utf-8")
        if len(pb) > 512:
            raise ValueError(f"rootfs-files 路径超长: {rel}")
        out += struct.pack("<i", len(pb))
        out += pb
        out += struct.pack("<q", len(content))
        out += content
    return bytes(out)


def size_to_inch(size: str) -> str:
    """尺寸（"7寸"/"7"）→ 文件名 inch 段（7inch/4inch；与 PC FirmwareFolderService.SizeToInchTag 同口径）。"""
    s = (size or "").strip().lower()
    if not s:
        return "unknown"
    if s.endswith("inch"):
        return s
    return s.replace("寸", "") + "inch"


def build(version: str, components, out_dir: str, size: str = "", name_ts: str = "") -> str:
    if not version or len(version) > 16:
        raise ValueError("version 必填且 ≤16 字符")
    if not all(c in "0123456789." for c in version) or not 1 <= len(version.split(".")) <= 3:
        raise ValueError("version 格式 x.y.z 纯数字段")

    # 校验组件 + 构建 payload
    payload = b""
    table = []
    for name, ctype, target, path in components:
        if ctype not in ALLOWED_TYPES:
            raise ValueError(f"非法组件类型: {ctype}（允许 app/rootfs/kernel）")
        # O-D D-3：rootfs 组件两种形态——目录（文件级，本函数内已展开为 payload）或镜像文件路径
        if os.path.isdir(path):
            entries = collect_file_entries(path)
            if not entries:
                raise ValueError(f"rootfs-files 目录为空: {path}")
            data = build_rootfs_files_payload(entries)
        else:
            if not os.path.isfile(path):
                raise FileNotFoundError(f"组件文件不存在: {path}")
            with open(path, "rb") as f:
                data = f.read()
        payload += data
        table.append((name, ctype, target, len(data), sha256_hex(data)))

    payload_sha = sha256_hex(payload)
    timestamp = int(time.time())

    # Header
    out = bytearray()
    out += MAGIC
    out += fixed(version, 16)
    out += struct.pack("<q", timestamp)      # 8B LE
    out += struct.pack("<i", len(table))     # 4B LE
    out += fixed(payload_sha, 64)
    out += b" " * (HEADER_SIZE - len(out))

    # Component Table（184B/项）——循环变量用 csize 防覆盖 build 的 size 参数（2026-09 命名参数冲突修复）
    for name, ctype, target, csize, sha in table:
        out += fixed(name, 32)
        out += fixed(ctype, 16)
        out += fixed(target, 48)
        out += struct.pack("<q", csize)      # 8B LE
        out += fixed(sha, 64)
        out += fixed(version, 16)
        assert len(out) % ENTRY_SIZE == HEADER_SIZE % ENTRY_SIZE  # 布局防御

    # Payload
    out += payload

    os.makedirs(out_dir, exist_ok=True)
    # 命名（2026-09-04 用户调试 OTA 规范 + 旧格式兼容）：
    #   name_ts 提供 → 调试命名 NavigatorHMI_v<版本>_<尺寸inch>_<打包时刻 YYYYMMDDHHMMSS>.fw
    #     （调试期每次编译版本恒 v1.1.0，靠时间戳区分——时刻由调用方给本地时间，防容器 UTC 差 8 小时；
    #       GUI ParseFwVersion 从 "_v" 截取语义版本，尾段非数字归 0 → 仍解析为 1.1.0）
    #   size 提供（无 name_ts）→ 标准命名 NavigatorHMI_<尺寸>inch_v<版本>.fw（7寸→7inch）
    #   无 → 旧命名兼容 NavigatorHMI_v<版本>.fw
    if name_ts:
        fw_name = f"NavigatorHMI_v{version}_{size_to_inch(size)}_{name_ts}.fw"
    elif size:
        fw_name = f"NavigatorHMI_{size_to_inch(size)}_v{version}.fw"
    else:
        fw_name = f"NavigatorHMI_v{version}.fw"
    fw_path = os.path.join(out_dir, fw_name)
    tmp = fw_path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(out)
    os.replace(tmp, fw_path)  # 原子替换（与 C# File.Move overwrite 同语义）
    print(f"[pack_fw] {fw_path} ({len(out)} bytes, {len(table)} components)")
    return fw_path


def main():
    ap = argparse.ArgumentParser(description="D1 OTA 固件打包（NHFW，与 PC FwPackageBuilder 互读）")
    ap.add_argument("--version", required=True, help="固件版本（x.y.z 纯数字）")
    ap.add_argument("--out", required=True, help="输出目录")
    ap.add_argument("--size", default="", help="设备尺寸（7寸→文件名 inch 段 7inch；省略 → 旧命名 NavigatorHMI_v...fw）")
    ap.add_argument("--name-ts", default="", help="调试命名时间戳（YYYYMMDDHHMMSS，本地时间；提供则输出 NavigatorHMI_v<版本>_<尺寸>_<时间戳>.fw——2026-09-04 用户调试 OTA 规范）")
    ap.add_argument("--app", help="app 组件路径（/usr/bin/navigatorhmi-fw 源文件）")
    ap.add_argument("--rootfs-files", help="rootfs 文件级组件目录（递归收集常规文件 → 文件段 payload）")
    ap.add_argument("--kernel", help="kernel 组件路径（boot.img——O-D D-3 backup 容量不足，本轮不建议）")
    ap.add_argument("--rootfs", help="整镜像 rootfs 组件路径（rootfs.img——O-D D-3 建议改用 --rootfs-files）")
    args = ap.parse_args()

    comps = []
    if args.app:
        comps.append(("app", "app", "/usr/bin/navigatorhmi-fw", args.app))
    if args.rootfs_files:
        comps.append(("rootfs-files", "rootfs", "/", args.rootfs_files))
    if args.kernel:
        comps.append(("kernel", "kernel", "/dev/block/by-name/boot", args.kernel))
    if args.rootfs:
        comps.append(("rootfs", "rootfs", "/", args.rootfs))
    if not comps:
        ap.error("至少提供一个组件：--app / --rootfs-files / --rootfs / --kernel")
    build(args.version, comps, args.out, args.size, args.name_ts)


if __name__ == "__main__":
    main()

