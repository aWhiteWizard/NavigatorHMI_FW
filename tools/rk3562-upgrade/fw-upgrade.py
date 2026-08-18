#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ============================================================
# RK3562 一键命令行升级 (B-3 整理, 2026-08-18)
# 用法: python fw-upgrade.py <update.img路径> [板子IP] [--prepare-only]
# 流程: 起HTTP容器 -> 板子wget固件 -> 生成manifest -> prepare(校验) 
#       -> reboot(重启进recovery应用) -> 轮询等待恢复
# 依赖: Windows python + paramiko; Docker Desktop 运行中; 板子 SSH root/topeet
# ============================================================
import argparse, hashlib, os, subprocess, socket, sys, time, json

def get_board_ip(default='192.168.1.146'):
    return default

def compute_sha256(path, progress=False):
    h = hashlib.sha256()
    total = os.path.getsize(path)
    done = 0
    with open(path, 'rb') as f:
        while True:
            chunk = f.read(1 << 22)
            if not chunk: break
            h.update(chunk)
            done += len(chunk)
            if progress and done % (1 << 30) == 0:
                print(f"  sha256: {done/1e9:.1f}/{total/1e9:.1f} GB")
    return h.hexdigest()

def run_ssh(ip, cmd, timeout=60):
    import paramiko
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(ip, username='root', password='topeet', timeout=10)
    stdin, stdout, stderr = ssh.exec_command(cmd, timeout=timeout)
    out = stdout.read().decode(errors='replace')
    err = stderr.read().decode(errors='replace')
    ssh.close()
    return out, err

def main():
    ap = argparse.ArgumentParser(description='RK3562 命令行升级')
    ap.add_argument('firmware', help='update.img 路径')
    ap.add_argument('--ip', default='192.168.1.146', help='板子 IP')
    ap.add_argument('--prepare-only', action='store_true', help='只 prepare 不 reboot')
    ap.add_argument('--stage', default='/var/lib/rknetupdate/updateimg-v1',
                    help='板子 stage 目录')
    args = ap.parse_args()

    fw = os.path.abspath(args.firmware)
    if not os.path.isfile(fw):
        print(f"❌ 固件不存在: {fw}"); sys.exit(1)

    print(f"=== RK3562 命令行升级 ===")
    print(f"固件: {fw} ({os.path.getsize(fw)/1e9:.2f} GB)")
    print(f"板子: {args.ip}")

    # 1. 算 sha256
    print("1. 计算固件 sha256 ...")
    sha = compute_sha256(fw, progress=True)
    print(f"   sha256: {sha}")

    # 2. 起 HTTP 容器提供固件下载
    print("2. 启动 HTTP 传输容器 ...")
    fw_dir = os.path.dirname(fw)
    fw_name = os.path.basename(fw)
    subprocess.run(['docker', 'rm', '-f', 'http-file'], capture_output=True)
    r = subprocess.run(['docker', 'run', '-d', '--name', 'http-file', '-p', '8000:8000',
                        '-v', f'{fw_dir}:/srv/http',
                        'swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/rk3562-builder-env:v1.1-ubuntu20',
                        'python3', '-m', 'http.server', '8000', '--directory', '/srv/http'],
                       capture_output=True)
    if r.returncode != 0:
        print(f"❌ HTTP 容器启动失败: {r.stderr.decode()[:200]}"); sys.exit(1)
    time.sleep(3)
    print("   HTTP 容器就绪")

    # 3. 板子 wget 固件
    print("3. 板子下载固件 ...")
    out, err = run_ssh(args.ip,
        f'wget -q http://192.168.1.14:8000/{fw_name} -O {args.stage}/update.img && '
        f'echo DL_OK && sha256sum {args.stage}/update.img', timeout=1800)
    if 'DL_OK' not in out:
        print(f"❌ 板子下载失败: {out[-300:]} {err[-200:]}"); sys.exit(1)
    board_sha = out.strip().split()[0] if out.strip() else ''
    if board_sha != sha:
        print(f"❌ sha256 不匹配: 板子={board_sha} 本地={sha}"); sys.exit(1)
    print(f"   传输+校验 OK ({board_sha[:12]}...)")

    # 4. 生成 manifest (调用 gen-updateimg-manifest.py)
    print("4. 生成 manifest ...")
    script_dir = os.path.dirname(os.path.abspath(__file__))
    gen_out = os.path.join(os.path.dirname(fw), 'updateimg.manifest.json')
    r = subprocess.run([sys.executable, os.path.join(script_dir, 'gen-updateimg-manifest.py'),
                        fw, gen_out], capture_output=True)
    if r.returncode != 0:
        print(f"❌ manifest 生成失败: {r.stderr.decode()[:300]}"); sys.exit(1)
    print(f"   manifest: {gen_out}")

    # 5. 传 manifest 到板子
    print("5. 上传 manifest ...")
    import paramiko
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(args.ip, username='root', password='topeet', timeout=10)
    sftp = ssh.open_sftp()
    sftp.put(gen_out, f'{args.stage}/updateimg.manifest.json')
    ssh.exec_command(f'chmod 644 {args.stage}/updateimg.manifest.json')
    sftp.close(); ssh.close()
    print("   manifest 已上传")

    # 6. prepare
    print("6. prepare-updateimg ...")
    out, err = run_ssh(args.ip,
        f'cd {args.stage} && rknetupdate-control prepare-updateimg --allow-write '
        f'--confirm PREPARE_UPDATEIMG_V1:{sha}', timeout=600)
    if '"success":true' not in out:
        print(f"❌ prepare 失败: {out[-500:]} {err[-200:]}"); sys.exit(1)
    print(f"   prepare OK: {out.strip()[:120]}")

    if args.prepare_only:
        print("✅ prepare 完成 (未 reboot, 可检查后手动 reboot)")
        sys.exit(0)

    # 7. reboot
    print("7. reboot-updateimg (重启进 recovery 升级) ...")
    out, err = run_ssh(args.ip,
        f'cd {args.stage} && rknetupdate-control reboot-updateimg --allow-write '
        f'--confirm REBOOT_UPDATEIMG_V1:{sha}', timeout=60)
    if '"success":true' not in out and 'reboot_requested":true' not in out:
        print(f"⚠️ reboot 输出异常: {out[-400:]} {err[-200:]}")
    print("   板子已重启进 recovery ...")

    # 8. 轮询等待恢复 (调用 wait-upgrade.py)
    print("8. 等待升级完成 (轮询 SSH 恢复) ...")
    subprocess.run([sys.executable, os.path.join(script_dir, 'wait-upgrade.py')])
    print("✅ 升级流程完成")

if __name__ == '__main__':
    main()
