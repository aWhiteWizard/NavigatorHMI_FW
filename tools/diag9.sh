#!/bin/sh
# [部分废弃 2026-08-23] touch-calibrate 已删除(校准集成进 FW), 残留检查段仅查导航进程
echo "===校准/FW 残留进程==="
ps | grep -v grep | grep -E "navigator|calibrate"
echo "===event3 grab 测试==="
timeout 2 evtest --grab /dev/input/event3 2>&1 | head -6
echo "===FW 进程与 TSLIB==="
for p in /proc/[0-9]*; do
  cmd=$(tr '\0' ' ' < $p/cmdline 2>/dev/null)
  if echo "$cmd" | grep -q navigatorhmi; then
    echo "PID ${p#/proc/}: $cmd"
    tr '\0' '\n' < $p/environ 2>/dev/null | grep -iE "tslib|tsdevice"
  fi
done
echo "===TSLIB_TSDEVICE 测试==="
export TSLIB_TSDEVICE=/dev/input/event3
export TSLIB_CONFFILE=/etc/ts.conf
timeout 2 ts_print 2>&1 | head -4
