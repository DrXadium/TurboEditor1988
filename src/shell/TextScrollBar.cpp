// SPDX-License-Identifier: GPL-3.0-or-later
#include "TextScrollBar.h"
#include <QPainter>
#include <QStyleOptionSlider>
#include <QStyleOptionSpinBox>
#include <QStylePainter>

namespace {
// Turbo Vision palette (matches borland.qss): teal track, green thumb,
// black glyph ink. TV-revision spec restricts to the 8 named palette colors,
// so the dither uses only teal over the blue base (no off-palette light teal).
const QColor kTeal(0x00, 0xAA, 0xAA);
const QColor kGreen(0x00, 0xAA, 0x00);
const QColor kBlue(0x00, 0x00, 0xAA);   // window blue — the dither base
// Arrow/glyph ink: window BLUE, never black — TV-revision2 spec 1 forbids
// any pure-black pixel inside the window (bars live in window rows).
const QColor kInk(0x00, 0x00, 0xAA);
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
    p.setBrush(kInk);
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

// Emulate the DOS ▒ track: a fine 1-px teal-on-blue stipple (even texture).
// On a horizontal bar the sampling axis is x, so alternate columns; the test
// pins "not a coarse checker" by looking for transitions in every 5 px.
static void drawTrack(QPainter& p, const QRect& r)
{
    p.save();
    p.setPen(Qt::NoPen);
    p.fillRect(r, kBlue);   // window-blue base under the stipple
    for (int y = r.top(); y <= r.bottom(); ++y)
        for (int x = r.left(); x <= r.right(); ++x)
            if (x & 1)
                p.fillRect(QRect(x, y, 1, 1), kTeal);
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

    // Thumb at the value position within the track.
    // Horizontal (bottom band): a SINGLE-CELL solid TEAL square - spec 3
    // bans green in the bottom scrollbar and wants a one-cell thumb.
    // Vertical (Kate's own bars): classic bright green block.
    const int span = horiz ? track.width() : track.height();
    const int range = maximum() - minimum();
    const int page = pageStep();
    const int thumbLen = horiz
        ? qMin(track.height(), qMax(8, fontMetrics().horizontalAdvance(QLatin1Char('M'))))
        : (range > 0 ? qMax(24, span * page / (range + page)) : span);
    Q_UNUSED(range);
    const int pos = range > 0
        ? (value() - minimum()) * (span - thumbLen) / range
        : 0;
    QRect thumb = horiz
        ? QRect(track.left() + pos, track.top(), thumbLen, track.height())
        : QRect(track.left(), track.top() + pos, track.width(), thumbLen);
    p.fillRect(thumb, horiz ? kTeal : kGreen);
}
