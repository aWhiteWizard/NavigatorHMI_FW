#!/bin/bash
# qtmultimedia 6.4.3 交叉编译（P-6c，2026-09-04）——qthttpserver（build-qthttp.sh）同先例：
# 用板端 Qt 6.4.3 sysroot（buildroot）编译官方 qtmultimedia，ffmpeg 后端（轻量路线，非 gstreamer）。
# 前置：buildroot ffmpeg 已启用且含 swscale（BR2_PACKAGE_FFMPEG=y + BR2_PACKAGE_FFMPEG_SWSCALE=y，
#       libavformat/avcodec/swresample/swscale/avutil 均在 sysroot/usr/lib + pkgconfig）。
# 用法（docker 内）：
#   docker run --rm -v <qtmultimedia源码>:/workspace/qtmm-src -v <FW>:/workspace -v <SDK>:/sdk \
#     swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/rk3562-builder-env:v1.1-ubuntu20 \
#     bash /workspace/build-qtmm.sh
# 产物装 staging sysroot（编译链接用）；运行镜像固化（overlay + rootfs）由 build-qtmm-overlay.sh 承接。
set -euo pipefail
SDK=/sdk
OUT=${SDK}/buildroot/output/rk3562_navihmi
SYSROOT=${OUT}/host/aarch64-buildroot-linux-gnu/sysroot
PKG_SRC=/workspace/qtmm-src
BUILD_DIR=/workspace/build/qtmm-build

echo "=== 0. 前置检查：sysroot ffmpeg 库 ==="
ls ${SYSROOT}/usr/lib/libavformat.so* ${SYSROOT}/usr/lib/libavcodec.so* \
   ${SYSROOT}/usr/lib/libswresample.so* ${SYSROOT}/usr/lib/libswscale.so* \
   ${SYSROOT}/usr/lib/libavutil.so* >/dev/null 2>&1 \
   || { echo "ffmpeg 库缺失（需 swscale）——先 make ffmpeg-reconfigure"; exit 1; }
ls ${PKG_SRC}/CMakeLists.txt >/dev/null

echo "=== 1. 清理并创建构建目录 ==="
rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}

echo "=== 2. CMake 配置（板端 Qt 6.4.3 sysroot + buildroot ffmpeg，FEATURE_ffmpeg 强制 ON）==="
cd ${BUILD_DIR}
export PKG_CONFIG_PATH=${SYSROOT}/usr/lib/pkgconfig
cmake ${PKG_SRC} \
  -DCMAKE_TOOLCHAIN_FILE=/workspace/cmake/aarch64-buildroot-toolchain.cmake \
  -DCMAKE_PREFIX_PATH=${SYSROOT}/usr \
  -DQT_HOST_PATH=${OUT}/host \
  -DCMAKE_INSTALL_PREFIX=${SYSROOT}/usr \
  -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF \
  -DBUILD_WITH_PCH=OFF \
  -DQT_FEATURE_ffmpeg=ON \
  2>&1 | tail -15

echo "=== 3. 编译（-j8）==="
make -j8 2>&1 | tail -15

echo "=== 4. 安装到 staging sysroot ==="
make install 2>&1 | tail -10

echo "=== 5. 产物检查 ==="
ls ${SYSROOT}/usr/lib/libQt6Multimedia.so* 2>/dev/null || echo "libQt6Multimedia 缺失"
echo "--- qml/QtMultimedia（QML 插件）---"
ls ${BUILD_DIR}/qml/QtMultimedia/ 2>/dev/null | head -8 || echo "qml/QtMultimedia 缺失"
echo "--- ffmpeg 后端插件 ---"
find ${BUILD_DIR} -name 'libffmpegmediaplugin.so' 2>/dev/null || echo "ffmpegmediaplugin 缺失"
echo "=== DONE（staging 就绪；固化: bash /workspace/build-qtmm-overlay.sh）==="
