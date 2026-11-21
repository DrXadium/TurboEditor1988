// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QStringList>
#include <QRect>

// Kate-style "scrollbar minimap": a preview of the document painted in the
// scroll track, a viewport thumb over it, and one scroll button at each end.
//
// House style (the same one the TV scrollbars use): the two end rows are cyan
// (#00AAAA) with window-blue (#0000AA) arrow glyphs, the preview sits on the
// editor's window blue (#0000AA) with editor-yellow (#FFFF55) text, and the
// thumb is drawn in the frame gray (#AAAAAA). No pure black: spec 1 bans it
// inside the window.
//
// The widget is a dumb view. It is fed the document's lines and the visible
// range by the composition root (main.cpp) and reports user intent through
// signals - crt-shell does not link KTextEditor.
class Minimap : public QWidget
{
    Q_OBJECT
public:
    explicit Minimap(QWidget* parent = nullptr);

    void setLines(const QStringList& lines);
    QStringList lines() const { return m_lines; }

    // Which source lines are visible in the editor: firstVisible + count.
    void setViewport(int firstVisibleLine, int visibleLineCount);
    QRect viewportRect() const;         // in minimap widget coordinates

    // Geometry, in widget coordinates. Public so the composition root (and the
    // tests) can reason about the same rectangles the painter uses.
    QRect topButtonRect() const;
    QRect bottomButtonRect() const;
    QRect bodyRect() const;             // the code preview between the buttons

    // Paints the minimap into \p rect. Exposed for tests (which drive it on an
    // offscreen image instead of showing a window).
    void render(QPainter* painter, const QRect& rect);

    QSize sizeHint() const override;

signals:
    // -1 = page up (top row), +1 = page down (bottom row).
    void pageRequested(int pages);
    // A click/drag in the preview: put this source line on screen.
    void lineActivated(int line);
    // Wheel over the preview: scroll by this many lines (positive = down).
    void scrollLinesRequested(int lines);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void wheelEvent(QWheelEvent* ev) override;

private:
    // Height the document preview occupies inside the body. Short documents
    // are shown at up to twice their natural pitch and top-aligned, the way
    // Kate does it, instead of being stretched over the whole track.
    int documentHeight(int bodyHeight) const;
    int lineAt(int y) const;

    QStringList m_lines;
    int m_viewportFirst = 0;
    int m_viewportCount = 0;
    bool m_dragging = false;
};
