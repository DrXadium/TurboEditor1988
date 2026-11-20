// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QLineEdit>
#include <QColor>
#include <QTimer>

// A QLineEdit that draws its OWN caret.
//
// Why: the shell window is WA_DontShowOnScreen and never mapped, so it is never
// the ACTIVE window. Qt paints a QLineEdit caret only for a focused widget in an
// active window, which means every field on the CRT chrome shows no insertion
// point at all - the user's report was literally "I can't see where the mouse
// cursor is in the input line".
//
// The caret here is the DOS block: the cell at the insertion point is filled
// with ink and the character under it is redrawn in the field colour, so the
// block reads as "you are here" without hiding what is there. It blinks on its
// own timer, because FocusIn never arrives on an unmapped shell either.
//
// "Focused" is the window's focus widget, not hasFocus(): hasFocus() consults
// QApplication::focusWidget(), which is null whenever no window is active -
// always, for this app. That is the same rule ContentBridge uses to route keys.
class TvLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit TvLineEdit(QWidget* parent = nullptr);

    // Caret colours. Defaults are the dock's field pair (white ink on the blue
    // entry field). The block is drawn only when the widget holds key focus,
    // has no selection, and the blink phase is on.
    void setCaretColors(const QColor& ink, const QColor& field);

    // The cell the block occupies, in widget coordinates. Empty when no caret
    // is shown. Exposed so tests assert geometry instead of pixel timing.
    QRect caretRect() const;
    bool caretOn() const { return m_caretOn; }
    bool holdsKeyFocus() const;

    // Tests turn the blink off (a repainting widget makes pixel probes flaky);
    // the app leaves it on.
    void setCaretBlinkEnabled(bool on);
    bool caretBlinkEnabled() const { return m_blink.isActive(); }
    void setBlinkPeriod(int ms);

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    void advanceBlink();
    void syncBlinkTimer();

    QTimer m_blink;
    bool m_caretOn = true;
    QColor m_ink{ 0xFF, 0xFF, 0xFF };
    QColor m_field{ 0x00, 0x00, 0xAA };
    bool m_blinkEnabled = true;
};
