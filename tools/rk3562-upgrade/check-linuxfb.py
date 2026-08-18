#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# B-3: 试 linuxfb 平台跑 qt-test (无 GPU 快速验证)
import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.146', username='root', password='topeet', timeout=10)

checks = [
    ("fb0 设备", "ls -la /dev/fb* 2>&1"),
    ("linuxfb 插件", "ls /usr/plugins/platforms/libqlinuxfb.so 2>&1"),
]
for n, c in checks:
    print(f"=== {n} ===")
    stdin, stdout, stderr = ssh.exec_command(c)
    print(stdout.read().decode(errors='replace')[:300])

# 试 linuxfb 跑 qt-test
print("=== 试 linuxfb 跑 qt-test (5s) ===")
cmd = ("export QT_QPA_PLATFORM=linuxfb QT_QPA_PLATFORM_PLUGIN_PATH=/usr/plugins/platforms QT_PLUGIN_PATH=/usr/plugins; "
       "killall qt-test 2>/dev/null; timeout 6 /usr/bin/qt-test 2>&1 | head -8; echo EXIT=$?")
stdin, stdout, stderr = ssh.exec_command(cmd, timeout=30)
print(stdout.read().decode(errors='replace')[:500])
ssh.close()
