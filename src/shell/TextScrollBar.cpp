// SPDX-License-Identifier: GPL-3.0-or-later
#include "TextScrollBar.h"
#include <QPainter>
#include <QStyleOptionSlider>
#include <QStyleOptionSpinBox>
#include <QStylePainter>

namespace {
// Turbo Vision palette (matches borland.qss): teal track, green thumb,
// black glyph ink.
const QColor kTeal(0x1A, 0xAF, 0xAF);
const QColor kGreen(0x19, 0xA8, 0x19);
const QColor kBlack(0, 0, 0);
const QColor kLightTeal(0x4F, 0xCF, 0xCF);
} // namespace

TextScrollBar::TextScrollBar(Qt::Orientation o, QWidget* parent)
    : QScrollBar(o, parent)
{
}

// Draw a solid triangle arrow (◄ ► ▲ ▼) pointing `dir` into the rect `r`,
// in black ink.
static void drawArrow(QPainter& p, const QRect& r, Qt::ArrowType dir)
{
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(kBlack);
    QPolygon tri;
    const int cx = r.center().x(), cy = r.center().y();
    const int hw = r.width() / 2 - 1, hh = r.height() / 2 - 1;
    switch (dir) {
    case Qt::LeftArrow:
        tri << QPoint(r.left() + 1, cy) << QPoint(cx + hw / 2, cy - hh)
            << QPoint(cx + hw / 2, cy + hh);
        break;
    case Qt::RightArrow:
        tri << QPoint(r.right() - 1, cy) << QPoint(cx - hw / 2, cy - hh)
            << QPoint(cx - hw / 2, cy + hh);
        break;
    case Qt::UpArrow:
        tri << QPoint(cx, r.top() + 1) << QPoint(cx - hw, cy + hh / 2)
            << QPoint(cx + hw, cy + hh / 2);
        break;
    default: // Down
        tri << QPoint(cx, r.bottom() - 1) << QPoint(cx - hw, cy - hh / 2)
            << QPoint(cx + hw, cy - hh / 2);
        break;
    }
    p.drawPolygon(tri);
    p.restore();
}

// Emulate the DOS ▒ track: a fine checker/dither of teal on the base color.
static void drawTrack(QPainter& p, const QRect& r)
{
    p.save();
    p.setPen(Qt::NoPen);
    const int step = 4;
    for (int y = r.top(); y <= r.bottom(); y += step)
        for (int x = r.left(); x <= r.right(); x += step) {
            // offset every other row for a ▒ checker feel
            const int ox = (y / step) % 2 ? step / 2 : 0;
            p.setBrush((x / step + y / step) % 2 ? kTeal : kLightTeal);
            p.drawRect(QRect(x + ox, y, step, step));
        }
    p.restore();
}

void TextScrollBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    QStyleOptionSlider opt;
    initStyleOption(&opt);

    const bool horiz = orientation() == Qt::Horizontal;
    const int thick = horiz ? height() : width();

    // Arrow button rects (TV reserves a square button at each end).
    QRect subBtn, addBtn;
    if (horiz) {
        subBtn = QRect(0, 0, thick, height());
        addBtn = QRect(width() - thick, 0, thick, height());
    } else {
        subBtn = QRect(0, 0, width(), thick);
        addBtn = QRect(0, height() - thick, width(), thick);
    }

    // Track (between the buttons).
    QRect track = rect();
    if (horiz)
        track.adjust(thick, 0, -thick, 0);
    else
        track.adjust(0, thick, 0, -thick);
    p.fillRect(track, kTeal);
    drawTrack(p, track);

    // Arrow buttons: teal base + black arrow glyph.
    p.fillRect(subBtn, kTeal);
    p.fillRect(addBtn, kTeal);
    drawArrow(p, subBtn, horiz ? Qt::LeftArrow : Qt::UpArrow);
    drawArrow(p, addBtn, horiz ? Qt::RightArrow : Qt::DownArrow);

    // Thumb (█ solid green block) at the value position within the track.
    const int span = horiz ? track.width() : track.height();
    const int range = maximum() - minimum();
    const int page = pageStep();
    const int thumbLen =
        range > 0 ? qMax(24, span * page / (range + page)) : span;
    const int pos = range > 0
        ? (value() - minimum()) * (span - thumbLen) / range
        : 0;
    QRect thumb = horiz
        ? QRect(track.left() + pos, track.top(), thumbLen, track.height())
        : QRect(track.left(), track.top() + pos, track.width(), thumbLen);
    p.fillRect(thumb, kGreen);
    // Light highlight line on the thumb's leading edge (raised handle).
    p.setPen(kLightTeal);
    if (horiz)
        p.drawLine(thumb.topLeft(), thumb.bottomLeft());
    else
        p.drawLine(thumb.topLeft(), thumb.topRight());
}
