#!/bin/sh
echo "===PROC==="
ps | grep -v grep | grep -E "navigator|touch|calibrate" 
echo "===LOG==="
grep -E "触摸校准" /tmp/navihmi-mirror.log 2>/dev/null | tail -5
echo "===VNC==="
ss -tln 2>/dev/null | grep 5900
