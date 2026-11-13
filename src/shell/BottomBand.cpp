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
const QColor kGreen(0x00, 0xAA, 0x00);       // bottom-right corner join (spec: green)
const int kCursorColX = 75;                  // spec: white text at cols 8-12 (x in [75,125])
} // namespace

// TWO text rows: the horizontal scrollbar sits on its own row, and the window's
// bottom border row (═ with the position text) runs underneath it. With a
// single row the scrollbar painted over the rule and the border disappeared
// from x≈180 rightwards.
int BottomBand::bandHeight() const { return qMax(10, fontMetrics().height()) * 2; }

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
    // The scrollbar owns the band's TOP row; the border row below it is left
    // for the ═ rule, so the rule is never painted over.
    const int left = 0;
    const int cellW = qMax(6, fontMetrics().horizontalAdvance(QLatin1Char('M')));
    const int right = width() - cellW - 1;
    const int rowH = qMax(10, fontMetrics().height());
    m_hbar->setGeometry(QRect(QPoint(left, 0), QPoint(right, rowH - 1)));
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

    // The band owns the window's BOTTOM BORDER ROW (the lower of its two rows).
    // TV's bottom edge reads: ╚ at the left, ═ across, the "L:C" position
    // interrupting it with a SPACE on each side, ═ again, and a GREEN corner
    // where it meets the vertical scrollbar column at the right.
    const int rowH = qMax(10, fontMetrics().height());
    const int borderTop = height() - rowH;
    const QRect border(0, borderTop, width(), rowH);
    const int cellW = qMax(6, fontMetrics().horizontalAdvance(QLatin1Char('M')));

    // Rule rows inside the border row, matching TvFrame::doubleH's geometry.
    const int gap = qMax(2, rowH / 3);
    const int cy = borderTop + rowH / 2;
    const int upperRule = cy - gap / 2 - 1;

    // The rule BREAKS around the position text: space, "L:C", space.
    const QString pos = cursorPosText();
    const int space = qMax(4, fontMetrics().horizontalAdvance(QLatin1Char(' ')));
    const int textW = fontMetrics().horizontalAdvance(pos);
    const int textLeft = kCursorColX;
    const int gapStart = qMax(cellW, textLeft - space);
    const int gapEnd = qMin(width() - cellW, textLeft + textW + space);

    TvFrame::doubleH(p, QRect(0, borderTop, gapStart, rowH), kWhite);
    if (gapEnd < width())
        TvFrame::doubleH(p, QRect(gapEnd, borderTop, width() - gapEnd, rowH), kWhite);

    // ╚ bottom-left: the ║ column from above stops AT the upper rule. Drawing
    // it the full height made a cross instead of a corner.
    TvFrame::doubleV(p, QRect(0, borderTop, cellW, qMax(1, upperRule - borderTop + 1)), kWhite);

    // Green corner where the bottom rule meets the scrollbar column.
    TvFrame::doubleV(p, QRect(width() - cellW, borderTop, cellW,
                              qMax(1, upperRule - borderTop + 1)), kGreen);

    // Cursor position, white, on the border row.
    p.setPen(kWhite);
    p.drawText(QPoint(textLeft, height() - 1), pos);

    layoutHBar();
}
