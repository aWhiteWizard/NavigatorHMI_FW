################################################################################
#
# qtvirtualkeyboard
#
# D 循环移植: 迅为 SDK buildroot(2024.02) 无 Qt6 VirtualKeyboard, 按 qt6declarative
# 移植先例手写, 版本对齐 qt6base 6.4.3。
# 依赖: qt6base + qt6declarative (Qml/Quick) + qt6svg (Svg, 必需否则 skip build)。
# 内置输入法: pinyin(拼音, 3rdparty 词库 dict_pinyin.dat/rawdict_utf16_65105_freq.txt)。
################################################################################

QT6VIRTUALKEYBOARD_VERSION = $(QT6_VERSION)
QT6VIRTUALKEYBOARD_SITE = $(QT6_SITE)
QT6VIRTUALKEYBOARD_SOURCE = qtvirtualkeyboard-$(QT6_SOURCE_TARBALL_PREFIX)-$(QT6VIRTUALKEYBOARD_VERSION).tar.xz
QT6VIRTUALKEYBOARD_CPE_ID_VENDOR = qt
QT6VIRTUALKEYBOARD_CPE_ID_PRODUCT = qt

QT6VIRTUALKEYBOARD_CMAKE_BACKEND = ninja

QT6VIRTUALKEYBOARD_LICENSE = \
	GPL-2.0+ or LGPL-3.0, \
	GPL-3.0 with exception, \
	GFDL-1.3 (docs), \
	BSD-3-Clause, \
	LicenseRef-Qt-Commercial, \
	Qt-GPL-exception-1.0

QT6VIRTUALKEYBOARD_LICENSE_FILES = \
	LICENSES/GFDL-1.3-no-invariants-only.txt \
	LICENSES/GPL-3.0-only.txt \
	LICENSES/LicenseRef-Qt-Commercial.txt \
	LICENSES/Qt-GPL-exception-1.0.txt

QT6VIRTUALKEYBOARD_DEPENDENCIES = \
	qt6base \
	qt6declarative \
	qt6svg

QT6VIRTUALKEYBOARD_INSTALL_STAGING = YES

# QtVirtualKeyboard: 依赖 Quick/Svg (qt6declarative/qt6svg 提供);
# 内置 pinyin 拼音输入法 + 词库 (3rdparty, 默认启用);
# 关闭示例/测试/文档; 关闭商业输入法 (cerence/myscript 需商业库)
QT6VIRTUALKEYBOARD_CONF_OPTS = \
	-DQT_HOST_PATH=$(HOST_DIR) \
	-DQT_BUILD_EXAMPLES=OFF \
	-DQT_BUILD_TESTS=OFF \
	-DBUILD_WITH_PCH=OFF \
	-DFEATURE_vkb_cerence=OFF \
	-DFEATURE_vkb_myscript=OFF

$(eval $(cmake-package))
