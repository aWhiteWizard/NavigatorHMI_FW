# 连板子 VNC(5900) 注入鼠标点击并截屏 —— 模拟用户交互 (D+ 轮)
# 用法: python vnc-click.py <x> <y> <out.png> [click_times]
import sys, time, os
from vncdotool import api

HOST = "192.168.1.146"
x = int(sys.argv[1]); y = int(sys.argv[2])
OUT = sys.argv[3]
times = int(sys.argv[4]) if len(sys.argv) > 4 else 1

client = api.connect(f"{HOST}::5900", password="")
time.sleep(1)
for i in range(times):
    client.mouseMove(x, y)
    time.sleep(0.2)
    client.mouseDown(1)
    time.sleep(0.15)
    client.mouseUp(1)
    time.sleep(0.6)
time.sleep(1.0)
client.captureScreen(OUT)
try:
    client.disconnect()
except Exception:
    pass
print(f"clicked ({x},{y}) x{times} -> {OUT}")
os._exit(0)
