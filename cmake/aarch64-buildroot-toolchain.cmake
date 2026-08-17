# NavigatorHMI RK3562 交叉编译工具链 (buildroot 外部工具链 + sysroot)
# 用法: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-buildroot-toolchain.cmake \
#            -DCMAKE_PREFIX_PATH=<buildroot-output>/host/aarch64-buildroot-linux-gnu/sysroot/usr ..
# 2026-08-16 B-1: 基于 SDK prebuilts gcc 10.3 + buildroot staging sysroot (Qt 6.4.3)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(TC_PREFIX /sdk/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-)
set(SYSROOT /sdk/buildroot/output/rk3562_navihmi/host/aarch64-buildroot-linux-gnu/sysroot)

set(CMAKE_C_COMPILER ${TC_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TC_PREFIX}g++)
set(CMAKE_SYSROOT ${SYSROOT})

set(CMAKE_FIND_ROOT_PATH ${SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
