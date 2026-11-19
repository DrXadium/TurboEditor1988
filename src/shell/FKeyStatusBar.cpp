// SPDX-License-Identifier: GPL-3.0-or-later
#include "FKeyStatusBar.h"
#include <QLabel>
#include <QPainter>
#include <QRegularExpression>

namespace {
// Turbo Vision status line rich text: key names red (#AA0000), words black.
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
        html += QStringLiteral("<span style=\"color:#AA0000;\">")
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
    // Seed a zeroed count line so the bar is never blank; the composition
    // root overwrites it with real counts.
    setCounts(0, 0);
}

void FKeyStatusBar::ensureLabel()
{
    QLabel* lab = findChild<QLabel*>(QStringLiteral("fkeyLabel"));
    if (lab)
        return;
    lab = new QLabel(this);
    lab->setObjectName(QStringLiteral("fkeyLabel"));
    lab->setTextFormat(Qt::RichText);
    lab->setAutoFillBackground(false);
    lab->setAttribute(Qt::WA_TranslucentBackground, true);
    // Transparent label background: the app-wide QWidget rule gives labels
    // a window-blue base that would otherwise paint over the gray bar.
    lab->setStyleSheet(QStringLiteral("QLabel { background: transparent; }"));
    // Left margin so the line's first ink sits near column 12 (spec 7: x≈8..24),
    // not flush against the window edge.
    lab->setContentsMargins(12, 0, 0, 0);
    // The stretch-1 slot is far wider than the line: without a left alignment
    // the count would float centered, not start at column 12.
    lab->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    addWidget(lab, 1);
}

void FKeyStatusBar::refreshLabel()
{
    ensureLabel();
    QLabel* lab = findChild<QLabel*>(QStringLiteral("fkeyLabel"));
    if (!lab)
        return;
    const QString text = displayedText();
    // The count line arrives as explicit rich HTML (labels black, values
    // red); tvRich() would re-wrap it and only knows how to RED key names.
    // The context hint is plain text and still needs that treatment.
    lab->setText(text.startsWith(QLatin1Char('<')) ? text : tvRich(text));
}

// The count line, exactly as the spec pins it: black label, red value, two
// spaces between the two pairs (echoing the old F-key rhythm).
QString FKeyStatusBar::countLine(qint64 words, qint64 chars)
{
    const QString black = QStringLiteral("<span style=\"color:#000000;\">");
    const QString red = QStringLiteral("<span style=\"color:#AA0000;\">");
    const QString close = QStringLiteral("</span>");
    return black + QObject::tr("Word Count:") + close + QLatin1Char(' ') + red
           + QString::number(words) + close + QStringLiteral("  ") + black
           + QObject::tr("Character Count:") + close + QLatin1Char(' ') + red
           + QString::number(chars) + close;
}

void FKeyStatusBar::setCounts(qint64 words, qint64 chars)
{
    m_text = countLine(words, chars);
    refreshLabel();
}

// The four indicators sit on the right of the row, after the F-key label.
// Added as a status-bar widget with stretch 0 so the F-key label (stretch 1)
// keeps the leftover space and elides first on narrow windows.
void FKeyStatusBar::setStatusSelectors(QWidget* selectors)
{
    if (m_selectors == selectors)
        return;
    if (m_selectors)
        removeWidget(m_selectors);
    m_selectors = selectors;
    if (m_selectors) {
        m_selectors->show();   // offscreen render skips hidden children
        addWidget(m_selectors, 0);
    }
}

// Spec 7: while a dropdown is open the status line is REPLACED by a context
// hint for the highlighted item: "F1" red, " Help " black, a black "│", then
// the hint text in black.
void FKeyStatusBar::setContextHint(const QString& hint)
{
    m_hintActive = true;
    m_hint = QStringLiteral("F1 Help \u2502 ") + hint;
    // While the hint is up the four indicators step aside (they share the same
    // real estate and the hint replaces the whole row's content, spec 7).
    if (m_selectors)
        m_selectors->hide();
    refreshLabel();
}

void FKeyStatusBar::clearContextHint()
{
    if (!m_hintActive)
        return;
    m_hintActive = false;
    m_hint.clear();
    if (m_selectors)
        m_selectors->show();
    refreshLabel();
}

// Explicitly fill the status bar TV light-gray (palette alone was being
// overridden by the app-wide window-blue QWidget rule).
void FKeyStatusBar::paintEvent(QPaintEvent* ev)
{
    QPainter p(this);
    p.fillRect(rect(), kBarGray);
    QStatusBar::paintEvent(ev);
}
