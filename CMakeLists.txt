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
project(turbo-kate LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS Gui Widgets Qml Quick OpenGL ShaderTools Test)
# Task 5: Kate text editing (tabs, zoom, tools).
find_package(KF6TextEditor REQUIRED)
# Status-line selectors: the spellcheck-language indicator reads the dictionary
# list from Sonnet (installed in the crt-kate distrobox as kf6-sonnet 6.30.0).
# The package is KF6Sonnet; it provides the KF6::SonnetCore target.
find_package(KF6Sonnet REQUIRED)

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
  src/shell/EditorPane.cpp
  src/shell/EditorPane.h
  src/shell/BorlandStyle.cpp
    src/shell/BorlandMenu.cpp
  src/shell/BorlandStyle.h
  src/shell/MenuBar.cpp
  src/shell/MenuBar.h
  src/shell/TabStrip.cpp
  src/shell/TabStrip.h
  src/shell/TabListMenu.cpp
  src/shell/TabListMenu.h
  src/shell/Minimap.cpp
  src/shell/Minimap.h
  src/shell/FKeyStatusBar.cpp
  src/shell/FKeyStatusBar.h
  src/shell/TextScrollBar.cpp
  src/shell/TextScrollBar.h
  src/shell/RightScrollBar.cpp
  src/shell/RightScrollBar.h
  src/shell/BottomBand.cpp
  src/shell/BottomBand.h
  src/shell/TvFileDock.cpp
  src/shell/TvLineEdit.cpp
  src/shell/TvModal.cpp
  src/shell/TvModal.h
  src/shell/FieldCell.cpp
  src/shell/FieldCell.h
  src/shell/TvCheckBox.cpp
  src/shell/TvCheckBox.h
  src/shell/TvSpinBox.cpp
  src/shell/TvSpinBox.h
  src/shell/TvTabBar.cpp
  src/shell/TvTabBar.h
  src/shell/TvPlate.cpp
  src/shell/TvPlate.h
  src/shell/AboutModal.cpp
  src/shell/AboutModal.h
  src/shell/TvFileDock.h
  src/shell/TvLineEdit.h
  src/shell/RecentFilesStore.cpp
  src/shell/RecentFilesStore.h
  src/shell/ScrollBarInstaller.cpp
  src/shell/ScrollBarInstaller.h
  fonts/fonts.qrc
)
target_include_directories(crt-shell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(crt-shell PUBLIC Qt6::Gui Qt6::Widgets)
target_link_libraries(crt-shell PUBLIC KF6::SonnetCore)
qt_add_resources(crt-shell "shell_qss"
    PREFIX "/"
    BASE src/shell
    FILES src/shell/borland.qss
)

# Turbo Vision default typeface (PxPlus IBM VGA8 - a VGA 8x16 CP437 bitmap
# font). Compiled from fonts/fonts.qrc via CMAKE_AUTORCC (a hand-written .qrc
# keeps the alias under our control): prefix "/" + alias "fonts/..." ->
# :/fonts/PxPlus_IBM_VGA8.ttf, loaded in ShellWindow. Also installed to
# share/fonts below so fontconfig resolves the family in the AppImage
# (AppRun points XDG_DATA_DIRS at $APPDIR/usr/share).
set(CRT_FONT_TTF ${CMAKE_CURRENT_SOURCE_DIR}/fonts/PxPlus_IBM_VGA8.ttf)

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
target_link_libraries(tst_bridge PRIVATE crt-bridge crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_bridge COMMAND tst_bridge)

# Input-forwarding test: ctest -R tst_input
add_executable(tst_input tests/tst_input.cpp)
# The arrow/backspace regression tests drive a REAL KTextEditor view through
# the bridge (crt-editor), because the defect lives in how a Kate action is
# reached, not in the event's arrival.
target_link_libraries(tst_input PRIVATE crt-bridge crt-shell crt-editor
                      Qt6::Test Qt6::Widgets Qt6::Quick KF6::TextEditor)
add_test(NAME tst_input COMMAND tst_input)

# Task 5: KTextEditor integration — tabs, zoom, and text tools (case/sort).
add_library(crt-editor STATIC
  src/editor/EditorTabManager.cpp
  src/editor/EditorTabManager.h
  src/editor/FindReplace.cpp
  src/editor/FindReplace.h
  src/editor/FindReplaceDialog.cpp
  src/editor/FindReplaceDialog.h
  src/editor/LineAffixDialog.cpp
  src/editor/LineAffixDialog.h
  src/editor/GoToLineDialog.cpp
  src/editor/GoToLineDialog.h
  src/editor/FileChangeTracker.cpp
  src/editor/FileChangeTracker.h
  src/editor/ReloadDialog.cpp
  src/editor/ReloadDialog.h
  src/editor/StatusSelectors.cpp
  src/editor/StatusSelectors.h
  src/editor/BracketColorizer.cpp
  src/editor/BracketColorizer.h
  src/editor/StringColorizer.cpp
  src/editor/BlockFinder.cpp
  src/editor/FoldIndicator.cpp
  src/editor/StringColorizer.h
  src/editor/ColourWiring.cpp
  src/editor/ColourWiring.h
  src/editor/tvtheme/tvtheme.qrc
)
target_include_directories(crt-editor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(crt-editor PUBLIC crt-shell Qt6::Widgets KF6::TextEditor)

# Task 5 test: ctest -R tst_zoomtools
add_executable(tst_zoomtools tests/tst_zoomtools.cpp)
target_link_libraries(tst_zoomtools PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_zoomtools COMMAND tst_zoomtools)

# 2026-11-19 (second round): View > Go.
add_executable(tst_gotoline tests/tst_gotoline.cpp)
target_link_libraries(tst_gotoline PRIVATE crt-editor crt-bridge Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_gotoline COMMAND tst_gotoline)

# 2026-11-20: Edit > Select All / Deselect All.
add_executable(tst_selectall tests/tst_selectall.cpp)
target_link_libraries(tst_selectall PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_selectall COMMAND tst_selectall)

# 2026-11-20: View > Show / Hide Nonprintable Characters.
add_executable(tst_nonprintable tests/tst_nonprintable.cpp)
target_link_libraries(tst_nonprintable PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_nonprintable COMMAND tst_nonprintable)

# 2026-11-20: Coloured Brackets - the pure pair-matching/colour logic.
add_executable(tst_brackets tests/tst_brackets.cpp)
target_link_libraries(tst_brackets PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_brackets COMMAND tst_brackets)

# 2026-11-20: Coloured Brackets through the renderer (pixel proof).
add_executable(tst_bracketrender tests/tst_bracketrender.cpp)
target_link_libraries(tst_bracketrender PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_bracketrender COMMAND tst_bracketrender)

# 2026-11-20: string colouring independent of the syntax mode (pure scanning).
add_executable(tst_strings tests/tst_strings.cpp)
target_link_libraries(tst_strings PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_strings COMMAND tst_strings)

# 2026-11-20: the folding ("nesting") menu - Collapse All / Expand All / Toggle.
add_executable(tst_folding tests/tst_folding.cpp)
target_link_libraries(tst_folding PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_folding COMMAND tst_folding)

# 2026-11-20: session restore must replace the empty placeholder tab.
add_executable(tst_restore tests/tst_restore.cpp)
target_link_libraries(tst_restore PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_restore COMMAND tst_restore)

# 2026-11-20: single-quoted text must be #40A769 in code files too (the theme's
# Char style; a MovingRange cannot recolour text the highlighter owns).
add_executable(tst_theme tests/tst_theme.cpp)
target_link_libraries(tst_theme PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
target_compile_definitions(tst_theme PRIVATE
  CRT_KATE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME tst_theme COMMAND tst_theme)

# 2026-11-20: the block matcher behind the context menu's Collapse/Expand Block.
add_executable(tst_blockfinder tests/tst_blockfinder.cpp)
target_link_libraries(tst_blockfinder PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_blockfinder COMMAND tst_blockfinder)

# 2026-11-20: the pure foldable-line computation behind the gutter fold marker.
add_executable(tst_foldindicator tests/tst_foldindicator.cpp)
target_link_libraries(tst_foldindicator PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_foldindicator COMMAND tst_foldindicator)

# 2026-11-20: the block commands + context-menu actions.
add_executable(tst_blockmenu tests/tst_blockmenu.cpp)
target_link_libraries(tst_blockmenu PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_blockmenu COMMAND tst_blockmenu)

add_executable(tst_ctxblock tests/tst_ctxblock.cpp)
target_link_libraries(tst_ctxblock PRIVATE crt-editor crt-shell Qt6::Test Qt6::Widgets)
add_test(NAME tst_ctxblock COMMAND tst_ctxblock)

add_executable(tst_blockmarkers tests/tst_blockmarkers.cpp)
target_link_libraries(tst_blockmarkers PRIVATE crt-editor crt-shell Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_blockmarkers COMMAND tst_blockmarkers)

# 2026-11-20: pure file-change decision logic for the external-change prompt.
add_executable(tst_filechange tests/tst_filechange.cpp)
target_link_libraries(tst_filechange PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_filechange COMMAND tst_filechange)

# 2026-11-20: the reload/keep modal itself.
add_executable(tst_reloaddlg tests/tst_reloaddlg.cpp)
target_link_libraries(tst_reloaddlg PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_reloaddlg COMMAND tst_reloaddlg)

# 2026-11-21: split view (EditorPane / EditorSplitter / two views).
add_executable(tst_splitview tests/tst_splitview.cpp)
target_link_libraries(tst_splitview PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_splitview COMMAND tst_splitview)

# Dev aid (not a gate suite): dumps PNGs of the reload dialog and the file dock
# so their chrome can be compared side by side. Build: agi_dialogshots.
add_executable(agi_dialogshots .relay/agi_shots.cpp)
target_link_libraries(agi_dialogshots PRIVATE crt-editor Qt6::Widgets KF6::TextEditor)

# 2026-11-20: the external-change watcher wiring (real files, real watcher).
add_executable(tst_reload tests/tst_reload.cpp)
target_link_libraries(tst_reload PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_reload COMMAND tst_reload)

# 2026-11-19: Edit-menu line tools + Word Wrap.
add_executable(tst_linetools tests/tst_linetools.cpp)
target_link_libraries(tst_linetools PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_linetools COMMAND tst_linetools)

# TV-revision Task 1 test: flat blue field, yellow text, lavender selection.
add_executable(tst_tveditor tests/tst_tveditor.cpp)
target_link_libraries(tst_tveditor PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_tveditor COMMAND tst_tveditor)

# Find and Replace (2026-11-18) test: the matching engine, on plain strings.
add_executable(tst_find tests/tst_find.cpp)
target_link_libraries(tst_find PRIVATE crt-editor Qt6::Test Qt6::Core KF6::TextEditor)
add_test(NAME tst_find COMMAND tst_find)

# Find and Replace (2026-11-18) test: the dialog's chrome, controls and layout.
add_executable(tst_finddlg tests/tst_finddlg.cpp)
target_link_libraries(tst_finddlg PRIVATE crt-editor crt-shell crt-bridge Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_finddlg COMMAND tst_finddlg)

# Status-line selectors: F-row indicators + dropdowns (spell, encoding, doc
# type, INS/OVR) and the blinking caret. ctest -R tst_statussel
add_executable(tst_statussel tests/tst_statussel.cpp)
target_link_libraries(tst_statussel PRIVATE crt-editor Qt6::Test Qt6::Widgets KF6::SonnetCore KF6::TextEditor)
add_test(NAME tst_statussel COMMAND tst_statussel)

# Task 6: CRT settings (C++ model with QSettings persistence + dialog).
add_library(crt-settings STATIC
  src/settings/CrtSettings.cpp
  src/settings/CrtSettings.h
  src/settings/SettingsDialog.cpp
  src/settings/SettingsDialog.h
)
target_include_directories(crt-settings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(crt-settings PUBLIC Qt6::Gui Qt6::Widgets crt-shell)

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
# Links crt-shell too: the stepper-parity test loads :/borland.qss (owned by
# crt-shell's resources) to replicate the app's 16px font environment.
add_executable(tst_settings tests/tst_settings.cpp)
target_link_libraries(tst_settings PRIVATE crt-settings crt-shell Qt6::Test Qt6::Widgets)
add_test(NAME tst_settings COMMAND tst_settings)

# Task 7 test: ctest -R tst_session
add_executable(tst_session tests/tst_session.cpp)
target_link_libraries(tst_session PRIVATE crt-session Qt6::Test)
add_test(NAME tst_session COMMAND tst_session)

# Task 8 test: ctest -R tst_autosave
add_executable(tst_autosave tests/tst_autosave.cpp)
target_link_libraries(tst_autosave PRIVATE crt-autosave crt-session Qt6::Test)
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

# Task 4: Turbo Vision text-mode scrollbars (ASCII arrow glyphs + teal track).
add_executable(tst_scrollbars tests/tst_scrollbars.cpp)
target_link_libraries(tst_scrollbars PRIVATE crt-shell crt-editor Qt6::Test Qt6::Widgets KF6::TextEditor)
add_test(NAME tst_scrollbars COMMAND tst_scrollbars)

# GUI pass (2026-11-17): Turbo Vision file dialogs drawn on the app screen.
add_executable(tst_filedlg tests/tst_filedlg.cpp)
target_link_libraries(tst_filedlg PRIVATE crt-shell crt-bridge Qt6::Test Qt6::Widgets)
add_test(NAME tst_filedlg COMMAND tst_filedlg)

# File dock modern layout (2026-11-18): the persisted MRU-10 behind the
# recents dropdown. ctest -R tst_recents
add_executable(tst_recents tests/tst_recents.cpp)
target_link_libraries(tst_recents PRIVATE crt-shell Qt6::Test Qt6::Widgets)
add_test(NAME tst_recents COMMAND tst_recents)

# House-style modals (2026-11-18): TvModal chrome, About and Settings.
# ctest -R tst_tvmodal
add_executable(tst_tvmodal tests/tst_tvmodal.cpp)
target_link_libraries(tst_tvmodal PRIVATE crt-shell crt-settings Qt6::Test Qt6::Widgets)
add_test(NAME tst_tvmodal COMMAND tst_tvmodal)

# The TV widget kit: checkbox, spin box, tab bar. ctest -R tst_tvkit
add_executable(tst_tvkit tests/tst_tvkit.cpp)
target_link_libraries(tst_tvkit PRIVATE crt-shell Qt6::Test Qt6::Widgets)
add_test(NAME tst_tvkit COMMAND tst_tvkit)

# GUI pass 2026-11-17, Task 3: the spell section of the editor right-click
# menu (bold headline, hover popout, suggestions / add / ignore). ctest -R tst_spellmenu
add_executable(tst_spellmenu tests/tst_spellmenu.cpp)
target_link_libraries(tst_spellmenu PRIVATE crt-shell crt-bridge crt-editor
                      Qt6::Test Qt6::Widgets Qt6::Quick
                      KF6::SonnetCore KF6::TextEditor)
add_test(NAME tst_spellmenu COMMAND tst_spellmenu)

# GUI pass 2026-11-17, Task 4: syntax highlighting palette for the supported
# token types, adapted to the Borland color scheme. ctest -R tst_syntax
add_executable(tst_syntax tests/tst_syntax.cpp)
target_link_libraries(tst_syntax PRIVATE crt-editor Qt6::Test Qt6::Widgets
                      KF6::TextEditor KF6::SyntaxHighlighting)
add_test(NAME tst_syntax COMMAND tst_syntax)

# Task 6: the app entry point (ShellWindow + ContentBridge + EffectChain QML).
# Renamed crt-kate -> turbo-kate (user-facing name is TURBO Kate). The installed
# QML layout stays share/crt-kate so the runtime lookup is untouched.
add_executable(turbo-kate src/main.cpp)
target_link_libraries(turbo-kate PRIVATE
  crt-bridge crt-editor crt-settings crt-session crt-autosave Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::OpenGL Qt6::ShaderTools
)
target_compile_definitions(turbo-kate PRIVATE
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
add_dependencies(turbo-kate crt_shaders)
qt_add_resources(turbo-kate "crt_qsb_res"
  PREFIX "/crt-kate"
  BASE ${CMAKE_CURRENT_BINARY_DIR}/qml
  FILES ${CRT_QSB_OUT}
)

# Task 11: install targets (DESTDIR=staging cmake --install build).
install(TARGETS turbo-kate RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/
        DESTINATION share/crt-kate/src
        FILES_MATCHING PATTERN "*.qml" PATTERN "qmldir")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/turbo-kate.desktop
        DESTINATION share/applications)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/turbo-kate.png
        DESTINATION share/icons/hicolor/256x256/apps)
# AppImage: bundle the TV typeface where fontconfig scans (AppRun points
# XDG_DATA_DIRS at $APPDIR/usr/share), so "PxPlus IBM VGA8" resolves as a
# real family, not just via the embedded :/ copy.
install(FILES ${CRT_FONT_TTF}
        DESTINATION share/fonts/TTF
        OPTIONAL)

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
