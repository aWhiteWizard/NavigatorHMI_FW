#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# B-3: 从官方 update.img 提取 recovery.img (用 manifest items)
import struct, json, sys, os

img_path = r"D:\资料\iTOP-3562开发板\系统烧写\RGB_7_0屏幕镜像\update.img"
mani_path = r"D:\ProgramData\Agent_backup\updateimg.manifest.json"
out_dir = r"D:\ProgramData\Agent_backup\unpk"

with open(mani_path, encoding='utf-8') as f:
    manifest = json.load(f)

os.makedirs(out_dir, exist_ok=True)
with open(img_path, 'rb') as f:
    for item in manifest['items']:
        name = item['name']
        off, size = item['offset'], item['size']
        if name in ('recovery', 'boot', 'uboot', 'parameter', 'bootloader', 'misc'):
            f.seek(off)
            data = f.read(size)
            with open(os.path.join(out_dir, f'{name}.img'), 'wb') as of:
                of.write(data)
            print(f"{name}.img: {size} bytes 提取完成")

print(f"\n提取目录: {out_dir}")
