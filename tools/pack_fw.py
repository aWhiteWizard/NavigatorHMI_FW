#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
D1 OTA 固件打包脚本（2026-08-30 批 2-5）——Docker 构建后产出 .fw（NHFW 格式，与 PC FwPackageBuilder 互读）。
字节级布局以 PC 侧 FwPackageBuilder.cs 为单一事实源（本脚本与之严格一致）：

  Header 128B：magic "NHFW"(4) + version(16, ASCII 右补空格) + timestamp(8, LE) +
                component_count(4, LE) + sha256(64, payload 整体 SHA256 hex 小写, 右补空格)
  Component Table：每项 184B——name(32) + type(16) + target(48) + size(8, LE) + sha256(64, 组件 hex 小写) + version(16)
  Payload：各组件二进制按表序拼接（偏移由表序+size 推导，无 offset 字段）

用法：
  pack_fw.py --version 1.1.0 --out /path/to \
      --app /usr/bin/navigatorhmi-fw  \
      [--kernel /path/to/boot.img] [--rootfs /path/to/rootfs.img]
组件类型（用户 2026-08-30 分组）：app / kernel / rootfs；U-Boot 不打包。
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


def build(version: str, components, out_dir: str) -> str:
    if not version or len(version) > 16:
        raise ValueError("version 必填且 ≤16 字符")
    if not all(c in "0123456789." for c in version) or not 1 <= len(version.split(".")) <= 3:
        raise ValueError("version 格式 x.y.z 纯数字段")

    # 校验组件 + 构建 payload
    payload = b""
    table = []
    for name, ctype, target, path in components:
        if ctype not in ALLOWED_TYPES:
            raise ValueError(f"非法组件类型: {ctype}（允许 app/kernel/rootfs）")
        if not os.path.isfile(path):
            raise FileNotFoundError(f"组件文件不存在: {path}")
        data = open(path, "rb").read()
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

    # Component Table（184B/项）
    for name, ctype, target, size, sha in table:
        out += fixed(name, 32)
        out += fixed(ctype, 16)
        out += fixed(target, 48)
        out += struct.pack("<q", size)       # 8B LE
        out += fixed(sha, 64)
        out += fixed(version, 16)
        assert len(out) % ENTRY_SIZE == HEADER_SIZE % ENTRY_SIZE  # 布局防御

    # Payload
    out += payload

    os.makedirs(out_dir, exist_ok=True)
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
    ap.add_argument("--app", required=True, help="app 组件路径（/usr/bin/navigatorhmi-fw 源文件）")
    ap.add_argument("--kernel", help="kernel 组件路径（boot.img）")
    ap.add_argument("--rootfs", help="rootfs 组件路径（rootfs.img）")
    args = ap.parse_args()

    comps = [("app", "app", "/usr/bin/navigatorhmi-fw", args.app)]
    if args.kernel:
        comps.append(("kernel", "kernel", "/dev/block/by-name/boot", args.kernel))
    if args.rootfs:
        comps.append(("rootfs", "rootfs", "/dev/block/by-name/rootfs", args.rootfs))
    build(args.version, comps, args.out)


if __name__ == "__main__":
    main()
