// SPDX-License-Identifier: GPL-3.0-or-later
#include "ShellWindow.h"
#include "BorlandStyle.h"
#include "ScrollBarInstaller.h"
#include "MenuBar.h"
#include "TabStrip.h"
#include "Minimap.h"
#include "RightScrollBar.h"
#include "TextScrollBar.h"
#include "BorlandMenu.h"
#include "FKeyStatusBar.h"
#include "BottomBand.h"
#include "TvFrame.h"
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QFontDatabase>
#include <QFont>
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
// the frame layer). The TabStrip (the file-tab row) sits ABOVE the frame; the
// document's own top border row — ╔ ═ name ═ ╗ — is painted here, below it,
// in the band reserved by the (paint-less) title-row widget.
class EditorHost : public QWidget
{
public:
    explicit EditorHost(QWidget* parent = nullptr) : QWidget(parent)
    {
        setObjectName("editorHost");
        setAutoFillBackground(false);   // we paint the blue bg ourselves

        // The document's top border row is a BAND of this widget, below the tab
        // row and above the editor body. The band is reserved by a paint-less
        // widget the shell inserts into the layout (this class owns it so the
        // row's height and its painted glyphs can never disagree); everything
        // on it is painted here, edge to edge, so the corners can sit in the
        // frame's own cells.
        m_titleRow = new QWidget(this);
        m_titleRow->setObjectName(QStringLiteral("editorTitleRow"));
        m_titleRow->setAutoFillBackground(false);
        // Belt and braces with the QSS rule: a styled background here would
        // paint over the ╔ ═ ╗ that this class draws in the same band.
        m_titleRow->setAttribute(Qt::WA_NoSystemBackground, true);
        m_titleRow->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        // The band's position is decided by the layout, AFTER the label has
        // been placed. Track it: a label positioned once would keep the row's
        // construction-time y and paint the name on the tab row above.
        m_titleRow->installEventFilter(this);

        // The name itself is a real child widget (not painted text) so the
        // shell can retitle it per tab and tests can read it back.
        m_title = new QLabel(QStringLiteral("untitled"), this);
        m_title->setObjectName("editorTitle");
        m_title->setAlignment(Qt::AlignCenter);
        m_title->setAutoFillBackground(false);
        // Clicks on the border row belong to the frame, not to the label.
        m_title->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    // The band the shell must insert into its layout, between the tab row and
    // the editor body. It paints nothing: the glyphs are this widget's job.
    QWidget* titleRow() const { return m_titleRow; }

    // The row below the title row (editor body + scrollbar column). Only THIS
    // row is inset by the frame's left column: the tab row above it spans the
    // window edge to edge, so its left control sits in the window's first cell
    // (mock note 1), directly above the ║ column.
    void setContentRow(QWidget* w)
    {
        m_contentRow = w;
        updateFrameMargins();
    }

    // One character cell: the frame edge is a whole cell thick (TV look).
    int cellW() const { return qMax(6, fontMetrics().horizontalAdvance(QLatin1Char('M'))); }
    int cellH() const { return qMax(8, fontMetrics().height()); }

    void setTitle(const QString& name)
    {
        if (m_title->text() == name)
            return;
        m_title->setText(name);
        layoutTitle();
        update();
    }
    QString title() const { return m_title->text(); }

    // The layout's left margin is the frame column and its top margin is the
    // title row, so the children start inside the frame. Both must be
    // re-measured when the font lands: the QSS font is applied AFTER this
    // widget is constructed, so a margin taken at construction time is one
    // cell too narrow - the children then start ON the border column and paint
    // over its outer stroke (that is what made the left border look "thicker
    // on one side" at the bottom-left corner).
    void updateFrameMargins()
    {
        if (m_titleRow)
            m_titleRow->setFixedHeight(cellH());
        if (m_contentRow && m_contentRow->layout()) {
            const QMargins m = m_contentRow->layout()->contentsMargins();
            if (m.left() != cellW())
                m_contentRow->layout()->setContentsMargins(cellW(), m.top(), m.right(),
                                                           m.bottom());
        }
        layoutTitle();
    }

    // Place the name on the character grid: a whole number of cells wide,
    // centred in the row, leaving exactly two cells of rule-free daylight
    // either side for the paintEvent to skip. Coordinates are the HOST's, so
    // the label lines up with glyphs painted across the full width.
    void layoutTitle()
    {
        if (!m_title || !m_titleRow)
            return;
        const int cw = cellW();
        const int ch = cellH();
        const QFontMetrics fm(font());
        int cells = (fm.horizontalAdvance(m_title->text()) + cw - 1) / cw;
        const int maxCells = qMax(1, width() / cw - 6);   // corner + 2 + 2 + corner
        cells = qBound(1, cells, maxCells);
        const int w = cells * cw;
        const int x = ((width() - w) / 2 / cw) * cw;
        m_title->setGeometry(QRect(x, m_titleRow->geometry().top(), w, ch));
        m_title->raise();   // the band widget is later in the child list
        // Same crisp 1-bit rendering as the frame glyphs: antialiased text
        // tints the strokes and reads soft next to the box-drawing characters.
        QFont f = font();
        f.setStyleStrategy(QFont::NoAntialias);
        if (m_title->font() != f)
            m_title->setFont(f);
    }

    // The title row's cell range, minus the corner cells: [start, end) in px.
    int titleGapStart() const
    {
        return m_title ? m_title->geometry().left() - 2 * cellW() : -1;
    }
    int titleGapEnd() const
    {
        return m_title ? m_title->geometry().right() + 1 + 2 * cellW() : -1;
    }
protected:
    bool eventFilter(QObject* watched, QEvent* ev) override
    {
        if (watched == m_titleRow &&
            (ev->type() == QEvent::Move || ev->type() == QEvent::Resize))
            layoutTitle();
        return QWidget::eventFilter(watched, ev);
    }

    void resizeEvent(QResizeEvent* ev) override
    {
        QWidget::resizeEvent(ev);
        updateFrameMargins();
    }

    void changeEvent(QEvent* ev) override
    {
        QWidget::changeEvent(ev);
        if (ev->type() == QEvent::FontChange)
            updateFrameMargins();
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        const QRect r = rect();
        p.fillRect(r, kBgBlue);

        // --- Top border row: ╔ ═ (two spaces) name (two spaces) ═ ╗ ---
        // ╔ occupies the first cell; ╗ the LAST cell, which is the scrollbar's
        // own column, so the row terminates exactly on the bar's top edge.
        const int cw = cellW();
        const int ch = cellH();
        const int rowTop = m_titleRow ? m_titleRow->geometry().top() : 0;
        const int rowH = m_titleRow ? m_titleRow->geometry().height() : ch;
        TvFrame::glyph(p, QRect(0, rowTop, cw, rowH), QChar(0x2554), kFrameWhite);
        const int gapStart = titleGapStart();
        const int gapEnd = titleGapEnd();
        if (gapStart > cw)
            TvFrame::tileH(p, QRect(cw, rowTop, gapStart - cw, rowH), cw,
                           QChar(0x2550), kFrameWhite);
        if (gapEnd > 0 && gapEnd < r.width() - cw)
            TvFrame::tileH(p, QRect(gapEnd, rowTop, r.width() - cw - gapEnd, rowH), cw,
                           QChar(0x2550), kFrameWhite);
        TvFrame::glyph(p, QRect(r.width() - cw, rowTop, cw, rowH), QChar(0x2557),
                       kFrameWhite);

        // Left border column: the ║ GLYPH tiled down a cell-wide band. Drawing
        // it as two hairlines gave exactly 2 px per row and read as thin; the
        // glyph uses the font's own full-weight strokes.
        // No right verticals: TV-revision2 spec 1 hands the right border
        // column to the 1-cell RightScrollBar (teal ▲/▒/■/▼).
        // It starts BELOW the title row: the ╔'s strokes are the top of the ║.
        const int colTop = rowTop + rowH;
        TvFrame::tileV(p, QRect(0, colTop, cw, r.height() - colTop), ch,
                       QChar(0x2551), kFrameWhite);
        // NOTE: the bottom border row is owned by BottomBand (it also carries
        // the cursor position and the horizontal scrollbar). Drawing it here
        // too produced two rules with a blue gap between them.
    }

private:
    QWidget* m_titleRow = nullptr;
    QWidget* m_contentRow = nullptr;
    QLabel* m_title = nullptr;
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

    // Force the QSS + font resources into the link (static lib resources are
    // dropped by the linker unless referenced). The font .qrc is AUTORCC-
    // compiled as "fonts" -> qInitResources_fonts.
    Q_INIT_RESOURCE(shell_qss);
    Q_INIT_RESOURCE(fonts);

    // Load the bundled TV typeface FIRST so the QSS font-family below resolves.
    // addApplicationFontFromData registers the family in-process (tests and the
    // binary both hit this path - no fontconfig needed).
    QStringList tvFamilies;
    {
        QFile ttf(QStringLiteral(":/fonts/PxPlus_IBM_VGA8.ttf"));
        if (ttf.open(QIODevice::ReadOnly)) {
            const QByteArray fontData = ttf.readAll();
            const int id = QFontDatabase::addApplicationFontFromData(fontData);
            if (id >= 0)
                tvFamilies = QFontDatabase::applicationFontFamilies(id);
        }
    }
    // Set the app-wide default so every widget (and the QSS fallback) picks the
    // VGA face. Sizing still comes from the QSS (16px) and tabs.setFontSize.
    if (!tvFamilies.isEmpty()) {
        if (auto* qapp = qobject_cast<QApplication*>(QCoreApplication::instance())) {
            QFont def = qapp->font();
            def.setFamily(tvFamilies.first());
            qapp->setFont(def);
        }
    }

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
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    auto* tabs = new TabStrip(central);
    lay->addWidget(tabs);
    // The DOCUMENT's top border row (╔ ═ name ═ ╗) sits BELOW the tab row and
    // ABOVE the editor body. The band widget only reserves the row; EditorHost
    // paints it across the full width so ╔ and ╗ land in the frame's own cells.
    lay->addWidget(central->titleRow());
    // Body row: editor slot (stretch) + the window-border scrollbar column.
    auto* body = new QWidget(central);
    body->setObjectName(QStringLiteral("editorBody"));
    auto* bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);
    // The frame's left column inset applies to the CONTENT row only (set after
    // its own margins so it is not overwritten): the tab row above spans the
    // full width, the editor body starts inside the ║ column. The body itself
    // stays full-width and must not paint: its background would cover the ║
    // column the host draws under it (QSS makes it transparent; belt and
    // braces here, exactly as for the title-row band).
    body->setAttribute(Qt::WA_NoSystemBackground, true);
    central->setContentRow(body);
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

// The document's top border row reads ╔ ═ name ═ ╗. EditorHost owns that row
// (it paints the frame), so the name is forwarded to it rather than kept here:
// one owner for the row, and the label stays a real child the tests can read.
void ShellWindow::setEditorTitle(const QString& name)
{
    auto* host = findChild<QWidget*>(QStringLiteral("editorHost"));
    if (!host)
        return;
    static_cast<EditorHost*>(host)->setTitle(name.isEmpty() ? QStringLiteral("untitled")
                                                           : name);
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

// Editor right-click menu: the edit commands (Undo/Redo/Cut/Copy/Paste) plus
// the spellcheck toggle, drawn as a BorlandMenu so it matches the dropdowns.
//
// It REUSES the Edit menu's own QActions rather than duplicating them: one
// implementation per command, and Undo/Redo keep the enable state that
// EditorTabManager::historyChanged drives.
void ShellWindow::showEditorContextMenu(const QPoint& shellLocal)
{
    BorlandMenu* menu = findChild<BorlandMenu*>(QStringLiteral("editorContextMenu"));
    if (!menu) {
        menu = new BorlandMenu(this);
        menu->setObjectName(QStringLiteral("editorContextMenu"));
        auto* bar = findChild<MenuBar*>();
        auto add = [menu, bar](const char* name) {
            QAction* a = bar ? bar->findChild<QAction*>(QLatin1String(name)) : nullptr;
            if (a)
                menu->addAction(a);
        };
        add("actionEditUndo");
        add("actionEditRedo");
        menu->addSeparator();
        add("actionEditCut");
        add("actionEditCopy");
        add("actionEditPaste");
        menu->addSeparator();
        add("actionEditSpellcheck");
    }
    // popup(), never exec(): the shell is never mapped, so exec()'s nested
    // event loop and the native context-menu path do not behave here. The
    // bridge drives the open menu with synthesized events, like the menu bar.
    menu->popup(mapToGlobal(shellLocal));
}

bool ShellWindow::isEditorArea(QWidget* w) const
{
    return w && m_editor && (w == m_editor || m_editor->isAncestorOf(w));
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
