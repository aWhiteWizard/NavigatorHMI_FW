#!/bin/bash
# P-6 OTA 全量 rootfs 树构建（2026-09-04 21:4x 用户裁决：全量打包设备无关，弃增量）：
# 从 buildroot target 收集 QtMultimedia + ffmpeg 5 库 + **全部 NEEDED 依赖闭包**
# （pulse 链 libpulse/libsndfile/libdbus/libpulsecommon、openssl libssl/libcrypto、libz、libdrm——
# 板端旧镜像这些库是 symlink 布局，但**不打增量假设**：一律打入，FW 安装器安全替换符号链接
# （otaupdater 2026-09-04 支持：备份链接 → 删链接 → 写常规文件，不跟随写））。
# 所有条目为**常规文件**（pack_fw collect_file_entries 拒绝符号链接）：实文件按 SONAME 命名展开
# （.so.X 由 .so.X.Y.Z 复制——ld 运行时按 SONAME 查找，实文件承载即可）。
# 用法：bash build-ota-tree.sh <buildroot-out> <输出树目录>
#   bash build-ota-tree.sh /sdk/buildroot/output/rk3562_navihmi /workspace/build/ota-tree
set -euo pipefail
OUT=${1:?buildroot output 目录必填}
TREE=${2:?输出树目录必填}
T=$OUT/target/usr
# qtmultimedia 手工编译产物在 staging（host/.../sysroot/usr）——libQt6MultimediaQuick 等不在 target
S=$OUT/host/aarch64-buildroot-linux-gnu/sysroot/usr
[ -d "$T/lib" ] || { echo "target/usr 不存在: $T"; exit 1; }

# rm 防呆（审查 🟡）：拒绝根目录级误删
[ "$TREE" != "/" ] || { echo "拒绝删除 /"; exit 1; }
[ "${TREE#/workspace/build/}" != "$TREE" ] || { echo "树目录须在 /workspace/build/ 下（防误删）: $TREE"; exit 1; }

rm -rf "$TREE"
mkdir -p "$TREE/usr/lib/pulseaudio" "$TREE/usr/qml/QtMultimedia" "$TREE/usr/plugins/multimedia"

# QtMultimedia（SONAME 实文件）
cp $T/lib/libQt6Multimedia.so.6.4.3 "$TREE/usr/lib/libQt6Multimedia.so.6"
# QtMultimediaQuick（qml 插件 libquickmultimediaplugin 的依赖——NEEDED 校验发现，staging 源）
cp $S/lib/libQt6MultimediaQuick.so.6.4.3 "$TREE/usr/lib/libQt6MultimediaQuick.so.6"
# ffmpeg 5 库（SONAME 实文件）
cp $T/lib/libavcodec.so.58.134.100   "$TREE/usr/lib/libavcodec.so.58"
cp $T/lib/libavformat.so.58.76.100   "$TREE/usr/lib/libavformat.so.58"
cp $T/lib/libswresample.so.3.9.100   "$TREE/usr/lib/libswresample.so.3"
cp $T/lib/libswscale.so.5.9.100      "$TREE/usr/lib/libswscale.so.5"
cp $T/lib/libavutil.so.56.70.100     "$TREE/usr/lib/libavutil.so.56"
# 依赖闭包：openssl3（libavformat NEEDED）+ pulse 链（libQt6Multimedia NEEDED libpulse.so.0；
# RUNPATH=/usr/lib/pulseaudio 找 libpulsecommon）+ libsndfile/libdbus + libz/libdrm
cp $T/lib/libssl.so.3    "$TREE/usr/lib/"
cp $T/lib/libcrypto.so.3 "$TREE/usr/lib/"
cp $T/lib/libpulse.so.0.24.3                "$TREE/usr/lib/libpulse.so.0"
cp $T/lib/pulseaudio/libpulsecommon-17.0.so "$TREE/usr/lib/pulseaudio/"
cp $T/lib/libsndfile.so.1.0.37              "$TREE/usr/lib/libsndfile.so.1"
cp $T/lib/libdbus-1.so.3.32.4               "$TREE/usr/lib/libdbus-1.so.3"
cp $T/lib/libz.so.1.3.1                     "$TREE/usr/lib/libz.so.1"
cp $T/lib/libdrm.so.2.124.0                 "$TREE/usr/lib/libdrm.so.2"
# QML 插件目录 + ffmpeg 后端插件
cp $T/qml/QtMultimedia/*                    "$TREE/usr/qml/QtMultimedia/"
# 2026-09-05 设备实测（QT_DEBUG_PLUGINS）：后端插件必须放 multimedia/**直接目录**——Qt factoryloader
# 扫 plugins/multimedia 时只认直接子文件 .so，不递归 multimedia/ffmpeg/ 子目录（官方 cmake 布局 ffmpeg/
# 子目录在 buildroot Qt 上不生效 → 「could not load multimedia backend」+ abort 崩溃黑屏）。直接 multimedia/
cp $T/plugins/multimedia/ffmpeg/libffmpegmediaplugin.so "$TREE/usr/plugins/multimedia/"

echo "=== 树内容（$TREE）==="
find "$TREE" -type f | sort
du -sh "$TREE"

# NEEDED 闭包校验（审查 🟡：树内 .so 的依赖 ⊆ 树 ∪ 板端白名单——板端系统/Qt 基础库不打包；
# buildroot 升级文件名漂移时提前失败而非打出缺依赖的包）
echo "=== NEEDED 校验 ==="
WHITELIST='^(libQt6(Core|Gui|Network|Qml|Quick|OpenGL|QmlModels)\.so|libmali\.so|libc\.so|libm\.so|libpthread\.so|libdl\.so|librt\.so|libgcc_s\.so|libstdc\+\+\.so|ld-linux|libasound\.so)'
MISSING=0
for so in $(find "$TREE" -name '*.so*' -type f); do
  for dep in $(readelf -d "$so" 2>/dev/null | grep NEEDED | sed 's/.*\[//; s/\]//'); do
    if echo "$dep" | grep -qE "$WHITELIST"; then continue; fi
    if [ ! -f "$TREE/usr/lib/$dep" ] && [ ! -f "$TREE/usr/lib/pulseaudio/$dep" ]; then
      echo "MISSING-DEP: $(basename $so) → $dep"
      MISSING=1
    fi
  done
done
[ "$MISSING" -eq 0 ] || { echo "!! 树内依赖缺失（见上）——补入树或核对白名单"; exit 1; }
echo "NEEDED 校验通过（全部依赖 ∈ 树 ∪ 板端白名单）"
echo "=== DONE（接 pack_fw.py --rootfs-files $TREE）==="
