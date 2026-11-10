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

find_package(Qt6 6.10 REQUIRED COMPONENTS Gui Widgets Qml Quick OpenGL ShaderTools Test)
# Task 5: Kate text editing (tabs, zoom, tools).
find_package(KF6TextEditor REQUIRED)

enable_testing()

# Task 1 spike: proves a QWidget becomes a texture the QML scene can sample.
add_executable(crt-kate-spike
  spike/BridgeSpike.cpp
)
target_link_libraries(crt-kate-spike PRIVATE
  Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::OpenGL Qt6::ShaderTools
)

# Task 3: Borland chrome shell (static lib so tests + app share it).
add_library(crt-shell STATIC
  src/shell/ShellWindow.cpp
  src/shell/ShellWindow.h
  src/shell/BorlandStyle.cpp
  src/shell/BorlandStyle.h
  src/shell/MenuBar.cpp
  src/shell/MenuBar.h
  src/shell/TabStrip.cpp
  src/shell/TabStrip.h
  src/shell/Minimap.cpp
  src/shell/Minimap.h
  src/shell/FKeyStatusBar.cpp
  src/shell/FKeyStatusBar.h
)
target_include_directories(crt-shell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(crt-shell PUBLIC Qt6::Gui Qt6::Widgets)
qt_add_resources(crt-shell "shell_qss"
    PREFIX "/"
    BASE src/shell
    FILES src/shell/borland.qss
)

# Task 3 test: ctest -R tst_chrome
add_executable(tst_chrome tests/tst_chrome.cpp)
target_link_libraries(tst_chrome PRIVATE crt-shell Qt6::Test Qt6::Widgets)
add_test(NAME tst_chrome COMMAND tst_chrome)

# Task 4: ContentBridge — renders ShellWindow into a frame the QML CRT scene
# samples (the spec's approved fallback path: QWidget::render -> frame/texture).
add_library(crt-bridge STATIC
  src/bridge/ContentBridge.cpp
  src/bridge/ContentBridge.h
)
target_include_directories(crt-bridge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(crt-bridge PUBLIC crt-shell Qt6::Gui Qt6::Quick)

# Task 4 test: ctest -R tst_bridge
add_executable(tst_bridge tests/tst_bridge.cpp)
target_link_libraries(tst_bridge PRIVATE crt-bridge Qt6::Test Qt6::Widgets)
add_test(NAME tst_bridge COMMAND tst_bridge)

# Input-forwarding test: ctest -R tst_input
add_executable(tst_input tests/tst_input.cpp)
target_link_libraries(tst_input PRIVATE crt-bridge Qt6::Test Qt6::Widgets Qt6::Quick)
add_test(NAME tst_input COMMAND tst_input)

# Task 5: KTextEditor integration — tabs, zoom, and text tools (case/sort).
add_library(crt-editor STATIC
  src/editor/EditorTabManager.cpp
  src/editor/EditorTabManager.h
)
target_include_directories(crt-editor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(crt-editor PUBLIC crt-shell Qt6::Widgets KF6::TextEditor)

# Task 5 test: ctest -R tst_zoomtools
add_executable(tst_zoomtools tests/tst_zoomtools.cpp)
target_link_libraries(tst_zoomtools PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_zoomtools COMMAND tst_zoomtools)

# Task 6: CRT settings (C++ model with QSettings persistence + dialog).
add_library(crt-settings STATIC
  src/settings/CrtSettings.cpp
  src/settings/CrtSettings.h
  src/settings/SettingsDialog.cpp
  src/settings/SettingsDialog.h
)
target_include_directories(crt-settings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(crt-settings PUBLIC Qt6::Gui Qt6::Widgets)

# Task 7: named session manager (JSON snapshot of open files + cursor).
add_library(crt-session STATIC
  src/session/SessionManager.cpp
  src/session/SessionManager.h
)
target_include_directories(crt-session PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(crt-session PUBLIC Qt6::Core)

# Task 8: autosave / crash recovery (periodic modified-buffer snapshots).
add_library(crt-autosave STATIC
  src/autosave/AutosaveService.cpp
  src/autosave/AutosaveService.h
)
target_include_directories(crt-autosave PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(crt-autosave PUBLIC Qt6::Core)

# Task 6 test: ctest -R tst_settings
add_executable(tst_settings tests/tst_settings.cpp)
target_link_libraries(tst_settings PRIVATE crt-settings Qt6::Test Qt6::Widgets)
add_test(NAME tst_settings COMMAND tst_settings)

# Task 7 test: ctest -R tst_session
add_executable(tst_session tests/tst_session.cpp)
target_link_libraries(tst_session PRIVATE crt-session Qt6::Test)
add_test(NAME tst_session COMMAND tst_session)

# Task 8 test: ctest -R tst_autosave
add_executable(tst_autosave tests/tst_autosave.cpp)
target_link_libraries(tst_autosave PRIVATE crt-autosave Qt6::Test)
add_test(NAME tst_autosave COMMAND tst_autosave)

# Task 9A test: ctest -R tst_minimap
add_executable(tst_minimap tests/tst_minimap.cpp)
target_link_libraries(tst_minimap PRIVATE crt-shell Qt6::Test)
add_test(NAME tst_minimap COMMAND tst_minimap)

# Task 9B test: ctest -R tst_menus
add_executable(tst_menus tests/tst_menus.cpp)
target_link_libraries(tst_menus PRIVATE crt-shell crt-bridge Qt6::Test Qt6::Widgets)
add_test(NAME tst_menus COMMAND tst_menus)

# Task 12: Turbo Vision grid proof (two-size palette/layout) - ctest -R tst_tv_grid
add_executable(tst_tv_grid tests/tst_tv_grid.cpp)
target_link_libraries(tst_tv_grid PRIVATE crt-shell crt-bridge Qt6::Test Qt6::Widgets)
add_test(NAME tst_tv_grid COMMAND tst_tv_grid)

# Task 12: Turbo Vision title-line tab strip (boxes, segments, click) - ctest -R tst_tabstrip
add_executable(tst_tabstrip tests/tst_tabstrip.cpp)
target_link_libraries(tst_tabstrip PRIVATE crt-shell Qt6::Test Qt6::Widgets)
add_test(NAME tst_tabstrip COMMAND tst_tabstrip)

# Task 12: Turbo Vision window frame (title-line boxes + L/R edges) - ctest -R tst_frame
add_executable(tst_frame tests/tst_frame.cpp)
target_link_libraries(tst_frame PRIVATE crt-shell Qt6::Test Qt6::Widgets)
add_test(NAME tst_frame COMMAND tst_frame)

# Task 12: Turbo Vision status-line F-key contract - ctest -R tst_fkeys
add_executable(tst_fkeys tests/tst_fkeys.cpp)
target_link_libraries(tst_fkeys PRIVATE crt-shell Qt6::Test Qt6::Widgets)
add_test(NAME tst_fkeys COMMAND tst_fkeys)

# Task 6: the app entry point (ShellWindow + ContentBridge + EffectChain QML).
add_executable(crt-kate src/main.cpp)
target_link_libraries(crt-kate PRIVATE
  crt-bridge crt-editor crt-settings crt-session crt-autosave Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::OpenGL Qt6::ShaderTools
)
target_compile_definitions(crt-kate PRIVATE
  CRT_KATE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

# Task 11: packaging - install layout + the CRT shader EMBEDDED in the binary.
# The AppImage must not depend on the source tree or on the test scratch dir
# (.superpowers/qmlbuild) that EffectChain.qml's default resolves against in
# dev runs; main.qml points EffectChain at the qrc copy (tests keep the default).
set(CRT_QSB_DIR ${CMAKE_CURRENT_BINARY_DIR}/qml/shaders)
set(CRT_QSB_OUT ${CRT_QSB_DIR}/crt_effect.frag.qsb)
add_custom_command(
  OUTPUT ${CRT_QSB_OUT}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CRT_QSB_DIR}
  COMMAND Qt6::qsb --glsl "100 es,120,150" --hlsl 50 --msl 12
          -o ${CRT_QSB_OUT}
          ${CMAKE_CURRENT_SOURCE_DIR}/third_party/crt/app/shaders/crt_effect.frag
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/crt/app/shaders/crt_effect.frag
  COMMENT "qsb: compiling crt_effect.frag"
)
add_custom_target(crt_shaders DEPENDS ${CRT_QSB_OUT})
add_dependencies(crt-kate crt_shaders)
qt_add_resources(crt-kate "crt_qsb_res"
  PREFIX "/crt-kate"
  BASE ${CMAKE_CURRENT_BINARY_DIR}/qml
  FILES ${CRT_QSB_OUT}
)

# Task 11: install targets (DESTDIR=staging cmake --install build).
install(TARGETS crt-kate RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/
        DESTINATION share/crt-kate/src
        FILES_MATCHING PATTERN "*.qml" PATTERN "qmldir")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/crt-kate.desktop
        DESTINATION share/applications)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/crt-kate.png
        DESTINATION share/icons/hicolor/256x256/apps)

# Task 12 scratch probe (not a ctest test): renders the real ShellWindow plus
# editor via ContentBridge and prints region colors. Build on demand with:
#   cmake --build build --target probe_shell && ./build/probe_shell
add_executable(probe_shell .relay/probe_shell.cpp)
target_link_libraries(probe_shell PRIVATE
  crt-bridge crt-editor Qt6::Gui Qt6::Widgets Qt6::Quick KF6::TextEditor)
target_compile_definitions(probe_shell PRIVATE
  CRT_KATE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# Task 12 scratch probe: renders SettingsDialog to verify the TV modal treatment.
add_executable(probe_settings .relay/probe_settings.cpp)
target_link_libraries(probe_settings PRIVATE crt-settings Qt6::Widgets)
