#!/bin/sh
# ============================================================
# Buildroot post-build 脚本（rootfs 打包前执行）
# 由 build-linux-uboot.sh 通过 BR2_ROOTFS_POST_BUILD_SCRIPT 动态注入
# 可用变量: TARGET_DIR STAGING_DIR HOST_DIR BINARIES_DIR
# ============================================================

# --- 注入 glibc gconv 模块（iconv 字码表，Qt 文本编码转换需要）---
# Buildroot 2019.02.6 内部工具链不会把 gconv 装入 target，
# 导致 Qt 报 "iconv_open failed"，从 staging 补拷
GCONV_SRC="${STAGING_DIR}/usr/lib/gconv"
GCONV_DST="${TARGET_DIR}/usr/lib/gconv"
if [ -d "${GCONV_SRC}" ]; then
    cp -rf "${GCONV_SRC}" "${GCONV_DST}"
    echo ">>> post-build: gconv 模块已注入 ($(ls ${GCONV_DST} | wc -l) 个文件)"
else
    echo ">>> post-build WARNING: 未找到 gconv 源目录 ${GCONV_SRC}"
fi

# --- 允许 root 通过 SSH 登录（开发便利）---
# 注意：root 默认无密码，首次串口登录后请执行 passwd 设置密码，再用 SSH 登录
SSHD_CONF="${TARGET_DIR}/etc/ssh/sshd_config"
if [ -f "${SSHD_CONF}" ]; then
    sed -i 's/^#*PermitRootLogin .*/PermitRootLogin yes/' "${SSHD_CONF}"
    echo ">>> post-build: sshd 已允许 root 登录"
fi
