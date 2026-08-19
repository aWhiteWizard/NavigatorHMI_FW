################################################################################
#
# qt6declarative
#
# B-4 移植: 迅为 SDK buildroot(2024.02) 无 Qt6 QML 模块, 按 buildroot 2024.08+
# 上游 qt6declarative 包结构手写移植, 版本对齐 qt6base 6.4.3
# 依赖: qt6shadertools (qsb, Quick 必需) + host-qt6declarative (qmltyperegistrar/qmlcachegen)
################################################################################

QT6DECLARATIVE_VERSION = $(QT6_VERSION)
QT6DECLARATIVE_SITE = $(QT6_SITE)
QT6DECLARATIVE_SOURCE = qtdeclarative-$(QT6_SOURCE_TARBALL_PREFIX)-$(QT6DECLARATIVE_VERSION).tar.xz
QT6DECLARATIVE_CPE_ID_VENDOR = qt
QT6DECLARATIVE_CPE_ID_PRODUCT = qt

QT6DECLARATIVE_CMAKE_BACKEND = ninja

QT6DECLARATIVE_LICENSE = \
	GPL-2.0+ or LGPL-3.0, \
	GPL-3.0 with exception (tools), \
	GFDL-1.3 (docs), \
	BSD-3-Clause, \
	LicenseRef-Qt-Commercial, \
	Qt-GPL-exception-1.0

QT6DECLARATIVE_LICENSE_FILES = \
	LICENSES/BSD-3-Clause.txt \
	LICENSES/GFDL-1.3-no-invariants-only.txt \
	LICENSES/GPL-2.0-only.txt \
	LICENSES/GPL-3.0-only.txt \
	LICENSES/LGPL-3.0-only.txt \
	LICENSES/LicenseRef-Qt-Commercial.txt \
	LICENSES/Qt-GPL-exception-1.0.txt

QT6DECLARATIVE_DEPENDENCIES = \
	host-qt6declarative \
	host-qt6shadertools \
	qt6base \
	qt6shadertools

QT6DECLARATIVE_INSTALL_STAGING = YES

# Qt6 QML/Quick: 依赖 qt6base + qt6shadertools (qsb); host 端 qmltyperegistrar/qmlcachegen 由 host-qt6declarative 提供
QT6DECLARATIVE_CONF_OPTS = \
	-DQT_HOST_PATH=$(HOST_DIR) \
	-DQT_BUILD_EXAMPLES=OFF \
	-DQT_BUILD_TESTS=OFF \
	-DBUILD_WITH_PCH=OFF \
	-DFEATURE_qml_profiler=OFF

# host 端: qmltyperegistrar/qmlcachegen/qmlprofiler 等工具 (qtdeclarative 模块内)
# 必须完整启用 tools: target 构建 find_package Qt6QmlTools 需要全部 host QML 工具
# qml_profiler=OFF: HMI 不需要 profiling, 且避免 host/target 工具不匹配
HOST_QT6DECLARATIVE_DEPENDENCIES = host-qt6base host-qt6shadertools
HOST_QT6DECLARATIVE_CONF_OPTS = \
	-DQT_BUILD_EXAMPLES=OFF \
	-DQT_BUILD_TESTS=OFF \
	-DBUILD_WITH_PCH=OFF \
	-DQT_BUILD_TOOLS=ON \
	-DFEATURE_qml_profiler=OFF

$(eval $(cmake-package))
$(eval $(host-cmake-package))
