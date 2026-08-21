#!/bin/bash
set -e
BUILD=/workspace/build/qtvk-build
OVERLAY=/workspace/hwt/rk3562/buildroot/fs-overlay/usr
echo "=== 1. QML 模块 ==="
rm -rf ${OVERLAY}/qml/QtQuick/VirtualKeyboard
mkdir -p ${OVERLAY}/qml/QtQuick
cp -rf ${BUILD}/qml/QtQuick/VirtualKeyboard ${OVERLAY}/qml/QtQuick/
echo "=== 2. 库 ==="
mkdir -p ${OVERLAY}/lib
cp -f ${BUILD}/lib/libQt6VirtualKeyboard.so* ${OVERLAY}/lib/
echo "=== 3. platforminputcontext 插件 ==="
find ${BUILD} -path '*platforminputcontexts*' -name '*.so' 2>/dev/null | head -5
mkdir -p ${OVERLAY}/plugins/platforminputcontexts
cp -f ${BUILD}/plugins/platforminputcontexts/*.so ${OVERLAY}/plugins/platforminputcontexts/ 2>/dev/null || echo "(主插件内嵌输入上下文, 无独立插件)"
echo "=== 4. 验证 ==="
find ${OVERLAY}/qml/QtQuick/VirtualKeyboard -type f | wc -l
ls ${OVERLAY}/lib/ | grep VirtualKeyboard
echo "=== DONE ==="