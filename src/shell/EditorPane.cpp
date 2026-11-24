// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorPane.h"
#include "RightScrollBar.h"
#include "BottomBand.h"
#include "TvFrame.h"

#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace {
const QColor kFrameWhite(0xFF, 0xFF, 0xFF);   // TV-revision spec 3: white double line
const QColor kBgBlue(0x00, 0x00, 0xAA);
const QColor kCloseGreen(0x00, 0xAA, 0x00);   // the tab chip's ■ square
const QColor kCloseGreenBright(0x55, 0xFF, 0x55);
} // namespace

EditorPane::EditorPane(QWidget* parent) : QWidget(parent)
{
    // We paint the field and frame ourselves; the QSS would otherwise give the
    // pane a flat blue background and erase the glyphs.
    setAutoFillBackground(false);
    setObjectName(QStringLiteral("editorHost"));
    buildChrome();
}

// Build the internal layout [titleRow, body, bottomBand] exactly as the old
// central arrangement stacked them, so pane 0's geometry is unchanged. The
// pane's own layout positions the children (no manual setGeometry); paintEvent
// draws the frame in the pane's coordinate space.
void EditorPane::buildChrome()
{
    auto* paneLay = new QVBoxLayout(this);
    paneLay->setContentsMargins(0, 0, 0, 0);
    paneLay->setSpacing(0);

    // The document's top border row: a fixed-cell-height band the pane paints
    // (╔═ name ═╗) in paintEvent.
    m_titleRow = new QWidget(this);
    m_titleRow->setObjectName(QStringLiteral("editorTitleRow"));
    m_titleRow->setAutoFillBackground(false);
    m_titleRow->setAttribute(Qt::WA_NoSystemBackground, true);
    m_titleRow->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_titleRow->installEventFilter(this);
    paneLay->addWidget(m_titleRow, 0);

    // The name is a real child widget (not painted text) so the shell can
    // retitle it per tab and tests can read it back.
    m_title = new QLabel(QStringLiteral("untitled"), this);
    m_title->setObjectName(QStringLiteral("editorTitle"));
    m_title->setAlignment(Qt::AlignCenter);
    m_title->setAutoFillBackground(false);
    m_title->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    // Body: editor slot (stretch) + the 1-cell RightScrollBar column. The
    // body-layout's LEFT margin = the frame column insets its CHILDREN (slot
    // starts inside the ║); the body rectangle itself is full-width (its top
    // is right under the title band, and the bar is pinned to the right edge).
    m_body = new QWidget(this);
    m_body->setObjectName(QStringLiteral("editorBody"));
    auto* bodyLay = new QHBoxLayout(m_body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);
    m_body->setAttribute(Qt::WA_NoSystemBackground, true);

    m_slot = new QWidget(m_body);
    m_slot->setObjectName(QStringLiteral("editorSlot"));
    auto* slotLay = new QVBoxLayout(m_slot);
    slotLay->setContentsMargins(0, 0, 0, 0);
    slotLay->setSpacing(0);
    auto* placeholder = new QLabel(QStringLiteral("Editor"), m_slot);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setObjectName(QStringLiteral("editorPlaceholder"));
    slotLay->addWidget(placeholder, 1);

    m_rightBar = new RightScrollBar(m_body);
    bodyLay->addWidget(m_slot, 1);
    bodyLay->addWidget(m_rightBar, 0);
    paneLay->addWidget(m_body, 1);

    // Bottom band (cursor position + horizontal scrollbar): the pane's bottom
    // border row. It is created here but is NOT a layout item of the pane -
    // the pane's frame (║) must reach its own bottom edge so ║ meets the band's
    // ╚ at the boundary (tst_frame: left_border_reaches... / strokes_match...).
    // The shell stacks the band directly below the pane (sibling, as the old
    // centralWrap did), one per pane for the split.
    m_bottomBand = new BottomBand(this);

    // The split's ■ close box: a real child so it can take the click. Hidden
    // until showCloseBox(true) (pane 1 only). Not a layout item - it floats in
    // the title row's right cell, painted over the top rule.
    m_closeBox = new QWidget(this);
    m_closeBox->setObjectName(QStringLiteral("splitCloseBox"));
    m_closeBox->setAutoFillBackground(false);
    // Like m_titleRow/m_title/m_body: the global QWidget QSS rule paints an
    // opaque blue background, and this box is a child composited OVER the pane's
    // paintEvent. Without WA_NoSystemBackground the child's inherited blue bg
    // covered the green ■ the pane painted (the \u25a0 looked missing on the split).
    m_closeBox->setAttribute(Qt::WA_NoSystemBackground, true);
    m_closeBox->hide();

    updateFrameMargins();
}

void EditorPane::setPaneIndex(int index)
{
    m_index = index;
    // Pane 0 keeps the ORIGINAL names (existing tests + findChild lookups);
    // pane 1 gets the "2" suffix so both panes are addressable.
    const QString suffix = index == 0 ? QString() : QStringLiteral("2");
    setObjectName(QStringLiteral("editorHost") + suffix);
    if (m_titleRow)
        m_titleRow->setObjectName(QStringLiteral("editorTitleRow") + suffix);
    if (m_title)
        m_title->setObjectName(QStringLiteral("editorTitle") + suffix);
    if (m_body)
        m_body->setObjectName(QStringLiteral("editorBody") + suffix);
    if (m_slot)
        m_slot->setObjectName(QStringLiteral("editorSlot") + suffix);
}

