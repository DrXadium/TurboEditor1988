// SPDX-License-Identifier: GPL-3.0-or-later
#include "Minimap.h"
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QPolygon>
#include <QWheelEvent>

namespace {
// Turbo Vision palette (matches borland.qss and the TV scrollbars). Only named
// palette colours: spec 1 bans pure black inside the window.
const QColor kCyan(0x00, 0xAA, 0xAA);     // end-row fill
const QColor kBlue(0x00, 0x00, 0xAA);     // preview background + arrow ink
const QColor kYellow(0xFF, 0xFF, 0x55);   // code ink (editor text colour)
const QColor kGray(0xAA, 0xAA, 0xAA);     // frame + viewport thumb

const int kFrame = 1;          // 1 px frame around the strip
const int kButtonHeight = 10;  // one end row, like Kate's scroll buttons
const int kWidth = 96;         // 12 character cells of chrome
const int kNaturalPitch = 3;   // natural preview line pitch (px)

// A solid triangle pointing up or down, centred in \p r, in window-blue ink.
void drawArrow(QPainter* p, const QRect& r, bool up)
{
    const int cx = r.center().x();
    const int w = 9;
    const int h = 7;
    const int top = r.center().y() - h / 2;
    QPolygon tri;
    if (up)
        tri << QPoint(cx, top) << QPoint(cx - w / 2, top + h - 1)
            << QPoint(cx + w / 2, top + h - 1);
    else
        tri << QPoint(cx, top + h - 1) << QPoint(cx - w / 2, top)
            << QPoint(cx + w / 2, top);
    p->save();
    p->setPen(Qt::NoPen);
    p->setBrush(kBlue);
    p->drawPolygon(tri);
    p->restore();
}

// 2x2 checkerboard of frame gray: a palette-pure "translucent" thumb that lets
// the code preview show through, the way Kate's shield does.
const QBrush& thumbBrush()
{
    static const QBrush brush = [] {
        QPixmap pm(2, 2);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.fillRect(QRect(0, 0, 1, 1), kGray);
        p.fillRect(QRect(1, 1, 1, 1), kGray);
        p.end();
        return QBrush(pm);
    }();
    return brush;
}

} // namespace

Minimap::Minimap(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("minimap"));
    // A fixed-width strip beside the editor; it takes the row's full height.
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAutoFillBackground(false);
}

