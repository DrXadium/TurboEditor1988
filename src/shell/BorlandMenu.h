// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMenu>

// TV-revision2 spec 6 dropdown: a solid light-gray box with NO line-drawing
// border; black item text with a RED hotkey letter; the highlighted row is a
// FULL-WIDTH green bar (edge to edge, black text); a check-mark slot in
// column 1 (text from column 2); shortcuts right-aligned with one cell of
// padding; separators are one full-width single black line.
//
// Why a subclass: QStyleSheetStyle intercepts CE_MenuItem painting whenever the
// app carries a stylesheet that matches the menu widget (the generic QWidget
// rule matches every widget), so BorlandStyle::drawControl(CE_MenuItem) is
// never reached. Owning paintEvent sidesteps QStyle entirely for popups.
class BorlandMenu : public QMenu
{
    Q_OBJECT
public:
    explicit BorlandMenu(QWidget* parent = nullptr);

    // How far the black border sits inside the panel edge (the gray panel shows
    // outside it on all four sides, per the reference screenshot), and how thick
    // the stroke is. Content geometry is derived from both.
    static int borderInset() { return 12; }
    static int borderWidth() { return 2; }
    // Gray gap between the border stroke and the selected row's green bar.
    static int highlightInset() { return 3; }

    // --- Scrolling -------------------------------------------------------
    // A long list (every highlighting mode, every dictionary) is taller than
    // the popup is allowed to be. Qt's QMenu responds by WRAPPING into columns
    // and laying actions beyond the widget, with no scrollers, so a capped
    // popup is simply unscrollable. When the content overflows we lay the rows
    // out ourselves in one column and scroll that column (wheel, arrows,
    // click). Short menus (the menu bar, the context menu) do not overflow and
    // keep the plain QMenu geometry path untouched.
    bool scrolls() const;
    int scrollOffset() const { return m_scroll; }
    int contentHeight() const;
    // Scrollbar geometry, shared by the painter and hit-testing so a click can
    // never land on the wrong part of the bar. Empty when the menu does not
    // scroll.
    QRect scrollBarRect() const;
    QRect scrollThumbRect() const;

protected:
    void paintEvent(QPaintEvent* ev) override;
    void wheelEvent(QWheelEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void keyPressEvent(QKeyEvent* ev) override;
    void hideEvent(QHideEvent* ev) override;

private:
    int rowHeight() const;
    // Uniform row height used by the self-scrolling layout.
    int scrollRowHeight() const;
    int contentTop() const;
    QList<QAction*> visibleActions() const;
    QRect rowRect(int index) const;          // content coords, before scrolling
    int rowAt(int viewportY) const;          // row under a viewport y, -1 = none
    int maxScroll() const;
    void scrollBy(int dy);
    void ensureVisible(int index);
    void activateRow(int index);
    void paintScrollBar(QPainter& p) const;
    int m_scroll = 0;                        // pixels scrolled from the top
    // Thumb dragging: set on a press inside the thumb, cleared on release. The
    // offset keeps the grab point under the cursor so the thumb does not jump.
    bool m_draggingThumb = false;
    int m_dragGrabOffset = 0;
};
