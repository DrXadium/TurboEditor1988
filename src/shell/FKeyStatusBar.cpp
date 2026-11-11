// SPDX-License-Identifier: GPL-3.0-or-later
#include "FKeyStatusBar.h"
#include <QLabel>
#include <QPainter>
#include <QRegularExpression>

namespace {
// Turbo Vision status line rich text: key names red (#C41E1E), words black.
// The status text is a fixed ASCII literal, so no HTML escaping is needed.
QString tvRich(const QString& text)
{
    static const QRegularExpression keyRe(
        QStringLiteral("\\b(F\\d{1,2}|Ctrl\\+?[A-Za-z0-9]+)\\b"));
    QString html;
    int last = 0;
    auto it = keyRe.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        html += QStringLiteral("<span style=\"color:#000000;\">")
              + text.mid(last, m.capturedStart() - last) + QStringLiteral("</span>");
        html += QStringLiteral("<span style=\"color:#C41E1E;\">")
              + m.captured() + QStringLiteral("</span>");
        last = m.capturedEnd();
    }
    html += QStringLiteral("<span style=\"color:#000000;\">")
          + text.mid(last) + QStringLiteral("</span>");
    return html;
}
const QColor kBarGray(0xAA, 0xAA, 0xAA);
} // namespace

FKeyStatusBar::FKeyStatusBar(QWidget* parent) : QStatusBar(parent)
{
    setObjectName("fkeyStatusBar");
    setAutoFillBackground(false);   // paintEvent draws the gray bar itself
    // Spec 7: flat solid gray row across the full width — no resize grip
    // (its default blue handle broke the flat fill at the right edge).
    setSizeGripEnabled(false);
    setFKeys(QStringLiteral("F2 Save  F3 Open  F5 Zoom  F6 Next  F10 Menu"));
}

void FKeyStatusBar::setFKeys(const QString& text)
{
    m_text = text;
    QLabel* lab = findChild<QLabel*>(QStringLiteral("fkeyLabel"));
    if (!lab) {
        lab = new QLabel(this);
        lab->setObjectName(QStringLiteral("fkeyLabel"));
        lab->setTextFormat(Qt::RichText);
        lab->setAutoFillBackground(false);
        lab->setAttribute(Qt::WA_TranslucentBackground, true);
        // Transparent label background: the app-wide QWidget rule gives labels
        // a window-blue base that would otherwise paint over the gray bar.
        lab->setStyleSheet(QStringLiteral("QLabel { background: transparent; }"));
        // Left margin so the first F-key sits near column 12 (spec 7: x≈8..20),
        // not flush against the window edge.
        lab->setContentsMargins(12, 0, 0, 0);
        addWidget(lab, 1);
    }
    lab->setText(tvRich(text));
}

// Explicitly fill the status bar TV light-gray (palette alone was being
// overridden by the app-wide window-blue QWidget rule).
void FKeyStatusBar::paintEvent(QPaintEvent* ev)
{
    QPainter p(this);
    p.fillRect(rect(), kBarGray);
    QStatusBar::paintEvent(ev);
}
