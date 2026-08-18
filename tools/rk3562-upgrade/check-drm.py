#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# B-3: DRM connector 诊断
import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.146', username='root', password='topeet', timeout=10)

c1 = "ls /sys/class/drm/ | grep card; echo ---; for d in /sys/class/drm/card*-*/; do echo \"${d}: status=$(cat ${d}/status 2>/dev/null) type=$(cat ${d}/connector_type 2>/dev/null)\"; done"
print("=== DRM 输出 ===")
stdin, stdout, stderr = ssh.exec_command(c1)
print(stdout.read().decode(errors='replace')[:600])

c2 = "dmesg | grep -iE 'drm|kms|connector|lcd|rgb' | head -15"
print("=== DRM 日志 ===")
stdin, stdout, stderr = ssh.exec_command(c2)
print(stdout.read().decode(errors='replace')[:700])
ssh.close()
