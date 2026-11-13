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
const QColor kGreen(0x00, 0xFF, 0x00);       // bottom-right corner join (BRIGHT green)
const int kCursorColX = 75;                  // spec: white text at cols 8-12 (x in [75,125])
// The horizontal scrollbar starts at this character column, leaving a long
// stretch of ═ between the position text and the bar (the reference IDE runs
// the rule on well past the numbers before the scrollbar begins).
const int kHBarStartCol = 23;
} // namespace

// ONE text row: the position text, the ═ rule and the horizontal scrollbar all
// share the window's bottom border row, exactly as the reference IDE does. The
// scrollbar starts a cell after the numbers, so the rule is visible between the
// ╚ corner and the scrollbar and the two never overlap.
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
    // The scrollbar shares the border row but starts at a fixed column, leaving
    // a long run of ═ after the position text. It stops TWO cells short of the
    // right edge: the last two cells carry the green "─┘" corner.
    const int cellW = qMax(6, fontMetrics().horizontalAdvance(QLatin1Char('M')));
    const int left = qMin(width() - 3 * cellW, kHBarStartCol * cellW);
    const int right = width() - 2 * cellW - 1;
    if (right <= left) {
        m_hbar->setGeometry(QRect());
        return;
    }
    m_hbar->setGeometry(QRect(QPoint(left, 0), QPoint(right, height() - 1)));
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

    // The band owns the window's BOTTOM BORDER ROW, drawn with the box-drawing
    // GLYPHS so it matches the reference exactly:
    //   ╚ at the left (joins EditorHost's ║ column to the ═ rule),
    //   ═ across, breaking for the "L:C" position with a space either side,
    //   ═ again up to the scrollbar, and a GREEN ┘ at the right (joins the
    //   scrollbar row to the vertical scrollbar column).
    const int cellW = qMax(6, fontMetrics().horizontalAdvance(QLatin1Char('M')));
    const int rowH = height();
    const QString pos = cursorPosText();
    const int space = qMax(4, fontMetrics().horizontalAdvance(QLatin1Char(' ')));
    const int textW = fontMetrics().horizontalAdvance(pos);
    const int textLeft = kCursorColX;
    const int gapStart = qMax(cellW, textLeft - space);
    const int gapEnd = qMin(width() - cellW, textLeft + textW + space);

    // ╚ bottom-left corner.
    TvFrame::glyph(p, QRect(0, 0, cellW, rowH), QChar(0x255A), kWhite);
    // ═ from the corner to the position text.
    TvFrame::tileH(p, QRect(cellW, 0, gapStart - cellW, rowH), cellW, QChar(0x2550), kWhite);
    // ═ from the text on to the scrollbar start: a long run of rule, not the
    // single cell it used to be.
    const int hbarLeft = qMin(width() - 3 * cellW, kHBarStartCol * cellW);
    if (hbarLeft > gapEnd)
        TvFrame::tileH(p, QRect(gapEnd, 0, hbarLeft - gapEnd, rowH), cellW, QChar(0x2550), kWhite);
    // Bright-green corner reading "─┘": a light horizontal in the cell before
    // the corner, then the corner itself.
    TvFrame::glyph(p, QRect(width() - 2 * cellW, 0, cellW, rowH), QChar(0x2500), kGreen);
    TvFrame::glyph(p, QRect(width() - cellW, 0, cellW, rowH), QChar(0x2518), kGreen);

    // Cursor position, white, on the same row as the scrollbar.
    p.setPen(kWhite);
    p.drawText(QRect(textLeft, 0, textW + 2, rowH),
               Qt::AlignLeft | Qt::AlignVCenter, pos);

    layoutHBar();
}
