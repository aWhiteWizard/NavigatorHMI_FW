#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# B-2: 轮询板子 SSH 恢复 (升级完成后), 报启动信息
import paramiko, time, socket, sys

def ssh_up(host='192.168.1.146', port=22, timeout=3):
    try:
        s = socket.create_connection((host, port), timeout=timeout)
        s.close()
        return True
    except OSError:
        return False

print("等待板子升级完成 (SSH 恢复)...")
start = time.time()
waiting = 0
while True:
    if ssh_up():
        print(f"[{int(time.time()-start)}s] SSH 端口通了, 等待 sshd 就绪...")
        time.sleep(10)
        try:
            ssh = paramiko.SSHClient()
            ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            ssh.connect('192.168.1.146', username='root', password='topeet', timeout=10)
            stdin, stdout, stderr = ssh.exec_command('uptime; uname -r; dmesg | grep -iE "Booting Linux|rootfs|mmcblk" | head -5; df -h / | tail -1')
            print("=== 升级后板子状态 ===")
            print(stdout.read().decode(errors='replace')[:800])
            ssh.close()
            print(f"=== 升级完成 (用时 {int(time.time()-start)}s) ===")
            sys.exit(0)
        except Exception as e:
            print(f"  sshd 未就绪 ({e}), 继续等...")
            time.sleep(15)
            continue
    waiting += 1
    if waiting % 6 == 0:
        print(f"[{int(time.time()-start)}s] 板子未上线 (升级中或网络恢复中)...")
    time.sleep(10)
