// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorSplitter.h"
#include "EditorPane.h"
#include "BottomBand.h"
#include "TvFrame.h"

#include <QPainter>
#include <QPaintEvent>
#include <QVBoxLayout>

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
    // Each pane sits in a COLUMN with its own band directly below it. Both panes
    // are wrapped so they keep the SAME height (their text rows stay aligned)
    // and each gets its own L:C row. Pane 0's band leaves the host layout here;
    // closeSplit() hands it back. Inserting the columns (not the panes) is what
    // gives pane 1 a place for its band - before this the band was an unparented
    // child of the pane and painted as a blob at the pane's top-left.
    if (!m_col0) {
        m_col0 = new QWidget(this);
        auto* v0 = new QVBoxLayout(m_col0);
        v0->setContentsMargins(0, 0, 0, 0);
        v0->setSpacing(0);
        v0->addWidget(m_pane0, 1);
        v0->addWidget(m_pane0->bottomBand(), 0);
    }
    QSplitter::insertWidget(0, m_col0);

    if (!m_col1) {
        m_col1 = new QWidget(this);
        auto* v1 = new QVBoxLayout(m_col1);
        v1->setContentsMargins(0, 0, 0, 0);
        v1->setSpacing(0);
        v1->addWidget(m_pane1, 1);
        v1->addWidget(m_pane1->bottomBand(), 0);
    }
    // Insert pane 1's column at index 1 (right after pane 0), before the minimap.
    QSplitter::insertWidget(1, m_col1);
    // Reparenting a pane out of the splitter hides it; a hidden item gets zero
    // space from the column layout (which then hands the whole column to the
    // band), so both panes must be shown explicitly.
    m_pane0->show();
    m_pane1->show();
    m_col0->show();
    m_col1->show();
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
    if (m_col1) {
        // The column still owns pane 1's band; it dies with the column.
        m_col1->setParent(nullptr);
        m_col1->deleteLater();
        m_col1 = nullptr;
    }
    // Restore pane 0 to a direct child and hand its band back to the host
    // layout (appended, i.e. below the splitter - the unsplit shape).
    if (m_col0) {
        if (auto* lay = m_col0->layout())
            lay->removeWidget(m_pane0);
        QSplitter::insertWidget(0, m_pane0);
        if (auto* host = parentWidget()) {
            if (auto* hostLay = host->layout())
                hostLay->addWidget(m_pane0->bottomBand());
        }
        m_col0->setParent(nullptr);
        m_col0->deleteLater();
        m_col0 = nullptr;
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
