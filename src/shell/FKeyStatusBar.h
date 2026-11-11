// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QStatusBar>

// Bottom F-key / status line (Turbo Vision: F2 Save ... F10 Menu), keys red.
// Paints its own light-gray bar (palette alone was overridden by the app-wide
// window-blue QWidget rule).
class FKeyStatusBar : public QStatusBar
{
    Q_OBJECT
public:
    explicit FKeyStatusBar(QWidget* parent = nullptr);
    void setFKeys(const QString& text);
    QString fkeys() const { return m_text; }

    // TV-revision2 spec 7: while a dropdown is open the status line is
    // REPLACED by a context hint ("F1 Help | <hint>"); setContextHint()
    // shows it, clearContextHint() restores the F-key row.
    void setContextHint(const QString& hint);
    void clearContextHint();
    bool contextHintActive() const { return m_hintActive; }
    // The text currently on the bar (F-key row or context hint).
    QString displayedText() const { return m_hintActive ? m_hint : m_text; }

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    void ensureLabel();
    void refreshLabel();
    QString m_text;
    QString m_hint;
    bool m_hintActive = false;
};
