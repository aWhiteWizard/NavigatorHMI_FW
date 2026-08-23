# vnc-calib-test.py — 重启 FW 并立即经 VNC 完成校准全流程（时序精确控制）
# 用 ssh 重启 FW, 立即连 VNC, 在 3 秒 autoStart 窗口内长按校准按钮
import socket, time, threading, subprocess, os, sys

HOST = "192.168.1.146"

# 1) ssh 重启 FW（参数列表方式, 避免嵌套引号转义问题）
import subprocess
env = dict(os.environ)
env["SSH_ASKPASS"] = r"D:\workspace\tools\ssh-askpass.cmd"
env["SSH_ASKPASS_REQUIRE"] = "force"
env["DISPLAY"] = ":0"
remote_cmd = ("killall navigatorhmi-fw 2>/dev/null; sleep 0.3; "
              "export QT_QPA_PLATFORM=eglfs; export QT_QPA_PLATFORM_PLUGIN_PATH=/usr/plugins/platforms; "
              "export QT_PLUGIN_PATH=/usr/plugins; export QML_IMPORT_PATH=/usr/qml; export QML2_IMPORT_PATH=/usr/qml; "
              "export NAVIHMI_VNC=2; "
              "nohup /usr/bin/navigatorhmi-fw --project /mnt/user/userdata/app.navihmi > /tmp/navihmi-mirror.log 2>&1 &")
subprocess.run(["ssh", "-o", "StrictHostKeyChecking=no", "root@192.168.1.146", remote_cmd],
               capture_output=True, timeout=30, env=env)
time.sleep(0.5)

# 2) 连 VNC（等 FW 启动 + VNC 监听, 最多 8s）
s = None
for attempt in range(16):
    try:
        s = socket.create_connection((HOST, 5900), timeout=3)
        break
    except Exception:
        time.sleep(0.5)
if s is None:
    print("VNC connect failed", flush=True)
    sys.exit(1)
s.settimeout(3)
s.recv(12); s.sendall(b"RFB 003.003\n"); s.recv(4); s.sendall(b"\x01")

def ptr(m, x, y):
    s.sendall(bytes([5, m, (x >> 8) & 0xFF, x & 0xFF, (y >> 8) & 0xFF, y & 0xFF]))

def reader():
    try:
        while True:
            s.sendall(bytes([3, 1, 0, 0, 0, 0, 0, 0, 0, 0]))
            hdr = b""
            while len(hdr) < 12:
                c = s.recv(12 - len(hdr))
                if not c: return
                hdr += c
            w = (hdr[4] << 8) | hdr[5]; h = (hdr[6] << 8) | hdr[7]
            n = w * h * 4
            while n > 0:
                c = s.recv(n)
                if not c: return
                n -= len(c)
    except Exception:
        pass

threading.Thread(target=reader, daemon=True).start()
time.sleep(0.3)

# 3) 长按校准按钮 (612,303) 3.5s——FW 启动约 1.5s, 长按期间 3 秒 autoStart 到点会被
#    onPressed 的 navRoot.userAction() 取消, 校准按钮保持可见
ptr(0, 612, 303); time.sleep(0.15)
ptr(1, 612, 303); time.sleep(3.5)
ptr(0, 612, 303)
print("long-pressed", flush=True)
time.sleep(1.2)

# 4) 点 5 个十字
for i, (x, y) in enumerate([(80, 80), (944, 80), (512, 300), (80, 520), (944, 520)]):
    ptr(1, x, y); time.sleep(0.12); ptr(0, x, y)
    print(f"pt {i+1}/5 ({x},{y})", flush=True)
    time.sleep(0.25)
time.sleep(1.0)

# 5) 确定按钮 (512,365)
ptr(1, 512, 365); time.sleep(0.15); ptr(0, 512, 365)
time.sleep(0.8)
s.close()
print("done", flush=True)
