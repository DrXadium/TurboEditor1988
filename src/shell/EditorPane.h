// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QRect>
#include <QString>

class QLabel;
class RightScrollBar;
class BottomBand;

// One editor pane in the TV chrome: the window-blue field, the double-line
// frame (╔═ name ═╗ top row + ║ left column), the body slot the view stack is
// installed into, the 1-cell RightScrollBar column, the BottomBand (L:C +
// horizontal bar), and - only on the split pane - the green ■ close box.
//
// Extracted verbatim from ShellWindow's private EditorHost so pane 0 renders
// byte-identically (Task 1) while a second instance becomes pane 1 (Task 4).
// crt-shell does NOT link KTextEditor: the pane is a dumb widget; the
// composition root (main.cpp) resolves views and wires scrollbars.
class EditorPane : public QWidget
{
    Q_OBJECT
public:
    explicit EditorPane(QWidget* parent = nullptr);

    // The widget the view stack goes into (the old "slot" below the tab row).
    QWidget* bodySlot() const { return m_slot; }
    // Per-pane chrome: the right border scrollbar column and the bottom band.
    RightScrollBar* rightBar() const { return m_rightBar; }
    BottomBand* bottomBand() const { return m_bottomBand; }

    // Document name in the pane's top border row.
    void setTitle(const QString& name);
    QString title() const;

    // Pane identity. Pane 0 keeps the ORIGINAL object names (editorHost,
    // editorTitleRow, editorTitle, editorBody, editorSlot) so existing tests
    // and lookups stay valid; pane 1 appends "2".
    void setPaneIndex(int index);
    int paneIndex() const { return m_index; }

    // The split's close affordance - a real child widget (the title row and
    // its label are WA_TransparentForMouseEvents, so only a child can take the
    // click). Shown on pane 1 only.
    void showCloseBox(bool show);
    QWidget* closeBox() const { return m_closeBox; }
    QRect closeBoxRect() const;

    // One character cell (frame thickness / glyph cell), as EditorHost had it.
    int cellWidth() const;
    int cellHeight() const;

signals:
    // The ■ was clicked: the composition root closes the split.
    void closeRequested();

protected:
    void paintEvent(QPaintEvent*) override;
    bool eventFilter(QObject* watched, QEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;
    void changeEvent(QEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;

private:
    void buildChrome();
    void updateFrameMargins();
    void layoutTitle();
    int titleGapStart() const;
    int titleGapEnd() const;

    int m_index = 0;
    QWidget* m_titleRow = nullptr;
    QWidget* m_body = nullptr;
    QWidget* m_slot = nullptr;
    QLabel* m_title = nullptr;
    QWidget* m_closeBox = nullptr;
    RightScrollBar* m_rightBar = nullptr;
    BottomBand* m_bottomBand = nullptr;
};
