#!/bin/sh
echo "===UPTIME==="
uptime
echo "===PROC==="
ps | grep -v grep | grep -E "navigator|touch|calibrate"
echo "===POINTERCAL==="
ls -la /etc/pointercal 2>&1
cat /etc/pointercal 2>/dev/null
echo
echo "===FW日志==="
tail -25 /tmp/navihmi-mirror.log 2>/dev/null | grep -vE "Failed to move cursor"
echo "===S99日志==="
tail -10 /tmp/navihmi.log 2>/dev/null | grep -vE "Failed to move cursor"
echo "===CALIB日志==="
ls -la /tmp/calib*.log 2>/dev/null
