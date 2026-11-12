// SPDX-License-Identifier: GPL-3.0-or-later
#include "BottomBand.h"
#include "TvFrame.h"
#include "TextScrollBar.h"
#include <QPainter>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QtGlobal>

namespace {
const QColor kBgBlue(0x00, 0x00, 0xAA);      // band sits on the window blue
const QColor kWhite(0xFF, 0xFF, 0xFF);       // cursor-position text (spec 6) + frame
const int kCursorColX = 75;                  // spec: white text at cols 8-12 (x in [75,125])
} // namespace

// The band IS the window bottom border row: exactly one text row tall, so the
// ═ rule sits directly above the status line with no blue gap.
int BottomBand::bandHeight() const { return qMax(10, fontMetrics().height()); }

BottomBand::BottomBand(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("bottomBand"));
    setAutoFillBackground(false);   // paintEvent fills the blue ourselves
    m_hbar = new TextScrollBar(Qt::Horizontal, this);
    m_hbar->setObjectName(QStringLiteral("bottomBandHScroll"));
    // Explicit show(): the shell window is never shown (it is rendered
    // offscreen through QWidget::render), so a child that was never shown is
    // skipped by render() and the scrollbar silently did not paint.
    m_hbar->show();
}

void BottomBand::setCursorPos(int line, int col)
{
    m_line = line;
    m_col = col;
    update();
}

QString BottomBand::cursorPosText() const
{
    return QStringLiteral("%1:%2").arg(m_line).arg(m_col);
}

void BottomBand::mirrorHorizontal(QScrollBar* source)
{
    if (!source || !m_hbar)
        return;
    // Range/value: seed, then keep both directions in sync.
    m_hbar->setRange(source->minimum(), source->maximum());
    m_hbar->setValue(source->value());
    connect(source, &QScrollBar::rangeChanged, m_hbar, &QScrollBar::setRange);
    connect(m_hbar, &QScrollBar::rangeChanged, source, &QScrollBar::setRange);
    connect(source, &QScrollBar::valueChanged, m_hbar, &QScrollBar::setValue);
    connect(m_hbar, &QScrollBar::valueChanged, source, &QScrollBar::setValue);
}

TextScrollBar* BottomBand::horizontalBar() const
{
    return m_hbar;
}

QSize BottomBand::sizeHint() const
{
    return QSize(400, bandHeight());
}

QSize BottomBand::minimumSizeHint() const
{
    return QSize(120, bandHeight());
}

int BottomBand::heightForWidth(int) const
{
    return bandHeight();
}

// Position the horizontal scrollbar: from ~col 18 (x=180) to the right edge,
// full band height. Called from both resizeEvent and paintEvent so the
// geometry is correct regardless of whether a repaint has happened yet.
void BottomBand::layoutHBar()
{
    if (!m_hbar)
        return;
    const int left = 180;                          // ~col 18 @10px cell
    const int right = width() - 4;                 // stop just short of the right
                                                   // frame edge (cols right-4..right-1)
    // ONE CELL tall (spec 3), bottom-aligned onto the double-line border rows.
    const int cellH = fontMetrics().height();
    m_hbar->setGeometry(QRect(QPoint(left, height() - cellH), QPoint(right, height() - 1)));
}

void BottomBand::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    layoutHBar();
}

void BottomBand::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), kBgBlue);

    // The band owns the window's BOTTOM frame edge (spec 9): white double line
    // + left/right vertical stubs joining the EditorHost's verticals above, so
    // all four frame corners stay solid. Drawn first; cursor + hbar on top.
    // The band owns the window's BOTTOM border row: one cell tall, carrying the
    // ═ double rule through its centre (the cursor position text interrupts it,
    // exactly like Turbo Vision's "26:24").
    TvFrame::doubleH(p, rect(), kWhite);

    // Cursor position "L:C" white, left-anchored around column 8 (x≈75).
    p.setPen(kWhite);
    // Plain white text SITTING ON the double-line border rows: the baseline
    // is the band bottom, so the digits interrupt the border like TV's 26:24
    // (and the glyph top drops into the lower half of the band).
    const int baseline = height() - 1;
    p.drawText(QPoint(kCursorColX, baseline), cursorPosText());

    // Horizontal scrollbar spans from ~col 18 to the right edge, full band height.
    layoutHBar();
}
