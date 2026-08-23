# 长按注入 (VNC) — 验证长按校准触发 (E 循环)
# 用法: python vnc-press.py <x> <y> <press_ms>
import sys, time, os
from vncdotool import api

HOST = "192.168.1.146"
x = int(sys.argv[1]); y = int(sys.argv[2]); ms = int(sys.argv[3])

client = api.connect(f"{HOST}::5900", password="")
time.sleep(1)
client.mouseMove(x, y)
time.sleep(0.3)
client.mouseDown(1)
time.sleep(ms / 1000.0)
client.mouseUp(1)
time.sleep(1)
print(f"long-pressed ({x},{y}) {ms}ms")
try:
    client.disconnect()
except Exception:
    pass
os._exit(0)
