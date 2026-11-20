// SPDX-License-Identifier: GPL-3.0-or-later
#include "TvLineEdit.h"
#include <QFocusEvent>
#include <QFontMetrics>
#include <QPainter>

namespace {
// The DOS caret cadence: on and off for the same beat, 500 ms each. The
// editor's own caret uses Kate's timer, so there is no shared constant to
// borrow; this is the number that reads as "blinking" rather than "flickering".
const int kBlinkMs = 500;
} // namespace

TvLineEdit::TvLineEdit(QWidget* parent) : QLineEdit(parent)
{
    m_blink.setInterval(kBlinkMs);
    connect(&m_blink, &QTimer::timeout, this, &TvLineEdit::advanceBlink);
    // The window is never active, so FocusIn/FocusOut do not arrive reliably.
    // Run the timer from the start and gate the PAINT on holdsKeyFocus(): the
    // widget is the only thing that knows when it is the key target.
    syncBlinkTimer();
}

void TvLineEdit::setCaretColors(const QColor& ink, const QColor& field)
{
    m_ink = ink;
    m_field = field;
    update();
}

bool TvLineEdit::holdsKeyFocus() const
{
    // Deliberately NOT hasFocus(): QApplication::focusWidget() is null while no
    // window is active, which is this app's permanent state. window()->focusWidget()
    // is the shell's own answer and is what ContentBridge::keyTarget() follows.
    if (!isVisible())
        return false;
    const QWidget* win = window();
    return win && win->focusWidget() == this;
}

QRect TvLineEdit::caretRect() const
{
    if (!holdsKeyFocus() || hasSelectedText() || !m_caretOn)
        return QRect();
    const QRect cr = cursorRect();
    if (cr.isNull())
        return QRect();
    // One character cell wide, font height tall: a block, not an I-beam. The
    // text is a fixed-width CRT face, so the advance of one glyph is the cell.
    const QFontMetrics fm(font());
    const int w = qMax(2, fm.horizontalAdvance(QLatin1Char('M')));
    const int h = qMax(2, fm.height());
    return QRect(cr.left(), cr.top(), w, h).intersected(rect());
}

void TvLineEdit::paintEvent(QPaintEvent* ev)
{
    QLineEdit::paintEvent(ev);

    const QRect block = caretRect();
    if (block.isEmpty())
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    // Inverted block: ink over the cell, then the character that was there is
    // redrawn in the field colour so it stays readable.
    p.fillRect(block, m_ink);
    if (cursorPosition() < text().size()) {
        const QString ch = text().mid(cursorPosition(), 1);
        QFont inkFont = font();
        inkFont.setStyleStrategy(QFont::NoAntialias);
        p.setFont(inkFont);
        p.setPen(m_field);
        p.drawText(block, Qt::AlignLeft | Qt::AlignVCenter, ch);
    }
}

void TvLineEdit::advanceBlink()
{
    if (!holdsKeyFocus()) {
        // Nothing to blink at: stay lit so the first paint after focus shows it.
        if (!m_caretOn) {
            m_caretOn = true;
            update();
        }
        return;
    }
    m_caretOn = !m_caretOn;
    // Repaint only the cell: the rest of the field is static, and a full
    // update() twice a second would fight the dock's own paint pass.
    const QRect block = QRect(cursorRect().left(), cursorRect().top(),
                              qMax(2, fontMetrics().horizontalAdvance(QLatin1Char('M'))),
                              qMax(2, fontMetrics().height()));
    update(block.adjusted(-1, -1, 1, 1));
}

void TvLineEdit::setCaretBlinkEnabled(bool on)
{
    m_blinkEnabled = on;
    syncBlinkTimer();
    update();
}

void TvLineEdit::setBlinkPeriod(int ms)
{
    m_blink.setInterval(qMax(1, ms));
}

void TvLineEdit::syncBlinkTimer()
{
    if (m_blinkEnabled && !m_blink.isActive()) {
        m_caretOn = true;
        m_blink.start();
    } else if (!m_blinkEnabled && m_blink.isActive()) {
        m_blink.stop();
        m_caretOn = true;
    }
}
