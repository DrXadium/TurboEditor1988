// SPDX-License-Identifier: GPL-3.0-or-later
#include "TabStrip.h"
#include "TvFrame.h"
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QFontMetrics>

namespace {
const QColor kBgBlue(0x00, 0x00, 0xAA);      // TV window blue desktop
const QColor kGreen(0x00, 0xAA, 0x00);       // the per-tab close square
const QColor kWhite(0xFF, 0xFF, 0xFF);       // active chip ink
// Inactive chip ink. It sits on the EDITOR blue now (Task 23), not on the band,
// so the old #555555 - which existed only to stay darker than the #AAAAAA
// dither - read as nearly black against the window. #888888 is a clear step
// lighter while still obviously dimmer than the active chip's white.
const QColor kTabGrey(0x88, 0x88, 0x88);
// Task 19 (mock note 2): the row's background is the scrollbar track's stipple
// design with GREY ink instead of the bars' teal, so the row reads as distinct
// from them.
// Task 22 (user): #555555 read as a near-black mud against the window blue -
// the mock's band is a BRIGHT grey, so the stipple is TV light gray #AAAAAA.
const QColor kBandGrey(0xAA, 0xAA, 0xAA);
// The controls (mock notes 1, 4, 5): the scrollbars' END-CELL design - a SOLID
// CYAN cell with a DARK BLUE glyph in it, not a bare teal glyph floating on the
// grey dither (which read as noise; user report).
const QColor kTeal(0x00, 0xAA, 0xAA);
const QColor kBlue(0x00, 0x00, 0xAA);
} // namespace

// Solid triangle arrow, same shape as TextScrollBar's end-block glyphs.
static void drawArrow(QPainter& p, const QRect& r, Qt::ArrowType dir, const QColor& ink)
{
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
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
    default: // Down
        tri << QPoint(cx, r.bottom() - 1) << QPoint(cx - hw, cy - hh / 2)
            << QPoint(cx + hw, cy - hh / 2);
        break;
    }
    p.drawPolygon(tri);
    p.restore();
}

// Crisp 1-bit text: subpixel/LCD antialiasing tints the strokes, which both
// looks wrong through the CRT pass and breaks an exact-ink assertion.
static void inkText(QPainter& p, const QRect& r, const QString& s, const QColor& ink,
                    int flags = Qt::AlignLeft | Qt::AlignVCenter)
{
    p.save();
    QFont f = p.font();
    f.setStyleStrategy(QFont::NoAntialias);
    p.setFont(f);
    p.setPen(ink);
    p.drawText(r, flags, s);
    p.restore();
}

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
    if (ev->type() == QEvent::FontChange) {
        updateCellSize();
        ensureActiveVisible();
    }
}

void TabStrip::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    ensureActiveVisible();
}

int TabStrip::cellW() const
{
    return qMax(6, fontMetrics().horizontalAdvance(QLatin1Char('M')));
}

int TabStrip::nameCells(const QString& title) const
{
    const int cw = cellW();
    return qMax(1, (fontMetrics().horizontalAdvance(title) + cw - 1) / cw);
}

// The controls are anchored to the row's ends in CELL units (mock note 1):
// ◄ in the window's first cell, and ► + ▼ against the right edge with ▼ in
// the LAST cell - the same column the right scrollbar occupies below it.
QRect TabStrip::prevTabRect() const
{
    return QRect(0, 0, cellW(), height());
}

QRect TabStrip::tabListRect() const
{
    return QRect(width() - cellW(), 0, cellW(), height());
}

QRect TabStrip::newTabRect() const
{
    return QRect(width() - 2 * cellW(), 0, cellW(), height());
}

QRect TabStrip::nextTabRect() const
{
    return QRect(width() - 3 * cellW(), 0, cellW(), height());
}

// Visible chips, left to right, starting after the ◄ cell. Chips are adjacent
// (the mock shows `][` touching) and the run stops before the ► cell.
QVector<QRect> TabStrip::tabRects() const
{
    QVector<QRect> out;
    if (m_titles.isEmpty())
        return out;
    const int cw = cellW();
    const int limit = nextTabRect().left();
    int x = cw;
    for (int i = firstShown(); i < m_titles.size(); ++i) {
        const int w = chipCells(m_titles.at(i)) * cw;
        // One whole cell of daylight between chips (user: the chips read as a
        // single run when they touched `][`).
        if (x + w > limit)
            break;
        out.append(QRect(x, 0, w, height()));
        x += w + cw;
    }
    return out;
}

// The green square inside chip `tabIndex` (its close button). Returns an empty
// rect when that tab is scrolled out of the window.
QRect TabStrip::closeRect(int tabIndex) const
{
    const QVector<QRect> rects = tabRects();
    const int j = tabIndex - firstShown();
    if (j < 0 || j >= rects.size())
        return {};
    const QRect r = rects.at(j);
    const int cw = cellW();
    // '[' ' ' name ' ' '■' ' ' ']' - the square is the (3 + nameCells)th cell.
    const int n = nameCells(m_titles.at(tabIndex));
    return QRect(r.left() + (3 + n) * cw, 0, cw, r.height());
}

