#!/bin/sh
echo "===POINTERCAL==="
cat /etc/pointercal
echo
echo "===PROC==="
for p in 672 721; do
  if [ -d /proc/$p ]; then
    echo "--- PID $p cmdline: $(tr '\0' ' ' < /proc/$p/cmdline)"
    echo "  env TSLIB: $(tr '\0' '\n' < /proc/$p/environ | grep -i tslib)"
    echo "  env QPA: $(tr '\0' '\n' < /proc/$p/environ | grep -i 'QPA\|IM_MODULE')"
    echo "  ppid: $(awk '{print $4}' /proc/$p/stat)"
  fi
done
echo "===ALL navigatorhmi==="
ps -ef | grep navigatorhmi | grep -v grep
echo "===touch devices==="
ls -la /dev/input/
