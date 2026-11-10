# SPDX-License-Identifier: GPL-3.0-or-later
# CRT-Kate top-level build.
#
# Bridge mechanism note (Task 1 spike, 2026-11-09, Qt 6.10.3 / Fedora 43):
#   QWidgetTextureBridge is NOT present in this Qt build (verified: no
#   `*TextureBridge*` header under /usr/include/qt6). Available candidates
#   confirmed in headers: QRhiWidget (QtWidgets/qrhiwidget.h) and
#   QQuickRenderTarget (QtQuick/qquickrendertarget.h). The spike selects
#   between them; the spec-approved fallback is the FBO/QQuickRenderTarget
#   path.
cmake_minimum_required(VERSION 3.31)
project(crt-kate LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS Gui Widgets Qml Quick OpenGL ShaderTools)

enable_testing()

# Task 1 spike: proves a QWidget becomes a texture the QML scene can sample.
add_executable(crt-kate-spike
  spike/BridgeSpike.cpp
)
target_link_libraries(crt-kate-spike PRIVATE
  Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::OpenGL Qt6::ShaderTools
)
