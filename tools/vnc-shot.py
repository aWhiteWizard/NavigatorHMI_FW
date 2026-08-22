# 连板子 VNC(5900) 截屏存 PNG —— 验证 FW 画面 (D+ 轮)
import sys, time, os
from vncdotool import api

HOST = "192.168.1.146"
OUT = sys.argv[1] if len(sys.argv) > 1 else "vnc_screen.png"

client = api.connect(f"{HOST}::5900", password="")
time.sleep(2)
client.captureScreen(OUT)
try:
    client.disconnect()
except Exception:
    pass
print(f"saved {OUT}")
os._exit(0)
