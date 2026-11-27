// SPDX-License-Identifier: GPL-3.0-or-later
#include "TvColorSwatch.h"

#include <QPainter>
#include <QPaintEvent>

namespace {
const QColor kBorder(Qt::black);
const QColor kFieldBlue(0x00, 0x00, 0xAA);
} // namespace

TvColorSwatch::TvColorSwatch(QWidget* parent) : QPushButton(parent)
{
    setAutoDefault(false);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
    setMinimumSize(1, 1);
    // The app-wide QPushButton QSS paints a green plate with padding; this
    // widget owns its pixels.
    setStyleSheet(QStringLiteral(
        "background: transparent; border: none; padding: 0px;"
        " min-width: 0px; min-height: 0px;"));
}

void TvColorSwatch::setColor(const QColor& colour)
{
    if (!colour.isValid() || m_colour == colour)
        return;
    m_colour = colour;
    update();
}

QSize TvColorSwatch::sizeHint() const
{
    return QSize(44, 18);
}

void TvColorSwatch::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    const QRect field = rect().adjusted(0, 0, -1, -1);
    if (field.isEmpty())
        return;
    p.fillRect(field, m_colour.isValid() ? m_colour : kFieldBlue);
    p.setPen(kBorder);
    p.drawRect(field);
}
