#!/bin/bash
# qtmqtt 6.4.3 运行产物固化（Y-1 2026-09-10 承接 build-qtmqtt.sh）——qtmultimedia/qtHttpServer 同先例：
# 把 libQt6Mqtt 拷入 SDK buildroot fs-overlay（BR2_ROOTFS_OVERLAY=board/rockchip/rk3562/fs-overlay，下次 rootfs 打包自动合入镜像）
# 用法（docker 内，build-qtmqtt.sh 成功后）：
#   bash /workspace/build-qtmqtt-overlay.sh
# 然后重打 rootfs（上板前）：rm <OUT>/images/rootfs.ext4 && cd /sdk/buildroot && make O=<OUT>
set -euo pipefail
SDK=/sdk
OUT=${SDK}/buildroot/output/rk3562_navihmi
SYSROOT=${OUT}/host/aarch64-buildroot-linux-gnu/sysroot
OVERLAY=${SDK}/buildroot/board/rockchip/rk3562/fs-overlay

echo "=== 1. 固化 libQt6Mqtt（staging → overlay/usr/lib）==="
ls ${SYSROOT}/usr/lib/libQt6Mqtt.so* >/dev/null 2>&1 || { echo "libQt6Mqtt 缺失——先跑 build-qtmqtt.sh"; exit 1; }
mkdir -p ${OVERLAY}/usr/lib
cp -av ${SYSROOT}/usr/lib/libQt6Mqtt.so* ${OVERLAY}/usr/lib/
echo "--- libQt6Mqtt NEEDED（预期仅 Core/Network——qt6base 既有库）---"
readelf -d ${SYSROOT}/usr/lib/libQt6Mqtt.so | grep NEEDED || true

echo "=== 2. 结果校验 ==="
find ${OVERLAY}/usr/lib -name 'libQt6Mqtt*' | head -6
echo "=== DONE（overlay 已固化；重打 rootfs 见脚本头注释）==="
