#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 只读检查官方系统的升级服务（B-2 调研，不动板子）
import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.146', username='root', password='topeet', timeout=8)

checks = [
    ("17888 端口监听", "ss -tlnp 2>/dev/null | grep 17888; netstat -tlnp 2>/dev/null | grep 17888"),
    ("RKNetUpdate 进程", "ps aux 2>/dev/null | grep -iE 'rknet|17888' | grep -v grep"),
    ("升级相关命令", "which rknetupdate RKNetUpdate 2>/dev/null; ls /usr/bin/ /usr/local/bin/ 2>/dev/null | grep -iE 'rknet|netupd' | head -5"),
    ("systemd 服务", "systemctl list-units --all 2>/dev/null | grep -iE 'rknet|update' | head -5"),
    ("RKNetUpdate 安装位置", "find / -maxdepth 4 -iname '*rknet*' 2>/dev/null | head -8"),
    ("17888 服务进程名", "ss -tlnp 2>/dev/null | grep -E ':(80|8080|17888)' | head -5"),
]
for name, c in checks:
    print(f"=== {name} ===")
    stdin, stdout, stderr = ssh.exec_command(c)
    out = stdout.read().decode(errors='replace')[:500]
    print(out if out.strip() else "(空)")
ssh.close()
