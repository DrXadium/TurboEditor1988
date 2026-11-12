// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMainWindow>
#include <QVBoxLayout>
#include <QPointer>
#include <QScrollBar>
class MenuBar;
class TabStrip;
class FKeyStatusBar;
class BorlandStyle;

// Top-level Borland chrome window: MenuBar (top), TabStrip, central editor
// placeholder, FKeyStatusBar (bottom). Uses BorlandStyle. CRT effects are
// applied over this whole window in Task 4 (via ContentBridge).
class ShellWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit ShellWindow(QWidget* parent = nullptr);
    // Central editor host (replaced by KTextEditor in Task 5).
    void setCentralWidget(QWidget* w);
    // Replaces the placeholder area BELOW the TabStrip with the real editor
    // widget (EditorTabManager's container). TabStrip stays intact above it.
    void setEditorWidget(QWidget* w);
    // Focus target for forwarded keys: the editor if one is set, else the shell.
    QWidget* keyFocusWidget() const;
    TabStrip* tabStrip() const;
    FKeyStatusBar* fkeyStatusBar() const;
    // Bottom border band (cursor position + horizontal scrollbar), spec 5h/6.
    class BottomBand* bottomBand() const;
    // The code minimap shown beside the editor (Task 9), or nullptr before
    // setEditorWidget() installs the editor row.
    class Minimap* minimap() const { return m_minimap; }
    // The Borland style this window installed (QSS wrapping hides it from
    // w.style(), so the test asserts on this directly).
    BorlandStyle* borlandStyle() const { return m_borland; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    BorlandStyle* m_borland = nullptr;
    QVBoxLayout* m_editorLayout = nullptr;   // central layout: [TabStrip, editorRow]
    QWidget* m_editor = nullptr;             // current editor widget (focus default)
    Minimap* m_minimap = nullptr;
    class BottomBand* m_bottomBand = nullptr;
    // TV-revision2 spec 1: 1-cell right-border scrollbar + the (hidden) Kate
    // document vertical it mirrors. QPointer: the view can die on tab close.
    class RightScrollBar* m_rightBar = nullptr;
    QPointer<QScrollBar> m_docV;
};
