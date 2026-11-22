// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorSplitter.h"
#include "EditorPane.h"
#include "TvFrame.h"

#include <QPainter>
#include <QPaintEvent>

namespace {
const QColor kFrameWhite(0xFF, 0xFF, 0xFF);
const QColor kBgBlue(0x00, 0x00, 0xAA);
const QColor kGripTeal(0x00, 0xAA, 0xAA);
// Minimum pane width so a drag can never zero a pane (design §goal 1): 20 cells.
const int kMinCells = 20;
} // namespace

// --- TvSplitHandle ---------------------------------------------------------
// The divider column: a TV frame ║ band with a small grip glyph centred
// vertically so it reads as draggable. Painted, not styled, to match the frame.
class TvSplitHandle : public QSplitterHandle
{
protected:
    TvSplitHandle(Qt::Orientation orientation, QSplitter* parent)
        : QSplitterHandle(orientation, parent)
    {
        setAutoFillBackground(false);
    }

public:
    // Needed so EditorSplitter (a friend via createHandle) can construct it;
    // QSplitterHandle's ctor is protected, so subclass it here.
    friend class EditorSplitter;

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.fillRect(rect(), kBgBlue);

        const int cw = qMax(6, fontMetrics().horizontalAdvance(QLatin1Char('M')));
        const int ch = qMax(8, fontMetrics().height());
        const int x = (width() - cw) / 2;   // centre the cell in the handle
        if (x < 0)
            return;

        // Frame column down the middle.
        TvFrame::tileV(p, QRect(x, 0, cw, height()), ch, QChar(0x2551), kFrameWhite);

        // A 3-cell teal grip glyph centred vertically (the box the reference
        // screenshot shows on the divider).
        const int gy = (height() - ch) / 2;
        const QRect grip(x, gy, cw, ch);
        p.fillRect(grip, kGripTeal);
        // Two dark notches so the grip reads as a handle, not a solid block.
        p.fillRect(QRect(x + 1, gy + ch / 2 - 1, cw - 2, 2), kBgBlue);
    }
};

EditorSplitter::EditorSplitter(QWidget* parent) : QSplitter(Qt::Horizontal, parent)
{
    setChildrenCollapsible(false);   // a pane can never be dragged to nothing
    setHandleWidth(0);               // hidden until a split exists
    setObjectName(QStringLiteral("editorSplitter"));
}

void EditorSplitter::addPane(EditorPane* pane)
{
    if (!pane || !m_pane0) {
        m_pane0 = pane;
        QSplitter::addWidget(pane);
    }
    applyMinWidth();
}

void EditorSplitter::setMinimap(QWidget* minimap)
{
    if (minimap)
        QSplitter::addWidget(minimap);   // last child = far right
}

void EditorSplitter::openSplit()
{
    if (splitOpen())
        return;
    if (!m_pane1) {
        m_pane1 = new EditorPane(this);
        m_pane1->setPaneIndex(1);
    }
    // Insert pane 1 at index 1 (right after pane 0), before the minimap.
    QSplitter::insertWidget(1, m_pane1);
    m_pane1->show();
    setHandleWidth(m_pane0 ? m_pane0->cellWidth() : 8);
    // Ensure both panes get at least the minimum width.
    applyMinWidth();
    updateGeometry();
}

void EditorSplitter::closeSplit()
{
    if (!splitOpen())
        return;
    if (m_pane1) {
        // QSplitter has no removeWidget - reparenting OUT of the splitter
        // removes it from count(). Delete it: Task 3 rebuilds pane 1's view on
        // the next split, so nothing here must survive the collapse.
        m_pane1->setParent(nullptr);
        m_pane1->deleteLater();
        m_pane1 = nullptr;
    }
    setHandleWidth(0);
    applyMinWidth();
    updateGeometry();
}

bool EditorSplitter::splitOpen() const
{
    // The handle exists exactly while the split is open; with handleWidth 0
    // QSplitter lays out as a single row (the single-pane look).
    return handleWidth() > 0 && m_pane1;
}

EditorPane* EditorSplitter::pane(int i) const
{
    if (i == 0)
        return m_pane0;
    if (i == 1)
        return splitOpen() ? m_pane1 : nullptr;
    return nullptr;
}

int EditorSplitter::handleWidth() const
{
    return QSplitter::handleWidth();
}

QSplitterHandle* EditorSplitter::createHandle()
{
    return new TvSplitHandle(Qt::Horizontal, this);
}

// Apply the minimum-width floor to the panes so a drag can't zero either.
void EditorSplitter::applyMinWidth()
{
    const int cw = m_pane0 ? m_pane0->cellWidth() : 8;
    const int minW = kMinCells * cw;
    if (m_pane0)
        m_pane0->setMinimumWidth(minW);
    if (m_pane1 && splitOpen())
        m_pane1->setMinimumWidth(minW);
}
