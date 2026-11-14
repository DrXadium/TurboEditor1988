// SPDX-License-Identifier: GPL-3.0-or-later
#include "BorlandMenu.h"
#include <QPainter>

namespace {
const QColor kMenuGray(0xAA, 0xAA, 0xAA);
const QColor kSelectGreen(0x00, 0xAA, 0x00);   // spec 6 highlight
const QColor kBlack(0x00, 0x00, 0x00);
const QColor kHotRed(0xAA, 0x00, 0x00);
} // namespace

BorlandMenu::BorlandMenu(QWidget* parent) : QMenu(parent)
{
    setWindowFlags(windowFlags() | Qt::NoDropShadowWindowHint);
    // Reference screenshot: the gray panel carries a SINGLE black border set
    // WELL INSIDE the panel edge (the gray runs outside it on all four sides),
    // with a little padding between that border and the item text. Both the
    // inset and the padding are content margins, so Qt sizes the popup to
    // include them and every actionGeometry is already inside the border.
    const QFontMetrics fm = fontMetrics();
    const int cell = qMax(1, fm.horizontalAdvance(QLatin1Char('M')));
    const int m = borderInset() + borderWidth() + 2;   // gray margin + stroke + padding
    setContentsMargins(m, m, m, m);
}

void BorlandMenu::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    // Solid gray panel, flat fills only.
    p.fillRect(rect(), kMenuGray);
    // Black ASCII border INSET inside the panel (gray runs outside it on every
    // side). Drawn as four filled strips rather than a stroked rect: a 2px pen
    // straddles the path and lands on half pixels, which shows up as a ragged
    // 1/2px line at this size.
    const int inset = borderInset();
    const int bw = borderWidth();
    const int right = width() - inset - bw;
    const int bottom = height() - inset - bw;
    p.fillRect(QRect(inset, inset, width() - 2 * inset, bw), kBlack);          // top
    p.fillRect(QRect(inset, bottom, width() - 2 * inset, bw), kBlack);         // bottom
    p.fillRect(QRect(inset, inset, bw, height() - 2 * inset), kBlack);         // left
    p.fillRect(QRect(right, inset, bw, height() - 2 * inset), kBlack);         // right

    const QFontMetrics fm = fontMetrics();
    const int cell = qMax(1, fm.horizontalAdvance(QLatin1Char('M')));
    // Item band spans the area just inside the stroke, edge to edge.
    const int contentLeft = inset + bw;
    const int contentRight = right - 1;

    for (QAction* a : actions()) {
        if (!a->isVisible())
            continue;
        const QRect r = actionGeometry(a);

        // Separator: one full-width single black line across the content area.
        if (a->isSeparator()) {
            // Divider matches the border stroke thickness (reference: the rule
            // and the box read as the same weight).
            p.fillRect(QRect(contentLeft, r.center().y() - bw / 2,
                             contentRight - contentLeft + 1, bw), kBlack);
            continue;
        }

        const bool selected = (a == activeAction()) && a->isEnabled();
        // Spec 6: the highlighted row is a solid green bar spanning the FULL
        // width of the dropdown, exactly one cell tall.
        // The highlight sits inside the border with a small gray gap on BOTH
        // sides (reference screenshot: ~3 px of panel gray either side of the
        // green bar), so it never butts up against the stroke.
        const int hi = highlightInset();
        p.fillRect(QRect(contentLeft + hi, r.top(),
                         contentRight - contentLeft + 1 - 2 * hi, r.height()),
                   selected ? kSelectGreen : kMenuGray);

        const int baseline = r.y() + (r.height() - fm.height()) / 2 + fm.ascent();

        // Task 26 (user): the ✓ goes at the RIGHT end of the item, one cell in
        // from the content edge. Drawn at the panel's left edge it collided with
        // the label; the label's column stays clean now, so the check column is
        // reserved on the right instead.
        const bool showCheck = a->isCheckable() && a->isChecked();
        int shortcutRight = contentRight - cell;
        if (showCheck) {
            const int cw = fm.horizontalAdvance(QStringLiteral("\u2713"));
            p.setPen(kBlack);
            p.drawText(QPoint(contentRight - cell - cw + 1, baseline),
                       QStringLiteral("\u2713"));
            // A shortcut on a checked item moves left of the check, so the two
            // never overlap.
            shortcutRight -= cw + cell;
        }

        // "label\tshortcut" (Qt's convention for menu display text). The
        // shortcut is drawn black, right-aligned, one cell of right padding.
        QString label = a->text();
        QString shortcut;
        const int tab = label.indexOf(QLatin1Char('\t'));
        if (tab >= 0) {
            shortcut = label.mid(tab + 1);
            label.truncate(tab);
        } else if (!a->shortcut().isEmpty()) {
            shortcut = a->shortcut().toString(QKeySequence::NativeText);
        }

        int x = contentLeft + cell;
        bool firstVisible = true;
        const QColor textColor = selected || a->isEnabled() ? kBlack : kBlack;
        for (int i = 0; i < label.size(); ++i) {
            const QChar c = label.at(i);
            if (c == QLatin1Char('&')) {
                if (i + 1 < label.size() && label.at(i + 1) == QLatin1Char('&')) {
                    p.setPen(firstVisible ? kHotRed : textColor);
                    p.drawText(QPoint(x, baseline), QStringLiteral("&"));
                    x += fm.horizontalAdvance(QLatin1Char('&'));
                    firstVisible = false;
                    ++i;
                }
                continue;   // mnemonic marker
            }
            // Spec 6: the hotkey letter stays RED on the green bar too.
            p.setPen(firstVisible ? kHotRed : textColor);
            p.drawText(QPoint(x, baseline), QString(c));
            // Screenshot styling: the hotkey letter is underlined.
            if (firstVisible) {
                const int w = fm.horizontalAdvance(c);
                p.drawLine(x, baseline + 2, x + w - 1, baseline + 2);
            }
            x += fm.horizontalAdvance(c);
            firstVisible = false;
        }

        if (!shortcut.isEmpty()) {
            p.setPen(kBlack);
            const int sw = fm.horizontalAdvance(shortcut);
            p.drawText(QPoint(shortcutRight - sw, baseline), shortcut);
        }
    }
}