QSize Minimap::sizeHint() const
{
    return QSize(kWidth, 400);
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

QRect Minimap::topButtonRect() const
{
    return QRect(kFrame, kFrame, width() - 2 * kFrame, kButtonHeight);
}

QRect Minimap::bottomButtonRect() const
{
    return QRect(kFrame, height() - kFrame - kButtonHeight,
                 width() - 2 * kFrame, kButtonHeight);
}

QRect Minimap::bodyRect() const
{
    return QRect(kFrame, kFrame + kButtonHeight, width() - 2 * kFrame,
                 qMax(0, height() - 2 * kFrame - 2 * kButtonHeight));
}

int Minimap::documentHeight(int bodyHeight) const
{
    if (m_lines.isEmpty() || bodyHeight <= 0)
        return 0;
    // Short documents are shown at up to twice their natural pitch and
    // top-aligned (Kate's behaviour); long ones are squeezed into the track.
    const int natural = m_lines.size() * kNaturalPitch;
    return qMin(bodyHeight, natural * 2);
}

QRect Minimap::viewportRect() const
{
    const QRect body = bodyRect();
    const int n = m_lines.size();
    const int docH = documentHeight(body.height());
    if (n <= 0 || docH <= 0 || m_viewportCount <= 0)
        return QRect(body.left(), body.top(), body.width(), 0);

    const int top = body.top() + (m_viewportFirst * docH) / n;
    int h = qMax(2, (m_viewportCount * docH) / n);
    if (top + h > body.bottom() + 1)
        h = body.bottom() + 1 - top;
    return QRect(body.left(), top, body.width(), qMax(0, h));
}

int Minimap::lineAt(int y) const
{
    const QRect body = bodyRect();
    const int n = m_lines.size();
    const int docH = documentHeight(body.height());
    if (n <= 0 || docH <= 0)
        return 0;
    int line = ((y - body.top()) * n) / docH;
    return qBound(0, line, n - 1);
}

void Minimap::render(QPainter* painter, const QRect& rect)
{
    if (!painter || !painter->isActive())
        return;

    painter->save();
    painter->setClipRect(rect);

    const QRect topRow(rect.left() + kFrame, rect.top() + kFrame,
                       rect.width() - 2 * kFrame, kButtonHeight);
    // Use top+height (not rect.bottom()) so render() agrees with
    // bottomButtonRect(), which is built from height().
    const QRect bottomRow(rect.left() + kFrame,
                          rect.top() + rect.height() - kFrame - kButtonHeight,
                          rect.width() - 2 * kFrame, kButtonHeight);
    const QRect body(rect.left() + kFrame, rect.top() + kFrame + kButtonHeight,
                     rect.width() - 2 * kFrame,
                     qMax(0, rect.height() - 2 * kFrame - 2 * kButtonHeight));

    // Preview background: the editor's window blue (never black).
    painter->fillRect(rect, kBlue);

    // The document preview.
    const int n = m_lines.size();
    const int docH = documentHeight(body.height());
    if (n > 0 && docH > 0) {
        const qreal pitch = qreal(docH) / qreal(n);
        QFont f(QStringLiteral("monospace"));
        f.setPixelSize(pitch >= 4.0 ? 3 : 2);
        f.setStyleStrategy(QFont::NoAntialias);
        painter->setFont(f);
        painter->setPen(kYellow);
        painter->setClipRect(body);
        const QFontMetrics fm(f);
        int lastY = -1000;
        for (int i = 0; i < n; ++i) {
            const int y = body.top() + (i * docH) / n;
            if (y == lastY)
                continue;   // squeezed: one text row per pixel row
            lastY = y;
            painter->drawText(QPointF(body.left(), y + fm.ascent()), m_lines.at(i));
        }
        painter->setClipping(false);
        painter->setClipRect(rect);
    }

    // Viewport thumb: dithered gray fill with a solid 1 px border.
    QRect vp = viewportRect();
    vp.moveTop(vp.top());
    if (vp.height() > 0) {
        painter->fillRect(vp, thumbBrush());
        painter->setPen(kGray);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(vp.adjusted(0, 0, -1, -1));
    }

    // End rows: cyan scroll buttons with blue triangles.
    painter->fillRect(topRow, kCyan);
    painter->fillRect(bottomRow, kCyan);
    drawArrow(painter, topRow, true);
    drawArrow(painter, bottomRow, false);

    // Frame, drawn last so nothing paints over it.
    painter->setPen(kGray);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect.adjusted(0, 0, -1, -1));

    painter->restore();
}

void Minimap::paintEvent(QPaintEvent* ev)
{
    QPainter p(this);
    render(&p, rect());
    Q_UNUSED(ev);
}

void Minimap::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(ev);
        return;
    }
    if (topButtonRect().contains(ev->pos())) {
        emit pageRequested(-1);
        ev->accept();
        return;
    }
    if (bottomButtonRect().contains(ev->pos())) {
        emit pageRequested(1);
        ev->accept();
        return;
    }
    if (bodyRect().contains(ev->pos())) {
        m_dragging = true;
        emit lineActivated(lineAt(ev->pos().y()));
        ev->accept();
        return;
    }
    QWidget::mousePressEvent(ev);
}

void Minimap::mouseMoveEvent(QMouseEvent* ev)
{
    if (m_dragging) {
        emit lineActivated(lineAt(ev->pos().y()));
        ev->accept();
        return;
    }
    QWidget::mouseMoveEvent(ev);
}

void Minimap::mouseReleaseEvent(QMouseEvent* ev)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(ev);
}

void Minimap::wheelEvent(QWheelEvent* ev)
{
    // Wheel down (negative delta) scrolls the document forward.
    const int steps = ev->angleDelta().y() < 0 ? 3 : -3;
    emit scrollLinesRequested(steps);
    ev->accept();
}
