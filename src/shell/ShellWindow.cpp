// SPDX-License-Identifier: GPL-3.0-or-later
#include "ShellWindow.h"
#include "BorlandStyle.h"
#include "ScrollBarInstaller.h"
#include "MenuBar.h"
#include "TabStrip.h"
#include "Minimap.h"
#include "RightScrollBar.h"
#include "TextScrollBar.h"
#include "FKeyStatusBar.h"
#include "BottomBand.h"
#include "TvFrame.h"
#include <QApplication>
#include <QFile>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>
#include <QLayout>
#include <QPainter>
#include <QPaintEvent>

namespace {
const QColor kFrameGray(0xAA, 0xAA, 0xAA);
const QColor kFrameWhite(0xFF, 0xFF, 0xFF);   // TV-revision spec 3: white double line
const QColor kBgBlue(0x00, 0x00, 0xAA);

// Central editor host: fills the TV window-blue desktop, then draws the
// left/right/bottom double-line frame edges as part of its OWN paint (so a
// child editor painting inside won't cover the frame — this widget's paint is
// the frame layer). The TabStrip title line supplies the top edge + boxes.
class EditorHost : public QWidget
{
public:
    explicit EditorHost(QWidget* parent = nullptr) : QWidget(parent)
    {
        setObjectName("editorHost");
        setAutoFillBackground(false);   // we paint the blue bg ourselves
    }

    // One character cell: the frame edge is a whole cell thick (TV look).
    int cellW() const { return qMax(6, fontMetrics().horizontalAdvance(QLatin1Char('M'))); }
    int cellH() const { return qMax(8, fontMetrics().height()); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        const QRect r = rect();
        p.fillRect(r, kBgBlue);
        // Left border column: a CELL-WIDE band carrying the ║ double rule.
        // No right verticals: TV-revision2 spec 1 hands the right border
        // column to the 1-cell RightScrollBar (teal ▲/▒/■/▼).
        TvFrame::doubleV(p, QRect(0, 0, cellW(), r.height()), kFrameWhite);
        // NOTE: the bottom border row is owned by BottomBand (it also carries
        // the cursor position and the horizontal scrollbar). Drawing it here
        // too produced two rules with a blue gap between them.
    }
};
} // namespace

ShellWindow::ShellWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("CRT-Kate"));
    setObjectName("shellWindow");
    resize(900, 600);

    // Borland style applied app-wide (spec: qApp->setStyle(new BorlandStyle)).
    m_borland = new BorlandStyle;
    qApp->setStyle(m_borland);
    // Convert plain QScrollBars to TV TextScrollBars app-wide.
    auto* sbInstall = new ScrollBarInstaller(qApp);
    qApp->installEventFilter(sbInstall);

    // Force the QSS resource into the link (static lib resources are dropped
    // by the linker unless referenced).
    Q_INIT_RESOURCE(shell_qss);

    // Load the navy QSS (qt_add_resources BASE=src/shell => :/borland.qss).
    QFile qss(QStringLiteral(":/borland.qss"));
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text))
        qApp->setStyleSheet(QString::fromUtf8(qss.readAll()));

    // Menu bar (top).
    auto* menu = new MenuBar(this);
    setMenuBar(menu);

    // Central editor host (sunken panel) + TabStrip above it. EditorHost fills
    // the TV window-blue desktop and draws the frame edges as its own paint.
    // The layout has a 3px margin so child content (editor/placeholder) never
    // covers the 3px frame border the host paints at its edges.
    auto* central = new EditorHost(this);
    auto* lay = new QVBoxLayout(central);
    // Right margin 0: the 1-cell RightScrollBar IS the right border column;
    // bottom 0: the BottomBand owns the bottom border (content reaches it).
    // Reserve the frame: EditorHost paints the left double line at x=1..2 and
    // the bottom pair on the last two rows. The margins must clear it, or the
    // editor/tab children paint over the border and the frame disappears.
    lay->setContentsMargins(central->cellW(), 0, 0, 0);
    lay->setSpacing(0);
    auto* tabs = new TabStrip(central);
    lay->addWidget(tabs);
    // Body row: editor slot (stretch) + the window-border scrollbar column.
    auto* body = new QWidget(central);
    body->setObjectName(QStringLiteral("editorBody"));
    auto* bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);
    auto* slot = new QWidget(body);
    slot->setObjectName(QStringLiteral("editorSlot"));
    auto* slotLay = new QVBoxLayout(slot);
    slotLay->setContentsMargins(0, 0, 0, 0);
    slotLay->setSpacing(0);
    auto* placeholder = new QLabel(QStringLiteral("Editor"), slot);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setObjectName("editorPlaceholder");
    slotLay->addWidget(placeholder, 1);
    m_rightBar = new RightScrollBar(body);
    bodyLay->addWidget(slot, 1);
    bodyLay->addWidget(m_rightBar, 0);
    lay->addWidget(body, 1);
    m_editorLayout = slotLay;   // slot below TabStrip (setEditorWidget swaps here)
    setCentralWidget(central);

    // Bottom band (cursor position + horizontal scrollbar) sits between the
    // editor and the status line (spec 9 row order). The EditorHost's bottom
    // frame edge is suppressed: the band now supplies the bottom border row.
    m_bottomBand = new BottomBand(this);
    auto* outer = new QVBoxLayout;
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    auto* centralWrap = new QWidget(this);
    auto* cw = new QVBoxLayout(centralWrap);
    cw->setContentsMargins(0, 0, 0, 0);
    cw->setSpacing(0);
    cw->addWidget(central, 1);
    cw->addWidget(m_bottomBand, 0);
    outer->addWidget(centralWrap);
    setCentralWidget(centralWrap);

    // F-key status bar (bottom).
    auto* fkeys = new FKeyStatusBar(this);
    setStatusBar(fkeys);
}

