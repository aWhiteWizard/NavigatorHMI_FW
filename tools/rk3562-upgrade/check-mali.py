#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# B-3: 板子 mali 状态诊断
import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.146', username='root', password='topeet', timeout=10)
checks = [
    ("mali 设备节点", "ls -la /dev/mali* /dev/dri/ 2>&1"),
    ("libmali 库", "ls /usr/lib/libmali* /usr/lib/aarch64-linux-gnu/libmali* 2>&1"),
    ("内核 mali 日志", "dmesg | grep -iE 'mali|gpu' | head -8"),
    ("内核模块目录", "ls /lib/modules/ 2>&1"),
    ("mali 内核模块", "find /lib/modules -name '*mali*' 2>/dev/null | head -5"),
    ("libEGL", "find /usr -name 'libEGL*' 2>/dev/null | head -5"),
]
for n, c in checks:
    print(f"=== {n} ===")
    stdin, stdout, stderr = ssh.exec_command(c)
    print(stdout.read().decode(errors='replace')[:400] or "(空)")
ssh.close()
