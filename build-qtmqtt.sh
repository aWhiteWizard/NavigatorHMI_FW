#!/bin/bash
# qtmqtt 6.4.3 交叉编译（Y-1 2026-09-10）——qtmultimedia（build-qtmm.sh）/qthttpserver（build-qthttp.sh）同先例
# 用板端 Qt 6.4.3 sysroot（buildroot）编译 Qt 官方维护仓库 qt/qtmqtt v6.4.3（github tag）
# 前置：buildroot rk3562_navihmi output 已构建（Qt6 base/network 在 sysroot）
# 用法（docker 内）：
#   docker run --rm -v <qtmqtt源码父目录>:/workspace/qtmqtt-src -v <FW>:/workspace -v <SDK>:/sdk \
#     swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/rk3562-builder-env:v1.1-ubuntu20 \
#     bash /workspace/build-qtmqtt.sh
# 产物装 staging sysroot（编译链接用）；运行镜像固化（overlay + rootfs）由 build-qtmqtt-overlay.sh 承接。
set -euo pipefail
SDK=/sdk
OUT=${SDK}/buildroot/output/rk3562_navihmi
SYSROOT=${OUT}/host/aarch64-buildroot-linux-gnu/sysroot
PKG_SRC=/workspace/qtmqtt-src
BUILD_DIR=/workspace/build/qtmqtt-build

echo "=== 0. 前置检查：源码与 sysroot Qt6 ==="
ls ${PKG_SRC}/CMakeLists.txt >/dev/null
ls ${SYSROOT}/usr/lib/libQt6Network.so* >/dev/null || { echo "libQt6Network 缺失（qt6base 未装全）"; exit 1; }

echo "=== 1. 清理并创建构建目录 ==="
rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}

echo "=== 2. CMake 配置（板端 Qt 6.4.3 sysroot；模块 Qt6Mqtt）==="
cd ${BUILD_DIR}
export PKG_CONFIG_PATH=${SYSROOT}/usr/lib/pkgconfig
cmake ${PKG_SRC} \
  -DCMAKE_TOOLCHAIN_FILE=/workspace/cmake/aarch64-buildroot-toolchain.cmake \
  -DCMAKE_PREFIX_PATH=${SYSROOT}/usr \
  -DQT_HOST_PATH=${OUT}/host \
  -DCMAKE_INSTALL_PREFIX=${SYSROOT}/usr \
  -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF \
  -DQT_FEATURE_websockets=OFF \
  -DQT_FEATURE_quick=OFF \
  2>&1 | tail -12

echo "=== 3. 编译（-j8）==="
make -j8 2>&1 | tail -12

echo "=== 4. 安装到 staging sysroot ==="
make install 2>&1 | tail -6

echo "=== 5. 产物检查 ==="
ls ${SYSROOT}/usr/lib/libQt6Mqtt.so* 2>/dev/null || { echo "libQt6Mqtt 缺失——构建失败"; exit 1; }
echo "--- cmake 配置（供上层 find_package(Qt6Mqtt)）---"
ls ${SYSROOT}/usr/lib/cmake/Qt6Mqtt/ 2>/dev/null | head -8 || echo "cmake/Qt6Mqtt 缺失"
echo "=== DONE（staging 就绪；固化: bash /workspace/build-qtmqtt-overlay.sh）==="
