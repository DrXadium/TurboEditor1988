// SPDX-License-Identifier: GPL-3.0-or-later
#include "BorlandStyle.h"
#include <QPainter>
#include <QStyleOption>
#include <QPushButton>

namespace {
const QColor kBevelLight(190, 190, 190);
const QColor kBevelDark(70, 70, 70);
const QColor kNavy(0x00, 0x00, 0xAA);   // TV window blue desktop
// TV-revision2 spec 5 palette: open menu title plate + glyph colors.
const QColor kBlackPlate(0x00, 0x00, 0x00);
const QColor kWhite(0xFF, 0xFF, 0xFF);
const QColor kBlack(0x00, 0x00, 0x00);
const QColor kHotRed(0xAA, 0x00, 0x00);
const QColor kSelectGreen(0x00, 0xAA, 0x00);   // spec 6 menu highlight
const QColor kMenuGray(0xAA, 0xAA, 0xAA);
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
        // the OPEN menu's title is a BLACK plate with WHITE text (rev2 spec 5),
        // its hotkey letter staying red.
        const bool selected = o->state & State_Selected;
        if (selected)
            p->fillRect(o->rect, kBlackPlate);   // spec 5: black plate
        const QColor base = selected ? kWhite : kBlack;
        const QColor hot = kHotRed;
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
            // TV/screenshot styling: the hotkey letter is underlined.
            if (firstVisible) {
                const int w = fm.horizontalAdvance(c);
                p->drawLine(x, baseline + 2, x + w - 1, baseline + 2);
            }
            x += fm.horizontalAdvance(c);
            firstVisible = false;
        }
        return;
    }
    if (ce == CE_MenuItem) {
        const auto* mi = qstyleoption_cast<const QStyleOptionMenuItem*>(opt);
        if (!mi)
            return QProxyStyle::drawControl(ce, opt, p, w);
        // Spec 6: separator = one FULL-WIDTH single black line (edge to edge).
        if (mi->menuItemType == QStyleOptionMenuItem::Separator) {
            p->fillRect(QRect(mi->rect.x(), mi->rect.center().y(),
                              mi->rect.width(), 1), kBlack);
            return;
        }
        const bool selected = mi->state & State_Selected;
        // Spec 6: selected row = solid green bar spanning the FULL width of
        // the dropdown, black text, hotkey letter stays red.
        p->fillRect(mi->rect, selected ? kSelectGreen : kMenuGray);
        const QFontMetrics fm(p->font());
        const int cell = qMax(1, fm.horizontalAdvance(QLatin1Char('M')));
        const int baseline = mi->rect.y()
            + (mi->rect.height() - fm.height()) / 2 + fm.ascent();
        // "label\tshortcut": Qt joins the shortcut with a tab; it is drawn
        // right-aligned with one cell of padding (spec 6: only F5 exists).
        QString label = mi->text;
        QString shortcut;
        const int tab = label.indexOf(QLatin1Char('\t'));
        if (tab >= 0) {
            shortcut = label.mid(tab + 1);
            label.truncate(tab);
        }
        // Check slot in column 1, item text from column 2 (one blank cell of
        // left padding, then the check column).
        if (mi->checked) {
            p->setPen(kBlack);
            p->drawText(QPoint(mi->rect.x() + cell, baseline),
                        QStringLiteral("\u2713"));
        }
        int x = mi->rect.x() + 2 * cell;
        bool firstVisible = true;
        for (int i = 0; i < label.size(); ++i) {
            const QChar c = label.at(i);
            if (c == QLatin1Char('&')) {
                if (i + 1 < label.size() && label.at(i + 1) == QLatin1Char('&')) {
                    p->setPen(firstVisible ? kHotRed : kBlack);
                    p->drawText(QPoint(x, baseline), QStringLiteral("&"));
                    x += fm.horizontalAdvance(QLatin1Char('&'));
                    firstVisible = false;
                    ++i;
                }
                continue;   // mnemonic marker
            }
            p->setPen(firstVisible ? kHotRed : kBlack);
            p->drawText(QPoint(x, baseline), QString(c));
            x += fm.horizontalAdvance(c);
            firstVisible = false;
        }
        if (!shortcut.isEmpty()) {
            p->setPen(kBlack);
            const int sw = fm.horizontalAdvance(shortcut);
            p->drawText(QPoint(mi->rect.right() - cell - sw, baseline), shortcut);
        }
        return;
    }
    QProxyStyle::drawControl(ce, opt, p, w);
}

void BorlandStyle::drawPrimitive(PrimitiveElement pe, const QStyleOption* opt,
                                 QPainter* p, const QWidget* w) const
{
    if (pe == PE_PanelMenu) {
        // Spec 6: dropdown panel = solid light gray, NO line-drawing border.
        // Painted here (not QSS) so QStyleSheetStyle never takes over item
        // painting - see the CE_MenuItem branch.
        p->fillRect(opt->rect, kMenuGray);
        return;
    }
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
