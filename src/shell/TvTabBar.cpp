// SPDX-License-Identifier: GPL-3.0-or-later
#include "TvTabBar.h"

#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

namespace {
const QColor kPanelGray(0xAA, 0xAA, 0xAA);
const QColor kFieldBlue(0x00, 0x00, 0xAA);
// Ink for the active chip's frame and the INACTIVE chip's label (white on the
// blue field). The active chip's frame used to be white too; 2026-11-26
// Xadium: white trim on dialogs is illegible with the CRT effects on, so the
// frame is black now.
const QColor kInk(Qt::white);
// A disabled chip's label: visible on the blue field, clearly dimmer than the
// active label.
const QColor kInkDim(0x55, 0x55, 0x55);
const QColor kBlack(Qt::black);
const int kStartX = 2;
} // namespace

TvTabBar::TvTabBar(QWidget* parent)
    : QWidget(parent)
{
    setAutoFillBackground(false);
    setCursor(Qt::ArrowCursor);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

int TvTabBar::cellW() const
{
    return fontMetrics().horizontalAdvance(QLatin1Char('M'));
}

int TvTabBar::cellH() const
{
    return fontMetrics().height();
}

void TvTabBar::addTab(const QString& text)
{
    m_tabs.append(text);
    m_rects.append(QRect());
    m_enabled.append(true);
    if (m_current < 0)
        m_current = 0;      // the first tab is current, silently
    layoutChips();
    updateGeometry();
    update();
}

void TvTabBar::setTabEnabled(int index, bool on)
{
    if (index < 0 || index >= m_enabled.size() || m_enabled.at(index) == on)
        return;
    m_enabled[index] = on;
    update();
}

bool TvTabBar::isTabEnabled(int index) const
{
    return index >= 0 && index < m_enabled.size() && m_enabled.at(index);
}

void TvTabBar::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_tabs.size() || index == m_current)
        return;
    m_current = index;
    update();
    emit currentChanged(m_current);
}

QRect TvTabBar::tabRect(int index) const
{
    if (index < 0 || index >= m_rects.size())
        return QRect();
    return m_rects.at(index);
}

QSize TvTabBar::sizeHint() const
{
    int w = kStartX;
    const QFontMetrics fm(font());
    for (const QString& t : m_tabs)
        w += qMax(6 * fm.horizontalAdvance(QLatin1Char('M')),
                  fm.horizontalAdvance(t) + 3 * fm.horizontalAdvance(QLatin1Char('M')));
    return QSize(w + 4, fm.height() + 2);
}

void TvTabBar::layoutChips()
{
    const int cw = cellW();
    const QFontMetrics fm(font());
    int x = kStartX;
    for (int i = 0; i < m_tabs.size(); ++i) {
        const int w = qMax(6 * cw, fm.horizontalAdvance(m_tabs.at(i)) + 3 * cw);
        m_rects[i] = QRect(x, 0, w, cellH() + 2);
        x += w;
    }
}

void TvTabBar::resizeEvent(QResizeEvent* ev)
{
    layoutChips();
    QWidget::resizeEvent(ev);
}

void TvTabBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    QFont f = font();
    f.setStyleStrategy(QFont::NoAntialias);
    p.setFont(f);

    for (int i = 0; i < m_rects.size(); ++i) {
        const QRect r = m_rects.at(i);
        if (r.isNull())
            continue;
        const bool active = (i == m_current);
        p.fillRect(r, active ? kPanelGray : kFieldBlue);
        if (active) {
            // Black frame, and the chip is open at the bottom so it reads as
            // continuous with the page underneath. (Was white - the chips are
            // dialog chrome and 2026-11-26 Xadium ruled white illegible with
            // the CRT effects on.)
            p.fillRect(QRect(r.left(), r.top(), r.width(), 1), kBlack);
            p.fillRect(QRect(r.left(), r.top(), 1, r.height()), kBlack);
            p.fillRect(QRect(r.right(), r.top(), 1, r.height()), kBlack);
        } else {
            p.fillRect(QRect(r.left(), r.bottom(), r.width(), 1), kBlack);
        }
        // Labels: black on the panel-grey active chip, white on the blue
        // inactive ones - a white label on the active chip would be white-on-
        // white once the frame is black.
        p.setPen(!isTabEnabled(i) ? kInkDim : (active ? kBlack : kInk));
        p.drawText(r, Qt::AlignCenter, m_tabs.at(i));
    }
}

void TvTabBar::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(ev);
        return;
    }
    for (int i = 0; i < m_rects.size(); ++i) {
        if (m_rects.at(i).contains(ev->pos())) {
            if (!isTabEnabled(i)) {
                // Clicked a chip that is not available (no selection, say):
                // swallow it and keep the current scope.
                ev->accept();
                return;
            }
            setCurrentIndex(i);
            ev->accept();
            return;
        }
    }
    QWidget::mousePressEvent(ev);
}
