#!/bin/sh
# [已废弃 2026-08-23] 修 S99 防双启——touch-calibrate 已删除(校准集成进 FW), 脚本仅供历史参考
# 加 exit 0 防止误执行(会改设备 /etc/init.d/S99qt-test)
exit 0
sed -i 's|^  start)|  start)\n    killall navigatorhmi-fw 2>/dev/null; sleep 1|' /etc/init.d/S99qt-test
echo "===S99 start 段==="
grep -A 4 "start)" /etc/init.d/S99qt-test | head -6
echo "===KILLALL==="
killall navigatorhmi-fw 2>/dev/null
sleep 2
ps | grep -v grep | grep navigatorhmi || echo "no navigatorhmi"
echo "===单实例启动==="
export QT_QPA_PLATFORM=eglfs
export QT_QPA_PLATFORM_PLUGIN_PATH=/usr/plugins/platforms
export QT_PLUGIN_PATH=/usr/plugins
export QML_IMPORT_PATH=/usr/qml
export QML2_IMPORT_PATH=/usr/qml
export NAVIHMI_VNC=2
nohup /usr/bin/navigatorhmi-fw --project /mnt/user/userdata/app.navihmi > /tmp/navihmi-mirror.log 2>&1 &
sleep 8
echo "===FW 进程==="
ps | grep -v grep | grep navigatorhmi
echo "===日志 tslib==="
grep -E "tslib|触摸校准" /tmp/navihmi-mirror.log
