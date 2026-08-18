#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# B-3: 等板子起来, 验证 qt-test (eglfs/mali)
import paramiko, socket, time

def ssh_up(host='192.168.1.146', port=22, timeout=3):
    try:
        s = socket.create_connection((host, port), timeout=timeout); s.close(); return True
    except OSError: return False

print("等板子起来 (SSH)...")
start = time.time()
while time.time() - start < 240:
    if ssh_up():
        time.sleep(5)
        try:
            ssh = paramiko.SSHClient()
            ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            ssh.connect('192.168.1.146', username='root', password='topeet', timeout=10)
            checks = [
                ("内核/mali", "uname -r; dmesg | grep -iE 'mali|bifrost' | head -5"),
                ("mali 设备", "ls -la /dev/mali* 2>&1"),
                ("qt-test 进程", "ps | grep qt-test | grep -v grep"),
                ("qt-test 日志", "cat /tmp/qt.log 2>&1 | tail -8"),
                ("IP", "ifconfig eth0 | grep inet"),
            ]
            for n, c in checks:
                print(f"=== {n} ===")
                stdin, stdout, stderr = ssh.exec_command(c, timeout=15)
                print(stdout.read().decode(errors='replace')[:400])
            ssh.close()
            print("=== 验证完成 ===")
            break
        except Exception as e:
            print(f"  SSH 未就绪: {e}")
            time.sleep(10)
    else:
        time.sleep(8)
