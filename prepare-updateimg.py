#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# B-2: 传 manifest 到板子 + prepare-updateimg
import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.146', username='root', password='topeet', timeout=10)

# 传 manifest
sftp = ssh.open_sftp()
sftp.put(r"D:\ProgramData\Agent_backup\updateimg.manifest.json",
         "/var/lib/rknetupdate/updateimg-v1/updateimg.manifest.json")
sftp.close()
print("manifest 已上传")

# 板子侧确认文件 + 权限
stdin, stdout, stderr = ssh.exec_command(
    'chmod 644 /var/lib/rknetupdate/updateimg-v1/updateimg.manifest.json; '
    'ls -la /var/lib/rknetupdate/updateimg-v1/; '
    'sha256sum /var/lib/rknetupdate/updateimg-v1/update.img')
print(stdout.read().decode(errors='replace'))

# prepare-updateimg
sha = '71c456c7021c9a6afdba94e5400c798d3702f5562e4ebcdc26dca14a2d5993e9'
cmd = (f'cd /var/lib/rknetupdate/updateimg-v1 && '
       f'rknetupdate-control prepare-updateimg --allow-write '
       f'--confirm PREPARE_UPDATEIMG_V1:{sha}')
print("=== prepare-updateimg ===")
stdin, stdout, stderr = ssh.exec_command(cmd, timeout=600)
out = stdout.read().decode(errors='replace')
err = stderr.read().decode(errors='replace')
print("STDOUT:", out[-1000:])
print("STDERR:", err[-400:] if err else "(无)")
ssh.close()
