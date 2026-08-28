#!/bin/bash
# qthttpserver v6.4.3 交叉编译（K-8a，2026-08-30）——与 qtvk 同先例：
# 用板端 Qt 6.4.3 sysroot（buildroot）编译官方 QtHttpServer（Technology Preview 模块，Buildroot 无包）。
# 用法（docker 内）：
#   docker run --rm -v <qthttpserver源码>:/workspace/qthttp-src -v <FW>:/workspace -v <SDK>:/sdk \
#     rk3562-builder-env:v1.1-ubuntu20 bash /workspace/build-qthttp.sh
set -e
SDK=/sdk
OUT=${SDK}/buildroot/output/rk3562_navihmi
SYSROOT=${OUT}/host/aarch64-buildroot-linux-gnu/sysroot
PKG_SRC=/workspace/qthttp-src
BUILD_DIR=/workspace/build/qthttp-build

echo "=== 1. 清理并创建构建目录 ==="
rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}
ls ${PKG_SRC}/CMakeLists.txt

echo "=== 2. CMake 配置（板端 Qt 6.4.3 sysroot）==="
cd ${BUILD_DIR}
cmake ${PKG_SRC} \
  -DCMAKE_TOOLCHAIN_FILE=/workspace/cmake/aarch64-buildroot-toolchain.cmake \
  -DCMAKE_PREFIX_PATH=${SYSROOT}/usr \
  -DQT_HOST_PATH=${OUT}/host \
  -DCMAKE_INSTALL_PREFIX=${SYSROOT}/usr \
  -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF \
  -DBUILD_WITH_PCH=OFF \
  2>&1 | tail -8

echo "=== 3. 编译 ==="
make -j8 2>&1 | tail -10

echo "=== 4. 安装到 sysroot ==="
make install 2>&1 | tail -8

echo "=== 5. 产物检查 ==="
ls ${BUILD_DIR}/lib/libQt6HttpServer* 2>/dev/null || echo "lib 缺失"
find ${SYSROOT}/usr/lib/cmake -maxdepth 1 -iname "*HttpServer*" 2>/dev/null
ls ${SYSROOT}/usr/include/QtHttpServer 2>/dev/null | head -3
echo "=== DONE（产物：lib + include + cmake 配置已入 sysroot）==="
