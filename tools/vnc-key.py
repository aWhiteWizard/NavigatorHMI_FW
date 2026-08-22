# 连板子 VNC(5900) 注入按键/点击 —— 验证键盘交互 (D+ 轮)
# 用法: python vnc-key.py <key|click:x,y> [out.png]
import sys, time, os
from vncdotool import api

HOST = "192.168.1.146"
action = sys.argv[1]
OUT = sys.argv[2] if len(sys.argv) > 2 else ""

client = api.connect(f"{HOST}::5900", password="")
time.sleep(1)

if action.startswith("click:"):
    _, xy = action.split(":", 1)
    x, y = map(int, xy.split(","))
    client.mouseMove(x, y)
    time.sleep(0.2)
    client.mouseDown(1)
    time.sleep(0.12)
    client.mouseUp(1)
    time.sleep(0.5)
    print(f"clicked ({x},{y})")
elif action == "enter":
    client.keyDown("return")
    time.sleep(0.1)
    client.keyUp("return")
    time.sleep(0.5)
    print("pressed Enter")
elif action == "esc":
    client.keyDown("esc")
    time.sleep(0.1)
    client.keyUp("esc")
    time.sleep(0.5)
    print("pressed Esc")
elif action.startswith("type:"):
    txt = action.split(":", 1)[1]
    for ch in txt:
        client.keyPress(ch)
        time.sleep(0.1)
    time.sleep(0.3)
    print(f"typed {txt}")

if OUT:
    time.sleep(0.8)
    client.captureScreen(OUT)
    print("saved", OUT)

try:
    client.disconnect()
except Exception:
    pass
os._exit(0)
