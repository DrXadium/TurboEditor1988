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

    // The four status indicators (spell, encoding, doc type, INS/OVR), hosted
    // on the right of this row. Owned by the composition root (crt-editor's
    // StatusSelectors needs KTextEditor, which crt-shell does not link), so it
    // arrives as an opaque QWidget* and is added as a status-bar widget.
    void setStatusSelectors(QWidget* selectors);
    QWidget* statusSelectors() const { return m_selectors; }

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    void ensureLabel();
    void refreshLabel();
    QString m_text;
    QString m_hint;
    bool m_hintActive = false;
    QWidget* m_selectors = nullptr;
};
