#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# B-2 Plan 调研: 官方 Debian 网络配置/自启方式 (只读)
import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.146', username='root', password='topeet', timeout=8)

checks = [
    ("netplan", "ls /etc/netplan/ 2>&1; cat /etc/netplan/*.yaml 2>&1 | head -15"),
    ("network/interfaces", "cat /etc/network/interfaces 2>&1 | head -10"),
    ("NetworkManager", "ls /etc/NetworkManager/ 2>&1 | head -3; systemctl is-active NetworkManager 2>&1"),
    ("当前 eth0", "ip -4 addr show eth0 2>&1 | grep inet"),
    ("dhcpcd", "cat /etc/dhcpcd.conf 2>&1 | head -8"),
    ("systemd 服务样例", "ls /etc/systemd/system/*.service 2>&1 | head -8"),
    ("内核 cmdline 静态IP", "cat /proc/cmdline 2>&1"),
]
for name, c in checks:
    print(f"=== {name} ===")
    stdin, stdout, stderr = ssh.exec_command(c)
    print(stdout.read().decode(errors='replace')[:400] or "(空)")
ssh.close()