int TabStrip::firstShown() const
{
    if (m_titles.isEmpty())
        return 0;
    return qBound(0, m_firstVisible, m_titles.size() - 1);
}

void TabStrip::setTabs(const QStringList& titles, int active)
{
    m_titles = titles;
    m_active = active;
    ensureActiveVisible();
    update();
}

// Keep the active tab inside the visible window: scroll forward until it fits.
// Called on setTabs / resize / font change - never from tabRects(), which is
// const and must agree with what was last painted.
void TabStrip::ensureActiveVisible()
{
    if (m_titles.isEmpty()) {
        m_firstVisible = 0;
        return;
    }
    if (width() <= 4 * cellW())   // not laid out yet: nothing fits, do not scroll
        return;
    const int last = m_titles.size() - 1;
    m_firstVisible = qBound(0, m_firstVisible, last);
    if (m_active < m_firstVisible)
        m_firstVisible = m_active;
    for (;;) {
        int x = cellW();
        int lastFitting = m_firstVisible - 1;
        for (int i = m_firstVisible; i <= last; ++i) {
            const int w = chipCells(m_titles.at(i)) * cellW();
            if (x + w > nextTabRect().left())
                break;
            lastFitting = i;
            x += w;
        }
        if (m_active <= lastFitting || m_firstVisible >= last)
            return;
        ++m_firstVisible;
    }
}

void TabStrip::scrollChips(int delta)
{
    if (m_titles.isEmpty())
        return;
    const int next = qBound(0, m_firstVisible + delta, m_titles.size() - 1);
    if (next == m_firstVisible)
        return;   // clamped: the arrows do not wrap
    m_firstVisible = next;
    update();
}

void TabStrip::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);   // crisp DOS pixels

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

    // Controls (mock notes 1, 4, 5): ◄ ► scroll the chip window, + opens a new
    // document, ▼ lists every open tab. All four are persistent. Each is the
    // scrollbar END-CELL design: a solid cyan cell, blue glyph on top.
    const QRect controls[] = { prevTabRect(), nextTabRect(), newTabRect(), tabListRect() };
    for (const QRect& c : controls)
        p.fillRect(c, kTeal);
    drawArrow(p, prevTabRect(), Qt::LeftArrow, kBlue);
    drawArrow(p, nextTabRect(), Qt::RightArrow, kBlue);
    drawArrow(p, tabListRect(), Qt::DownArrow, kBlue);
    TvFrame::glyph(p, newTabRect(), QLatin1Char('+'), kBlue);

    // Chips: [ name ■ ] - bracketed, on the EDITOR's background (user, Task 23).
    // The chip is a window onto the document: everything from `[` to `]` is
    // filled with the window blue, so the strip's brighter dither stops at the
    // brackets and each tab reads as a separate opening. Same for both states -
    // the state shows in the title ink, not in the plate.
    const QVector<QRect> rects = tabRects();
    const int cw = cellW();
    for (int j = 0; j < rects.size(); ++j) {
        const int idx = firstShown() + j;
        if (idx >= m_titles.size())
            break;
        const QRect r = rects.at(j);
        p.fillRect(r, kBlue);
        const bool active = (idx == m_active);
        // Mock note 3: the active title is bright white, an inactive one grey;
        // the green square stays bright in BOTH states.
        const QColor ink = active ? kWhite : kTabGrey;
        inkText(p, QRect(r.left(), 0, cw, r.height()), QStringLiteral("["), ink,
                Qt::AlignCenter);
        inkText(p, QRect(r.right() - cw + 1, 0, cw, r.height()), QStringLiteral("]"), ink,
                Qt::AlignCenter);
        const int n = nameCells(m_titles.at(idx));
        inkText(p, QRect(r.left() + 2 * cw, 0, n * cw, r.height()), m_titles.at(idx), ink);
        // The square: inset inside its cell so the brackets keep their daylight.
        p.fillRect(closeRect(idx).adjusted(2, 3, -2, -3), kGreen);
    }
}

void TabStrip::mousePressEvent(QMouseEvent* ev)
{
    const QPoint pos = ev->pos();

    // Controls first: they overlap the row's ends, never a chip.
    if (newTabRect().contains(pos)) {
        emit newTabRequested();
        return;
    }
    if (tabListRect().contains(pos)) {
        emit tabListRequested();
        return;
    }
    if (nextTabRect().contains(pos)) {
        scrollChips(+1);
        return;
    }
    if (prevTabRect().contains(pos)) {
        scrollChips(-1);
        return;
    }

    // The close square is INSIDE its chip, so it is tested before the chip body
    // - otherwise closing a tab would first activate it.
    const QVector<QRect> rects = tabRects();
    for (int j = 0; j < rects.size(); ++j) {
        const int idx = firstShown() + j;
        if (closeRect(idx).contains(pos)) {
            emit tabClosed(idx);
            return;
        }
        if (rects.at(j).contains(pos)) {
            if (idx != m_active)
                emit tabActivated(idx);
            return;
        }
    }
    QWidget::mousePressEvent(ev);
}
