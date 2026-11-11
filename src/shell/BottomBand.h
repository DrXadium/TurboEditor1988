// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QScrollBar>

class TextScrollBar;

// TV-revision spec 5h + 6: the bottom border row of the editor window —
// carries the cursor position "L:C" (white, cols 8-12) and a horizontal
// scrollbar spanning from ~col 18 to the right edge. Sits between the editor
// and the status line (spec 9 row order). Replaces KTextEditor's chip strip
// (KateViewBar is hidden by EditorTabManager::applyTvStyle).
class BottomBand : public QWidget
{
    Q_OBJECT
public:
    explicit BottomBand(QWidget* parent = nullptr);

    // 1-based line/col -> "L:C" rendered white on the band.
    void setCursorPos(int line, int col);
    QString cursorPosText() const;

    // Mirror a source horizontal scrollbar (range + value, both directions
    // kept in sync by connecting the two bars' signals/slots).
    void mirrorHorizontal(QScrollBar* source);
    TextScrollBar* horizontalBar() const;

    // Fixed band height: one text row (spec: each element one cell high).
    int heightForWidth(int w) const override;   // unused; sizeHint carries it
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;

private:
    void layoutHBar();
    int m_line = 1;
    int m_col = 1;
    TextScrollBar* m_hbar = nullptr;
};
