// SPDX-License-Identifier: GPL-3.0-or-later
#include "ShellWindow.h"
#include "EditorPane.h"
#include "EditorSplitter.h"
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
#include "src/AppIdentity.h"
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
#include <sonnet/speller.h>

namespace {
const QColor kBgBlue(0x00, 0x00, 0xAA);
// Task 22: daylight between the menu bar and the tab row (user: "a bit more
// padding"). The menu bar sits above the central widget, not in its layout, so
// the gap is this layout's top margin.
const int kTabsTopMargin = 4;

} // namespace

ShellWindow::ShellWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(AppIdentity::displayName());
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

    // Central host: the TV window-blue desktop with the TabStrip above the
    // editor. The pane's frame (╔═╗ / ║ / band) is EditorPane's job; this host
    // holds the tab row, the splitter (panes + minimap), and the band. Its top
    // margin is the daylight between the menu bar and the tab row (Task 22).
    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("editorHostDesktop"));
    central->setAutoFillBackground(false);
    auto* lay = new QVBoxLayout(central);
    lay->setContentsMargins(0, kTabsTopMargin, 0, 0);
    lay->setSpacing(0);
    auto* tabs = new TabStrip(central);
    lay->addWidget(tabs);

    // The row below the tab strip: an EditorSplitter hosting pane 0 (and pane 1
    // when split) with the minimap as the last child (far right). The splitter
    // collapses cleanly to one child while unsplit, so the single-pane look is
    // unchanged.
    m_splitter = new EditorSplitter(central);
    m_pane0 = new EditorPane(m_splitter);
    m_pane0->setPaneIndex(0);
    m_splitter->addPane(m_pane0);
    m_rightBar = m_pane0->rightBar();
    m_editorLayout = qobject_cast<QVBoxLayout*>(m_pane0->bodySlot()->layout());
    lay->addWidget(m_splitter, 1);

    // Bottom band: a SIBLING directly below the splitter (so the pane's ║
    // reaches its bottom edge and meets the band's ╚), one band per pane in the
    // split (pane 0's here; pane 1's is added in Task 4).
    m_bottomBand = m_pane0->bottomBand();
    m_bottomBand->setParent(central);
    lay->addWidget(m_bottomBand, 0);
    setCentralWidget(central);

    auto* outer = new QVBoxLayout;
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    auto* centralWrap = new QWidget(this);
    auto* cw = new QVBoxLayout(centralWrap);
    cw->setContentsMargins(0, 0, 0, 0);
    cw->setSpacing(0);
    cw->addWidget(central, 1);
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
    // The editor is the shell's default focus target (see comment below).
    m_editor = m_splitter ? static_cast<QWidget*>(m_splitter) : w;
    m_editorWidget = w;   // the focus/proxy target (keyFocusWidget walks this)
    // Remove the current placeholder (everything in pane 0's slot) and install
    // the editor widget directly. takeAt(0): with a single item takeAt(1)
    // returns nullptr and the loop would spin forever.
    while (m_editorLayout->count() > 0) {
        QLayoutItem* old = m_editorLayout->takeAt(0);
        if (old) {
            if (old->widget() && old->widget() != w && !qobject_cast<Minimap*>(old->widget()))
                old->widget()->deleteLater();
            delete old;
        }
    }

    // The editor widget lives in pane 0's body slot.
    w->setParent(m_pane0 ? m_pane0->bodySlot() : nullptr);
    m_editorLayout->addWidget(w, 1);

    // The minimap is the splitter's LAST child (far right of the whole row).
    // Hidden by default (the View > Show Minimap action is unchecked).
    if (m_splitter) {
        auto* mini = new Minimap(m_splitter);
        mini->setObjectName(QStringLiteral("minimap"));
        mini->setVisible(false);
        m_splitter->setMinimap(mini);
        m_minimap = mini;
    }

    // Now that the editor is in its final parent, give it focus: the shell has
    // no other key target, and KTextEditor only enables its editing actions
    // (cursor keys, Backspace, Delete) for a view that holds focus.
    w->setFocus(Qt::OtherFocusReason);

    // TV-revision2 spec 1: Kate's own document vertical is HIDDEN (see
    // EditorTabManager::applyTvStyle); the window-border RightScrollBar and
    // the bottom band's horizontal bar are the visible controls. Binding them
    // to the view's scrollbars is the composition root's job (main.cpp calls
    // wireScrollbars(), which does the KTextEditor lookup — crt-shell does not
    // link KTextEditor). This method only installs the editor widget.
}

