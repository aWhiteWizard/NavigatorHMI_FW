#!/bin/bash
set -e

ARCH=arm
CROSS_COMPILE=arm-linux-gnueabihf-
JOBS=${1:-4}         # 并行线程数，默认 4
TARGET=${2:-all}     # 编译目标: all, linux, uboot, app, linux+app, qt, rootfs, image

echo "========================================="
echo "  编译目标: ${TARGET}"
echo "  并行线程: ${JOBS}"
echo "========================================="

# ===========================================
# 1. 编译 Linux Kernel
# ===========================================
build_linux() {
echo "========================================="
echo "  [1/3] 编译 Linux Kernel 4.1.15 ..."
echo "========================================="

# 解压内核源码（首次运行解压，之后可复用）
KERNEL_SRC=/tmp/linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek
if [ ! -d ${KERNEL_SRC} ]; then
    mkdir -p /tmp
    echo ">>> 解压 Linux 内核源码 ..."
    tar -xjf /root/source/linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek.tar.bz2 -C /tmp
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
    echo ">>> 使用 hwt 自定义配置: linux_hwt_defconfig"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} linux_hwt_defconfig
elif [ -f arch/arm/configs/imx_alientek_emmc_defconfig ]; then
    echo ">>> 使用板级默认配置: imx_alientek_emmc_defconfig"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} imx_alientek_emmc_defconfig
else
    echo ">>> 使用 i.MX 通用配置: imx_v7_defconfig"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} imx_v7_defconfig
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
echo "  [2/3] 编译 U-Boot (rel_imx_4.1.15_2.1.0_ga_alientek) ..."
echo "========================================="

# 解压 U-Boot 源码
UBOOT_SRC=/tmp/uboot-imx-rel_imx_4.1.15_2.1.0_ga_alientek
if [ ! -d ${UBOOT_SRC} ]; then
    mkdir -p /tmp
    echo ">>> 解压 U-Boot 源码 ..."
    tar -xjf /root/source/uboot-imx-rel_imx_4.1.15_2.1.0_ga_alientek.tar.bz2 -C /tmp
fi

