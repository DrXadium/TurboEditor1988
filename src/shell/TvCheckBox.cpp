// SPDX-License-Identifier: GPL-3.0-or-later
#include "TvCheckBox.h"
#include "TvFrame.h"

#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

namespace {
const QColor kInk(Qt::white);
// The control reads [ ] when empty and [square] when checked: the BRACKETS are
// black, the filled square inside them is the TV green.
const QColor kBracket(Qt::black);
const QColor kSquare(0x00, 0xAA, 0x00);
const int kGap = 6;   // daylight between the box and the label
} // namespace

TvCheckBox::TvCheckBox(const QString& text, QWidget* parent)
    : QWidget(parent)
    , m_text(text)
{
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::ArrowCursor);
    layoutBox();
}

void TvCheckBox::setChecked(bool on)
{
    if (m_checked == on)
        return;
    m_checked = on;
    update();
    emit toggled(m_checked);
}

QSize TvCheckBox::sizeHint() const
{
    const QFontMetrics fm(font());
    const int box = fm.height();
    return QSize(box + kGap + fm.horizontalAdvance(m_text) + 4, box + 4);
}

void TvCheckBox::layoutBox()
{
    const int box = fontMetrics().height();
    m_boxRect = QRect(2, (height() - box) / 2, box, box);
}

void TvCheckBox::resizeEvent(QResizeEvent* ev)
{
    layoutBox();
    QWidget::resizeEvent(ev);
}

void TvCheckBox::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // Bracket box in black, the frame's own double rule so it reads at frame
    // weight. Checked fills the space between the inner rules with green -
    // TvFrame::boxInterior derives that space from the same gap the brackets
    // are drawn with, so the fill cannot drift out from under them.
    TvFrame::box(p, m_boxRect, kBracket);
    if (m_checked) {
        const QRect inner = TvFrame::boxInterior(m_boxRect);
        if (inner.width() > 0 && inner.height() > 0)
            p.fillRect(inner, kSquare);
    }

    QFont f = font();
    f.setStyleStrategy(QFont::NoAntialias);
    p.setFont(f);
    p.setPen(kInk);
    p.drawText(QRect(m_boxRect.right() + kGap, 0, width() - m_boxRect.right() - kGap, height()),
               Qt::AlignLeft | Qt::AlignVCenter, m_text);
}

void TvCheckBox::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::LeftButton) {
        setChecked(!m_checked);
        ev->accept();
        return;
    }
    QWidget::mousePressEvent(ev);
}

void TvCheckBox::keyPressEvent(QKeyEvent* ev)
{
    if (ev->key() == Qt::Key_Space) {
        setChecked(!m_checked);
        ev->accept();
        return;
    }
    QWidget::keyPressEvent(ev);
}
