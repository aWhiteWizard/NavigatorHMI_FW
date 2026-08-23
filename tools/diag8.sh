#!/bin/sh
echo "===pointercal==="
ls -la /etc/pointercal 2>&1
cat /etc/pointercal 2>/dev/null
echo
echo "===calib.log==="
cat /tmp/calib.log 2>/dev/null | grep -aE "校准|矩阵|质量|设备|点"
echo "===FW 环境==="
for p in /proc/[0-9]*; do
  if tr '\0' ' ' < $p/cmdline 2>/dev/null | grep -q navigatorhmi; then
    echo "PID ${p#/proc/}:"
    tr '\0' '\n' < $p/environ 2>/dev/null | grep -iE "tslib|tsdevice"
    break
  fi
done
echo "===tslib日志==="
grep -a "tslib 启用" /tmp/navihmi-mirror.log 2>/dev/null | tail -2
