#!/bin/sh
echo "===ft5x06 设备 ABS 能力==="
cat /proc/bus/input/devices | grep -A 8 "ft5x06"
echo "===evtest 支持事件==="
timeout 2 evtest /dev/input/event3 2>&1 | head -40
