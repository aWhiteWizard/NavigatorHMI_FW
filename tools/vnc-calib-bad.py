# vnc-calib-bad.py — 单连接: 长按进校准 → 点 5 个"错位"点 → 验证质量门拒绝写入
# 用法: python vnc-calib-bad.py
import socket, time, threading, sys

HOST = "192.168.1.146"
x_btn, y_btn, hold_ms = 612, 303, 3500   # 校准按钮(导航页) + 长按时长
bad_pts = [(400,300),(600,200),(100,100),(900,500),(500,400)]  # 故意点偏

s = socket.create_connection((HOST, 5900), timeout=5)
s.settimeout(3)
s.recv(12); s.sendall(b"RFB 003.003\n"); s.recv(4); s.sendall(b"\x01")

def pointer(mask, px, py):
    s.sendall(bytes([5, mask, (px >> 8) & 0xFF, px & 0xFF, (py >> 8) & 0xFF, py & 0xFF]))

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

# 长按校准按钮
pointer(0, x_btn, y_btn); time.sleep(0.2)
pointer(1, x_btn, y_btn); time.sleep(hold_ms / 1000.0)
pointer(0, x_btn, y_btn)
print(f"long-pressed ({x_btn},{y_btn}) {hold_ms}ms", flush=True)
time.sleep(1.2)   # 等校准 overlay 激活

# 点 5 个错位点
for i, (x, y) in enumerate(bad_pts):
    pointer(1, x, y); time.sleep(0.12); pointer(0, x, y)
    print(f"bad pt {i+1}/5 ({x},{y})", flush=True)
    time.sleep(0.3)
time.sleep(1.5)
s.close()
print("done")
