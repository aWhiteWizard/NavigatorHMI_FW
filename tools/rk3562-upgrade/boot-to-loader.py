#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# B-3: 等官方系统起来 -> SSH reboot loader
import paramiko, socket, time, sys

def ssh_up(host='192.168.1.146', port=22, timeout=3):
    try:
        s = socket.create_connection((host, port), timeout=timeout)
        s.close(); return True
    except OSError:
        return False

print("等板子系统起来 (SSH)...")
start = time.time()
for attempt in range(40):  # ~6 分钟
    if ssh_up():
        print(f"[{int(time.time()-start)}s] SSH 通了, 登录...")
        try:
            ssh = paramiko.SSHClient()
            ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            ssh.connect('192.168.1.146', username='root', password='topeet', timeout=10)
            stdin, stdout, stderr = ssh.exec_command('uname -r; echo ---; reboot loader', timeout=10)
            print("系统信息 + 已触发 reboot loader")
            print(stdout.read().decode(errors='replace')[:200])
            ssh.close()
            print("板子将进入 loader 模式 (USB 设备重现)")
            sys.exit(0)
        except Exception as e:
            print(f"  SSH 异常: {e}, 重试...")
    time.sleep(9)
print("超时: 板子未起来 (检查屏幕/串口)")
