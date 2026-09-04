#!/bin/bash
# qtmultimedia 6.4.3 运行产物固化（P-6c 承接，2026-09-04）——build-qtmm.sh 装 staging sysroot 后执行：
# 把 QtMultimedia 库 + QML 插件 + ffmpeg 后端插件拷入 SDK buildroot fs-overlay
# （BR2_ROOTFS_OVERLAY=board/rockchip/rk3562/fs-overlay，下次 rootfs 打包自动合入镜像）。
# 用法（docker 内，build-qtmm.sh 成功后）：
#   bash /workspace/build-qtmm-overlay.sh
# 然后重打 rootfs（P-7 上板前）：rm <OUT>/images/rootfs.ext4 && cd /sdk/buildroot && make O=<OUT>
# 依赖：必须先跑 build-qtmm.sh（产物在 /workspace/build/qtmm-build + staging sysroot）——
# 产物缺失时 fail-closed（exit 1），不做「固化不完整」的静默成功。
set -euo pipefail
SDK=/sdk
OUT=${SDK}/buildroot/output/rk3562_navihmi
SYSROOT=${OUT}/host/aarch64-buildroot-linux-gnu/sysroot
OVERLAY=${SDK}/buildroot/board/rockchip/rk3562/fs-overlay
BUILD_DIR=/workspace/build/qtmm-build

echo "=== 1. 固化 libQt6Multimedia（staging → overlay/usr/lib）==="
mkdir -p ${OVERLAY}/usr/lib
cp -av ${SYSROOT}/usr/lib/libQt6Multimedia.so* ${OVERLAY}/usr/lib/
# 依赖检查（预期 NEEDED 仅 Core/Gui/Network 等 qt6base 既有库）
echo "--- libQt6Multimedia NEEDED ---"
readelf -d ${SYSROOT}/usr/lib/libQt6Multimedia.so | grep NEEDED || true

echo "=== 2. 固化 QML 插件（overlay/usr/qml/QtMultimedia）==="
if [ -d ${BUILD_DIR}/qml/QtMultimedia ]; then
    mkdir -p ${OVERLAY}/usr/qml
    cp -av ${BUILD_DIR}/qml/QtMultimedia ${OVERLAY}/usr/qml/
else
    echo "!! qml/QtMultimedia 未找到（declarative 部分未构建）——固化不完整，中止"
    exit 1
fi

echo "=== 3. 固化 ffmpeg 后端插件（overlay/usr/plugins/multimedia/ffmpeg）==="
PLUGIN=$(find ${BUILD_DIR} -name 'libffmpegmediaplugin.so' | head -1)
if [ -n "${PLUGIN}" ]; then
    mkdir -p ${OVERLAY}/usr/plugins/multimedia/ffmpeg
    cp -av ${PLUGIN} ${OVERLAY}/usr/plugins/multimedia/ffmpeg/
else
    echo "!! libffmpegmediaplugin.so 未找到（QT_FEATURE_ffmpeg 未生效？）——固化不完整，中止"
    exit 1
fi

echo "=== 4. 结果树 ==="
find ${OVERLAY}/usr/lib -name 'libQt6Multimedia*' -o -path '*QtMultimedia*' -o -path '*ffmpegmediaplugin*' 2>/dev/null | head -20
echo "=== DONE（overlay 已固化；重打 rootfs 见脚本头注释）==="
