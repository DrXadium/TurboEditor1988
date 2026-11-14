// SPDX-License-Identifier: GPL-3.0-or-later
#include "TabStrip.h"
#include "TvFrame.h"
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
// Task 19 (mock note 2): the row's background is the scrollbar track's stipple
// design with GREY ink instead of the bars' teal, so the row reads as distinct
// from them. #555555 (TV dark gray) rather than #AAAAAA: the lighter gray is
// what the dim tab titles are drawn in, and grey-on-grey would be unreadable -
// the mock's band is clearly darker than its inactive titles.
const QColor kBandGrey(0x55, 0x55, 0x55);
} // namespace

TabStrip::TabStrip(QWidget* parent) : QWidget(parent)
{
    setObjectName("tabStrip");
    setAutoFillBackground(false);   // paintEvent draws the row itself
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    updateCellSize();
}

// The row is exactly ONE character cell tall - the same row height as the
// document's border row below it (mock: the tab row and the ╔ ═ ╗ row are the
// same band). The QSS font is applied after construction, so the height is
// re-measured whenever the font changes; otherwise the row keeps the default
// font's height (24 px against the PxPlus face's 16 px cell).
void TabStrip::updateCellSize()
{
    const int h = qMax(8, fontMetrics().height());
    if (minimumHeight() != h || maximumHeight() != h)
        setFixedHeight(h);
}

void TabStrip::changeEvent(QEvent* ev)
{
    QWidget::changeEvent(ev);
    if (ev->type() == QEvent::FontChange)
        updateCellSize();
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

    // Row background: the scrollbar track's 1-px stipple, grey ink on window
    // blue (mock note 2). Dithered in BOTH axes - alternating on one axis alone
    // leaves continuous bands that read as stripes (the defect the bars had).
    p.fillRect(rect(), kBgBlue);
    p.save();
    p.setPen(Qt::NoPen);
    for (int y = 0; y < height(); ++y)
        for (int x = 0; x < width(); ++x)
            if ((x + y) & 1)
                p.fillRect(QRect(x, y, 1, 1), kBandGrey);
    p.restore();

    // NO rule in this row: the document's own top border row - ╔ ═ name ═ ╗,
    // painted by EditorHost directly below - is the only border rule (mock).
    // The left ║ column is painted by EditorHost beneath this strip; the right
    // border column belongs to RightScrollBar. No verticals here.

    // Close box [■] — top-left: white brackets around a bright green cell
    // (TV-revision2 spec 1: white brackets + green ■; no black ink).
    const QRect cb = closeBoxRect();
    p.fillRect(cb, kGreen);
    TvFrame::box(p, cb, kWhite);

    // Zoom box [↕] — top-right: white brackets, window-blue face, GREEN ↕
    // glyphs (spec: white brackets, green arrow).
    const QRect zb = zoomBoxRect();
    p.fillRect(zb, kBgBlue);
    TvFrame::box(p, zb, kWhite);
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
