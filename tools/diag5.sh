#!/bin/sh
echo "===校准日志==="
grep -aE "校准|触摸校准|设备=|EVIOC|质量" /tmp/navihmi-mirror.log 2>/dev/null | tail -20
echo "===pointercal==="
ls -la /etc/pointercal 2>&1
cat /etc/pointercal 2>/dev/null
echo
echo "===proc==="
ps | grep -v grep | grep -E "navigator|touch|calibrate"
