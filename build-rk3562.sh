#!/bin/bash
# ============================================================
# NavigatorHMI RK3562 编译脚本 (容器内执行)
# 挂载约定: SDK -> /sdk, 工程 -> /workspace
# 用法: build-rk3562.sh [JOBS] [TARGET]
#   TARGET: all (默认) | buildroot | kernel(linux) | uboot | qt | rootfs | image
#   JOBS: 并行线程数 (默认 8)
# 说明 (2026-08-14):
#   - 工具链直接用 SDK 自带 prebuilts (aarch64-none-linux-gnu, gcc 10.3)
#   - Buildroot 用 rockchip_rk3562_defconfig, DL 缓存 /sdk/buildroot/dl
#   - Qt: V1.0 用 SDK 内置 Qt 6.4.3 (buildroot 产出 host/bin/qmake 交叉编译)
#   - hwt 覆盖: /workspace/hwt/rk3562/{kernel,uboot,buildroot} -> SDK 对应目录
#     其中 .config 存在时直接使用 (menuconfig 保存的配置), 否则用板级 defconfig
# ============================================================
set -e

JOBS=${1:-8}
TARGET=${2:-all}

# 容器网络容错: 本机网络经代理(TUN)做 TLS 中间人, 容器内证书不受信,
# git/wget 关闭证书验证 (buildroot 联网下载源码包时需要)
export GIT_SSL_NO_VERIFY=1

SDK=/sdk
OUT=/workspace/build/rk3562
HWT=/workspace/hwt/rk3562

# 交叉编译器 (SDK 自带, 相对路径由 buildroot defconfig 自动解析)
CROSS_AARCH64=${SDK}/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-

# Buildroot 输出目录 (Rockchip buildroot: O=output/<defconfig 名>)
BR_OUT=${SDK}/buildroot/output/rk3562_navihmi
if [ ! -d ${BR_OUT} ]; then
    BR_OUT=${SDK}/buildroot/output/rockchip_rk3562
fi

echo "========================================="
echo "  RK3562 Build: TARGET=${TARGET} JOBS=${JOBS}"
echo "  SDK: ${SDK}"
echo "  OUT: ${OUT}"
echo "========================================="

mkdir -p ${OUT}/kernel ${OUT}/uboot ${OUT}/rootfs

