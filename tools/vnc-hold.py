# vnc-hold.py — 原生 RFB 3.3 客户端: 按住鼠标指定时长 (不依赖 twisted reactor, send 立即生效)
# 用法: python vnc-hold.py <x> <y> <hold_ms> [click_times]
# 与 VncMirror 状态机精确匹配: recv 12 "RFB 003.003\n" -> send 12 -> recv 4 secType -> send 1 ClientInit
import socket, sys, time

HOST = "192.168.1.146"
PORT = 5900
x = int(sys.argv[1]); y = int(sys.argv[2]); ms = int(sys.argv[3])
times = int(sys.argv[4]) if len(sys.argv) > 4 else 1

s = socket.create_connection((HOST, PORT), timeout=5)
s.settimeout(3)

# 1) 服务器发版本
ver = s.recv(12)
assert ver.startswith(b"RFB 003.003"), f"bad version: {ver}"
# 2) 客户端发版本
s.sendall(b"RFB 003.003\n")
# 3) 服务器发安全类型数 (4 字节大端)
sec = s.recv(4)
# 4) 客户端发 ClientInit (1 字节共享标志)
s.sendall(b"\x01")

def pointer(mask, px, py):
    # PointerEvent: type=5, mask 1B, x 2B BE, y 2B BE
    s.sendall(bytes([5, mask, (px >> 8) & 0xFF, px & 0xFF, (py >> 8) & 0xFF, py & 0xFF]))

def move(px, py):
    pointer(0, px, py)

t0 = time.time()
move(x, y)
time.sleep(0.3)
for i in range(times):
    pointer(1, x, y)      # 左键按下
    print(f"[hold] down ({x},{y}) at {time.time()-t0:.2f}s hold={ms}ms", flush=True)
    time.sleep(ms / 1000.0)
    pointer(0, x, y)      # 松开
    print(f"[hold] up at {time.time()-t0:.2f}s", flush=True)
    if i < times - 1:
        time.sleep(0.5)
s.close()
