#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# B-2 Do: 备份官方 rootfs + 查分区 metadata (准备 bundle 升级)
import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.146', username='root', password='topeet', timeout=10)

steps = [
    ("分区表", "ls -la /dev/disk/by-partlabel/ 2>&1"),
    ("rootfs 分区大小", "blockdev --getsize64 /dev/disk/by-partlabel/rootfs 2>&1"),
    ("rootfs 文件系统", "blkid /dev/disk/by-partlabel/rootfs 2>&1"),
    ("boot 分区", "blockdev --getsize64 /dev/disk/by-partlabel/boot 2>&1; blkid /dev/disk/by-partlabel/boot 2>&1"),
    ("磁盘空间", "df -h / /tmp 2>&1 | tail -3"),
    ("stage 目录", "ls -la /var/lib/rknetupdate/ 2>&1"),
    ("bundle-v1 目录", "ls -la /var/lib/rknetupdate/bundle-v1/ 2>&1"),
    ("tune2fs rootfs", "tune2fs -l /dev/disk/by-partlabel/rootfs 2>&1 | grep -E 'Filesystem UUID|Block size|Block count|Filesystem features' | head -5"),
]
for name, c in steps:
    print(f"=== {name} ===")
    stdin, stdout, stderr = ssh.exec_command(c)
    print(stdout.read().decode(errors='replace')[:400] or "(空)")
ssh.close()