# 从挂载的 /workspace/hwt/uboot 覆盖到 U-Boot 源码
if [ -d /workspace/hwt/uboot ] && [ "$(ls -A /workspace/hwt/uboot 2>/dev/null)" ]; then
    echo ">>> 应用 hwt/uboot 覆盖到 U-Boot 源码 ..."
    # 逐个复制各子目录，确保覆盖所有内容
    for item in /workspace/hwt/uboot/*; do
        cp -rf "$item" ${UBOOT_SRC}/
    done
fi

# 如果 hwt/uboot/configs 下有自定义配置，复制到 U-Boot 的 configs 目录
if [ -f /workspace/hwt/uboot/configs/uboot_hwt_defconfig ]; then
    cp /workspace/hwt/uboot/configs/uboot_hwt_defconfig ${UBOOT_SRC}/configs/uboot_hwt_defconfig
fi

# 验证 board 文件覆盖是否成功
if [ -f /workspace/hwt/uboot/board/freescale/mx6ull_alientek_emmc/mx6ull_alientek_emmc.c ]; then
    cp /workspace/hwt/uboot/board/freescale/mx6ull_alientek_emmc/mx6ull_alientek_emmc.c \
        ${UBOOT_SRC}/board/freescale/mx6ull_alientek_emmc/mx6ull_alientek_emmc.c
    echo ">>> board 文件覆盖完成"
fi

# 验证关键文件是否覆盖成功
if grep -q "NavigatorHMI" ${UBOOT_SRC}/board/freescale/mx6ull_alientek_emmc/mx6ull_alientek_emmc.c 2>/dev/null; then
    echo ">>> Board Name 覆盖验证通过"
else
    echo ">>> WARNING: Board Name 覆盖验证失败！"
fi

cd ${UBOOT_SRC}

# 使用 hwt 自定义 defconfig 配置 U-Boot（优先使用 uboot_hwt_defconfig）
if [ -f configs/uboot_hwt_defconfig ]; then
    echo ">>> 使用 hwt 自定义配置: uboot_hwt_defconfig"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} uboot_hwt_defconfig
elif [ -f configs/mx6ull_alientek_emmc_defconfig ]; then
    echo ">>> 使用板级默认配置: mx6ull_alientek_emmc_defconfig"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mx6ull_alientek_emmc_defconfig
else
    echo ">>> 使用 EVK 默认配置: mx6ull_14x14_evk_defconfig"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mx6ull_14x14_evk_defconfig
fi

make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -j${JOBS}

# 生成 i.MX 启动镜像（带 IVT 头的 u-boot.imx）
# 旧版 U-Boot 需要 tools/mkimage 生成 .imx，但 make 会自动调用
# 如果没生成则手动尝试
if [ ! -f u-boot.imx ] && [ ! -f u-boot-dtb.imx ]; then
    if [ -f tools/mkimage ]; then
        echo ">>> 手动生成 u-boot.imx ..."
        ./tools/mkimage -n board/freescale/mx6ull_alientek_emmc/imximage.cfg -T imximage -e 0x87800000 -d u-boot.bin u-boot.imx 2>/dev/null || true
    fi
fi

# 将 U-Boot 编译产物输出
mkdir -p /workspace/build/uboot
cp u-boot.bin /workspace/build/uboot/ 2>/dev/null || true
cp u-boot-dtb.imx /workspace/build/uboot/ 2>/dev/null || true
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
# Menuconfig: Linux Kernel
# ===========================================
menuconfig_linux() {
# 解压内核源码
KERNEL_SRC=/tmp/linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek
if [ ! -d ${KERNEL_SRC} ]; then
    mkdir -p /tmp
    echo ">>> 解压 Linux 内核源码 ..."
    tar -xjf /root/source/linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek.tar.bz2 -C /tmp
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
elif [ -f ${KERNEL_SRC}/arch/arm/configs/imx_alientek_emmc_defconfig ]; then
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} imx_alientek_emmc_defconfig
    mkdir -p $(dirname ${HWT_LINUX_DEFCONFIG})
    cp .config $(dirname ${HWT_LINUX_DEFCONFIG})/.config
    make ARCH=${ARCH} savedefconfig
    cp defconfig ${HWT_LINUX_DEFCONFIG}
    echo ">>> 已创建 hwt: linux_hwt_defconfig (基于 imx_alientek_emmc_defconfig)"
else
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} imx_v7_defconfig
    mkdir -p $(dirname ${HWT_LINUX_DEFCONFIG})
    cp .config $(dirname ${HWT_LINUX_DEFCONFIG})/.config
    make ARCH=${ARCH} savedefconfig
    cp defconfig ${HWT_LINUX_DEFCONFIG}
    echo ">>> 已创建 hwt: linux_hwt_defconfig (基于 imx_v7_defconfig)"
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
UBOOT_SRC=/tmp/uboot-imx-rel_imx_4.1.15_2.1.0_ga_alientek
if [ ! -d ${UBOOT_SRC} ]; then
    mkdir -p /tmp
    echo ">>> 解压 U-Boot 源码 ..."
    tar -xjf /root/source/uboot-imx-rel_imx_4.1.15_2.1.0_ga_alientek.tar.bz2 -C /tmp
fi

# 从挂载的 /workspace/hwt/uboot 覆盖到 U-Boot 源码
if [ -d /workspace/hwt/uboot ] && [ "$(ls -A /workspace/hwt/uboot 2>/dev/null)" ]; then
    echo ">>> 应用 hwt/uboot 覆盖到 U-Boot 源码 ..."
    cp -rf /workspace/hwt/uboot/* ${UBOOT_SRC}/
fi

cd ${UBOOT_SRC}

# 直接使用 hwt 下的 uboot_hwt_defconfig（不存在则用 Alientek eMMC 默认创建一份）
HWT_UBOOT_DEFCONFIG=/workspace/hwt/uboot/configs/uboot_hwt_defconfig
if [ -f ${HWT_UBOOT_DEFCONFIG} ]; then
    cp ${HWT_UBOOT_DEFCONFIG} ${UBOOT_SRC}/configs/uboot_hwt_defconfig
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} uboot_hwt_defconfig
    echo ">>> 已加载 hwt: uboot_hwt_defconfig"
elif [ -f configs/mx6ull_alientek_emmc_defconfig ]; then
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mx6ull_alientek_emmc_defconfig
    mkdir -p $(dirname ${HWT_UBOOT_DEFCONFIG})
    cp .config $(dirname ${HWT_UBOOT_DEFCONFIG})/.config
    make ARCH=${ARCH} savedefconfig
    cp defconfig ${HWT_UBOOT_DEFCONFIG}
    echo ">>> 已创建 hwt: uboot_hwt_defconfig (基于 mx6ull_alientek_emmc_defconfig)"
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

BR_VERSION=2019.02.6
BR_DIR=/tmp/buildroot-${BR_VERSION}
BR_BUILD_DIR=/tmp/buildroot-${BR_VERSION}-build
BR_OUTPUT_DIR=/workspace/build/rootfs

# 解压 Buildroot 源码（首次运行解压，之后可复用）
if [ ! -d ${BR_DIR} ]; then
    mkdir -p /tmp
    echo ">>> 解压 Buildroot 源码 ..."
    tar -xjf /root/source/buildroot-${BR_VERSION}.tar.bz2 -C /tmp
fi

# 创建外部构建目录
mkdir -p ${BR_BUILD_DIR}

cd ${BR_DIR}

# 使用工程中的自定义 Buildroot 配置
BR_CONFIG_SRC=/workspace/hwt/buildroot
if [ -f ${BR_CONFIG_SRC}/alientek_emmc_defconfig ]; then
    echo ">>> 使用工程中的自定义 Buildroot 配置: alientek_emmc_defconfig"
    # 复制 defconfig 到 Buildroot 的 configs 目录，然后用 make 加载
    cp ${BR_CONFIG_SRC}/alientek_emmc_defconfig ${BR_DIR}/configs/alientek_emmc_defconfig
    make O=${BR_BUILD_DIR} alientek_emmc_defconfig
elif [ -f ${BR_CONFIG_SRC}/config ] || [ -f ${BR_CONFIG_SRC}/.config ]; then
    echo ">>> 使用工程中的自定义 Buildroot 配置 ..."
    if [ -f ${BR_CONFIG_SRC}/.config ]; then
        cp ${BR_CONFIG_SRC}/.config ${BR_BUILD_DIR}/.config
    else
        make O=${BR_BUILD_DIR} imx6ulevk_defconfig
    fi
else
    echo ">>> 使用 Buildroot 默认 i.MX6UL EVK 配置 (imx6ulevk_defconfig) ..."
    make O=${BR_BUILD_DIR} imx6ulevk_defconfig
fi

# ========== 内核头文件版本由 Buildroot 默认决定 ==========
# 使用 Linux 4.1.15 内核，与 Buildroot 2019.02.6 默认头文件版本匹配
# 无需额外配置

# olddefconfig 使自定义配置生效
make O=${BR_BUILD_DIR} olddefconfig 2>/dev/null || true

# 下载缓存目录（使用独立的 DL 缓存，与镜像内预置的 buildroot-dl 区分）
CACHE_DIR=${BR2_DL_DIR:-/workspace/build/buildroot-dl}
mkdir -p ${CACHE_DIR}
echo ">>> Buildroot 下载缓存: ${CACHE_DIR}"
echo "BR2_DL_DIR=\"${CACHE_DIR}\"" >> ${BR_BUILD_DIR}/.config

# 创建 rootfs-overlay 并将应用放入
mkdir -p ${BR_BUILD_DIR}/rootfs-overlay/usr/bin
if [ -d ${BR_CONFIG_SRC}/rootfs-overlay ]; then
    echo ">>> 使用工程中的 rootfs-overlay ..."
    cp -rf ${BR_CONFIG_SRC}/rootfs-overlay/* ${BR_BUILD_DIR}/rootfs-overlay/ 2>/dev/null || true
fi
echo "BR2_ROOTFS_OVERLAY=\"${BR_BUILD_DIR}/rootfs-overlay\"" >> ${BR_BUILD_DIR}/.config

# post-build 脚本（gconv 模块注入等，修复 Qt iconv_open failed）
if [ -f /workspace/hwt/buildroot/post-build.sh ]; then
    echo "BR2_ROOTFS_POST_BUILD_SCRIPT=\"/workspace/hwt/buildroot/post-build.sh\"" >> ${BR_BUILD_DIR}/.config
fi

# 如果编译了应用，把应用放入 rootfs-overlay
if [ -f /workspace/build/bin/NavigatorHMI_FW ]; then
    echo ">>> 将 NavigatorHMI_FW 应用加入 rootfs ..."
    cp /workspace/build/bin/NavigatorHMI_FW ${BR_BUILD_DIR}/rootfs-overlay/usr/bin/
fi

# 如果已编译 Qt，把 Qt 运行时注入 rootfs (/opt/qt5.12.9)
QT_STAGING=/workspace/build/qt5.12.9-arm
if [ -f /workspace/build/.qt5.12.9-done ] && [ -d ${QT_STAGING}/lib ]; then
    echo ">>> 将 Qt 5.12.9 运行时注入 rootfs (/opt/qt5.12.9) ..."
    QT_OVERLAY=${BR_BUILD_DIR}/rootfs-overlay/opt/qt5.12.9
    mkdir -p ${QT_OVERLAY}
    cp -rf ${QT_STAGING}/lib ${QT_OVERLAY}/
    cp -rf ${QT_STAGING}/plugins ${QT_OVERLAY}/ 2>/dev/null || true
    cp -rf ${QT_STAGING}/qml ${QT_OVERLAY}/ 2>/dev/null || true
    # 删除开发文件，减小 rootfs 体积
    find ${QT_OVERLAY}/lib -type f \( -name "*.a" -o -name "*.la" -o -name "*.prl" \) -delete
    rm -rf ${QT_OVERLAY}/lib/cmake ${QT_OVERLAY}/lib/pkgconfig
    echo "    Qt 运行时注入完成 ($(du -sh ${QT_OVERLAY} | cut -f1))"
else
    echo ">>> 未检测到已编译的 Qt，跳过 Qt 注入（先执行: ./docker-build.ps1 -Target qt）"
fi

# 注入外部编译的 Linux 内核和 U-Boot 产物到 images 目录
echo ">>> 注入外部编译的 Linux/U-Boot 产物到 images 目录 ..."
mkdir -p ${BR_BUILD_DIR}/images

# 复制 zImage
if [ -f /workspace/build/linux/zImage ]; then
    cp /workspace/build/linux/zImage ${BR_BUILD_DIR}/images/zImage
    echo "    zImage 注入成功"
fi

# 复制 dtb（同时复制一份为 Alientek U-Boot 期望的名称）
if [ -f /workspace/build/linux/imx6ull-14x14-evk.dtb ]; then
    cp /workspace/build/linux/imx6ull-14x14-evk.dtb ${BR_BUILD_DIR}/images/
    cp /workspace/build/linux/imx6ull-14x14-evk.dtb ${BR_BUILD_DIR}/images/imx6ull-alientek-emmc.dtb
    echo "    dtb 注入成功 (imx6ull-14x14-evk.dtb + imx6ull-alientek-emmc.dtb)"
fi

# 复制 u-boot 镜像（post-image 的 uboot_image() 函数依赖 .config 中的标记，
# 但由于 BR2_TARGET_UBOOT 未启用，标记会被 olddefconfig 清掉。
# 所以直接手动将 u-boot.imx 注入到 sdcard.img 的正确偏移位置）
if [ -f /workspace/build/uboot/u-boot-dtb.imx ]; then
    cp /workspace/build/uboot/u-boot-dtb.imx ${BR_BUILD_DIR}/images/
    echo "    u-boot-dtb.imx 注入成功"
elif [ -f /workspace/build/uboot/u-boot.imx ]; then
    cp /workspace/build/uboot/u-boot.imx ${BR_BUILD_DIR}/images/
    echo "    u-boot.imx 注入成功"
else
    echo "    WARNING: 未找到 u-boot.imx 或 u-boot-dtb.imx！"
fi

# 编译 Buildroot
echo ">>> 开始编译 Buildroot (首次需下载源码包，耗时较长)..."
FORCE_UNSAFE_CONFIGURE=1 make O=${BR_BUILD_DIR} -j${JOBS}

# 输出产物
echo ""
echo ">>> Buildroot 编译完成!"
ls -lh ${BR_BUILD_DIR}/images/ 2>/dev/null || true

# 手动注入 U-Boot 到 sdcard.img（因为 genimage 模板中 %UBOOTBIN% 为空）
if [ -f ${BR_BUILD_DIR}/images/u-boot.imx ] && [ -f ${BR_BUILD_DIR}/images/sdcard.img ]; then
    echo ">>> 手动注入 U-Boot 到 sdcard.img (偏移 1024)..."
    dd if=${BR_BUILD_DIR}/images/u-boot.imx of=${BR_BUILD_DIR}/images/sdcard.img bs=1024 seek=1 conv=notrunc 2>/dev/null
    echo "    U-Boot 注入完成"
fi

# 重建 boot.vfat 并注入 zImage 和 dtb
# 注意：Buildroot 的 genimage 已经在 sdcard.img 中创建了 boot 分区，
# 我们直接在 sdcard.img 内部的 FAT 分区操作
echo ">>> 注入 zImage 和 dtb 到 sdcard.img 的 boot 分区 ..."
if [ -f /workspace/build/linux/zImage ] && [ -f /workspace/build/linux/imx6ull-alientek-emmc.dtb ] && [ -f ${BR_BUILD_DIR}/images/sdcard.img ]; then
    # 确保 mtools 可用
    apt-get install -y mtools 2>/dev/null || true

    BOOT_OFFSET=8M
    echo "    写入 zImage ..."
    MTOOLS_SKIP_CHECK=1 mcopy -sp -i ${BR_BUILD_DIR}/images/sdcard.img@@${BOOT_OFFSET} \
        /workspace/build/linux/zImage ::zImage 2>&1 || true
    echo "    写入 dtb ..."
    MTOOLS_SKIP_CHECK=1 mcopy -sp -i ${BR_BUILD_DIR}/images/sdcard.img@@${BOOT_OFFSET} \
        /workspace/build/linux/imx6ull-alientek-emmc.dtb :: 2>&1 || true
    echo "    boot 分区注入完成"
fi

# 复制产物到 /workspace/build/rootfs
mkdir -p ${BR_OUTPUT_DIR}
cp ${BR_BUILD_DIR}/images/rootfs.tar ${BR_OUTPUT_DIR}/ 2>/dev/null || true
cp ${BR_BUILD_DIR}/images/sdcard.img ${BR_OUTPUT_DIR}/ 2>/dev/null || true
echo ">>> Rootfs 已输出到 ${BR_OUTPUT_DIR}/"
echo ">>> SD 卡镜像: ${BR_OUTPUT_DIR}/sdcard.img"
}

# ===========================================
# 编译 Qt 5.12.9（交叉编译，仅首次需要，约 1~2 小时）
# ===========================================
build_qt() {
echo ""
echo "========================================="
echo "  编译 Qt 5.12.9 (arm-linux-gnueabihf)"
echo "========================================="

QT_VERSION=5.12.9
QT_SRC=/tmp/qt-everywhere-src-${QT_VERSION}
QT_EXTPREFIX=/workspace/build/qt${QT_VERSION}-arm   # 宿主侧安装位置（CMake find_package 用）
QT_PREFIX=/opt/qt${QT_VERSION}                      # 目标设备部署路径（随 rootfs 烧录）
QT_DONE=/workspace/build/.qt${QT_VERSION}-done

# 已完成则跳过（staging 在挂载卷上持久化，每台机器只需编译一次）
if [ -f ${QT_DONE} ] && [ -f ${QT_EXTPREFIX}/lib/libQt5Core.so ]; then
    echo ">>> 检测到已编译的 Qt (${QT_EXTPREFIX})，跳过"
    echo ">>> 如需重新编译，请删除: build/qt${QT_VERSION}-arm/ 和 build/.qt${QT_VERSION}-done"
    return 0
fi

# 解压 Qt 源码到容器 /tmp（容器原生文件系统，编译 IO 远快于挂载卷）
if [ ! -d ${QT_SRC} ]; then
    echo ">>> 解压 Qt 源码 ..."
    tar -xJf /root/source/qt-everywhere-src-${QT_VERSION}.tar.xz -C /tmp
fi

# 应用 hwt 自定义 mkspec（arm-linux-gnueabihf 工具链）
if [ -d /workspace/hwt/qt/mkspecs/linux-arm-gnueabihf-g++ ]; then
    echo ">>> 应用 hwt/qt mkspec: linux-arm-gnueabihf-g++"
    cp -rf /workspace/hwt/qt/mkspecs/linux-arm-gnueabihf-g++ ${QT_SRC}/qtbase/mkspecs/
else
    echo "错误: 未找到 hwt/qt/mkspecs/linux-arm-gnueabihf-g++"
    exit 1
fi

cd ${QT_SRC}

# 配置 Qt（i.MX6ULL 无 GPU → linuxfb + 软件渲染；第三方库全用 Qt 内置版，无需 sysroot）
if [ ! -f ${QT_SRC}/.configured ]; then
    echo ">>> 配置 Qt ..."
    ./configure \
        -prefix ${QT_PREFIX} \
        -extprefix ${QT_EXTPREFIX} \
        -opensource -confirm-license \
        -release -strip \
        -xplatform linux-arm-gnueabihf-g++ \
        -no-opengl -linuxfb -no-xcb \
        -no-glib -no-dbus -no-cups -no-openssl \
        -no-libudev -no-mtdev \
        -qt-zlib -qt-libpng -qt-libjpeg -qt-freetype -qt-pcre \
        -sql-sqlite \
        -make libs -nomake examples -nomake tests -nomake tools \
        -skip qt3d -skip qtactiveqt -skip qtandroidextras \
        -skip qtcanvas3d -skip qtcharts -skip qtconnectivity \
        -skip qtdatavis3d -skip qtdoc -skip qtgamepad \
        -skip qtlocation -skip qtmacextras -skip qtmultimedia \
        -skip qtnetworkauth -skip qtpurchasing -skip qtquickcontrols \
        -skip qtremoteobjects -skip qtscript -skip qtscxml \
        -skip qtsensors -skip qtserialbus -skip qtspeech \
        -skip qttools -skip qttranslations -skip qtvirtualkeyboard \
        -skip qtwayland -skip qtwebchannel -skip qtwebengine \
        -skip qtwebglplugin -skip qtwebsockets -skip qtwebview \
        -skip qtwinextras -skip qtx11extras \
        -silent \
        && touch ${QT_SRC}/.configured
fi

echo ">>> 编译 Qt (-j${JOBS}) ..."
make -j${JOBS}
make install
touch ${QT_DONE}

echo ">>> Qt 编译完成"
echo "    宿主侧: ${QT_EXTPREFIX}  (CMake find_package 使用)"
echo "    目标侧: ${QT_PREFIX}     (编译 rootfs 时自动注入)"
}

# ===========================================
# 主调度
# ===========================================
case "${TARGET}" in
    all)
        build_app
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
    image)
        build_linux
        build_uboot
        build_app
        build_rootfs
        ;;
    qt)
        build_qt
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
        echo "  TARGET: all (默认), linux, uboot, app, linux+app, qt, rootfs, image, menuconfig_linux, menuconfig_uboot"
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