// The document's top border row reads ╔ ═ name ═ ╗. EditorPane owns that row
// (it paints the frame), so the name is forwarded to it rather than kept here:
// one owner for the row, and the label stays a real child the tests can read.
void ShellWindow::setEditorTitle(const QString& name)
{
    if (m_pane0)
        m_pane0->setTitle(name);
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

// Editor right-click menu: the edit commands (Undo/Redo/Cut/Copy/Paste), drawn
// as a BorlandMenu so it matches the dropdowns.
//
// It REUSES the Edit menu's own QActions rather than duplicating them: one
// implementation per command, and Undo/Redo keep the enable state that
// EditorTabManager::historyChanged drives.
//
// The spellcheck toggle is NOT here: it lives in the Edit menu bar only (user,
// 2026-11-17 - "that's in the menu bar"), so the menu ends after Paste.
// Sonnet's own default dictionary, used only when the composition root has not
// named one yet. The root (main.cpp) owns the speller the status selector uses
// and pushes its language down through setSpellLanguage(), so the popout and
// the status tag never disagree about what is misspelled.
namespace {
QString sonnetDefaultLanguage()
{
    static const QString lang = [] {
        Sonnet::Speller s;
        return s.language();
    }();
    return lang;
}
} // namespace

void ShellWindow::setBlockMenuActions(QAction* collapse, QAction* expand, QAction* highlight,
                                      std::function<void()> refresh)
{
    m_blockCollapse = collapse;
    m_blockExpand = expand;
    m_blockHighlight = highlight;
    m_blockRefresh = std::move(refresh);
}

void ShellWindow::showEditorContextMenu(const QPoint& shellLocal)
{
    showEditorContextMenu(shellLocal, SpellWord());
}

void ShellWindow::showEditorContextMenu(const QPoint& shellLocal, const SpellWord& word)
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
    }

    // Block commands (2026-11-20). Added once, on whichever popup comes first -
    // the menu may already have been built before the editor manager handed us
    // the actions. They are never removed or deleted here: they belong to
    // EditorTabManager, which keeps them alive across right-clicks.
    if (m_blockCollapse && m_blockExpand && m_blockHighlight
        && !menu->actions().contains(m_blockCollapse)) {
        menu->addSeparator();
        menu->addAction(m_blockCollapse);
        menu->addAction(m_blockExpand);
        menu->addAction(m_blockHighlight);
    }
    // The caret decides which block the commands act on, so ask for a fresh
    // enabled state on every right-click.
    if (m_blockRefresh)
        m_blockRefresh();

    // Rebuild the spell section on EVERY right-click so it always describes the
    // word actually under the pointer. Remove anything a previous click left,
    // then add the section only if the word is genuinely misspelled (a correct
    // word leaves the menu exactly as Task 2 left it).
    for (QAction* stale : menu->actions())
        if (stale->objectName() == QLatin1String("spellWord")
            || stale->objectName() == QLatin1String("spellDivider")) {
            menu->removeAction(stale);
            stale->deleteLater();
        }
    delete m_spellPopout;
    m_spellPopout = nullptr;
    m_spellWord = SpellWord();

    Sonnet::Speller speller(m_spellLang.isEmpty() ? sonnetDefaultLanguage() : m_spellLang);
    if (word.valid() && !word.text.isEmpty() && speller.isMisspelled(word.text)) {
        m_spellWord = word;
        buildSpellSection(menu, word, speller);
    }

    // popup(), never exec(): the shell is never mapped, so exec()'s nested
    // event loop and the native context-menu path do not behave here. The
    // bridge drives the open menu with synthesized events, like the menu bar.
    menu->popup(mapToGlobal(shellLocal));
}

