#!/bin/sh
echo "===POINTERCAL==="
ls -la /etc/pointercal 2>&1
cat /etc/pointercal 2>/dev/null
echo
echo "===谁打开 event3==="
for p in /proc/[0-9]*; do
  if ls -l $p/fd 2>/dev/null | grep -q event3; then
    echo "PID ${p#/proc/}: $(tr '\0' ' ' < $p/cmdline 2>/dev/null)"
  fi
done
echo "===FW 进程==="
ps | grep -v grep | grep -E "navigator|touch|calibrate"
echo "===ts_print 触摸测试(需用户触摸, 3秒)==="
export TSLIB_TSDEVICE=/dev/input/event3
export TSLIB_CONFFILE=/etc/ts.conf
timeout 3 ts_print 2>&1 | head -8