# ============================================================
# 修复 Windows 解压丢失的 dt-bindings include
# (SDK 在 Windows 解压时符号链接被降级/丢失; Docker Desktop 挂载上
#  符号链接跨容器不稳定, 故用真实目录复制 include/dt-bindings 头文件,
#  体积小(几百 KB)且稳定)
#   1) arch/arm64/boot/dts/include/dt-bindings (引号 include "dt-bindings/...")
#   2) scripts/dtc/include-prefixes/dt-bindings (尖括号 include <dt-bindings/...>)
# ============================================================
fix_symlinks() {
    local SRC_DT_BINDINGS=${SDK}/kernel-6.1/include/dt-bindings

    # 迅为 6.1 包遗漏 dt-bindings/input/linux-event-codes.h (上游 6.1 有);
    # 用 uapi/linux/input-event-codes.h 副本补齐 (纯宏定义, 零 include 依赖,
    # 文件注释明确为 dts 设计)
    if [ ! -e ${SRC_DT_BINDINGS}/input/linux-event-codes.h ]; then
        echo ">>> 补 linux-event-codes.h (迅为包遗漏) ..."
        cp ${SDK}/kernel-6.1/include/uapi/linux/input-event-codes.h \
           ${SRC_DT_BINDINGS}/input/linux-event-codes.h
    fi

    local DTS_INC=${SDK}/kernel-6.1/arch/arm64/boot/dts/include
    if [ -L ${DTS_INC}/dt-bindings ] || [ ! -e ${DTS_INC}/dt-bindings/input/linux-event-codes.h ]; then
        echo ">>> 修复内核 dts include (复制 dt-bindings) ..."
        rm -rf ${DTS_INC}/dt-bindings
        mkdir -p ${DTS_INC}
        cp -r ${SRC_DT_BINDINGS} ${DTS_INC}/dt-bindings
    fi

    local INC_PREFIX=${SDK}/kernel-6.1/scripts/dtc/include-prefixes
    if [ -L ${INC_PREFIX} ] || [ ! -e ${INC_PREFIX}/dt-bindings/input/linux-event-codes.h ]; then
        echo ">>> 修复 scripts/dtc/include-prefixes (复制) ..."
        rm -rf ${INC_PREFIX}
        mkdir -p ${INC_PREFIX}
        cp -r ${SRC_DT_BINDINGS} ${INC_PREFIX}/dt-bindings
    fi

    # dt-bindings/display/media-bus-format.h 用相对路径 include uapi 头文件
    # (#include "../../uapi/linux/media-bus-format.h"), 复制品在 include-prefixes 下
    # 需要 include-prefixes/uapi/linux/media-bus-format.h 支撑
    local MBF=${INC_PREFIX}/uapi/linux/media-bus-format.h
    if [ ! -e ${MBF} ]; then
        echo ">>> 补 include-prefixes/uapi (media-bus-format 相对 include) ..."
        mkdir -p ${INC_PREFIX}/uapi/linux
        cp ${SDK}/kernel-6.1/include/uapi/linux/media-bus-format.h ${MBF}
    fi

    # 工具链 LTO 插件符号链接 (Windows 解压丢失 liblto_plugin.so, gcc LTO 需要)
    local TC_LIBEXEC=${SDK}/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/libexec/gcc/aarch64-none-linux-gnu/10.3.1
    if [ -f ${TC_LIBEXEC}/liblto_plugin.so.0.0.0 ] && [ ! -e ${TC_LIBEXEC}/liblto_plugin.so ]; then
        echo ">>> 修复工具链 liblto_plugin.so 符号链接 ..."
        ln -s liblto_plugin.so.0.0.0 ${TC_LIBEXEC}/liblto_plugin.so
    fi

    # rockchip-mali grabber.sh: 预编译库 optimize_N 目录缺变体 (如 optimize_s 只有 4 个
    # g52 文件, 缺 g24p0-gbm), 回退到完整 lib/ 目录; 包是 rsync 本地源模式 (external/libmali)
    # 不走 apply-patches, 故在此直接修源 (幂等: 已含 lib 回退则跳过)
    local GRABBER=${SDK}/external/libmali/scripts/grabber.sh
    if [ -f ${GRABBER} ] && ! grep -q 'find lib/${ARCH}' ${GRABBER}; then
        echo ">>> 修复 rockchip-mali grabber.sh (lib 回退) ..."
        sed -i '31a if [ -z "$LIBS" ]; then LIBS=$(find lib/${ARCH}* -regex ".*${LIB}.so" 2>/dev/null); fi' ${GRABBER}
    fi
}

