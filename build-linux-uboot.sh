#!/bin/bash
set -e

ARCH=arm
CROSS_COMPILE=arm-linux-gnueabihf-
JOBS=${1:-4}         # 并行线程数，默认 4
TARGET=${2:-all}     # 编译目标: all, linux, uboot, app, rootfs

echo "========================================="
echo "  编译目标: ${TARGET}"
echo "  并行线程: ${JOBS}"
echo "========================================="

# ===========================================
# 1. 编译 Linux Kernel
# ===========================================
build_linux() {
echo "========================================="
echo "  [1/3] 编译 Linux Kernel 6.6.144 ..."
echo "========================================="

# 解压内核源码（首次运行解压，之后可复用）
KERNEL_SRC=/tmp/linux-6.6.144
if [ ! -d ${KERNEL_SRC} ]; then
    mkdir -p /tmp
    echo ">>> 解压 Linux 内核源码 ..."
    tar -xJf /root/source/linux-6.6.144.tar.xz -C /tmp
fi

# 从挂载的 /workspace/hwt/linux 覆盖到内核源码
if [ -d /workspace/hwt/linux ] && [ "$(ls -A /workspace/hwt/linux 2>/dev/null)" ]; then
    echo ">>> 应用 hwt/linux 覆盖到内核源码 ..."
    cp -rf /workspace/hwt/linux/* ${KERNEL_SRC}/
fi

# 配置并编译内核
cd ${KERNEL_SRC}

# 使用 hwt 自定义 defconfig 配置内核
if [ -f arch/arm/configs/linux_hwt_defconfig ]; then
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} linux_hwt_defconfig
elif [ -f arch/arm/configs/imx_v6_v7_defconfig ]; then
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} imx_v6_v7_defconfig
else
    echo "WARNING: 未找到 defconfig，跳过内核配置"
fi

# 内核产物暂存目录（提前创建，供 dtb 复制使用）
mkdir -p /workspace/build/linux

make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -j${JOBS} zImage

# 编译设备树：只编译 hwt 下自定义的 dts 文件
HWT_DTS_DIR=/workspace/hwt/linux/arch/arm/boot/dts
DTS_FILES=$(find ${HWT_DTS_DIR} -name "*.dts" 2>/dev/null)
if [ -n "${DTS_FILES}" ]; then
    echo ">>> 编译 hwt 中自定义的 dtb ..."
    for dts_file in ${DTS_FILES}; do
        dtb_name=$(basename "${dts_file}" .dts).dtb
        echo "    ${dtb_name}"
        # 找到 dts 在内核中的相对路径，用完整路径编译
        dts_rel=$(find ${KERNEL_SRC}/arch/arm/boot/dts -name "${dtb_name%.dtb}.dts" -not -path "*/overlays/*" 2>/dev/null | sed "s|${KERNEL_SRC}/arch/arm/boot/dts/||" | sed 's/\.dts$//')
        if [ -n "${dts_rel}" ]; then
            make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} ${dts_rel}.dtb
            find arch/arm/boot/dts -name "${dtb_name}" -exec cp {} /workspace/build/linux/ \; 2>/dev/null || true
        else
            echo "    WARNING: 未找到 ${dtb_name} 在内核中的位置，尝试直接编译"
            make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} ${dtb_name} 2>/dev/null || true
        fi
    done
else
    echo ">>> 没有自定义 dts，跳过 dtb 编译"
fi

make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -j${JOBS} modules

# 复制内核镜像
cp arch/arm/boot/zImage /workspace/build/linux/
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

# 如果 hwt/uboot/configs 下有自定义配置，复制到 U-Boot 的 configs 目录
if [ -f /workspace/hwt/uboot/configs/uboot_hwt_defconfig ]; then
    cp /workspace/hwt/uboot/configs/uboot_hwt_defconfig ${UBOOT_SRC}/configs/uboot_hwt_defconfig
fi

cd ${UBOOT_SRC}

# 使用 hwt 自定义 defconfig 配置 U-Boot
if [ -f configs/uboot_hwt_defconfig ]; then
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} uboot_hwt_defconfig
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
cp u-boot-dtb.imx /workspace/build/uboot/ 2>/dev/null || true
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
# Menuconfig: Linux Kernel
# ===========================================
menuconfig_linux() {
# 解压内核源码
KERNEL_SRC=/tmp/linux-6.6.144
if [ ! -d ${KERNEL_SRC} ]; then
    mkdir -p /tmp
    echo ">>> 解压 Linux 内核源码 ..."
    tar -xJf /root/source/linux-6.6.144.tar.xz -C /tmp
fi

# 从挂载的 /workspace/hwt/linux 覆盖到内核源码
if [ -d /workspace/hwt/linux ] && [ "$(ls -A /workspace/hwt/linux 2>/dev/null)" ]; then
    echo ">>> 应用 hwt/linux 覆盖到内核源码 ..."
    cp -rf /workspace/hwt/linux/* ${KERNEL_SRC}/
fi

cd ${KERNEL_SRC}

# 直接使用 hwt 下的 linux_hwt_defconfig（不存在则用 imx 默认创建一份）
HWT_LINUX_DEFCONFIG=/workspace/hwt/linux/arch/arm/configs/linux_hwt_defconfig
if [ -f ${HWT_LINUX_DEFCONFIG} ]; then
    cp ${HWT_LINUX_DEFCONFIG} ${KERNEL_SRC}/arch/arm/configs/linux_hwt_defconfig
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} linux_hwt_defconfig
    echo ">>> 已加载 hwt: linux_hwt_defconfig"
else
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} imx_v6_v7_defconfig
    mkdir -p $(dirname ${HWT_LINUX_DEFCONFIG})
    cp .config $(dirname ${HWT_LINUX_DEFCONFIG})/.config
    make ARCH=${ARCH} savedefconfig
    cp defconfig ${HWT_LINUX_DEFCONFIG}
    echo ">>> 已创建 hwt: linux_hwt_defconfig (基于 imx_v6_v7_defconfig)"
fi

echo "========================================="
echo "  进入 Linux Kernel menuconfig ..."
echo "  修改后保存将自动写入 hwt/linux/arch/arm/configs/"
echo "========================================="
make ARCH=${ARCH} menuconfig

# 保存配置到 hwt
mkdir -p /workspace/hwt/linux/arch/arm/configs
cp .config /workspace/hwt/linux/arch/arm/configs/.config
make ARCH=${ARCH} savedefconfig
cp defconfig ${HWT_LINUX_DEFCONFIG}
echo ">>> Linux 配置已保存到 ${HWT_LINUX_DEFCONFIG}"
}

# ===========================================
# Menuconfig: U-Boot
# ===========================================
menuconfig_uboot() {
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

# 直接使用 hwt 下的 uboot_hwt_defconfig（不存在则用 mx6ull 默认创建一份）
HWT_UBOOT_DEFCONFIG=/workspace/hwt/uboot/configs/uboot_hwt_defconfig
if [ -f ${HWT_UBOOT_DEFCONFIG} ]; then
    cp ${HWT_UBOOT_DEFCONFIG} ${UBOOT_SRC}/configs/uboot_hwt_defconfig
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} uboot_hwt_defconfig
    echo ">>> 已加载 hwt: uboot_hwt_defconfig"
else
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mx6ull_14x14_evk_defconfig
    mkdir -p $(dirname ${HWT_UBOOT_DEFCONFIG})
    cp .config $(dirname ${HWT_UBOOT_DEFCONFIG})/.config
    make ARCH=${ARCH} savedefconfig
    cp defconfig ${HWT_UBOOT_DEFCONFIG}
    echo ">>> 已创建 hwt: uboot_hwt_defconfig (基于 mx6ull_14x14_evk_defconfig)"
fi

echo "========================================="
echo "  进入 U-Boot menuconfig ..."
echo "  修改后保存将自动写入 hwt/uboot/configs/"
echo "========================================="
make ARCH=${ARCH} menuconfig

# 保存配置到 hwt
mkdir -p /workspace/hwt/uboot/configs
cp .config /workspace/hwt/uboot/configs/.config
make ARCH=${ARCH} savedefconfig
cp defconfig ${HWT_UBOOT_DEFCONFIG}
echo ">>> U-Boot 配置已保存到 ${HWT_UBOOT_DEFCONFIG}"
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
# 4. 编译 Buildroot Rootfs
# ===========================================
build_rootfs() {
echo "========================================="
echo "  [4/4] 编译 Buildroot Rootfs ..."
echo "========================================="

BR_VERSION=2025.05
BR_DIR=/tmp/buildroot-${BR_VERSION}
BR_BUILD_DIR=/tmp/buildroot-${BR_VERSION}-build
BR_OUTPUT_DIR=/workspace/build/rootfs

# 解压 Buildroot 源码（首次运行解压，之后可复用）
if [ ! -d ${BR_DIR} ]; then
    mkdir -p /tmp
    echo ">>> 解压 Buildroot 源码 ..."
    tar -xzf /root/source/buildroot-${BR_VERSION}.tar.gz -C /tmp
fi

# 创建外部构建目录
mkdir -p ${BR_BUILD_DIR}

cd ${BR_DIR}

# 如果工程中有自定义 Buildroot 配置，使用它
BR_CONFIG_SRC=/workspace/buildroot
if [ -f ${BR_CONFIG_SRC}/config ] || [ -f ${BR_CONFIG_SRC}/.config ]; then
    echo ">>> 使用工程中的自定义 Buildroot 配置 ..."
    if [ -f ${BR_CONFIG_SRC}/.config ]; then
        cp ${BR_CONFIG_SRC}/.config ${BR_BUILD_DIR}/.config
    else
        make O=${BR_BUILD_DIR} imx6ullevk_defconfig
    fi
else
    echo ">>> 使用 Buildroot 默认 i.MX6ULL 配置 (imx6ullevk_defconfig) ..."
    make O=${BR_BUILD_DIR} imx6ullevk_defconfig
fi

# ========== 配置 Buildroot 使用本地源码 ==========
# 使用 Docker 镜像内的 Linux 内核源码，避免重复下载
make O=${BR_BUILD_DIR} olddefconfig 2>/dev/null || true

# 启用自定义内核 tarball 模式
sed -i 's/BR2_LINUX_KERNEL_CUSTOM_VERSION=y/BR2_LINUX_KERNEL_CUSTOM_VERSION=n/' ${BR_BUILD_DIR}/.config 2>/dev/null || true
sed -i 's/# BR2_LINUX_KERNEL_CUSTOM_TARBALL is not set/BR2_LINUX_KERNEL_CUSTOM_TARBALL=y/' ${BR_BUILD_DIR}/.config 2>/dev/null || true
echo "BR2_LINUX_KERNEL_CUSTOM_TARBALL=y" >> ${BR_BUILD_DIR}/.config 2>/dev/null

# 设置内核源码路径
if grep -q "BR2_LINUX_KERNEL_CUSTOM_TARBALL_LOCATION" ${BR_BUILD_DIR}/.config 2>/dev/null; then
    sed -i "s|BR2_LINUX_KERNEL_CUSTOM_TARBALL_LOCATION=.*|BR2_LINUX_KERNEL_CUSTOM_TARBALL_LOCATION=\"file:///root/source/linux-6.6.144.tar.xz\"|" ${BR_BUILD_DIR}/.config
else
    echo "BR2_LINUX_KERNEL_CUSTOM_TARBALL_LOCATION=\"file:///root/source/linux-6.6.144.tar.xz\"" >> ${BR_BUILD_DIR}/.config
fi

# 同样，使用 Docker 镜像内的 U-Boot 源码
sed -i 's/BR2_TARGET_UBOOT_CUSTOM_VERSION=y/BR2_TARGET_UBOOT_CUSTOM_VERSION=n/' ${BR_BUILD_DIR}/.config 2>/dev/null || true
sed -i 's/# BR2_TARGET_UBOOT_CUSTOM_TARBALL is not set/BR2_TARGET_UBOOT_CUSTOM_TARBALL=y/' ${BR_BUILD_DIR}/.config 2>/dev/null || true
echo "BR2_TARGET_UBOOT_CUSTOM_TARBALL=y" >> ${BR_BUILD_DIR}/.config 2>/dev/null

if grep -q "BR2_TARGET_UBOOT_CUSTOM_TARBALL_LOCATION" ${BR_BUILD_DIR}/.config 2>/dev/null; then
    sed -i "s|BR2_TARGET_UBOOT_CUSTOM_TARBALL_LOCATION=.*|BR2_TARGET_UBOOT_CUSTOM_TARBALL_LOCATION=\"file:///root/source/u-boot-2020.04.tar.bz2\"|" ${BR_BUILD_DIR}/.config
else
    echo "BR2_TARGET_UBOOT_CUSTOM_TARBALL_LOCATION=\"file:///root/source/u-boot-2020.04.tar.bz2\"" >> ${BR_BUILD_DIR}/.config
fi

# 再次 olddefconfig 使配置生效
make O=${BR_BUILD_DIR} olddefconfig 2>/dev/null || true

# ========== 内核头文件版本由 Buildroot 默认决定 ==========
# 使用 Linux 6.6 LTS，与 Buildroot 2025.05 默认头文件版本（6.6.x）匹配
# 无需额外配置

# 下载缓存目录（优先使用环境变量 BR2_DL_DIR）
# 镜像内预缓存路径: /root/buildroot-dl（由 Dockerfile COPY 进去）
if [ -z "${BR2_DL_DIR}" ] && [ -d /root/buildroot-dl ]; then
    CACHE_DIR=/root/buildroot-dl
else
    CACHE_DIR=${BR2_DL_DIR:-/tmp/buildroot-dl}
fi
mkdir -p ${CACHE_DIR}
echo ">>> 下载缓存: ${CACHE_DIR}"
echo "BR2_DL_DIR=\"${CACHE_DIR}\"" >> ${BR_BUILD_DIR}/.config

# 复制工程中的 rootfs-overlay
if [ -d ${BR_CONFIG_SRC}/rootfs-overlay ]; then
    echo ">>> 使用工程中的 rootfs-overlay ..."
    mkdir -p ${BR_BUILD_DIR}/rootfs-overlay
    cp -rf ${BR_CONFIG_SRC}/rootfs-overlay/* ${BR_BUILD_DIR}/rootfs-overlay/ 2>/dev/null || true
    echo "BR2_ROOTFS_OVERLAY=\"${BR_BUILD_DIR}/rootfs-overlay\"" >> ${BR_BUILD_DIR}/.config
fi

# 如果编译了应用，把应用放入 rootfs-overlay
if [ -f /workspace/build/bin/NavigatorHMI_FW ]; then
    echo ">>> 将 NavigatorHMI_FW 应用加入 rootfs ..."
    mkdir -p ${BR_BUILD_DIR}/rootfs-overlay/usr/bin
    cp /workspace/build/bin/NavigatorHMI_FW ${BR_BUILD_DIR}/rootfs-overlay/usr/bin/
fi

# 编译 Buildroot
echo ">>> 开始编译 Buildroot (首次需下载源码包，耗时较长)..."
FORCE_UNSAFE_CONFIGURE=1 make O=${BR_BUILD_DIR} -j${JOBS}

# 输出产物
echo ""
echo ">>> Buildroot 编译完成!"
ls -lh ${BR_BUILD_DIR}/images/ 2>/dev/null || true

# 复制产物到 /workspace/build/rootfs
mkdir -p ${BR_OUTPUT_DIR}
cp ${BR_BUILD_DIR}/images/rootfs.tar ${BR_OUTPUT_DIR}/ 2>/dev/null || true
echo ">>> Rootfs 已输出到 ${BR_OUTPUT_DIR}/"
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
        build_rootfs
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
    rootfs)
        build_app
        build_rootfs
        ;;
    menuconfig_linux)
        menuconfig_linux
        exit 0
        ;;
    menuconfig_uboot)
        menuconfig_uboot
        exit 0
        ;;
    *)
        echo "错误: 未知编译目标 '${TARGET}'"
        echo "用法: $0 [JOBS] [TARGET]"
        echo "  TARGET: all (默认), linux, uboot, app, linux+app, rootfs, menuconfig_linux, menuconfig_uboot"
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