// The spell section (GUI pass, Task 3): the misspelled WORD as a bold headline
// row, a divider under it, and a hover popout of suggestions / add / ignore.
void ShellWindow::buildSpellSection(BorlandMenu* menu, const SpellWord& word,
                                    const Sonnet::Speller& speller)
{
    QFont head = menu->font();
    head.setBold(true);

    auto* headline = new QAction(menu);
    headline->setObjectName(QStringLiteral("spellWord"));
    headline->setText(word.text);
    // A word is not a command: no red hotkey, no underline, no mnemonic, and
    // bold by double-strike (the TV face is fixed-pitch bitmap - setBold()'s
    // advance is a no-op there).
    BorlandMenu::setRowFont(headline, head);
    BorlandMenu::setRowNoMnemonic(headline, true);
    BorlandMenu::setRowBold(headline, true);
    if (!menu->actions().isEmpty())
        menu->insertAction(menu->actions().first(), headline);
    else
        menu->addAction(headline);

    // Divider directly under the headline, ABOVE the edit commands. addSeparator()
    // appends, so move it; reading actions().value(1) blindly would rename a
    // real command (that clobbered actionEditUndo in an earlier draft).
    QAction* divider = menu->addSeparator();
    menu->insertAction(menu->actions().at(1), divider);
    divider->setObjectName(QStringLiteral("spellDivider"));

    buildSpellPopout(speller.suggest(word.text), speller);
    // HOVERING the headline opens the popout (the user asked for it on hover).
    connect(headline, &QAction::hovered, this, &ShellWindow::showSpellPopout);
}

// The hover popout: a BorlandMenu, so literally the same widget the right-click
// menu is made of. Layout: divider, suggestions, divider, add-to-dictionary,
// ignore.
void ShellWindow::buildSpellPopout(const QStringList& suggestions,
                                   const Sonnet::Speller& speller)
{
    Q_UNUSED(speller);
    auto* popout = new BorlandMenu(this);
    popout->setObjectName(QStringLiteral("spellPopout"));
    m_spellPopout = popout;

    QAction* topSep = popout->addSeparator();
    topSep->setObjectName(QStringLiteral("spellPopoutTopDivider"));

    for (int i = 0; i < suggestions.size(); ++i) {
        auto* s = new QAction(popout);
        s->setObjectName(QStringLiteral("spellSuggest%1").arg(i));
        s->setText(suggestions.at(i));
        popout->addAction(s);
        connect(s, &QAction::triggered, this, [this, s] {
            applySpellReplacement(s->text());
        });
    }

    QAction* sep = popout->addSeparator();
    sep->setObjectName(QStringLiteral("spellPopoutDivider"));

    auto* add = new QAction(popout);
    add->setObjectName(QStringLiteral("spellAddToDictionary"));
    add->setText(tr("Add to dictionary"));
    popout->addAction(add);
    connect(add, &QAction::triggered, this, [this] {
        if (!m_spellWord.valid())
            return;
        Sonnet::Speller sp(m_spellLang.isEmpty() ? sonnetDefaultLanguage() : m_spellLang);
        sp.addToPersonal(m_spellWord.text);
    });

    auto* ignore = new QAction(popout);
    ignore->setObjectName(QStringLiteral("spellIgnoreWord"));
    ignore->setText(tr("Ignore word"));
    popout->addAction(ignore);
    connect(ignore, &QAction::triggered, this, &ShellWindow::ignoreSpellWord);
}

// crt-shell does NOT link KTextEditor (a boundary this file keeps), so the edit
// is emitted as data for the composition root to perform.
void ShellWindow::applySpellReplacement(const QString& replacement)
{
    if (!m_spellWord.valid())
        return;
    emit spellReplacementRequested(m_spellWord.line, m_spellWord.startCol,
                                   m_spellWord.endCol, replacement);
    if (m_spellPopout)
        m_spellPopout->hide();
    if (BorlandMenu* menu = findChild<BorlandMenu*>(QStringLiteral("editorContextMenu")))
        menu->hide();
}

void ShellWindow::ignoreSpellWord()
{
    if (!m_spellWord.valid())
        return;
    emit spellIgnoreRequested(m_spellWord.line, m_spellWord.startCol, m_spellWord.text);
    if (m_spellPopout)
        m_spellPopout->hide();
    if (BorlandMenu* menu = findChild<BorlandMenu*>(QStringLiteral("editorContextMenu")))
        menu->hide();
}

bool ShellWindow::spellPopoutVisible() const
{
    return m_spellPopout && m_spellPopout->isVisible();
}