# ============================================================
# 应用 hwt 覆盖 (只读覆盖 -> SDK 源码目录)
# ============================================================
apply_hwt() {
    local src_dir=$1
    local dst_dir=$2
    if [ -d "${HWT}/${src_dir}" ] && [ "$(ls -A ${HWT}/${src_dir} 2>/dev/null)" ]; then
        echo ">>> 应用 hwt/${src_dir} 覆盖到 ${dst_dir} ..."
        for item in ${HWT}/${src_dir}/*; do
            cp -rf "$item" "${dst_dir}/"
        done
    fi
}

# ============================================================
# 1. Buildroot (rootfs, 含 Qt 6.4.3)
# ============================================================
build_buildroot() {
    echo ""
    echo "========================================="
    echo "  [Buildroot] rockchip_rk3562_defconfig"
    echo "========================================="
    apply_hwt buildroot ${SDK}/buildroot

    cd ${SDK}/buildroot
    if [ -f .config ]; then
        echo ">>> 使用已有 .config (hwt/menuconfig 保存的配置)"
    elif [ -f configs/rk3562_navihmi_defconfig ]; then
        # NavigatorHMI 定制配置 (hwt 提供): 去 chromium/Qt5/weston, 启 Qt 6.4.3
        echo ">>> 使用 NavigatorHMI 定制配置: rk3562_navihmi_defconfig"
        make rk3562_navihmi_defconfig
    else
        make rockchip_rk3562_defconfig
    fi

    echo ">>> Buildroot make -j${JOBS} ... (首次约 1~2 小时)"
    make BR2_WGET="wget --no-check-certificate" -j${JOBS}

    echo ">>> 复制 Buildroot 产物 ..."
    if [ -f ${BR_OUT}/images/rootfs.ext4 ]; then
        cp ${BR_OUT}/images/rootfs.ext4 ${OUT}/rootfs/
    fi
    if [ -f ${BR_OUT}/images/rootfs.img ]; then
        cp ${BR_OUT}/images/rootfs.img ${OUT}/rootfs/
    fi

    # B-4: GLES/EGL CMake 补丁注入 staging (libmali 提供库但无 CMake config,
    # Qt6Gui/Quick 交叉编译 find_package(EGL/GLESv2) 需要; 全新环境可复现)
    local SYSROOT=${BR_OUT}/host/aarch64-buildroot-linux-gnu/sysroot
    if [ -d "${HWT}/buildroot/cmake/EGL" ]; then
        echo ">>> 注入 GLES/EGL CMake 补丁到 staging ..."
        mkdir -p ${SYSROOT}/usr/lib/cmake/EGL ${SYSROOT}/usr/lib/cmake/GLESv2 ${SYSROOT}/usr/lib/cmake/Qt6
        cp -f ${HWT}/buildroot/cmake/EGL/EGLConfig.cmake ${SYSROOT}/usr/lib/cmake/EGL/
        cp -f ${HWT}/buildroot/cmake/GLESv2/GLESv2Config.cmake ${SYSROOT}/usr/lib/cmake/GLESv2/
        cp -f ${HWT}/buildroot/cmake/Qt6/FindEGL.cmake ${SYSROOT}/usr/lib/cmake/Qt6/
        cp -f ${HWT}/buildroot/cmake/Qt6/FindGLESv2.cmake ${SYSROOT}/usr/lib/cmake/Qt6/
    fi

    echo ">>> Buildroot 完成"
}

# ============================================================
# 2. Linux Kernel (6.1, aarch64)
# ============================================================
build_kernel() {
    echo ""
    echo "========================================="
    echo "  [Kernel] rockchip_linux_defconfig"
    echo "========================================="
    fix_symlinks
    apply_hwt kernel ${SDK}/kernel-6.1

    cd ${SDK}/kernel-6.1
    if [ -f .config ]; then
        echo ">>> 使用已有 .config (hwt/menuconfig 保存的配置)"
    else
        make ARCH=arm64 rockchip_linux_defconfig
    fi

    echo ">>> Kernel make Image dtbs modules -j${JOBS} ..."
    make ARCH=arm64 CROSS_COMPILE=${CROSS_AARCH64} -j${JOBS} Image dtbs modules

    cp arch/arm64/boot/Image ${OUT}/kernel/
    cp arch/arm64/boot/dts/rockchip/*.dtb ${OUT}/kernel/ 2>/dev/null || true
    echo ">>> Kernel 完成: ${OUT}/kernel/Image"
}

# ============================================================
# 3. U-Boot
# ============================================================
build_uboot() {
    echo ""
    echo "========================================="
    echo "  [U-Boot] rk3562_topeet_defconfig"
    echo "========================================="
    apply_hwt uboot ${SDK}/u-boot

    cd ${SDK}/u-boot
    if [ -f .config ]; then
        echo ">>> 使用已有 .config (hwt/menuconfig 保存的配置)"
    else
        make rk3562_topeet_defconfig
    fi

    echo ">>> U-Boot make -j${JOBS} ..."
    make CROSS_COMPILE=${CROSS_AARCH64} -j${JOBS}

    if [ -f u-boot.bin ]; then
        cp u-boot.bin ${OUT}/uboot/
    fi
    if [ -f u-boot-rockchip.bin ]; then
        cp u-boot-rockchip.bin ${OUT}/uboot/
    fi
    if [ -d spl ]; then
        cp -rf spl ${OUT}/uboot/ 2>/dev/null || true
    fi
    echo ">>> U-Boot 完成: ${OUT}/uboot/u-boot.bin"
}

# ============================================================
# 4. Qt 应用 (V1.0: SDK 内置 Qt 6.4.3, buildroot 产出 qmake)
# ============================================================
build_qt() {
    echo ""
    echo "========================================="
    echo "  [Qt] SDK 内置 Qt 6.4.3 交叉编译应用"
    echo "========================================="
    QMAKE=${BR_OUT}/host/bin/qmake
    if [ ! -f ${QMAKE} ]; then
        echo "错误: 未找到 ${QMAKE}, 请先编译 Buildroot (Target=buildroot)" >&2
        exit 1
    fi
    if [ ! -d /workspace/src ]; then
        echo "错误: 未找到 /workspace/src (Qt 工程源码), 暂无可编译应用" >&2
        exit 1
    fi

    cd /workspace
    mkdir -p build/rk3562/app
    cd build/rk3562/app
    ${QMAKE} /workspace/src/NavigatorHMI_FW.pro
    make -j${JOBS}
    echo ">>> Qt 应用完成: /workspace/build/rk3562/app/"
}

# ============================================================
# 5. rootfs / image (占位: V1.0 首版联调暂不打包完整烧录镜像)
# ============================================================
build_rootfs() {
    echo ">>> rootfs: 需先完成 Buildroot, 产物见 ${OUT}/rootfs/ (由 build_buildroot 复制)"
}

build_image() {
    echo ">>> image: V1.0 首版暂不打包完整烧录镜像 (SD 卡/U盘烧录走迅为工具)"
}

# ============================================================
# 目标分发 (先统一修复 SDK 符号链接/遗漏头文件, 再分发)
# ============================================================
fix_symlinks

case "${TARGET}" in
    buildroot)  build_buildroot ;;
    kernel|linux) build_kernel ;;
    uboot)      build_uboot ;;
    qt)         build_qt ;;
    rootfs)     build_rootfs ;;
    image)      build_image ;;
    all)
        build_buildroot
        build_kernel
        build_uboot
        ;;
    menuconfig_kernel|menuconfig_uboot|menuconfig_buildroot)
        # 交给下方 menuconfig 分发处理
        ;;
    *)
        echo "错误: 未知 TARGET=${TARGET}" >&2
        exit 1
        ;;
esac

# ============================================================
# Menuconfig 模式 (由 docker-build-rk3562.ps1 -Menuconfig 调用)
# ============================================================
case "${TARGET}" in
    menuconfig_kernel)
        apply_hwt kernel ${SDK}/kernel-6.1
        cd ${SDK}/kernel-6.1
        [ -f .config ] || make ARCH=arm64 rockchip_linux_defconfig
        make ARCH=arm64 CROSS_COMPILE=${CROSS_AARCH64} menuconfig
        mkdir -p ${HWT}/kernel
        cp .config ${HWT}/kernel/.config
        echo ">>> kernel 配置已保存: ${HWT}/kernel/.config"
        ;;
    menuconfig_uboot)
        apply_hwt uboot ${SDK}/u-boot
        cd ${SDK}/u-boot
        [ -f .config ] || make rk3562_topeet_defconfig
        make CROSS_COMPILE=${CROSS_AARCH64} menuconfig
        mkdir -p ${HWT}/uboot
        cp .config ${HWT}/uboot/.config
        echo ">>> uboot 配置已保存: ${HWT}/uboot/.config"
        ;;
    menuconfig_buildroot)
        apply_hwt buildroot ${SDK}/buildroot
        cd ${SDK}/buildroot
        [ -f .config ] || make rockchip_rk3562_defconfig
        make menuconfig
        mkdir -p ${HWT}/buildroot
        cp .config ${HWT}/buildroot/.config
        echo ">>> buildroot 配置已保存: ${HWT}/buildroot/.config"
        ;;
esac

echo ""
echo "========================================="
echo "  OK 全部完成! 产物: ${OUT}"
echo "========================================="
