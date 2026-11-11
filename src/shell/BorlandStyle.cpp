// SPDX-License-Identifier: GPL-3.0-or-later
#include "BorlandStyle.h"
#include <QPainter>
#include <QStyleOption>
#include <QPushButton>

namespace {
const QColor kBevelLight(190, 190, 190);
const QColor kBevelDark(70, 70, 70);
const QColor kNavy(0x1A, 0x1A, 0xAA);   // TV window blue desktop
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

int BorlandStyle::pixelMetric(PixelMetric metric, const QStyleOption* option,
                              const QWidget* widget) const
{
    // Turbo Vision menu bar: ~4 spaces of visible gap between titles
    // (the default packs them to ~6px). Tests: menu_titles_separated_by_four_spaces
    // and menu_items_use_tv_column_placement bound this window.
    if (metric == PM_MenuBarItemSpacing)
        return 30;
    return QProxyStyle::pixelMetric(metric, option, widget);
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
    if (ce == CE_MenuBarItem) {
        const auto* o = qstyleoption_cast<const QStyleOptionMenuItem*>(opt);
        if (!o)
            return QProxyStyle::drawControl(ce, opt, p, w);
        // Turbo Vision menu bar: FIRST visible letter hotkey red, rest black;
        // a selected item inverts (black plate from borland.qss, gray glyphs).
        const bool selected = o->state & State_Selected;
        if (selected)
            p->fillRect(o->rect, QColor(0, 0, 0));   // inverse plate (was QSS)
        const QColor base = selected ? QColor(0xAA, 0xAA, 0xAA) : QColor(0, 0, 0);
        const QColor hot = selected ? QColor(0xAA, 0xAA, 0xAA)
                                    : QColor(0xC4, 0x1E, 0x1E);
        const QFontMetrics fm(p->font());
        // Vertical centering: baseline = top + (h - (ascent+descent))/2 + ascent.
        const int baseline = o->rect.y()
            + (o->rect.height() - fm.height()) / 2 + fm.ascent();
        int x = o->rect.x();
        bool firstVisible = true;
        const QString title = o->text;
        for (int i = 0; i < title.size(); ++i) {
            const QChar c = title.at(i);
            if (c == QLatin1Char('&')) {
                if (i + 1 < title.size() && title.at(i + 1) == QLatin1Char('&')) {
                    // "&&" renders a literal '&'
                    p->setPen(firstVisible ? hot : base);
                    p->drawText(QPoint(x, baseline), QStringLiteral("&"));
                    x += fm.horizontalAdvance(QLatin1Char('&'));
                    firstVisible = false;
                    ++i;
                }
                continue;   // mnemonic marker; the next char renders normally
            }
            p->setPen(firstVisible ? hot : base);
            p->drawText(QPoint(x, baseline), QString(c));
            x += fm.horizontalAdvance(c);
            firstVisible = false;
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