// Popout hangs off the RIGHT edge of the context menu, clamped inside the shell
// so it stays in the composited area.
void ShellWindow::showSpellPopout()
{
    if (!m_spellPopout)
        return;
    BorlandMenu* menu = findChild<BorlandMenu*>(QStringLiteral("editorContextMenu"));
    if (!menu)
        return;
    const QSize size = m_spellPopout->sizeHint();
    const QPoint shellTopLeft = mapToGlobal(QPoint(0, 0));
    const QRect shellRect(shellTopLeft, this->size());
    int x = menu->geometry().right() + 1;
    int y = menu->geometry().top();
    x = qBound(shellRect.left(), x,
               qMax(shellRect.left(), shellRect.right() + 1 - size.width()));
    y = qBound(shellRect.top(), y,
               qMax(shellRect.top(), shellRect.bottom() + 1 - size.height()));
    m_spellPopout->popup(QPoint(x, y));
}

void ShellWindow::setSpellPopupHost(ContentBridge* bridge)
{
    m_spellBridge = bridge;
}

void ShellWindow::setSpellLanguage(const QString& lang)
{
    m_spellLang = lang;
}

void ShellWindow::setSpellWordResolver(std::function<SpellWord(const QPoint&)> fn)
{
    m_spellResolver = std::move(fn);
}

SpellWord ShellWindow::spellWordAt(const QPoint& shellLocal) const
{
    return m_spellResolver ? m_spellResolver(shellLocal) : SpellWord();
}

bool ShellWindow::isEditorArea(QWidget* w) const
{
    return w && m_editor && (w == m_editor || m_editor->isAncestorOf(w));
}

bool ShellWindow::splitOpen() const
{
    return m_splitter && m_splitter->splitOpen();
}

EditorPane* ShellWindow::paneWidget(int i) const
{
    return m_splitter ? m_splitter->pane(i) : nullptr;
}

// Which pane a press landed in. The bridge calls this after resolving the hit
// widget; it walks up to the enclosing pane. STUB in Task 2: always pane 0.
// Task 6 replaces the body with the real enclosing-pane walk + signal.
void ShellWindow::notePressInWidget(QWidget* w)
{
    Q_UNUSED(w);
    // Task 2: no active-pane tracking yet.
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

void ShellWindow::setKeyboardOwner(QWidget* owner)
{
    // A stale owner is worse than none: a deleted dock would leave the app
    // unable to type into the editor at all.
    if (owner && !qobject_cast<QWidget*>(owner))
        owner = nullptr;
    m_keyboardOwner = owner;
}

bool ShellWindow::keyboardIsOwned() const
{
    // Ownership lasts only while the owner is on screen. Hiding is the normal
    // end of a dock or modal, and both TvFileDock::hideEvent and TvModal's
    // Show/Hide filter also clear it - this is the belt-and-braces check for
    // an owner destroyed or hidden without either.
    //
    // isHidden(), NOT isVisible() (handoff trap #3): the shell renders
    // offscreen and is never mapped, and child visibility under it is
    // unreliable in both directions - an owner that IS shown can report
    // isVisible()==false, which would let every keystroke leak past the guard
    // to the menu and the editor behind the dialog (the 2026-11-21 report:
    // "backspacing ... also backspaces in the editor"). isHidden() tracks the
    // explicit show()/hide() pair these dialogs use.
    return m_keyboardOwner && !m_keyboardOwner->isHidden();
}

QWidget* ShellWindow::keyFocusWidget() const
{
    // focusWidget() is authoritative when it resolves; otherwise fall back to
    // the editor rather than the shell, because key events do NOT propagate
    // down to a child and the shell would silently swallow them.
    if (auto* f = focusWidget())
        return f;
    if (m_editorWidget) {
        // ...and follow the editor's focus-proxy chain: the tab container
        // proxies to the live KTextEditor::View, which proxies to the canvas
        // widget that actually handles keys. Stopping at the container was a
        // dead end - it takes no focus and ignores keys, so nothing typed
        // until the user clicked into the text. (m_editorWidget is the
        // installed editor widget; m_editor is the splitter, which has no
        // proxy and would dead-end the keys.)
        QWidget* w = m_editorWidget;
        for (int depth = 0; depth < 8; ++depth) {
            QWidget* proxy = w->focusProxy();
            if (!proxy || proxy == w)
                break;
            w = proxy;
        }
        return w;
    }
    return const_cast<ShellWindow*>(this);
}

void ShellWindow::setEditorShortcutHandler(std::function<bool(const QKeySequence&)> fn)
{
    m_editorShortcut = std::move(fn);
}

bool ShellWindow::triggerEditorShortcut(const QKeySequence& seq) const
{
    return m_editorShortcut && m_editorShortcut(seq);
}
