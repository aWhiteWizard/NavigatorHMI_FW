#!/bin/bash
set -e

ARCH=arm
CROSS_COMPILE=arm-linux-gnueabihf-
JOBS=${1:-4}         # 并行线程数，默认 4
TARGET=${2:-all}     # 编译目标: all, linux, uboot, app

echo "========================================="
echo "  编译目标: ${TARGET}"
echo "  并行线程: ${JOBS}"
echo "========================================="

# ===========================================
# 1. 编译 Linux Kernel
# ===========================================
build_linux() {
echo "========================================="
echo "  [1/3] 编译 Linux Kernel 5.4.234 ..."
echo "========================================="

# 解压内核源码（首次运行解压，之后可复用）
KERNEL_SRC=/tmp/linux-5.4.234
if [ ! -d ${KERNEL_SRC} ]; then
    mkdir -p /tmp
    echo ">>> 解压 Linux 内核源码 ..."
    tar -xzf /root/source/linux-5.4.234.tar.gz -C /tmp
fi

# 从挂载的 /workspace/hwt/linux 覆盖到内核源码
if [ -d /workspace/hwt/linux ] && [ "$(ls -A /workspace/hwt/linux 2>/dev/null)" ]; then
    echo ">>> 应用 hwt/linux 覆盖到内核源码 ..."
    cp -rf /workspace/hwt/linux/* ${KERNEL_SRC}/
fi

# 配置并编译内核
cd ${KERNEL_SRC}

# 如果 hwt 提供了自定义 defconfig 则使用，否则用 imx 默认配置
if [ -f arch/arm/configs/hwt_defconfig ]; then
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} hwt_defconfig
elif [ -f arch/arm/configs/imx_v6_v7_defconfig ]; then
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} imx_v6_v7_defconfig
else
    echo "WARNING: 未找到 defconfig，跳过内核配置"
fi

make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -j${JOBS} zImage
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -j${JOBS} dtbs
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -j${JOBS} modules

# 内核产物暂存到 /workspace/build/linux
mkdir -p /workspace/build/linux
cp arch/arm/boot/zImage /workspace/build/linux/
cp arch/arm/boot/dts/*.dtb /workspace/build/linux/ 2>/dev/null || true
echo ">>> Linux Kernel 编译完成: /workspace/build/linux/zImage"
}

# ===========================================
# 2. 编译 U-Boot
# ===========================================
build_uboot() {
echo ""
echo "========================================="
echo "  [2/3] 编译 U-Boot 2020.04 ..."
echo "========================================="

# 解压 U-Boot 源码
UBOOT_SRC=/tmp/u-boot-2020.04
if [ ! -d ${UBOOT_SRC} ]; then
    mkdir -p /tmp
    echo ">>> 解压 U-Boot 源码 ..."
    tar -xjf /root/source/u-boot-2020.04.tar.bz2 -C /tmp
fi

# 从挂载的 /workspace/hwt/uboot 覆盖到 U-Boot 源码
if [ -d /workspace/hwt/uboot ] && [ "$(ls -A /workspace/hwt/uboot 2>/dev/null)" ]; then
    echo ">>> 应用 hwt/uboot 覆盖到 U-Boot 源码 ..."
    cp -rf /workspace/hwt/uboot/* ${UBOOT_SRC}/
fi

cd ${UBOOT_SRC}

# 如果 hwt 提供了自定义 defconfig 则使用，否则用 mx6ull 默认配置
if [ -f configs/hwt_defconfig ]; then
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} hwt_defconfig
elif [ -f configs/mx6ull_14x14_evk_defconfig ]; then
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mx6ull_14x14_evk_defconfig
else
    echo "WARNING: 未找到 U-Boot defconfig，尝试 mx6ull_14x14_evk_defconfig"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mx6ull_14x14_evk_defconfig 2>/dev/null || true
fi

make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -j${JOBS}

# 将 U-Boot 编译产物输出
mkdir -p /workspace/build/uboot
cp u-boot.bin /workspace/build/uboot/ 2>/dev/null || true
cp u-boot.imx /workspace/build/uboot/ 2>/dev/null || true
cp SPL /workspace/build/uboot/ 2>/dev/null || true
echo ">>> U-Boot 编译完成: /workspace/build/uboot/"
}

# ===========================================
# 3. 编译 NavigatorHMI_FW 应用
# ===========================================
build_app() {
echo ""
echo "========================================="
echo "  [3/3] 编译 NavigatorHMI_FW 应用 ..."
echo "========================================="

mkdir -p /workspace/build
cd /workspace/build

# CMake 配置
cmake /workspace \
    -DCMAKE_TOOLCHAIN_FILE=/workspace/cmake/arm-linux-gnueabihf-toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release

# 编译
make -j${JOBS}

echo ">>> NavigatorHMI_FW 应用编译完成"
}

# ===========================================
# 收集产物 —— 应用 bin 放到内核 /bin 目录
# ===========================================
collect_artifacts() {
echo ""
echo "========================================="
echo "  收集编译产物 ..."
echo "========================================="

# 把应用可执行文件复制到内核的 /bin 目录（模拟根文件系统结构）
mkdir -p /workspace/build/linux/bin
if [ -f /workspace/build/bin/NavigatorHMI_FW ]; then
    cp /workspace/build/bin/NavigatorHMI_FW /workspace/build/linux/bin/
    echo ">>> 应用已部署到内核 /bin 目录: /workspace/build/linux/bin/NavigatorHMI_FW"
fi

# 收集模块到内核目录
if [ -d ${KERNEL_SRC} ]; then
    mkdir -p /workspace/build/linux/lib/modules
    make -C ${KERNEL_SRC} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules_install INSTALL_MOD_PATH=/workspace/build/linux 2>/dev/null || true
fi
}

# ===========================================
# 主调度
# ===========================================
case "${TARGET}" in
    all)
        build_linux
        build_uboot
        build_app
        collect_artifacts
        ;;
    linux)
        build_linux
        ;;
    uboot)
        build_uboot
        ;;
    app)
        build_app
        collect_artifacts
        ;;
    linux+app)
        build_linux
        build_app
        collect_artifacts
        ;;
    *)
        echo "错误: 未知编译目标 '${TARGET}'"
        echo "用法: $0 [JOBS] [TARGET]"
        echo "  TARGET: all (默认), linux, uboot, app, linux+app"
        exit 1
        ;;
esac

echo ""
echo "========================================="
echo "  ✓ 全部编译完成!"
echo "  内核:  /workspace/build/linux/zImage"
echo "  应用:  /workspace/build/linux/bin/NavigatorHMI_FW"
echo "  U-Boot: /workspace/build/uboot/u-boot.imx"
echo "========================================="