void ShellWindow::setEditorWidget(QWidget* w)
{
    if (!m_editorLayout || !w)
        return;
    // The editor is the shell's default focus target. Nothing in the app ever
    // called setFocus(), so focusWidget() stayed null and forwarded keys fell
    // through to the shell, which ignores them: typing did nothing.
    m_editor = w;
    // Targeting is not enough - an editor widget only consumes keys while it
    // HOLDS focus (QLineEdit in particular drops a press it was merely posted).
    // setFocus() follows the focus proxy, so the real view gets it, not the
    // stacked container that wraps it.
    w->setFocus(Qt::OtherFocusReason);
    // Remove the current placeholder/editor row (everything below the TabStrip)
    // and replace it with an editor+minimap row. Index 0 is always the TabStrip.
    while (m_editorLayout->count() > 0) {   // drain placeholder + stale rows alike
        // takeAt(0): with a single item takeAt(1) returns nullptr and the loop
        // would spin forever (this hung the real app while tests passed,
        // because tests never call setEditorWidget twice).
        QLayoutItem* old = m_editorLayout->takeAt(0);
        if (old) {
            if (old->widget() && old->widget() != w && !qobject_cast<Minimap*>(old->widget()))
                old->widget()->deleteLater();
            delete old;
        }
    }

    // Editor (stretch) + Minimap (fixed strip) side by side.
    auto* row = new QWidget;
    row->setObjectName(QStringLiteral("editorRow"));
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    w->setParent(row);
    h->addWidget(w, 1);
    auto* mini = new Minimap(row);
    mini->setObjectName(QStringLiteral("minimap"));
    h->addWidget(mini, 0);
    // TV-revision2 has no minimap panel: it is a VS-Code leftover that painted
    // a near-black strip inside the TV window. Hidden by default (the View >
    // Show Minimap action is unchecked); the action toggles it back on.
    mini->setVisible(false);
    m_minimap = mini;
    m_editorLayout->addWidget(row);

    // TV-revision2 spec 1: Kate's own document vertical is HIDDEN (see
    // EditorTabManager::applyTvStyle); the window-border RightScrollBar and
    // the bottom band's horizontal bar are the visible controls. Binding them
    // to the view's scrollbars is the composition root's job (main.cpp calls
    // wireScrollbars(), which does the KTextEditor lookup — crt-shell does not
    // link KTextEditor). This method only installs the editor widget.
}

void ShellWindow::setCentralWidget(QWidget* w)
{
    QMainWindow::setCentralWidget(w);
}

