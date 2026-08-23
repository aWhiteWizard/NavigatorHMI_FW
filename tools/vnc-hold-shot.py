# vnc-hold-shot.py — down 后保持指定时长并中途截图（验证按下状态是否保持）
# 用法: python vnc-hold-shot.py <x> <y> <hold_ms> <out.png>
import socket, sys, time

HOST = "192.168.1.146"
PORT = 5900
x = int(sys.argv[1]); y = int(sys.argv[2]); ms = int(sys.argv[3]); OUT = sys.argv[4]

s = socket.create_connection((HOST, PORT), timeout=5)
s.settimeout(3)
ver = s.recv(12)
s.sendall(b"RFB 003.003\n")
sec = s.recv(4)
s.sendall(b"\x01")

def pointer(mask, px, py):
    s.sendall(bytes([5, mask, (px >> 8) & 0xFF, px & 0xFF, (py >> 8) & 0xFF, py & 0xFF]))

# 先请求一帧并读掉, 建立增量基线
s.sendall(bytes([3, 1, 0, 0, 0, 0, 0, 0, 0, 0]))  # FBU: incremental=1
hdr = b""
while len(hdr) < 12:
    hdr += s.recv(12 - len(hdr))
w = (hdr[4] << 8) | hdr[5]; h = (hdr[6] << 8) | hdr[7]
data = b""
while len(data) < w * h * 4:
    data += s.recv(w * h * 4 - len(data))
print(f"baseline frame {w}x{h}")

pointer(1, x, y)   # down
time.sleep(1.2)
# 请求一帧看按下状态
s.sendall(bytes([3, 1, 0, 0, 0, 0, 0, 0, 0, 0]))
hdr2 = b""
while len(hdr2) < 12:
    hdr2 += s.recv(12 - len(hdr2))
w2 = (hdr2[4] << 8) | hdr2[5]; h2 = (hdr2[6] << 8) | hdr2[7]
data2 = b""
while len(data2) < w2 * h2 * 4:
    data2 += s.recv(w2 * h2 * 4 - len(data2))
with open(OUT, "wb") as f:
    # 转 PNG 太麻烦, 存原始 RGBA + 尺寸
    f.write(f"{w2} {h2}\n".encode())
    f.write(data2)
print(f"pressed frame saved {OUT}")

time.sleep(max(0, ms / 1000.0 - 1.2))
pointer(0, x, y)   # up
s.close()
