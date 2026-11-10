// SPDX-License-Identifier: GPL-3.0-or-later
#include "BorlandStyle.h"
#include <QPainter>
#include <QStyleOption>
#include <QPushButton>

namespace {
const QColor kBevelLight(190, 190, 190);
const QColor kBevelDark(70, 70, 70);
const QColor kNavy(0, 0, 128);
}

BorlandStyle::BorlandStyle(QStyle* base) : QProxyStyle(base) {}

// Raised 3D bevel around a rect (light top-left, dark bottom-right).
static void drawRaisedBevel(QPainter* p, const QRect& r)
{
    p->save();
    p->setPen(kBevelLight);
    p->drawLine(r.left(), r.top(), r.right() - 1, r.top());
    p->drawLine(r.left(), r.top(), r.left(), r.bottom() - 1);
    p->setPen(kBevelDark);
    p->drawLine(r.right(), r.top() + 1, r.right(), r.bottom());
    p->drawLine(r.left() + 1, r.bottom(), r.right(), r.bottom());
    p->restore();
}

void BorlandStyle::drawControl(ControlElement ce, const QStyleOption* opt,
                               QPainter* p, const QWidget* w) const
{
    if (ce == CE_PushButtonBevel) {
        // Borland push buttons: flat fill + raised bevel.
        const auto* o = qstyleoption_cast<const QStyleOptionButton*>(opt);
        p->fillRect(opt->rect, opt->palette.base().color());
        drawRaisedBevel(p, opt->rect);
        if (o && !o->text.isEmpty()) {
            p->setPen(opt->palette.text().color());
            p->drawText(opt->rect, Qt::AlignCenter, o->text);
        }
        return;
    }
    QProxyStyle::drawControl(ce, opt, p, w);
}

void BorlandStyle::drawPrimitive(PrimitiveElement pe, const QStyleOption* opt,
                                 QPainter* p, const QWidget* w) const
{
    if (pe == PE_Frame || pe == PE_FrameMenu || pe == PE_FrameTabWidget) {
        // Sunken/inset frame around panels.
        p->save();
        p->setPen(kBevelDark);
        p->drawRect(opt->rect.adjusted(0, 0, -1, -1));
        p->setPen(kBevelLight);
        p->drawRect(opt->rect.adjusted(1, 1, -2, -2));
        p->restore();
        return;
    }
    QProxyStyle::drawPrimitive(pe, opt, p, w);
}
