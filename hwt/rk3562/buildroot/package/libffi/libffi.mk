################################################################################
#
# libffi
#
################################################################################

LIBFFI_VERSION = 3.4.4
LIBFFI_SITE = \
	https://github.com/libffi/libffi/releases/download/v$(LIBFFI_VERSION)
LIBFFI_LICENSE = MIT
LIBFFI_LICENSE_FILES = LICENSE
LIBFFI_CPE_ID_VALID = YES
LIBFFI_INSTALL_STAGING = YES
# We're patching Makefile.am
LIBFFI_AUTORECONF = YES

# The static exec trampolines is enabled by default since
# libffi 3.4.2. However it doesn't work with gobject-introspection.
# 2026-08-14 NavigatorHMI: 追加 --disable-builddir —— libffi 3.4.4 configure
# 默认启用 builddir 分离构建 (--enable-builddir 默认 yes), 在 Docker Desktop
# Windows 挂载卷上于子目录重跑 configure 时写 config.log 失败 (FUSE 限制),
# 且相对 srcdir 下可能递归 exec; 禁用后走常规 in-tree 配置 (与其他包一致)。
# 注意: host 包选项走 HOST_LIBFFI_CONF_OPTS (host-autotools-package)。
LIBFFI_CONF_OPTS = --disable-exec-static-tramp --disable-builddir
HOST_LIBFFI_CONF_OPTS = --disable-builddir

$(eval $(autotools-package))
$(eval $(host-autotools-package))
