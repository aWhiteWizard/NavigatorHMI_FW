#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# B-2/B-3: 解析 update.img (RKFW) 生成 updateimg.manifest.json
# 依据 SDK external/rknetupdate/internal/rkimage/updateimg.go
# 用法: gen-updateimg-manifest.py <update.img路径> [输出manifest路径]
import struct, hashlib, json, sys, os

path = sys.argv[1] if len(sys.argv) > 1 else r"D:\资料\iTOP-3562开发板\系统烧写\RGB_7_0屏幕镜像\update.img"
out  = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(path), "updateimg.manifest.json")

def cstring(b):
    i = b.find(b'\x00')
    if i >= 0: b = b[:i]
    return b.decode(errors='replace').strip()

with open(path, 'rb') as f:
    fsize = f.seek(0, 2)
    f.seek(0)
    header = f.read(102)
    assert header[:4] == b'RKFW', f"magic: {header[:4]}"
    fw_offset = struct.unpack('<I', header[33:37])[0]
    fw_size   = struct.unpack('<I', header[37:41])[0]
    print(f"fw_offset={fw_offset} fw_size={fw_size} file={fsize}")

    f.seek(fw_offset)
    fw = f.read(fw_size)
    assert fw[:4] == b'RKAF', f"fw magic: {fw[:4]}"
    count = struct.unpack('<i', fw[136:140])[0]
    print(f"items={count}")

    items = []
    for i in range(count):
        entry = fw[140+i*112:140+(i+1)*112]
        name = cstring(entry[:32])
        offset = struct.unpack('<I', entry[96:100])[0]
        if entry[82:83] == b'H':
            offset |= struct.unpack('<I', entry[82+1:82+5])[0] << 32
        size = struct.unpack('<I', entry[108:112])[0]
        if entry[87:88] == b'H':
            size |= struct.unpack('<I', entry[88:92])[0] << 32
        flash_off = struct.unpack('<I', entry[100:104])[0]
        start = fw_offset + offset
        # item sha256
        f.seek(start)
        h = hashlib.sha256()
        remaining = size
        while remaining > 0:
            chunk = f.read(min(1 << 20, remaining))
            if not chunk: break
            h.update(chunk)
            remaining -= len(chunk)
        items.append({"name": name, "offset": start, "size": size,
                      "flash_offset": flash_off, "sha256": h.hexdigest()})
        print(f"  {name}: off={start} size={size} flash={flash_off}")

    # 整包 sha256
    f.seek(0)
    h = hashlib.sha256()
    while True:
        chunk = f.read(1 << 22)
        if not chunk: break
        h.update(chunk)
    pkg_sha = h.hexdigest()

manifest = {
    "schema": 1,
    "image": "update.img",
    "size": fsize,
    "sha256": pkg_sha,
    "expected_parent": "/dev/mmcblk0",
    "items": items,
}
with open(out, 'w', encoding='utf-8') as f:
    json.dump(manifest, f, indent=2)
print(f"\nmanifest 写入 {out}")
print(f"package sha256: {pkg_sha}")
print(f"package size: {fsize}")