void EditorPane::setTitle(const QString& name)
{
    const QString text = name.isEmpty() ? QStringLiteral("untitled") : name;
    if (m_title->text() == text)
        return;
    m_title->setText(text);
    layoutTitle();
    update();
}

QString EditorPane::title() const
{
    return m_title ? m_title->text() : QString();
}

void EditorPane::showCloseBox(bool show)
{
    if (!m_closeBox)
        return;
    m_closeBox->setVisible(show);
    if (show) {
        // Position it NOW: a pane that already has its final width never sees
        // another resize, so waiting for updateFrameMargins() left the box at
        // its default geometry - and since the widget has no paintEvent (the
        // pane paints the ■ from closeBoxRect), that meant no close control was
        // drawn at all. update() repaints the glyph on the same turn.
        updateFrameMargins();
        m_closeBox->raise();
        update();
    }
}

QRect EditorPane::closeBoxRect() const
{
    return m_closeBox ? m_closeBox->geometry() : QRect();
}

int EditorPane::cellWidth() const
{
    return qMax(6, fontMetrics().horizontalAdvance(QLatin1Char('M')));
}

int EditorPane::cellHeight() const
{
    return qMax(8, fontMetrics().height());
}

// The layout's left margin is the frame column and its top margin is the title
// The pane's internal layout [titleRow, body, band] positions the children;
// this only re-measures the things a layout can't know: the title row's CELL
// height, the body-layout's LEFT frame-column margin, and the floating ■ in
// the title row. Re-run when the font lands (QSS applies AFTER construction).
void EditorPane::updateFrameMargins()
{
    if (m_titleRow)
        m_titleRow->setFixedHeight(cellHeight());

    // Body: LEFT margin = frame column (insets slot + right bar as children);
    // the body rectangle itself stays full-width and top-aligned under the
    // title band, so the bar column reaches the window's right edge.
    if (m_body && m_body->layout()) {
        QMargins m = m_body->layout()->contentsMargins();
        m.setLeft(cellWidth());
        m.setRight(0);   // the RightScrollBar IS the right border column
        m.setTop(0);
        m.setBottom(0);  // the BottomBand owns the bottom border
        m_body->layout()->setContentsMargins(m);
    }

    layoutTitle();

    // The ■ floats in the title row's right cell (one cell in from the corner).
    // isHidden() (not isVisible()): a pane whose ancestor is still hidden must
    // still get the box's geometry, or it stays at the default rect and the
    // pane paints no ■ at all once shown (tst_splitview pins this).
    if (m_closeBox && !m_closeBox->isHidden()) {
        const int cw = cellWidth();
        const int ch = cellHeight();
        // [ ■ ] - three cells, the bracketed chip the tab strip uses, so the
        // split's close control reads as the same kind of object (Xadium
        // 2026-11-23). The middle cell carries the green square.
        const int boxW = 3 * cw;
        const int rowTop = m_titleRow ? m_titleRow->geometry().top() : 0;
        m_closeBox->setGeometry(QRect(width() - cw - boxW - cw / 2, rowTop, boxW, ch));
    }
}

// Place the name on the character grid: whole cells wide, centred, leaving two
// cells of rule-free daylight either side for paintEvent to skip. Coordinates
// are the PANE's, so the label lines up with glyphs painted across its width.
void EditorPane::layoutTitle()
{
    if (!m_title || !m_titleRow)
        return;
    const int cw = cellWidth();
    const int ch = cellHeight();
    const QFontMetrics fm(font());
    int cells = (fm.horizontalAdvance(m_title->text()) + cw - 1) / cw;
    const int maxCells = qMax(1, width() / cw - 6);   // corner + 2 + 2 + corner
    cells = qBound(1, cells, maxCells);
    const int w = cells * cw;
    const int x = ((width() - w) / 2 / cw) * cw;
    m_title->setGeometry(QRect(x, m_titleRow->geometry().top(), w, ch));
    m_title->raise();
    // Same crisp 1-bit rendering as the frame glyphs: antialiased text tints
    // the strokes and reads soft next to the box-drawing characters.
    QFont f = font();
    f.setStyleStrategy(QFont::NoAntialias);
    if (m_title->font() != f)
        m_title->setFont(f);
}

int EditorPane::titleGapStart() const
{
    return m_title ? m_title->geometry().left() - 2 * cellWidth() : -1;
}

int EditorPane::titleGapEnd() const
{
    return m_title ? m_title->geometry().right() + 1 + 2 * cellWidth() : -1;
}

bool EditorPane::eventFilter(QObject* watched, QEvent* ev)
{
    if (watched == m_titleRow &&
        (ev->type() == QEvent::Move || ev->type() == QEvent::Resize))
        layoutTitle();
    return QWidget::eventFilter(watched, ev);
}

