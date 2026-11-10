// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMainWindow>
#include <QVBoxLayout>
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
    TabStrip* tabStrip() const;
    FKeyStatusBar* fkeyStatusBar() const;
    // The Borland style this window installed (QSS wrapping hides it from
    // w.style(), so the test asserts on this directly).
    BorlandStyle* borlandStyle() const { return m_borland; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    BorlandStyle* m_borland = nullptr;
    QVBoxLayout* m_editorLayout = nullptr;   // central layout: [TabStrip, editor]
};
