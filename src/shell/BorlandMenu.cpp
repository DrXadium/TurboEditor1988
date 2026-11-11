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
}

void BorlandMenu::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    // Spec 6: solid gray panel, no border, flat fills only.
    p.fillRect(rect(), kMenuGray);

    const QFontMetrics fm = fontMetrics();
    const int cell = qMax(1, fm.horizontalAdvance(QLatin1Char('M')));

    for (QAction* a : actions()) {
        if (!a->isVisible())
            continue;
        const QRect r = actionGeometry(a);

        // Spec 6: separator = one full-width single black line, edge to edge.
        if (a->isSeparator()) {
            p.fillRect(QRect(0, r.center().y(), width(), 1), kBlack);
            continue;
        }

        const bool selected = (a == activeAction()) && a->isEnabled();
        // Spec 6: the highlighted row is a solid green bar spanning the FULL
        // width of the dropdown, exactly one cell tall.
        p.fillRect(QRect(0, r.top(), width(), r.height()),
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

        int x = 2 * cell;
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
            p.drawText(QPoint(width() - cell - sw, baseline), shortcut);
        }
    }
}
