#!/bin/bash
set -e
SDK=/sdk
OUT=${SDK}/buildroot/output/rk3562_navihmi
SYSROOT=${OUT}/host/aarch64-buildroot-linux-gnu/sysroot
PKG_SRC=/workspace/build/qtvk-src
BUILD_DIR=/workspace/build/qtvk-build
echo "=== 1. 源码解压到挂载路径 (持久) ==="
rm -rf ${PKG_SRC} ${BUILD_DIR}
mkdir -p ${PKG_SRC} ${BUILD_DIR}
tar -xf ${SDK}/buildroot/dl/qt6virtualkeyboard/qtvirtualkeyboard-everywhere-src-6.4.3.tar.xz -C ${PKG_SRC}
ls ${PKG_SRC}/qtvirtualkeyboard-everywhere-src-6.4.3/ | head -4
echo "=== 2. CMake 配置 ==="
cd ${BUILD_DIR}
cmake ${PKG_SRC}/qtvirtualkeyboard-everywhere-src-6.4.3 \
  -DCMAKE_TOOLCHAIN_FILE=/workspace/cmake/aarch64-buildroot-toolchain.cmake \
  -DCMAKE_PREFIX_PATH=${SYSROOT}/usr \
  -DQT_HOST_PATH=${OUT}/host \
  -DCMAKE_INSTALL_PREFIX=${SYSROOT}/usr \
  -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF \
  -DBUILD_WITH_PCH=OFF \
  2>&1 | tail -6
echo "=== 3. 编译 ==="
make -j8 2>&1 | tail -8
echo "=== 4. 产物检查 ==="
find ${BUILD_DIR}/qml/QtQuick/VirtualKeyboard -name '*.so' 2>/dev/null | wc -l
ls ${BUILD_DIR}/lib/libQt6VirtualKeyboard* 2>/dev/null
echo "=== DONE (产物在 /workspace/build/qtvk-build) ==="