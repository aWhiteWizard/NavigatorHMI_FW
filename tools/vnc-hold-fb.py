# vnc-hold-fb.py — 原生 RFB 3.3 客户端: 长按测试版, 连接后发 FBU 并后台持续读帧
# （模拟 VncViewer 正常读帧的客户端——验证 VNC 推帧阻塞是否节流 QML Timer）
# 用法: python vnc-hold-fb.py <x> <y> <hold_ms>
import socket, sys, time, threading

HOST = "192.168.1.146"
PORT = 5900
x = int(sys.argv[1]); y = int(sys.argv[2]); ms = int(sys.argv[3])

s = socket.create_connection((HOST, PORT), timeout=5)
s.settimeout(3)
ver = s.recv(12)
s.sendall(b"RFB 003.003\n")
sec = s.recv(4)
s.sendall(b"\x01")

def pointer(mask, px, py):
    s.sendall(bytes([5, mask, (px >> 8) & 0xFF, px & 0xFF, (py >> 8) & 0xFF, py & 0xFF]))

def move(px, py):
    pointer(0, px, py)

# 后台读帧线程: 持续发 FBU 并消费服务器推帧, 保持发送缓冲不阻塞
def reader():
    try:
        while True:
            s.sendall(bytes([3, 1, 0, 0, 0, 0, 0, 0, 0, 0]))  # FBU incremental
            hdr = b""
            while len(hdr) < 12:
                chunk = s.recv(12 - len(hdr))
                if not chunk: return
                hdr += chunk
            w = (hdr[4] << 8) | hdr[5]; h = (hdr[6] << 8) | hdr[7]
            left = (hdr[8] << 8) | hdr[9]; top = (hdr[10] << 8) | hdr[11]
            n = w * h * 4
            while n > 0:
                chunk = s.recv(n)
                if not chunk: return
                n -= len(chunk)
    except Exception:
        pass

t = threading.Thread(target=reader, daemon=True)
t.start()
time.sleep(0.3)

t0 = time.time()
move(x, y)
time.sleep(0.3)
pointer(1, x, y)
print(f"[hold-fb] down ({x},{y}) at {time.time()-t0:.2f}s hold={ms}ms", flush=True)
time.sleep(ms / 1000.0)
pointer(0, x, y)
print(f"[hold-fb] up at {time.time()-t0:.2f}s", flush=True)
time.sleep(0.5)
s.close()
