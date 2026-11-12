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
            p.fillRect(QRect(contentLeft, r.center().y(),
                             contentRight - contentLeft + 1, 1), kBlack);
            continue;
        }

        const bool selected = (a == activeAction()) && a->isEnabled();
        // Spec 6: the highlighted row is a solid green bar spanning the FULL
        // width of the dropdown, exactly one cell tall.
        // +1 on the width so the bar reaches the last pixel inside the border
        // (fillRect's width is exclusive; the old exclusive end left a 1 px gray
        // sliver against the border's right stroke).
        p.fillRect(QRect(contentLeft, r.top(),
                         contentRight - contentLeft + 1, r.height()),
                   selected ? kSelectGreen : kMenuGray);

        const int baseline = r.y() + (r.height() - fm.height()) / 2 + fm.ascent();

        // Check slot in column 1, item text from column 2 (one blank cell of
        // left padding, then the check column).
        if (a->isCheckable() && a->isChecked()) {
            p.setPen(kBlack);
            p.drawText(QPoint(cell, baseline), QStringLiteral("\u2713"));
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
            p.drawText(QPoint(contentRight - cell - sw, baseline), shortcut);
        }
    }
}
