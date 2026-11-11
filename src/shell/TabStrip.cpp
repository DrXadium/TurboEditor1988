// SPDX-License-Identifier: GPL-3.0-or-later
#include "TabStrip.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFontMetrics>

namespace {
const QColor kFrameGray(0xAA, 0xAA, 0xAA);   // active-tab plate
const QColor kFrameWhite(0xFF, 0xFF, 0xFF);  // TV-revision spec 3: white double line
const QColor kBgBlue(0x00, 0x00, 0xAA);      // TV window blue desktop
const QColor kBlack(0, 0, 0);
const QColor kGreen(0x00, 0xAA, 0x00);       // close/zoom box fill
const QColor kWhite(0xFF, 0xFF, 0xFF);       // inactive tab label (spec 3)
} // namespace

TabStrip::TabStrip(QWidget* parent) : QWidget(parent)
{
    setObjectName("tabStrip");
    setMinimumHeight(24);
    setAutoFillBackground(false);   // paintEvent draws the title line itself
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void TabStrip::setTabs(const QStringList& titles, int active)
{
    m_titles = titles;
    m_active = active;
    update();
}

// Box geometries (recomputed in paintEvent; queried by mousePressEvent).
QRect TabStrip::closeBoxRect() const
{
    return QRect(6, height() / 2 - 8, 18, 16);
}

QRect TabStrip::zoomBoxRect() const
{
    return QRect(width() - 24, height() / 2 - 8, 18, 16);
}

// Tab segment x-ranges, starting after the close box.
QVector<QRect> TabStrip::tabRects() const
{
    QVector<QRect> out;
    if (m_titles.isEmpty())
        return out;
    const QFontMetrics fm(font());
    int x = closeBoxRect().right() + 10;
    const int rightLimit = zoomBoxRect().left() - 8;
    const int gap = 6;
    for (int i = 0; i < m_titles.size(); ++i) {
        const QString label = m_titles.at(i);
        const int w = fm.horizontalAdvance(label) + 14;
        if (x + w > rightLimit)
            break;
        out.append(QRect(x, 4, w, height() - 8));
        x += w + gap;
    }
    return out;
}

void TabStrip::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);   // crisp DOS pixels
    const int midY = height() / 2;

    // Desktop blue background (the title line sits on the window blue).
    p.fillRect(rect(), kBgBlue);

    // Top + bottom frame edges (horizontal double-line), broken by the title
    // content: draw full-width, then the tabs/boxes/labels paint over it.
    p.setPen(kFrameWhite);
    p.drawLine(0, 1, width() - 1, 1);
    p.drawLine(0, 2, width() - 1, 2);
    p.drawLine(0, height() - 3, width() - 1, height() - 3);
    p.drawLine(0, height() - 2, width() - 1, height() - 2);

    // Left/right vertical frame stubs (corners connect to the edges above).
    p.drawLine(1, 0, 1, height() - 1);
    p.drawLine(2, 0, 2, height() - 1);
    p.drawLine(width() - 2, 0, width() - 2, height() - 1);
    p.drawLine(width() - 3, 0, width() - 3, height() - 1);

    // Close box [■] — top-left: white brackets around a bright green cell
    // (TV-revision2 spec 1: white brackets + green ■; no black ink).
    const QRect cb = closeBoxRect();
    p.fillRect(cb, kGreen);
    p.setPen(kWhite);
    p.drawRect(cb);

    // Zoom box [↕] — top-right: white brackets, window-blue face, GREEN ↕
    // glyphs (spec: white brackets, green arrow).
    const QRect zb = zoomBoxRect();
    p.fillRect(zb, kBgBlue);
    p.setPen(kWhite);
    p.drawRect(zb);
    p.setPen(kGreen);
    {
        const int cx = zb.center().x();
        const int top = zb.top() + 3;
        const int bot = zb.bottom() - 3;
        // Up arrow (top half).
        p.drawLine(cx, top, cx, top + 4);
        p.drawLine(cx - 3, top + 3, cx, top);
        p.drawLine(cx + 3, top + 3, cx, top);
        // Down arrow (bottom half).
        p.drawLine(cx, bot, cx, bot - 4);
        p.drawLine(cx - 3, bot - 3, cx, bot);
        p.drawLine(cx + 3, bot - 3, cx, bot);
    }

    // Tabs as title-line segments.
    const QVector<QRect> rects = tabRects();
    for (int i = 0; i < rects.size() && i < m_titles.size(); ++i) {
        const QRect r = rects.at(i);
        const bool active = (i == m_active);
        if (active) {
            // Active tab: gray plate with WHITE text (spec 3).
            p.fillRect(r, kFrameGray);
            p.setPen(kBlack);              // spec 2: black text ON the gray plate
            p.drawText(r, Qt::AlignCenter, m_titles.at(i));
        } else {
            // Inactive tab: transparent over blue, white label (spec 3).
            p.setPen(kWhite);
            p.drawText(r, Qt::AlignCenter, m_titles.at(i));
        }
    }
}

void TabStrip::mousePressEvent(QMouseEvent* ev)
{
    const QVector<QRect> rects = tabRects();
    for (int i = 0; i < rects.size(); ++i) {
        if (rects.at(i).contains(ev->pos())) {
            if (i != m_active)
                emit tabActivated(i);
            return;
        }
    }
    QWidget::mousePressEvent(ev);
}