// Bind the painted right-border and bottom-band scrollbars to the given
// document scrollbars (the KTextEditor::View's own verticalScrollBar() and
// horizontalScrollBar()), both directions.
//
// The old code identified the document scrollbar by scanning
// findChildren<QScrollBar*>() for the first vertical one and latched onto an
// empty placeholder (measured live: painted bar 0..0 vs the view's 0..366).
// Passing the view's accessors in is the reliable identification; the caller
// (main.cpp) does the KTextEditor lookup because crt-shell does not link
// KTextEditor. Safe to re-run on tab change: each pass disconnects the
// previous pair first, so nothing ping-pongs or double-fires.
void ShellWindow::wireScrollbars(QScrollBar* docV, QScrollBar* docH, QWidget* view)
{
    // KTextEditor::View::verticalScrollBar() can return an EMPTY wrapper in
    // this build: a live probe showed the accessor at 0..0 while the view's
    // real KateScrollBar carried 0..366. When the accessor has no range,
    // prefer the real KateScrollBar among the view's children — matched by
    // class name, so it is still found while the document is empty and the
    // bar has no range yet (it emits rangeChanged as the document grows).
    // A non-empty accessor is left alone: it is the one the tests and the
    // rest of the app identify the view by.
    auto pick = [view](QScrollBar* accessor, Qt::Orientation orient) -> QScrollBar* {
        if (!view || !accessor || accessor->maximum() > accessor->minimum())
            return accessor;
        QScrollBar* byClass = nullptr;
        QScrollBar* ranged = nullptr;
        for (QScrollBar* b : view->findChildren<QScrollBar*>()) {
            if (b->orientation() != orient || b == accessor)
                continue;
            if (b->metaObject()->className() == QLatin1String("KateScrollBar")) {
                if (!byClass)
                    byClass = b;
            } else if (b->maximum() > b->minimum() && !ranged) {
                ranged = b;
            }
        }
        return byClass ? byClass : (ranged ? ranged : accessor);
    };
    docV = pick(docV, Qt::Vertical);
    docH = pick(docH, Qt::Horizontal);

    // --- Vertical: the right-border bar mirrors the view's vertical ---
    if (m_rightBar) {
        if (m_docV && m_docV != docV) {
            disconnect(m_docV, nullptr, m_rightBar, nullptr);
            disconnect(m_rightBar, nullptr, m_docV, nullptr);
        }
        if (docV && docV != m_docV) {
            m_docV = docV;
            connect(docV, &QScrollBar::rangeChanged, m_rightBar, &QScrollBar::setRange);
            connect(docV, &QScrollBar::valueChanged, m_rightBar, &QScrollBar::setValue);
            connect(m_rightBar, &QScrollBar::valueChanged, docV, &QScrollBar::setValue);
        }
        // Seed on EVERY pass, not only when the object identity changes. The
        // view's KateScrollBar is the same object throughout, but it is 0..0
        // at construction and only gains its range once the document has
        // content. Seeding only on a new object left the painted bar stuck at
        // 0..0 for the whole session.
        if (docV) {
            m_rightBar->setRange(docV->minimum(), docV->maximum());
            m_rightBar->setPageStep(docV->pageStep());
            m_rightBar->setValue(docV->value());
        }
    }

    // --- Horizontal: the bottom band mirrors the view's horizontal ---
    BottomBand* band = m_bottomBand;
    TextScrollBar* hb = band ? band->horizontalBar() : nullptr;
    if (hb) {
        if (m_docH && m_docH != docH) {
            disconnect(m_docH, nullptr, hb, nullptr);
            disconnect(hb, nullptr, m_docH, nullptr);
        }
        if (docH && docH != m_docH) {
            m_docH = docH;
            connect(docH, &QScrollBar::rangeChanged, hb, &QScrollBar::setRange);
            connect(docH, &QScrollBar::valueChanged, hb, &QScrollBar::setValue);
            connect(hb, &QScrollBar::valueChanged, docH, &QScrollBar::setValue);
        }
        // Same always-seed rule as the vertical bar.
        if (docH) {
            hb->setRange(docH->minimum(), docH->maximum());
            hb->setPageStep(docH->pageStep());
            hb->setValue(docH->value());
        }
    }
}

TabStrip* ShellWindow::tabStrip() const
{
    return findChild<TabStrip*>();
}

FKeyStatusBar* ShellWindow::fkeyStatusBar() const
{
    return findChild<FKeyStatusBar*>();
}

BottomBand* ShellWindow::bottomBand() const
{
    return m_bottomBand;
}

void ShellWindow::paintEvent(QPaintEvent* ev)
{
    QMainWindow::paintEvent(ev);
}

QWidget* ShellWindow::keyFocusWidget() const
{
    // focusWidget() is authoritative when it resolves; otherwise fall back to
    // the editor rather than the shell, because key events do NOT propagate
    // down to a child and the shell would silently swallow them.
    if (auto* f = focusWidget())
        return f;
    if (m_editor)
        return m_editor;
    return const_cast<ShellWindow*>(this);
}
