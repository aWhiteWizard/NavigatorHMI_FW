################################################################################
#
# qt6shadertools
#
# B-4 移植: Qt6 Quick 依赖 (qsb 工具, qtdeclarative 编译必需)
################################################################################

QT6SHADERTOOLS_VERSION = $(QT6_VERSION)
QT6SHADERTOOLS_SITE = $(QT6_SITE)
QT6SHADERTOOLS_SOURCE = qtshadertools-$(QT6_SOURCE_TARBALL_PREFIX)-$(QT6SHADERTOOLS_VERSION).tar.xz
QT6SHADERTOOLS_CPE_ID_VENDOR = qt
QT6SHADERTOOLS_CPE_ID_PRODUCT = qt

QT6SHADERTOOLS_CMAKE_BACKEND = ninja

QT6SHADERTOOLS_LICENSE = \
	GPL-2.0+ or LGPL-3.0, \
	GPL-3.0 with exception (tools), \
	GFDL-1.3 (docs), \
	BSD-3-Clause, \
	LicenseRef-Qt-Commercial, \
	Qt-GPL-exception-1.0

QT6SHADERTOOLS_LICENSE_FILES = \
	LICENSES/BSD-3-Clause.txt \
	LICENSES/GFDL-1.3-no-invariants-only.txt \
	LICENSES/GPL-2.0-only.txt \
	LICENSES/GPL-3.0-only.txt \
	LICENSES/LGPL-3.0-only.txt \
	LICENSES/LicenseRef-Qt-Commercial.txt \
	LICENSES/Qt-GPL-exception-1.0.txt

QT6SHADERTOOLS_DEPENDENCIES = \
	host-qt6shadertools \
	host-qt6base \
	qt6base

QT6SHADERTOOLS_INSTALL_STAGING = YES

HOST_QT6SHADERTOOLS_DEPENDENCIES = host-qt6base

# qsb 工具 + Qt6ShaderTools 库 (host 端工具供 qtdeclarative 编译用)
# 必须 QT_BUILD_TOOLS=ON: qsb 由 tools/qsb 生成, 默认可能被跳过
HOST_QT6SHADERTOOLS_CONF_OPTS = \
	-DQT_BUILD_EXAMPLES=OFF \
	-DQT_BUILD_TESTS=OFF \
	-DBUILD_WITH_PCH=OFF \
	-DQT_BUILD_TOOLS=ON

$(eval $(cmake-package))
$(eval $(host-cmake-package))
