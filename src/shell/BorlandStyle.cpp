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
// House block shadow behind the recovery-bar action plates (TvFileDock /
// TvModal kButtonShadow = 4): the plate is the button rect inset by this on
// right+bottom, and the black shadow sits at +4,+4 behind it.
const int kButtonShadow = 4;
// Grey breathing room right of a message-bar plate: the shadow band alone
// (4px) is exactly the width of the black arm, so without it the arm sits
// flush against the next plate. One 8px cell keeps the pair readable.
const int kButtonGap = 8;
}

// Scoped the same way PE_PanelButtonTool and CE_ToolButtonLabel are scoped:
// only buttons whose PARENT is the crash-recovery bar get the house shadow.
static bool messageBarButton(const QWidget* w)
{
    const QWidget* parent = w ? w->parentWidget() : nullptr;
    if (!parent)
        return false;
    const QByteArray cls = parent->metaObject()->className();
    return cls == "KateMessageWidget" || cls == "KMessageWidget";
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

QSize BorlandStyle::sizeFromContents(ContentsType type, const QStyleOption* opt,
                                    const QSize& size, const QWidget* w) const
{
    QSize s = QProxyStyle::sizeFromContents(type, opt, size, w);
    // Menu-bar titles carry symmetric padding so the selected plate's green
    // reads evenly on both sides of the label.
    if (type == CT_MenuBarItem)
        s.rwidth() += 2 * menuBarItemPad();
    // The crash-recovery bar lays its action plates flush in a row, but each
    // plate reserves a 4px black block shadow INSIDE its own rect (see
    // PE_PanelButtonTool). With no extra room the shadow arm of one plate lands
    // on the green edge of the next - the 2026-11-24 "shadows are touching the
    // adjacent buttons" report. Reserve pane grey to the right of the button
    // rect so the arm has somewhere to live and never touches its neighbour.
    // Only for buttons whose parent is the message bar: every other
    // QToolButton in the app keeps its size.
    if (type == CT_ToolButton && w && messageBarButton(w))
        s.rwidth() += kButtonGap;
    return s;
}

int BorlandStyle::pixelMetric(PixelMetric metric, const QStyleOption* option,
                              const QWidget* widget) const
{
    // Turbo Vision menu bar: titles separated by ~4 spaces. Spaces are CELLS,
    // so derive from the widget's actual font rather than a pixel constant - a
    // fixed 30px only matched the old, wider face and pushed the gap past the
    // spec once the default became the 8px-cell VGA font. The action rect adds
    // its own ~2-cell padding on each side, so 2 cells of spacing lands the
    // visible gap at ~4 cells total.
    if (metric == PM_MenuBarItemSpacing) {
        int cell = 8;
        if (widget) {
            const QFontMetrics fm(widget->font());
            const int adv = fm.horizontalAdvance(QLatin1Char('M'));
            if (adv > 0)
                cell = adv;
        }
        return 2 * cell;
    }
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
        // Turbo Vision menu bar: FIRST visible letter hotkey red, rest black.
        // The OPEN menu's title is a GREEN plate with BLACK text (reference
        // screenshot: "Window" is green while its dropdown is open) - this
        // supersedes rev2 spec 5's black-plate/white-text rule.
        const bool selected = o->state & State_Selected;
        if (selected)
            p->fillRect(o->rect, kSelectGreen);
        const QColor base = kBlack;
        const QColor hot = kHotRed;
        const QFontMetrics fm(p->font());
        // Vertical centering: baseline = top + (h - (ascent+descent))/2 + ascent.
        const int baseline = o->rect.y()
            + (o->rect.height() - fm.height()) / 2 + fm.ascent();
        // Centre the label in the item rect so the selected plate shows the
        // SAME amount of green before the text as after it. A fixed left pad
        // was not enough: QMenuBar's own spacing left ~4px on the left against
        // ~12px on the right.
        const QString title = o->text;
        QString plain;
        for (int i = 0; i < title.size(); ++i) {
            if (title.at(i) == QLatin1Char('&')) {
                if (i + 1 < title.size() && title.at(i + 1) == QLatin1Char('&')) {
                    plain += QLatin1Char('&');
                    ++i;
                }
                continue;
            }
            plain += title.at(i);
        }
        const int textW = fm.horizontalAdvance(plain);
        int x = o->rect.x() + qMax(0, (o->rect.width() - textW) / 2);
        bool firstVisible = true;
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
    if (ce == CE_ToolButtonLabel) {
        // The recovery bar's action plates carry BLACK label ink on the green
        // plate. Without a QSS rule the button's text would come from
        // QPalette::ButtonText (the app's editor yellow here), so paint it black
        // explicitly. The label option is a QStyleOptionToolButton, not a
        // QStyleOptionButton - guard the cast (an unguarded deref crashed here)
        // and delegate layout (icon/arrow/centering) to the base style with a
        // palette whose text is black.
        const QWidget* parent = w ? w->parentWidget() : nullptr;
        const QByteArray cls = parent ? QByteArray(parent->metaObject()->className())
                                      : QByteArray();
        if (cls == "KateMessageWidget" || cls == "KMessageWidget") {
            if (const auto* to = qstyleoption_cast<const QStyleOptionToolButton*>(opt)) {
                QStyleOptionToolButton o(*to);
                // The rect carries the shadow band + reserved gap on the right,
                // so centring on it would push the label off the plate.
                o.rect = QRect(o.rect.left(), o.rect.top(),
                               o.rect.width() - kButtonShadow - kButtonGap,
                               o.rect.height() - kButtonShadow);
                o.palette.setColor(QPalette::ButtonText, kBlack);
                QProxyStyle::drawControl(ce, &o, p, w);
            } else {
                QProxyStyle::drawControl(ce, opt, p, w);
            }
            return;
        }
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
    // The crash-recovery bar's action plates (View Changes / Recover Data /
    // Discard). The reference plate (TvModal/TvFileDock/TvPlate) casts a black
    // block shadow as a rect TRANSLATED (+4,+4) BEHIND the green plate, so the
    // shadow is hidden by the plate at the top-right and bottom-left - those
    // two corners show the pane's grey, which is what makes it read as a hard
    // DOS block shadow rather than a border (TvPlate::paintEvent:
    // fillRect(plate.translated(4,4)) then fillRect(plate)).
    //
    // QSS cannot do this: a `border` paints a full-height arm flush to every
    // plate edge (black in exactly those two corners) and Qt ignores
    // box-shadow. The style engine runs DURING the button's paint - the one
    // place an event filter cannot reach, because a filter always runs before
    // the handler and is overwritten. So the notch is painted here.
    //
    // Scoped to buttons whose PARENT is the message bar so every other
    // QToolButton in the app keeps its normal panel.
    if (pe == PE_PanelButtonTool) {
        const QWidget* parent = w ? w->parentWidget() : nullptr;
        const QByteArray cls = parent ? QByteArray(parent->metaObject()->className())
                                      : QByteArray();
        if (cls == "KateMessageWidget" || cls == "KMessageWidget") {
            // Button rect = PLATE + the 4px shadow band (TvPlate's model): the
            // green plate is the rect inset 4px on right+bottom, the black
            // shadow fills the vacated band at +4,+4, and only the offset L
            // survives - hidden behind the plate at top-right/bottom-left.
            // The plate keeps its natural width: the button rect is the plate
            // plus the shadow band plus the grey gap sizeFromContents reserved.
            const QRect plate(opt->rect.left(), opt->rect.top(),
                              opt->rect.width() - kButtonShadow - kButtonGap,
                              opt->rect.height() - kButtonShadow);
            if (plate.width() > 0 && plate.height() > 0) {
                p->fillRect(opt->rect, kMenuGray);      // band backdrop / notch grey
                p->fillRect(plate.translated(kButtonShadow, kButtonShadow), kBlack);
                p->fillRect(plate, kSelectGreen);
            }
            // The label is drawn by CE_ToolButtonLabel (black ink via the
            // QSS `color` on the bar); nothing to do for the panel here.
            return;
        }
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
