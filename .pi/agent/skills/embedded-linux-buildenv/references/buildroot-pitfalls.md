# Buildroot 集成与踩坑

以 Buildroot 2019.02.6 为验证实例。

## 坑 1：gconv 缺失 → Qt 报 iconv_open failed

**现象**：目标机运行 Qt 应用报
`QIconvCodec::convertToUnicode: using Latin-1 for conversion, iconv_open failed`

**根因**：Buildroot 内部工具链编译的 glibc 不会把 gconv 字符集模块（/usr/lib/gconv/）
装进 target，iconv_open 找不到字码表。

**诊断**：`tar tf rootfs.tar | grep -c gconv` 返回 0 即确认。

**修复**：post-build 脚本从 staging 补拷（Buildroot 会把 TARGET_DIR/STAGING_DIR 等
变量导出给 post-build 脚本）：

```sh
# post-build.sh（由 BR2_ROOTFS_POST_BUILD_SCRIPT 注入）
cp -rf ${STAGING_DIR}/usr/lib/gconv ${TARGET_DIR}/usr/lib/gconv
```

构建脚本里动态注入：`echo "BR2_ROOTFS_POST_BUILD_SCRIPT=\"/workspace/hwt/buildroot/post-build.sh\"" >> .config`

## 坑 2：defconfig 字符串值必须带引号

**现象**：defconfig 里 `BR2_TARGET_ROOTFS_EXT2_SIZE=256M`（无引号）不生效，
打出来的 ext2 始终是默认 60M，内容多时报
`Could not allocate block in ext2 filesystem`。

**根因**：Kconfig defconfig 格式中字符串值**必须带引号**：
`BR2_TARGET_ROOTFS_EXT2_SIZE="256M"`。无引号的字符串会被静默丢弃回退默认值。

**教训**：手写 defconfig 后，构建时捞 `.config` 核对关键值是否真正生效
（`docker cp <容器>:/path/.config` 或构建日志），别信 defconfig 表面。

## 坑 3：外部工具链配置静默失效

**现象**：defconfig 写了 `BR2_TOOLCHAIN_EXTERNAL=y` + PATH/PREFIX，但 rootfs 里的
glibc/libstdc++ 版本跟外部工具链对不上。

**根因**：缺 `BR2_TOOLCHAIN_EXTERNAL_CUSTOM=y`（PATH 依赖它），Kconfig 静默丢弃
路径配置并回退到内部工具链。

**诊断**：解包 rootfs.tar 看 `./lib/libc-*.so` 和 `libstdc++.so.6.0.XX` 版本号，
与外部工具链 sysroot 对比（libstdc++ 版本速查：gcc4.9=6.0.20、gcc7=6.0.24、gcc8=6.0.25）。

**教训**：defconfig 的注释可能误导，以 rootfs 实际内容为准。内部工具链若工作正常
（glibc 向前兼容），不必强行切外部。

## 坑 4：libstdc++ "no version information available"

**现象**：应用启动刷多条 `libstdc++.so.6: no version information available`。
**结论**：宿主编译器与 rootfs libstdc++ 版本不一致时的 ld 警告，**无害**，应用正常运行。

## DL 下载缓存

- `BR2_DL_DIR` 指向挂载卷目录，跨容器复用下载包
- 目录结构为按包名子目录（`dl/zlib/zlib-1.2.11.tar.gz`）
- 可预置手头的 tarballs 免下载，**但版本必须与 Buildroot 包版本完全一致**（会校验 .hash）
- 查 Buildroot 包版本：`grep -m1 '_VERSION' package/<pkg>/<pkg>.mk`（先解压源码到容器 /tmp 再查）
- 哈希不匹配会让构建报错而非自动重下，预置前务必核对

## 常用配置项

```
BR2_TARGET_ROOTFS_EXT2_SIZE=256M     # Qt+字体+软件包要扩容，默认 60M/120M 不够
BR2_TARGET_GENERIC_HOSTNAME="..."    # 主机名（与 overlay 的 etc/hostname 同步）
BR2_ROOTFS_OVERLAY="..."             # 动态注入（构建脚本追加到 .config）
BR2_ROOTFS_POST_BUILD_SCRIPT="..."   # 动态注入（同上）
```

## rootfs-overlay 注意

- overlay 里所有文本配置文件必须 **Unix LF 行尾**（CRLF 会导致 `login: rHMI` 这类乱码——getty 读到 \r 光标回行首）
- 大文件（字体等）别进 git，用 .gitignore + 构建时注入

## post-image 打包含 zImage/dtb/U-Boot 的技巧

外部编译的 kernel/U-Boot 注入 sdcard.img：U-Boot 用 `dd ... bs=1024 seek=1 conv=notrunc`
写到 1K 偏移；zImage/dtb 用 mtools 写进 img 内 FAT 分区
（`MTOOLS_SKIP_CHECK=1 mcopy -i sdcard.img@@8M zImage ::zImage`，@@后接分区偏移）。
