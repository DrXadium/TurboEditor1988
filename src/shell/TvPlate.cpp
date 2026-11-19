// SPDX-License-Identifier: GPL-3.0-or-later
#include "TvPlate.h"

#include <QEnterEvent>
#include <QEvent>
#include <QPainter>

namespace {
// The plate palette, in the TV/DOS greens the rest of the chrome uses.
const QColor kPlate(0x00, 0xAA, 0x00);
const QColor kPlateHover(0x1F, 0xBE, 0x1F);
// The press flash: DOS bright green, with black ink so the label stays
// readable on the light plate.
const QColor kPlatePressed(0x55, 0xFF, 0x55);
const QColor kBlack(Qt::black);
} // namespace

TvPlate::TvPlate(const QString& text, QWidget* parent)
    : QPushButton(text, parent)
{
    setAutoDefault(false);   // the modals close on Return themselves
    setAutoFillBackground(false);
    // This widget paints itself, so the app-wide QPushButton QSS must not apply
    // to it. Its `min-width: 60px; padding: 3px 14px` inflates the minimum size
    // to 88 px, and setGeometry then clamps the widget WIDER than the plate the
    // parent asked for - the plate spills past its own shadow band and covers
    // the shadow it is supposed to sit on. (A widget-level rule wins over the
    // application one; the selector is unscoped because the parents set an
    // objectName after construction.)
    setStyleSheet(QStringLiteral(
        "background: transparent; border: none; padding: 0px;"
        " min-width: 0px; min-height: 0px;"));
    // The parents lay plates out by hand, so their geometry is authoritative.
    setMinimumSize(1, 1);
}

void TvPlate::setInk(const QColor& ink)
{
    m_ink = ink;
    update();
}

void TvPlate::setBackdrop(const QColor& colour)
{
    m_backdrop = colour;
    update();
}

void TvPlate::setShadowOffset(int offset)
{
    m_offset = qMax(0, offset);
    update();
}

QRect TvPlate::plateRect() const
{
    return rect().adjusted(0, 0, -m_offset, -m_offset);
}

void TvPlate::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QRect plate = plateRect();
    if (plate.width() <= 0 || plate.height() <= 0)
        return;

    // The widget's rect is the plate plus the shadow band. Painting the panel
    // first is what lets the plate travel: the band it vacates shows the panel.
    p.fillRect(rect(), m_backdrop);
    p.fillRect(plate.translated(m_offset, m_offset), kBlack);

    // Down: onto the shadow. Hover: one step brighter. Otherwise the flat plate.
    const bool down = isDown();
    const QRect face = down ? plate.translated(m_offset, m_offset) : plate;
    p.fillRect(face, down ? kPlatePressed : (underMouse() ? kPlateHover : kPlate));

    QFont f = font();
    f.setStyleStrategy(QFont::NoAntialias);
    p.setFont(f);
    p.setPen(down ? kBlack : m_ink);
    p.drawText(face, Qt::AlignCenter, text());
}

void TvPlate::enterEvent(QEnterEvent* ev)
{
    QPushButton::enterEvent(ev);
    update();
}

void TvPlate::leaveEvent(QEvent* ev)
{
    QPushButton::leaveEvent(ev);
    update();
}
