// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QStringList>
#include <QRect>

// A VS Code-style code minimap: paints one scaled horizontal strip per source
// line plus a translucent viewport rectangle that tracks the scroll position.
// Deliberately a lightweight painter (no text rendering) — it renders into the
// offscreen shell, so it only needs to read as "there's code over there".
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

    // Paints strips + viewport into \p rect. Exposed for tests (which drive it
    // on an offscreen image instead of showing a window).
    void render(QPainter* painter, const QRect& rect);

    QSize sizeHint() const override { return QSize(48, 400); }

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    int lineStripTop(int lineIndex, int height) const;

    QStringList m_lines;
    int m_viewportFirst = 0;
    int m_viewportCount = 0;
};
