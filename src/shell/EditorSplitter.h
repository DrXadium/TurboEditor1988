// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QSplitter>

class EditorPane;
class TvSplitHandle;

// The side-by-side editor host: holds pane 0, optionally pane 1 (the split),
// and the minimap as the last child. Wraps QSplitter so dragging the divider
// resizes the two panes; the handle is a TvSplitHandle (a painted TV divider
// column with a grip glyph). With one child the handle is hidden and layout
// behaves exactly like a plain row, so the single-pane look is unchanged.
//
// Max ONE split: openSplit() adds at most pane 1 (see design §1). crt-shell
// does NOT link KTextEditor - this is pure chrome.
class EditorSplitter : public QSplitter
{
    Q_OBJECT
public:
    explicit EditorSplitter(QWidget* parent = nullptr);

    // Attach pane 0 (already constructed). The minimap is added last so it
    // stays at the far right of the row.
    void addPane(EditorPane* pane);
    void setMinimap(QWidget* minimap);

    // Build / tear down the split. openSplit inserts pane 1 after pane 0 and
    // shows the divider; closeSplit removes it, collapsing to one child.
    void openSplit();
    void closeSplit();
    bool splitOpen() const;

    EditorPane* pane(int i) const;   // nullptr for 1 when no split

    // The handle is exactly one character cell wide (matches the pane frame).
    int handleWidth() const;

protected:
    QSplitterHandle* createHandle() override;

private:
    void applyMinWidth();

    EditorPane* m_pane0 = nullptr;
    EditorPane* m_pane1 = nullptr;
    // While split, each pane sits in a column with its BottomBand directly
    // below it (so each pane shows its own L:C row and both panes keep the same
    // height). Null while unsplit: pane 0 is then a direct child and its band
    // lives in the host layout.
    QWidget* m_col0 = nullptr;
    QWidget* m_col1 = nullptr;
};
