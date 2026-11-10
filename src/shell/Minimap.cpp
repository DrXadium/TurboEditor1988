// SPDX-License-Identifier: GPL-3.0-or-later
#include "Minimap.h"
#include <QPainter>
#include <QPaintEvent>

Minimap::Minimap(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("minimap"));
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    // Matches the Borland navy desktop so the strip blends into the chrome.
    setAutoFillBackground(false);
}

void Minimap::setLines(const QStringList& lines)
{
    m_lines = lines;
    update();
}

void Minimap::setViewport(int firstVisibleLine, int visibleLineCount)
{
    m_viewportFirst = qMax(0, firstVisibleLine);
    m_viewportCount = qMax(0, visibleLineCount);
    update();
}

int Minimap::lineStripTop(int lineIndex, int height) const
{
    if (m_lines.isEmpty())
        return 0;
    // Distribute all lines evenly across the available height.
    const qreal band = qreal(height) / qreal(m_lines.size());
    return int(lineIndex * band);
}

QRect Minimap::viewportRect() const
{
    const int h = height();
    if (m_lines.isEmpty() || m_viewportCount <= 0)
        return QRect(0, 0, width(), 0);
    const int top = lineStripTop(m_viewportFirst, h);
    const qreal band = qreal(h) / qreal(m_lines.size());
    const int vHeight = qMax(2, int(m_viewportCount * band));
    return QRect(0, top, width(), qMin(vHeight, h - top));
}

void Minimap::render(QPainter* painter, const QRect& rect)
{
    if (!painter || !painter->isActive())
        return;
    painter->save();
    painter->setClipRect(rect);

    const int h = rect.height();
    const int w = rect.width();
    if (!m_lines.isEmpty()) {
        const qreal band = qreal(h) / qreal(m_lines.size());
        // One "strip" per line: a short horizontal bar of varying width (so
        // consecutive lines aren't perfectly uniform, like real code).
        QPen pen(QColor(120, 160, 220), qMax(1.0, band * 0.6));
        painter->setPen(pen);
        for (int i = 0; i < m_lines.size(); ++i) {
            const qreal y = rect.top() + i * band + band / 2.0;
            const int lineWidth = int(w * (0.35 + 0.5 * ((i * 37) % 100) / 100.0));
            painter->drawLine(QPointF(rect.left() + 3, y),
                              QPointF(rect.left() + 3 + lineWidth, y));
        }
    }

    // Viewport rectangle (translucent highlight).
    QRect vp = viewportRect();
    vp.moveTop(vp.top() + rect.top());
    painter->fillRect(vp, QColor(200, 210, 255, 60));
    painter->setPen(QColor(230, 235, 255, 180));
    painter->drawRect(vp.adjusted(0, 0, -1, -1));
    painter->restore();
}

void Minimap::paintEvent(QPaintEvent* ev)
{
    QPainter p(this);
    p.fillRect(ev->rect(), QColor(10, 10, 40));   // slightly lighter than navy
    render(&p, rect());
}
