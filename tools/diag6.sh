#!/bin/sh
echo "===校准日志(诊断)==="
grep -aE "校准|触摸校准|屏幕|qrc:|error|Error|failed|Failed" /tmp/navihmi-mirror.log 2>/dev/null | tail -30
echo "===raw 日志==="
grep -a "校准点" /tmp/navihmi-mirror.log 2>/dev/null | tail -8
echo "===pointercal==="
cat /etc/pointercal 2>&1