void EditorPane::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    updateFrameMargins();
}

void EditorPane::changeEvent(QEvent* ev)
{
    QWidget::changeEvent(ev);
    if (ev->type() == QEvent::FontChange)
        updateFrameMargins();
}

void EditorPane::mousePressEvent(QMouseEvent* ev)
{
    if (m_closeBox && m_closeBox->isVisible() &&
        m_closeBox->geometry().contains(ev->pos())) {
        emit closeRequested();
        ev->accept();
        return;
    }
    QWidget::mousePressEvent(ev);
}

// The frame + field paint, moved verbatim from ShellWindow's EditorHost so
// pane 0 renders identically (Task 1). pane 0 keeps the original names; the
// only structural addition is the ■ in the title row when shown.
void EditorPane::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    const QRect r = rect();
    p.fillRect(r, kBgBlue);

    const int cw = cellWidth();
    const int ch = cellHeight();
    const int rowTop = m_titleRow ? m_titleRow->geometry().top() : 0;
    const int rowH = m_titleRow ? m_titleRow->geometry().height() : ch;

    // Top border row: ╔ ═ (two spaces) name (two spaces) ═ ╗
    TvFrame::glyph(p, QRect(0, rowTop, cw, rowH), QChar(0x2554), kFrameWhite);
    const int gapStart = titleGapStart();
    const int gapEnd = titleGapEnd();
    if (gapStart > cw)
        TvFrame::tileH(p, QRect(cw, rowTop, gapStart - cw, rowH), cw,
                       QChar(0x2550), kFrameWhite);
    // On the split pane the ■ interrupts the right rule, so split the rule
    // AROUND the box: run it up to the box's left edge, then resume from the
    // box's right edge to the corner - so the frame runs continuously INTO the
    // control with no blue gap on either side (Xadium 2026-11-22 finding #1:
    // "the gap in the right where the close control should be inserted").
    const bool boxShown = m_closeBox && m_closeBox->isVisible();
    const QRect boxR = boxShown ? m_closeBox->geometry() : QRect();
    int rightRuleEnd = r.width() - cw;
    if (boxShown)
        rightRuleEnd = boxR.left();          // rule stops AT the box (touches it)
    if (gapEnd > 0 && gapEnd < rightRuleEnd)
        TvFrame::tileH(p, QRect(gapEnd, rowTop, rightRuleEnd - gapEnd, rowH), cw,
                       QChar(0x2550), kFrameWhite);
    // Resume the rule from the box's right edge to the corner (fills ■→╗).
    if (boxShown) {
        const int resumeFrom = boxR.right() + 1;
        const int cornerX = r.width() - cw;
        if (resumeFrom < cornerX)
            TvFrame::tileH(p, QRect(resumeFrom, rowTop, cornerX - resumeFrom, rowH),
                           cw, QChar(0x2550), kFrameWhite);
    }
    TvFrame::glyph(p, QRect(r.width() - cw, rowTop, cw, rowH), QChar(0x2557),
                   kFrameWhite);

    // Left border column: the ║ GLYPH tiled down a cell-wide band, below the
    // title row. No right verticals: the RightScrollBar is the right column.
    const int colTop = rowTop + rowH;
    TvFrame::tileV(p, QRect(0, colTop, cw, r.height() - colTop), ch,
                   QChar(0x2551), kFrameWhite);
    // NOTE: the bottom border row is owned by BottomBand (it carries L:C and
    // the horizontal scrollbar). Drawing it here too makes two rules.

    // The close control: [ ■ ] - bracketed like the tab chips
    // (TabStrip paints "[ name ■ ]"). The white brackets sit in the outer
    // cells; the green square fills the middle one.
    if (m_closeBox && m_closeBox->isVisible()) {
        const QRect box = m_closeBox->geometry();
        const int cell = box.width() / 3;
        if (cell > 0) {
            TvFrame::glyph(p, QRect(box.left(), box.top(), cell, box.height()),
                           QLatin1Char('['), kFrameWhite);
            TvFrame::glyph(p, QRect(box.right() - cell + 1, box.top(), cell, box.height()),
                           QLatin1Char(']'), kFrameWhite);
            // The green ink must be a SQUARE, not a rectangle (Xadium
            // 2026-11-23: "the square comes out as a rectangle"). The slot is
            // one character cell wide but a whole ROW tall (8x16), so a plain
            // fillRect paints a tall rectangle. Same rule as the tab chip
            // (TabStrip::paintEvent): inset each axis by half the excess over
            // the SHORTER side, so the ink stays centred and square.
            const QRect slot(box.left() + cell, box.top(), cell, box.height());
            const int side = qMin(slot.width(), slot.height());
            const int insetX = (slot.width() - side) / 2;
            const int insetY = (slot.height() - side) / 2;
            const QRect sq = slot.adjusted(insetX, insetY, -insetX, -insetY);
            p.fillRect(sq, kCloseGreen);
            p.fillRect(sq.adjusted(1, 1, -1, -1), kCloseGreenBright);
        }
    }
}
