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
SSHD_CONF="${TARGET_DIR}/etc/ssh/sshd_config"
if [ -f "${SSHD_CONF}" ]; then
    sed -i 's/^#*PermitRootLogin .*/PermitRootLogin yes/' "${SSHD_CONF}"
    echo ">>> post-build: sshd 已允许 root 登录"
fi

# --- 预置 root 密码 + 修复 shadow 过期死循环 ---
# Buildroot shadow 第三字段(密码最后修改日) = build 时的 epoch 天数，
# 但板子 RTC=0(1970-01-01)，首次 passwd 后该字段被重置为 0，
# SSH 判定密码过期 → 强制改密码但 busybox passwd 不改该字段 → 死循环。
# 修复：预置密码哈希(root/root) + 第三字段=build 日天数，永不过期。
# 部署后: ssh root@板子, 密码 root, 登录后 passwd 改成自己的。
SHADOW_FILE="${TARGET_DIR}/etc/shadow"
if [ -f "${SHADOW_FILE}" ] && command -v python >/dev/null 2>&1; then
    ROOT_HASH=$(python -c "import crypt; print(crypt.crypt('root', '\$1\$ab\$'))")
    SHADOW_DAYS=$(( $(date +%s) / 86400 ))
    if [ -n "${ROOT_HASH}" ]; then
        awk -F: -v hash="${ROOT_HASH}" -v days="${SHADOW_DAYS}" \
            'BEGIN{OFS=":"} $1=="root"{$2=hash; $3=days; $4=0; $5=99999; $6=7; $7=""; $8=""; $9=""}1' \
            "${SHADOW_FILE}" > "${SHADOW_FILE}.new"
        mv "${SHADOW_FILE}.new" "${SHADOW_FILE}"
        echo ">>> post-build: root 密码已预置 (root/root)，永不过期"
    fi
fi
