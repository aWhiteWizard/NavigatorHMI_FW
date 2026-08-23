#!/bin/sh
echo "===fb 设备==="
ls -la /dev/fb* 2>&1
echo "===启动 ts_calibrate==="
export TSLIB_TSDEVICE=/dev/input/event3
export TSLIB_CONFFILE=/etc/ts.conf
export TSLIB_CALIBFILE=/etc/pointercal
echo "TSLIB_TSDEVICE=$TSLIB_TSDEVICE"
timeout 8 ts_calibrate 2>&1 | head -20
echo "EXIT=$?"
echo "===pointercal 变化==="
cat /etc/pointercal 2>&1
