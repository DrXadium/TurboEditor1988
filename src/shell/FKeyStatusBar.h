// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QStatusBar>

// Bottom status line. The LEFT line is a whole-document word/character count
// (labels black, values red) - the old Turbo Vision F-key reminder was
// retired (2026-11-19). Paints its own light-gray bar (palette alone was
// overridden by the app-wide window-blue QWidget rule).
class FKeyStatusBar : public QStatusBar
{
    Q_OBJECT
public:
    explicit FKeyStatusBar(QWidget* parent = nullptr);

    // The count line: HTML with black labels and red values. Static so tests
    // can assert the format without a live bar.
    static QString countLine(qint64 words, qint64 chars);
    void setCounts(qint64 words, qint64 chars);
    // Whole-document word count: maximal runs of non-whitespace, so a blank or
    // whitespace-only document is 0 (not 1). The composition root's only
    // counting rule - the status line never counts on its own.
    static qint64 wordCount(const QString& text);

    // TV-revision2 spec 7: while a dropdown is open the status line is
    // REPLACED by a context hint ("F1 Help | <hint>"); setContextHint()
    // shows it, clearContextHint() restores the count line.
    void setContextHint(const QString& hint);
    void clearContextHint();
    bool contextHintActive() const { return m_hintActive; }
    // The text currently on the bar (count line or context hint).
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
