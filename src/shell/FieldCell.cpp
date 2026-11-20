// SPDX-License-Identifier: GPL-3.0-or-later
#include "FieldCell.h"

#include <QLineEdit>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

namespace {

const QColor kFieldBlue(0x00, 0x00, 0xAA);
const QColor kYellow(0xFF, 0xFF, 0x55);
const int kRing = 2;
const int kFieldMinWidth = 220;

} // namespace

FieldCell::FieldCell(QWidget* parent)
    : QWidget(parent)
{
    setAutoFillBackground(false);
}

QSize FieldCell::sizeHint() const
{
    return QSize(kFieldMinWidth, fontMetrics().height() + 2 * kRing);
}

QSize FieldCell::minimumSizeHint() const
{
    return sizeHint();
}

void FieldCell::setEdit(QLineEdit* edit)
{
    m_edit = edit;
}

void FieldCell::setActive(bool on)
{
    if (m_active == on)
        return;
    m_active = on;
    update();
}

void FieldCell::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), kFieldBlue);
    if (m_active) {
        // Four hard 2 px bands: exact pixels, no pen-join surprises.
        p.fillRect(QRect(0, 0, width(), kRing), kYellow);
        p.fillRect(QRect(0, height() - kRing, width(), kRing), kYellow);
        p.fillRect(QRect(0, 0, kRing, height()), kYellow);
        p.fillRect(QRect(width() - kRing, 0, kRing, height()), kYellow);
    }
}

void FieldCell::resizeEvent(QResizeEvent*)
{
    if (m_edit)
        m_edit->setGeometry(rect().adjusted(kRing, kRing, -kRing, -kRing));
}

void styleAsTvField(QLineEdit* edit)
{
    if (!edit)
        return;
    edit->setStyleSheet(QStringLiteral(
        "QLineEdit { background-color: #0000AA; color: #FFFFFF;"
        " border: none; padding: 0px 3px; }"));
    edit->setFrame(false);
    edit->setAutoFillBackground(true);
}
