// SPDX-License-Identifier: GPL-3.0-or-later
#include "StatusSelectors.h"
#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>
#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

namespace {
// Bar ink: the F-row's own black text color (TV status line words are black).
const QColor kInk(0x00, 0x00, 0x00);
// OVR red: the same #AA0000 the F-key names use, so it reads as part of the
// bar's language (spec: INS normal, OVR red).
const QColor kOvrRed(0xAA, 0x00, 0x00);
// One space between indicators, in pixels (a cell's worth of daylight).
const int kGap = 8;
} // namespace

StatusSelectors::StatusSelectors(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("statusSelectors"));
    setAutoFillBackground(false);   // FKeyStatusBar's gray shows through
    setAttribute(Qt::WA_TranslucentBackground, true);
}

void StatusSelectors::setOverwrite(bool ovr)
{
    if (m_ovr == ovr)
        return;
    m_ovr = ovr;
    update();
}

void StatusSelectors::setDocument(KTextEditor::Document* doc)
{
    if (m_doc == doc)
        return;
    m_doc = doc;
    update();
}

void StatusSelectors::setView(KTextEditor::View* view)
{
    if (m_view == view)
        return;
    m_view = view;
    update();
}

void StatusSelectors::setSpellLanguage(const QString& code)
{
    if (m_spell == code)
        return;
    m_spell = code;
    update();
}

QString StatusSelectors::overwriteText() const
{
    return m_ovr ? QStringLiteral("OVR") : QStringLiteral("INS");
}

QString StatusSelectors::docTypeText() const
{
    if (!m_doc)
        return QStringLiteral("-");
    const QString mode = m_doc->highlightingMode();
    return mode.isEmpty() ? QStringLiteral("-") : mode;
}

QString StatusSelectors::encodingText() const
{
    if (!m_doc)
        return QStringLiteral("-");
    const QString enc = m_doc->encoding();
    return enc.isEmpty() ? QStringLiteral("Default") : enc;
}

QString StatusSelectors::spellText() const
{
    return m_spell.isEmpty() ? QStringLiteral("-") : m_spell;
}

QSize StatusSelectors::sizeHint() const
{
    const QFontMetrics fm(font());
    // spell + encoding + docType + overwrite, each with a gap between.
    const int w = fm.horizontalAdvance(spellText()) + fm.horizontalAdvance(encodingText()) +
                  fm.horizontalAdvance(docTypeText()) + fm.horizontalAdvance(overwriteText()) +
                  3 * kGap;
    return QSize(w, qMax(10, fm.height()));
}

QSize StatusSelectors::minimumSizeHint() const
{
    return QSize(qMax(60, fontMetrics().horizontalAdvance(overwriteText()) + 2 * kGap),
                 qMax(10, fontMetrics().height()));
}

// Each indicator is a fixed-width cell sized to its text; laid out left to
// right with a gap. Returns are in widget-local coordinates.
QRect StatusSelectors::spellRect() const
{
    const QFontMetrics fm(font());
    return QRect(0, 0, fm.horizontalAdvance(spellText()), height());
}

QRect StatusSelectors::encodingRect() const
{
    const QFontMetrics fm(font());
    const int left = spellRect().right() + 1 + kGap;
    return QRect(left, 0, fm.horizontalAdvance(encodingText()), height());
}

QRect StatusSelectors::docTypeRect() const
{
    const QFontMetrics fm(font());
    const int left = encodingRect().right() + 1 + kGap;
    return QRect(left, 0, fm.horizontalAdvance(docTypeText()), height());
}

QRect StatusSelectors::overwriteRect() const
{
    const QFontMetrics fm(font());
    const int left = docTypeRect().right() + 1 + kGap;
    return QRect(left, 0, fm.horizontalAdvance(overwriteText()), height());
}

void StatusSelectors::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, false);
    const QFontMetrics fm(font());

    // Spell language (leftmost).
    p.setPen(kInk);
    p.drawText(spellRect(), Qt::AlignCenter, spellText());
    // Encoding.
    p.drawText(encodingRect(), Qt::AlignCenter, encodingText());
    // Document type (highlighting mode).
    p.drawText(docTypeRect(), Qt::AlignCenter, docTypeText());
    // INS/OVR: red only when OVR.
    p.setPen(m_ovr ? kOvrRed : kInk);
    p.drawText(overwriteRect(), Qt::AlignCenter, overwriteText());
}

void StatusSelectors::mousePressEvent(QMouseEvent* ev)
{
    if (overwriteRect().contains(ev->pos())) {
        emit overwriteToggled(!m_ovr);
        return;
    }
    // The other three open their dropdowns (wired in later tasks).
    QWidget::mousePressEvent(ev);
}
