// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QApplication>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QClipboard>
#include "EditorTabManager.h"
#include "HexSwatchProvider.h"
#include "FindReplaceDialog.h"
#include "LineAffixDialog.h"
#include "ColourWiring.h"
#include <ktexteditor/editor.h>
#include <ktexteditor/document.h>
#include <ktexteditor/view.h>
#include <ktexteditor/cursor.h>
#include <ktexteditor/range.h>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/Theme>
#include <QFileInfo>
#include <QAction>
#include <KActionCollection>
#include <QWidget>
#include <QInputDialog>
#include <QScrollBar>
#include <QTimer>
#include <QPointer>
#include <QEvent>
#include <QScrollBar>
#include <QLineEdit>
#include <QRegularExpression>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QFile>
#include <QFileSystemWatcher>
#include <QMutex>
#include <QMutexLocker>
#include "ReloadDialog.h"
#include "FoldIndicator.h"
#include "src/shell/TvModal.h"

// Register the bundled TV face so the editor can actually resolve it. The font
// lives in fonts/fonts.qrc (linked into libcrt-shell), and resources in a
// STATIC library are not auto-registered - ShellWindow calls Q_INIT_RESOURCE
// (fonts), but that only helps binaries that link the shell. EditorTabManager
// must register it too, or setFamily("PxPlus IBM VGA8") silently falls back to
// the system face and the editor renders a different font than the chrome.
//
// NOTE: Q_INIT_RESOURCE expands to a do/while and references a symbol whose
// name is generated WITHOUT the enclosing namespace - so the call must live at
// file scope, never inside an anonymous namespace (or the compiler mangles it
// to (anonymous namespace)::qInitResources_fonts and the link fails).
static const QString& tvFamily()
{
    static QMutex mutex;
    static bool loaded = false;
    static QString family;
    QMutexLocker lock(&mutex);
    if (!loaded) {
        loaded = true;
        Q_INIT_RESOURCE(fonts);
        QFile ttf(QStringLiteral(":/fonts/PxPlus_IBM_VGA8.ttf"));
        if (ttf.open(QIODevice::ReadOnly)) {
            const int id = QFontDatabase::addApplicationFontFromData(ttf.readAll());
            if (id >= 0 && !QFontDatabase::applicationFontFamilies(id).isEmpty())
                family = QFontDatabase::applicationFontFamilies(id).first();
        }
    }
    return family;
}

EditorTabManager::EditorTabManager(QObject* parent) : QObject(parent)
{
    m_container = new QStackedWidget;
    m_container->setObjectName(QStringLiteral("editorStack"));
    // Coloured Brackets (2026-11-20): one wiring instance follows the active
    // view; it owns its own MovingRanges and re-points in trackActiveCursor().
    m_brackets = new ColourWiring(this);
    // Block commands for the right-click menu (2026-11-20): created once, then
    // re-synced from the caret so the menu always describes the block under it.
    buildBlockActions();
    // External-change detection (2026-11-20): Kate has no watcher of its own, so
    // this one is the only thing that can notice a file changing underneath us.
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this,
            [this](const QString& path) { onWatchedPathChanged(path); });
    // A rename-replace drops the file from the watcher without a second
    // fileChanged, so the directory is watched too: it tells us the path came
    // back and lets onWatchedPathChanged re-arm it.
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
            [this](const QString& dir) {
                const QStringList paths = m_watchedDoc.keys();   // snapshot: a
                // close inside the loop mutates the hash.
                for (const QString& path : paths) {
                    if (QFileInfo(path).absolutePath() == dir)
                        onWatchedPathChanged(path);
                }
            });
    // Ensure a first tab exists so activeView() is meaningful immediately.
    if (m_docs.isEmpty())
        newDocument();
}

// Task 12: Turbo Vision editor styling — TV window-blue field, yellow text.
// KTextEditor paints its own background from its syntax scheme, so the widget
// palette is not enough; set the view's background/foreground/selection keys.
namespace {
// TV-revision2 spec 1: Kate re-shows its document vertical scrollbar when it
// realizes overflow or relayouts, so a one-shot hide() does not stick. This
// filter re-hides vertical bars on the view's Show/Resize/LayoutRequest and
// hides any vertical bar the moment Kate adds it. The hide itself is deferred
// one event-loop turn (the ScrollBarInstaller lesson: mutating a widget while
// its own Show event is in flight is what segfaulted).
class VerticalScrollHider : public QObject
{
public:
    explicit VerticalScrollHider(QObject* target)
        : QObject(target)
    {
        target->installEventFilter(this);
    }
    bool eventFilter(QObject* watched, QEvent* ev) override
    {
        const auto t = ev->type();
        if (t == QEvent::ChildAdded) {
            if (auto* ce = static_cast<QChildEvent*>(ev))
                if (auto* sb = qobject_cast<QScrollBar*>(ce->child()))
                    if (sb->orientation() == Qt::Vertical) {
                        sb->installEventFilter(this);
                        rehide(sb);
                    }
        } else if (t == QEvent::Show || t == QEvent::Resize
                   || t == QEvent::LayoutRequest) {
            if (auto* bar = qobject_cast<QScrollBar*>(watched)) {
                // The bar itself re-shows (its own Show event) - catch it.
                if (bar->orientation() == Qt::Vertical)
                    rehide(bar);
            } else if (auto* w = qobject_cast<QWidget*>(watched)) {
                // Watch EVERY vertical bar, including ones that were born
                // before this filter attached (ChildAdded never fires for them).
                for (auto* sb : w->findChildren<QScrollBar*>())
                    if (sb->orientation() == Qt::Vertical) {
                        sb->installEventFilter(this);
                        rehide(sb);
                    }
            }
        }
        return QObject::eventFilter(watched, ev);
    }

private:
    static void rehide(QScrollBar* bar)
    {
        if (!bar || bar->isHidden())
            return;
        QPointer<QScrollBar> guard(bar);
        QTimer::singleShot(0, bar, [guard]() {
            if (guard && !guard->isHidden())
                guard->hide();
        });
    }
};

// The range every text tool works on: a single selection if there is one, else
// the whole document. The selection is treated as TEXT, not expanded to whole
// lines, so a mid-line selection gets the affix at the selection edge - the
// same mental model the case tools use.
KTextEditor::Range activeRange(KTextEditor::View* v, KTextEditor::Document* doc)
{
    if (v->selection() && v->selectionRanges().size() == 1)
        return v->selectionRanges().first();
    const int lastLine = doc->lines() - 1;
    return KTextEditor::Range(0, 0, lastLine, doc->lineLength(lastLine));
}
} // namespace

void EditorTabManager::applyTvStyle(KTextEditor::View* view)
{
    if (!view)
        return;
    // The whole chrome (menu, tabs, frame, scrollbars) is drawn with the
    // bundled PxPlus IBM VGA8 face on an 8 px character grid; KTextEditor's
    // own "font" config defaults to the system font (Adwaita Mono here), so the
    // editor text was a different face - the middle of the window then looked
    // narrower than the chrome around it. Pin the face here so every new view
    // gets it; setFontSize()/zoom only ever change the pixel size.
    const QString family = tvFamily();
    if (!family.isEmpty()) {
        QFont f = view->configValue(QStringLiteral("font")).value<QFont>();
        f.setFamily(family);
        f.setPixelSize(16);
        view->setConfigValue(QStringLiteral("font"), f);
        // setConfigValue() updates the "font" setting but does not repaint the
        // widget with it - the view kept rendering in Noto Sans. Push the font
        // onto the widget itself so the glyphs on screen are the bundled face.
        view->setFont(f);
    }
    const QColor blue(0x00, 0x00, 0xAA);
    const QColor yellow(0xFF, 0xFF, 0x55);
    view->setAutoFillBackground(true);
    QPalette pal = view->palette();
    pal.setColor(QPalette::Base, blue);
    pal.setColor(QPalette::Window, blue);
    pal.setColor(QPalette::Text, yellow);
    view->setPalette(pal);
    // Best-effort scheme-independent keys (validated against configKeys()).
    view->setConfigValue(QStringLiteral("background-color"), blue);
    // NOTE: "foreground-color" is NOT a view config key (configKeys() has no
    // such entry); text color comes from the active KSyntaxHighlighting theme,
    // selected below via the custom "CRTKTV" theme.
    // TV selection = light gray-lavender block (spec section 4).
    view->setConfigValue(QStringLiteral("selection-color"),
                         QColor(0xC8, 0xC8, 0xD8));
    // Spec 1: gutter/line numbers flat on the SAME blue (no black strip) and
    // line numbers yellow, matching the bundled CRTKTV theme.
    view->setConfigValue(QStringLiteral("icon-border-color"), blue);
    view->setConfigValue(QStringLiteral("line-number-color"), yellow);
    view->setConfigValue(QStringLiteral("current-line-number-color"), yellow);
    // Spec 6 addendum: NO green chips at all — hide KTextEditor's built-in
    // status strip. It has no public API; locate by class name (KateViewBar
    // hosts KateStatusBar's QPushButton row; hide both when present).
    for (QWidget* w : view->findChildren<QWidget*>()) {
        const char* cls = w->metaObject()->className();
        if (qstrcmp(cls, "KateViewBar") == 0 || qstrcmp(cls, "KateStatusBar") == 0)
            w->hide();
    }
    // TV-revision2 spec 1: the WINDOW border carries the vertical scrollbar
    // (ShellWindow's 1-cell RightScrollBar mirrors this hidden bar), so
    // Kate's own document vertical must never show.
    for (QScrollBar* sb : view->findChildren<QScrollBar*>())
        if (sb->orientation() == Qt::Vertical) {
            sb->hide();
            sb->installEventFilter(this);   // watch pre-existing bars now;
                                            // the hider re-installs (no-op)
        }
    new VerticalScrollHider(view);   // re-hide stub is idempotent
    applyTvTheme(view);
    // Word wrap is ON for every new view (2026-11-19). The View menu toggle
    // changes the active view only, so this is the default, not a global.
    view->setConfigValue(QStringLiteral("dynamic-word-wrap"), true);
    startCaretBlink(view);
}

// Kate starts its caret-blink timer ONLY in KateViewInternal::focusInEvent.
// This shell is never mapped (WA_DontShowOnScreen), so Qt never delivers
// FocusIn on its own and the caret renders solid forever. Deliver a FocusIn to
// the canvas by hand: it starts the repeating m_cursorTimer, which toggles
// renderer()->drawCaret() each tick - the blink. It also runs slotGotFocus(),
// which enables Kate's edit actions (a bonus: arrows/backspace depend on it).
// Find the canvas by class name (the same trick applyTvStyle uses for
// KateViewBar); it is a QWidget child of the view.
void EditorTabManager::startCaretBlink(KTextEditor::View* view)
{
    if (!view)
        return;
    for (QWidget* w : view->findChildren<QWidget*>()) {
        if (qstrcmp(w->metaObject()->className(), "KateViewInternal") == 0) {
            // Post (not send) so we return immediately; the event is handled
            // on the next event-loop pass, exactly like a real focus change.
            QApplication::postEvent(w, new QFocusEvent(QEvent::FocusIn, Qt::OtherFocusReason));
            return;
        }
    }
}

// Select the bundled Turbo Vision theme (src/editor/tvtheme/tvtheme.qrc,
// resource :/crttv/themes/crt_tv.theme) so editor TEXT is yellow on blue with
// no black current-line band. Registered once into the shared KSS repository.
void EditorTabManager::applyTvTheme(KTextEditor::View* view)
{
    static const bool registered = [] {
        // Static-library trap: qrc_tvtheme.cpp.o lives in libcrt-editor.a but no
        // symbol references it, so the linker drops it and the resource never
        // auto-registers — force it in (resource name = qrc base name "tvtheme").
        Q_INIT_RESOURCE(tvtheme);
        const auto& repo = KTextEditor::Editor::instance()->repository();
        const_cast<KSyntaxHighlighting::Repository&>(repo)
            .addCustomSearchPath(QStringLiteral(":/crttv"));
        return true;
    }();
    Q_UNUSED(registered);
    // Verified: the view's "theme" config key stores the theme NAME as a
    // QString (configValue("theme").typeName() == "QString", e.g. "Breeze
    // Light"). Passing a KSyntaxHighlighting::Theme variant is silently ignored.
    if (view->theme().name() == QLatin1String("CRTKTV"))
        return; // already active
    view->setConfigValue(QStringLiteral("theme"), QStringLiteral("CRTKTV"));
    if (view->theme().name() != QLatin1String("CRTKTV"))
        qWarning("CRTKTV theme not applied (readback=%s)",
                 qPrintable(view->theme().name()));
}

// Two views of one document (the split's pane 0 + pane 1) are BOTH children of
// this manager's stacks. Qt's default deleteChildren() destroys the stacks in
// undefined order; if the primary view dies first it can drop the document's
// last reference, and the secondary view's destructor then touches the freed
// Document -> SIGSEGV in ~DocumentPrivate. So tear the secondary views down
// SYNCHRONOUSLY first (deleteLater never runs without an event loop), then let
// the primary stack/doc go.
EditorTabManager::~EditorTabManager()
{
    destroySecondaryViews();
    if (m_stack1) {
        m_stack1->setParent(nullptr);   // not a manager child; no double-delete
        delete m_stack1;
        m_stack1 = nullptr;
    }
}

// Synchronously remove + delete every secondary (pane 1) view. No-ops when the
// hash is already clear.
//
// The shell can DESTROY m_stack1 underneath us: setSplitEditorWidget() reparents
// m_stack1 into pane 1's body layout, and ShellWindow::clearSplitEditorWidget()
// (the pane-1 ■ box) tears pane 1 down WITHOUT calling tabs.closeSplit() - so
// m_stack1 may already be a dangling raw pointer when this runs (the live
// "Open Document in New View segfaults after closing a split" report). A QPointer
// lets us detect that death: it self-clears when the stack is destroyed. Without
// it we call removeWidget() on freed memory (SIGSEGV at EditorTabManager.cpp:322,
// proven by tst_splitview::reopening_the_split_after_the_close_box_yields_a_live_view).
void EditorTabManager::destroySecondaryViews()
{
    const auto views = std::as_const(m_secondaryViews);
    m_secondaryViews.clear();   // clear FIRST: a view dtor may re-enter
    QStackedWidget* stack = m_stack1.data();   // null if the shell already killed it
    for (const QPointer<KTextEditor::View>& pv : views) {
        KTextEditor::View* v = pv;   // null if already destroyed with pane 1
        if (!v)
            continue;
        if (stack)
            stack->removeWidget(v);
        delete v;               // synchronous: safe here, no event loop needed
    }
    if (!m_stack1)              // the shell destroyed the stack with pane 1
        m_stack1 = nullptr;     // (QPointer already nulled; keep the raw ptr honest)
    m_splitDoc = nullptr;
}

KTextEditor::Document* EditorTabManager::createDoc()
{
    auto* editor = KTextEditor::Editor::instance();
    if (!editor)
        return nullptr;
    KTextEditor::Document* doc = editor->createDocument(this);
    watchHistory(doc);
    return doc;
}

KTextEditor::Document* EditorTabManager::activeDocument() const
{
    // Same pivot rule as activeView(): with pane 1 focused the user is looking
    // at the SPLIT document, even though pane 0's tab index still points at a
    // different file. Menu actions and count/stat readers go through here, so
    // reading m_docs.at(m_active) would show pane 0's document while the caret
    // is in pane 1 (Xadium 2026-11-23: stats/cells reading the wrong doc).
    if (m_activePane == 1 && m_splitDoc)
        return m_splitDoc;
    if (m_active < 0 || m_active >= m_docs.size())
        return nullptr;
    return m_docs.at(m_active);
}

KTextEditor::View* EditorTabManager::activeView() const
{
    if (m_active < 0 || m_active >= m_views.size())
        return nullptr;
    // The active PANE's view: pane 1 when split+focused, else pane 0. This is
    // the single pivot every feature reads, so a focused pane drives minimap,
    // status, folding, Find, etc. with no parallel path.
    //
    // Pane 1 is resolved through the SPLIT DOCUMENT, not through pane 0's
    // active tab (Xadium 2026-11-23). Pane 0 and pane 1 can show DIFFERENT
    // documents ("File > Open as Split"), and looking the secondary view up
    // under m_docs.at(m_active) found nothing -> activeView() returned null ->
    // activeViewChanged(nullptr) cleared every status indicator to "-" / 0.
    if (m_activePane == 1) {
        if (m_splitDoc) {
            if (KTextEditor::View* v = m_secondaryViews.value(m_splitDoc, nullptr))
                return v;
        }
        // The split document is gone (or its view died with pane 1): fall back
        // to pane 0's live view rather than handing every consumer a null.
        return m_active >= 0 && m_active < m_views.size() ? m_views.at(m_active)
                                                          : nullptr;
    }
    return m_active >= 0 && m_active < m_views.size() ? m_views.at(m_active)
                                                      : nullptr;
}

// --- Split view (2026-11-21) ----------------------------------------------

QStackedWidget* EditorTabManager::secondaryContainerWidget()
{
    if (!m_stack1) {
        m_stack1 = new QStackedWidget;
        m_stack1->setObjectName(QStringLiteral("editorStack2"));
    }
    return m_stack1;
}

KTextEditor::View* EditorTabManager::viewForPane(int pane) const
{
    if (pane == 0)
        return m_active >= 0 && m_active < m_views.size()
                   ? m_views.at(m_active) : nullptr;
    if (pane == 1)
        return m_splitDoc ? m_secondaryViews.value(m_splitDoc, nullptr) : nullptr;
    return nullptr;
}

KTextEditor::Document* EditorTabManager::documentForPane(int pane) const
{
    KTextEditor::View* v = viewForPane(pane);
    return v ? v->document() : nullptr;
}

// True when at least one pane-1 view is still alive. m_secondaryViews keeps
// QPointer values (a view destroyed with pane 1's teardown self-nulls), so
// isEmpty()/contains() can be stale - a hash of dead keys still has entries.
bool EditorTabManager::anyLiveSecondary() const
{
    for (auto it = m_secondaryViews.constBegin(); it != m_secondaryViews.constEnd(); ++it)
        if (it.value())
            return true;
    return false;
}

bool EditorTabManager::splitOpen() const
{
    return anyLiveSecondary();
}

// Run the per-view setup every pane-0/pane-1 view needs (style, key routing,
// block tracking, default-hidden markers). Shared so a secondary view is set up
// exactly like a primary one.
void EditorTabManager::finishView(KTextEditor::View* view)
{
    applyTvStyle(view);
    installKeyRouting(view);
    attachBlockTracking(view);
    setBlockMarkersVisible(false);
    attachSwatchProvider(view);
}

// Colour swatches (2026-11-23): a provider paints a box after every #rgb /
// #rrggbb token and reports clicks. Parented to the MANAGER, not the view: the
// view's own teardown must never leave a half-destroyed provider registered,
// and a view destroyed by the shell's pane teardown still drops its provider
// through the destroyed() link below.
void EditorTabManager::attachSwatchProvider(KTextEditor::View* view)
{
    if (!view || m_swatchProviders.contains(view))
        return;
    auto* provider = new HexSwatchProvider(view, this);
    m_swatchProviders.insert(view, provider);
    view->registerInlineNoteProvider(provider);
    // The view can be destroyed by pane teardown before the click is handled,
    // so the re-emit carries a QPointer and never a dangling view.
    connect(provider, &HexSwatchProvider::swatchClicked, this,
            [this, guard = QPointer<KTextEditor::View>(view)](
                const HexColorToken& token, int line, const QRect& rect) {
                emit swatchClicked(guard.data(), token, line, rect);
            });
    connect(view, &QObject::destroyed, this, [this, view]() {
        if (HexSwatchProvider* p = m_swatchProviders.take(view))
            p->deleteLater();
    });
}

HexSwatchProvider* EditorTabManager::swatchProvider(KTextEditor::View* view) const
{
    return m_swatchProviders.value(view, nullptr);
}


// The secondary view for `doc`, created on first use and added to pane 1's
// stack. Returns nullptr if the doc already has one.
KTextEditor::View* EditorTabManager::ensureSecondaryView(KTextEditor::Document* doc)
{
    if (!doc)
        return nullptr;
    auto it = m_secondaryViews.constFind(doc);
    if (it != m_secondaryViews.end()) {
        if (KTextEditor::View* existing = it.value())
            return existing;      // still alive: reuse it
        // The shell destroyed this view with pane 1's teardown (clearSplitEditor-
        // Widget never calls closeSplit). Fall through and make a FRESH view -
        // reusing the dead pointer is the live "Open Document in New View" crash.
        m_secondaryViews.remove(doc);
    }
    KTextEditor::View* v = doc->createView(nullptr);
    finishView(v);
    secondaryContainerWidget()->addWidget(v);
    m_secondaryViews.insert(doc, v);
    return v;
}

// View > Open Document in New View: a second view of the ACTIVE document so the
// user can look at two places at once (shared text, independent caret/folds).
void EditorTabManager::openSplitWithActiveDocument()
{
    KTextEditor::Document* doc = activeDocument();
    if (!doc)
        return;
    openSplitWithDocument(doc);
}

// File > Open as Split Document (and the re-point path): show `doc` in the
// right pane. At most ONE split: if a split exists on a different document we
// re-point pane 1 rather than adding a pane. The doc stays open as a tab either
// way (closing a split never closes a document).
void EditorTabManager::openSplitWithDocument(KTextEditor::Document* doc)
{
    if (!doc || m_docs.isEmpty())
        return;
    // Make sure the doc is one of our tabs (it must be listed for tab/close
    // semantics); if it somehow isn't, do nothing (the caller opens it first).
    if (!m_docs.contains(doc))
        return;

    KTextEditor::View* v = ensureSecondaryView(doc);
    if (!v)
        return;

    // Record which document pane 1 is showing, and point its stack at that
    // document's secondary view. Pane 0's stack (m_container) is UNTOUCHED:
    // pane 0 keeps whatever tab it was showing.
    m_splitDoc = doc;
    secondaryContainerWidget()->setCurrentWidget(v);

    const bool wasOpen = anyLiveSecondary();
    emit splitChanged(wasOpen);
    emit activeViewChanged(activeView());
    trackActiveCursor();
    refreshFoldIndicator();
}

// Collapse the split: drop every secondary view, keep pane 0's document and
// view. Closing a split never closes a document (it stays a tab).
void EditorTabManager::closeSplit()
{
    if (m_secondaryViews.isEmpty())
        return;
    for (const QPointer<KTextEditor::View>& pv : std::as_const(m_secondaryViews)) {
        KTextEditor::View* v = pv;      // null if the shell already destroyed it
        if (!v)
            continue;
        if (m_stack1)
            m_stack1->removeWidget(v);
        v->deleteLater();
    }
    m_secondaryViews.clear();
    m_splitDoc = nullptr;
    if (m_activePane == 1)
        m_activePane = 0;   // focus returns to pane 0
    emit splitChanged(false);
    emit activeViewChanged(activeView());
    trackActiveCursor();
    refreshFoldIndicator();
}

// Task 6's active-pane pivot: focus moves to `pane`, re-pointing activeView().
void EditorTabManager::setActivePane(int pane)
{
    const int p = (pane == 1 && splitOpen()) ? 1 : 0;
    if (p == m_activePane)
        return;
    m_activePane = p;
    emit activeViewChanged(activeView());
    trackActiveCursor();
    refreshFoldIndicator();
}

// The bottom band shows the cursor of the tab the user is LOOKING at. Keeping
// the connection here (rather than in the composition root) means every path
// that changes the active view - new tab, opened file, tab switch, closed tab -
// re-binds, and none of them can forget to.
void EditorTabManager::trackActiveCursor()
{
    trackPaneCursors();
    QObject::disconnect(m_cursorConn);
    KTextEditor::View* v = activeView();
    // Coloured Brackets follow the active view (2026-11-20). Every tab-change
    // path funnels through here, so re-pointing here keeps the colours on the
    // buffer the user is actually looking at.
    if (m_brackets) {
        m_brackets->setView(v);
        m_brackets->setEnabled(m_bracketsOn);
    }
    if (!v)
        return;
    // A tab that just became active behaves like one the user clicked: it takes
    // focus. That is also what enables Kate's edit actions (they are switched
    // on in the input mode's gotFocus), without which arrows and deletion stay
    // dead on every tab but the first.
    v->setFocus(Qt::OtherFocusReason);
    m_cursorConn = QObject::connect(
        v, &KTextEditor::View::cursorPositionChanged, this,
        [this](KTextEditor::View*, const KTextEditor::Cursor& c) {
            emit activeCursorPositionChanged(c.line(), c.column());
        });
    const KTextEditor::Cursor c = v->cursorPosition();
    emit activeCursorPositionChanged(c.line(), c.column());
    // The active view changed - let listeners (the status-line indicators)
    // re-point at the new view/document so none of them go stale (the L:C
    // band bug class). Every tab-change path funnels through here.
    emit activeViewChanged(v);
}

// One caret listener per pane. Pane 0's view is the ACTIVE TAB's primary view;
// pane 1's is the split document's secondary view (null while unsplit). Called
// from trackActiveCursor(), which every tab/split/pane change funnels through,
// so a re-point can never be forgotten.
void EditorTabManager::trackPaneCursors()
{
    for (int pane = 0; pane < 2; ++pane) {
        QObject::disconnect(m_paneCursorConns[pane]);
        m_paneCursorConns[pane] = QMetaObject::Connection();
        KTextEditor::View* v = viewForPane(pane);
        if (!v)
            continue;
        m_paneCursorConns[pane] = QObject::connect(
            v, &KTextEditor::View::cursorPositionChanged, this,
            [this, pane](KTextEditor::View*, const KTextEditor::Cursor& c) {
                emit paneCursorPositionChanged(pane, c.line(), c.column());
            });
        const KTextEditor::Cursor c = v->cursorPosition();
        emit paneCursorPositionChanged(pane, c.line(), c.column());
    }
}

bool EditorTabManager::triggerActiveViewShortcut(const QKeySequence& seq)
{
    KTextEditor::View* v = activeView();
    if (!v)
        return false;
    KActionCollection* ac = v->actionCollection();
    if (!ac)
        return false;
    const auto actions = ac->actions();
    for (QAction* a : actions) {
        if (!a || !a->shortcuts().contains(seq))
            continue;
        // Kate leaves its edit actions DISABLED until focusInEvent fires
        // (slotGotFocus -> activateEditActions). On this unmapped
        // WA_DontShowOnScreen shell Qt never delivers FocusIn (no active
        // window), so they would stay off forever - and QAction::trigger()
        // refuses to emit triggered for a disabled action (measured: the caret
        // did not move until we enabled it first). The app hands the shell real
        // focus at startup, so these actions are legitimately meant to be on;
        // this dispatcher just corrects Kate's focus-fence so a pressed key can
        // always do its job. Enable, fire, and LEAVE it on: re-running Kate's
        // activateEditActions is idempotent, so focusIn (wherever it fires)
        // won't fight us.
        if (!a->isEnabled())
            a->setEnabled(true);
        a->trigger();
        return true;
    }
    return false;
}

// User bug: Ctrl+Z does nothing while the QWidget editor has focus. The QML
// Keys handler (which reaches ContentBridge::forwardKeyEvent, and through it
// the menu's undo action) only runs while the QML item owns focus; once a click
// lands in the embedded view, the keystroke goes to KTextEditor, which in this
// build has no undo API and silently drops the chord. Route Ctrl+Z / Ctrl+Y
// from the focused view straight to our own undo/redo.
namespace {
class UndoKeyRouter : public QObject
{
public:
    UndoKeyRouter(KTextEditor::View* view, EditorTabManager* mgr)
        : QObject(view), m_mgr(mgr)
    {
        view->installEventFilter(this);
    }
    bool eventFilter(QObject* watched, QEvent* ev) override
    {
        if (ev->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(ev);
            if (ke->modifiers() == Qt::ControlModifier) {
                if (ke->key() == Qt::Key_Z) {
                    m_mgr->undo();
                    return true;
                }
                if (ke->key() == Qt::Key_Y) {
                    m_mgr->redo();
                    return true;
                }
            }
        }
        return QObject::eventFilter(watched, ev);
    }

private:
    EditorTabManager* m_mgr;
};

// Clicking a gutter fold marker (the ▶/▼ in the icon strip) folds or unfolds
// that line's block. KateIconBorder does not fold on click in this build (a
// 181-point sweep folded nothing), so we intercept the press ourselves: map the
// click to a line with View::coordinatesToCursor and drive the same
// collapse/expand the block menu uses. Installed on the icon-border widget.
class FoldClickRouter : public QObject
{
public:
    FoldClickRouter(KTextEditor::View* view, EditorTabManager* mgr)
        : QObject(view), m_view(view), m_mgr(mgr)
    {
        // Both the fold strip (KateIconBorder) and the text area
        // (KateViewInternal) are children of the view, created with it.
        for (QWidget* w : view->findChildren<QWidget*>()) {
            const QByteArray cls = w->metaObject()->className();
            if (cls.contains("KateIconBorder")) {
                w->installEventFilter(this);
                m_border = w;
            } else if (cls.contains("KateViewInternal")) {
                w->installEventFilter(this);
                m_text = w;
            }
        }
    }

    bool eventFilter(QObject* watched, QEvent* ev) override
    {
        if (ev->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(ev);
            if (me->button() == Qt::LeftButton) {
                // A click back in the text area clears the click-highlight.
                if (watched == m_text) {
                    m_mgr->clearClickedBlockHighlight();
                    return false;   // let the click place the caret as usual
                }
                // The press arrives in icon-border-local coords. Convert to
                // view coords, then to a document line.
                auto* border = qobject_cast<QWidget*>(watched);
                if (border) {
                    const QPoint viewPt = border->mapTo(m_view, me->pos());
                    // The icon strip lives in the gutter, LEFT of the text
                    // area. coordinatesToCursor needs an x inside the text
                    // area to resolve the line correctly (a gutter x resolves
                    // to line 0). Keep the press's y - that is the line - and
                    // substitute a text-area x.
                    const int textX = m_view->textAreaRect().left() + 5;
                    const KTextEditor::Cursor c =
                        m_view->coordinatesToCursor(QPoint(textX, viewPt.y()));
                    int line = c.line();
                    // coordinatesToCursor can return an invalid line for a
                    // beat after a fold, while the view is still relaying out
                    // (measured: it recovers on the next turn). Fall back to
                    // an arithmetic y -> line so a quick second click still
                    // lands on the right line.
                    if (line < 0)
                        line = lineAtViewY(viewPt.y());
                    // Only a marked line (a foldable opener) reacts. Kate's
                    // mark at that line means the glyph is there.
                    if (line >= 0 && m_view->document()
                        && m_view->document()->marks().contains(line)) {
                        const bool folded = !m_view->isLineVisible(line + 1);
                        if (folded)
                            m_mgr->expandBlockAtLine(line);
                        else
                            m_mgr->collapseBlockAtLine(line);
                        // Show which block was just folded/unfolded: green band,
                        // yellow text. Cleared by the next text-area click.
                        m_mgr->highlightClickedBlock(line);
                        return true;   // consumed: don't let Kate move the caret
                    }
                }
            }
        }
        return QObject::eventFilter(watched, ev);
    }

private:
    // Map a view-space y to a document line without coordinatesToCursor: the
    // first displayed line plus the row the y falls in, using a line height
    // measured from two visible lines' coordinates. Returns -1 if unavailable.
    int lineAtViewY(int y) const
    {
        const int first = m_view->firstDisplayedLine();
        const KTextEditor::Cursor a(first, 0);
        const QPoint pa = m_view->cursorToCoordinate(a);
        if (pa.y() < 0)
            return -1;
        // Line height from the next visible line, if there is one.
        int lineH = 16;
        for (int n = first + 1; n <= m_view->lastDisplayedLine(); ++n) {
            const QPoint pn = m_view->cursorToCoordinate(KTextEditor::Cursor(n, 0));
            if (pn.y() >= 0 && pn.y() > pa.y()) {
                lineH = pn.y() - pa.y();
                break;
            }
        }
        if (lineH <= 0)
            return -1;
        const int row = (y - pa.y()) / lineH;
        int line = first + row;
        if (line < 0 || line > m_view->lastDisplayedLine())
            return -1;
        return line;
    }

    KTextEditor::View* m_view;
    EditorTabManager* m_mgr;
    QWidget* m_border = nullptr;
    QWidget* m_text = nullptr;
};
} // namespace

void EditorTabManager::installKeyRouting(KTextEditor::View* view)
{
    if (!view)
        return;
    new UndoKeyRouter(view, this);
    new FoldClickRouter(view, this);
}

void EditorTabManager::newDocument()
{
    KTextEditor::Document* doc = createDoc();
    if (!doc)
        return;
    KTextEditor::View* view = doc->createView(nullptr);
    m_docs.append(doc);
    m_views.append(view);
    m_container->addWidget(view);       // host in the stacked container
    // The stacked container is what the shell is given as "the editor", so it
    // must forward focus to the live view: without a proxy a click inside the
    // view walks up to a container that cannot take focus, and keys die.
    m_container->setFocusProxy(view);
    m_active = m_docs.size() - 1;
    m_container->setCurrentIndex(m_active);
    syncTabs();
    applyTvStyle(view);
    installKeyRouting(view);
    attachBlockTracking(view);
    attachSwatchProvider(view);
    // Block markers are hidden by default (2026-11-20). Applied HERE, at view
    // creation, so a newly opened file comes up clean as well as the restored
    // ones - and so the write never races a fold.
    setBlockMarkersVisible(false);
    trackActiveCursor();
}

// Shared tab creation for File > Open and File > Open as Split. `activate`
// decides whether pane 0 moves onto the new tab:
//   true  - plain open: the new file becomes the visible tab (today's behaviour)
//   false - open-as-split: the tab is added but pane 0 keeps the document the
//           user was editing, so the split shows the file in the RIGHT pane only.
// The focus proxy is only re-pointed when activating: a non-active tab must not
// take pane 0's key path (see newDocument()).
KTextEditor::Document* EditorTabManager::addTabForUrl(const QUrl& url, bool activate)
{
    KTextEditor::Document* doc = createDoc();
    if (!doc)
        return nullptr;
    doc->openUrl(url);
    KTextEditor::View* view = doc->createView(nullptr);
    m_docs.append(doc);
    m_views.append(view);
    m_container->addWidget(view);
    if (activate) {
        m_container->setFocusProxy(view);   // see newDocument(): focus path for keys
        m_active = m_docs.size() - 1;
        m_container->setCurrentIndex(m_active);
    }
    syncTabs();
    applyTvStyle(view);
    installKeyRouting(view);
    attachBlockTracking(view);
    // Block markers are hidden by default (2026-11-20). Applied HERE, at view
    // creation, so a newly opened file comes up clean as well as the restored
    // ones - and so the write never races a fold.
    setBlockMarkersVisible(false);
    trackActiveCursor();
    armWatch(doc);
    return doc;
}

void EditorTabManager::openFile(const QUrl& url)
{
    addTabForUrl(url, /*activate=*/true);
}

// File > Open as Split Document (round 8 live fix): the picked file becomes a
// tab and shows in the right pane, while pane 0 stays on the document the user
// was editing. The production order is open-then-split, and plain openFile()
// activates the new tab in pane 0 - which made the split mirror the same file
// into both panes.
void EditorTabManager::openFileAsSplit(const QUrl& url)
{
    KTextEditor::Document* doc = addTabForUrl(url, /*activate=*/false);
    if (!doc)
        return;
    openSplitWithDocument(doc);
}

void EditorTabManager::setActiveIndex(int index)
{
    if (index < 0 || index >= m_docs.size() || index == m_active)
        return;
    m_active = index;
    m_container->setCurrentIndex(index);
    syncTabs();
    trackActiveCursor();
    // Marks live on the Document: switching tabs must show the newly active
    // document's fold marks (and clear them if markers are off for it).
    refreshFoldIndicator();
}

// Task 21: the tab chip's green square closes that tab. Indices shift with the
// removal, so the active index is adjusted rather than left pointing at a
// different document.
void EditorTabManager::closeTab(int index)
{
    if (index < 0 || index >= m_docs.size())
        return;
    KTextEditor::Document* doc = m_docs.at(index);
    // Closing a tab collapses the split if that document was showing in the
    // right pane (the split never outlives its document; pane 0 keeps its own
    // document). No-ops when there is no split or a different doc is split.
    // A doc is "split" only while its pane-1 view is ALIVE (a dead QPointer key
    // must not trigger a teardown against an already-destroyed view).
    const auto it = m_secondaryViews.constFind(doc);
    if (it != m_secondaryViews.constEnd() && it.value())
        closeSplit();
    doc = m_docs.takeAt(index);
    KTextEditor::View* view = m_views.takeAt(index);
    disarmWatch(doc);
    if (view) {
        m_container->removeWidget(view);
        view->deleteLater();
    }
    if (doc) {
        // The history is keyed by document; drop it with the document or the
        // hash keeps dangling keys for the life of the session.
        m_history.remove(doc);
        m_historyIndex.remove(doc);
        doc->deleteLater();
    }

    if (m_docs.isEmpty()) {
        // Closing the LAST tab leaves a fresh untitled document: the strip and
        // the editor never go empty (this app has no multi-window story to
        // fall back on). newDocument() emits tabsChanged itself.
        newDocument();
        return;
    }

    if (index < m_active)
        --m_active;
    else if (m_active >= m_docs.size())
        m_active = m_docs.size() - 1;   // closed the active LAST tab
    m_container->setCurrentIndex(m_active);
    if (KTextEditor::View* v = m_views.value(m_active))
        m_container->setFocusProxy(v);   // the proxy may have been the closed view
    syncTabs();
    trackActiveCursor();
}

QUrl EditorTabManager::viewFileUrl(int index) const
{
    if (index < 0 || index >= m_views.size())
        return {};
    return m_views[index]->document()->url();
}

QPoint EditorTabManager::viewCursorPosition(int index) const
{
    if (index < 0 || index >= m_views.size())
        return {};
    const KTextEditor::Cursor c = m_views[index]->cursorPosition();
    return { c.column(), c.line() };
}

void EditorTabManager::setZoom(int delta)
{
    if (delta == 0) {
        applyZoom(0);   // reset handled via stored baseline
        return;
    }
    applyZoom(delta);
}

void EditorTabManager::setFontSize(int pixelSize)
{
    if (pixelSize < 4)
        pixelSize = 4;
    if (pixelSize > 96)
        pixelSize = 96;
    for (KTextEditor::View* v : std::as_const(m_views)) {
        if (!v)
            continue;
        QFont f = v->configValue(QStringLiteral("font")).value<QFont>();
        f.setPixelSize(pixelSize);
        v->setConfigValue(QStringLiteral("font"), f);
    }
}

void EditorTabManager::applyZoom(int delta)
{
    KTextEditor::View* v = activeView();
    if (!v)
        return;
    QFont f = v->configValue(QStringLiteral("font")).value<QFont>();
    // Zoom in PIXEL units when the font is pixel-based (setFontSize sets
    // pixel size; a pixel-set font reports pointSize() == -1). Zooming in
    // point units there would hit the <=0 fallback and silently replace the
    // configured pixel size with a ~10pt default. Otherwise zoom in points.
    const bool pixelBased = f.pixelSize() > 0;
    if (pixelBased) {
        int base = (m_basePixelSize > 0) ? m_basePixelSize : f.pixelSize();
        if (delta == 0) {
            f.setPixelSize(base);
        } else {
            if (m_basePixelSize < 0)
                m_basePixelSize = f.pixelSize();   // lock baseline on first zoom
            int next = f.pixelSize() + delta;
            if (next < 4)  next = 4;
            if (next > 96) next = 96;
            f.setPixelSize(next);
        }
        v->setConfigValue(QStringLiteral("font"), f);
        return;
    }
    if (f.pointSize() <= 0)
        f.setPointSize(10);
    if (delta == 0) {                       // reset to baseline
        if (m_basePointSize > 0)
            f.setPointSize(m_basePointSize);
    } else {
        if (m_basePointSize < 0)
            m_basePointSize = f.pointSize(); // lock baseline on first zoom
        int next = f.pointSize() + delta;
        if (next < 4)  next = 4;
        if (next > 96) next = 96;
        f.setPointSize(next);
    }
    v->setConfigValue(QStringLiteral("font"), f);
}

// Case/sort operate on the selection if one exists, else the whole document.
// Uses document->text()/setText() with a cursor range so undo works via setText.
void EditorTabManager::changeCase(CaseKind kind)
{
    KTextEditor::View* v = activeView();
    KTextEditor::Document* doc = activeDocument();
    if (!v || !doc)
        return;

    KTextEditor::Range range(0, 0, 0, 0);
    bool hasSel = v->selection() && v->selectionRanges().size() == 1;
    if (hasSel)
        range = v->selectionRanges().first();
    else {
        const int lastLine = doc->lines() - 1;
        range = KTextEditor::Range(0, 0, lastLine, doc->lineLength(lastLine));
    }

    QString text = doc->text(range);
    switch (kind) {
    case Upper: text = text.toUpper(); break;
    case Lower: text = text.toLower(); break;
    case Title: {
        // Title-case each whitespace-delimited word's first letter.
        bool atWordStart = true;
        for (int i = 0; i < text.size(); ++i) {
            if (text[i].isSpace()) { atWordStart = true; continue; }
            if (atWordStart) { text[i] = text[i].toUpper(); atWordStart = false; }
            else             { text[i] = text[i].toLower(); }
        }
        break;
    }
    }
    doc->replaceText(range, text);
}

void EditorTabManager::copy()
{
    KTextEditor::View* v = activeView();
    KTextEditor::Document* doc = activeDocument();
    if (!v || !doc)
        return;
    const QList<KTextEditor::Range> ranges = v->selectionRanges();
    if (ranges.isEmpty())
        return;                       // nothing selected: leave the clipboard
    QStringList parts;
    for (const KTextEditor::Range& r : ranges)
        parts << doc->text(r);
    QGuiApplication::clipboard()->setText(parts.join(QLatin1Char('\n')));
}

void EditorTabManager::cut()
{
    KTextEditor::View* v = activeView();
    KTextEditor::Document* doc = activeDocument();
    if (!v || !doc)
        return;
    const QList<KTextEditor::Range> ranges = v->selectionRanges();
    if (ranges.isEmpty())
        return;
    copy();
    // Remove bottom-up so earlier ranges keep valid coordinates.
    QList<KTextEditor::Range> sorted = ranges;
    std::sort(sorted.begin(), sorted.end(),
              [](const KTextEditor::Range& a, const KTextEditor::Range& b) {
                  return a.start() > b.start();
              });
    for (const KTextEditor::Range& r : sorted)
        doc->removeText(r);
}

void EditorTabManager::paste()
{
    KTextEditor::View* v = activeView();
    KTextEditor::Document* doc = activeDocument();
    if (!v || !doc)
        return;
    const QString text = QGuiApplication::clipboard()->text();
    if (text.isEmpty())
        return;
    const KTextEditor::Range sel = v->selectionRange();
    if (sel.isValid() && !sel.isEmpty()) {
        // Paste over a selection replaces it (standard behaviour).
        doc->replaceText(sel, text);
        v->setCursorPosition(sel.start());
    } else {
        v->insertText(text);
    }
}

// ---------------------------------------------------------------------------
// Undo / redo
//
// This KTextEditor build exposes no undo API (no UndoInterface, no action
// hooks), so each document keeps its own snapshot stack here: the whole text
// plus the caret, pushed on every textChanged. Undo walks back one entry and
// re-applies it; redo walks forward. A new edit truncates the forward branch,
// which is what every editor does.
// ---------------------------------------------------------------------------

QList<EditorTabManager::HistoryEntry>&
EditorTabManager::historyFor(KTextEditor::Document* doc)
{
    return m_history[doc];
}

int& EditorTabManager::historyIndexFor(KTextEditor::Document* doc)
{
    return m_historyIndex[doc];
}

void EditorTabManager::watchHistory(KTextEditor::Document* doc)
{
    if (!doc)
        return;
    // Seed the baseline so the first edit has something to undo back to.
    QList<HistoryEntry>& hist = historyFor(doc);
    if (hist.isEmpty()) {
        hist.append({ doc->text(), KTextEditor::Cursor(0, 0) });
        historyIndexFor(doc) = 0;
    }
    connect(doc, &KTextEditor::Document::textChanged, this,
            [this, doc](KTextEditor::Document*) { pushHistory(doc); });
    // Task 25: an untitled tab names itself after the typed text, so every edit
    // to a document without a file must refresh the titles. Saved documents are
    // skipped - their title is the filename and cannot change by typing.
    connect(doc, &KTextEditor::Document::textChanged, this,
            [this, doc](KTextEditor::Document*) {
                if (doc->url().isEmpty())
                    syncTabs();
            });
    // Task 25: saving gives the document a real filename, which replaces the
    // typed-text name immediately - without this the tab kept showing the first
    // ten typed characters until some unrelated event re-emitted the titles.
    connect(doc, &KTextEditor::Document::documentUrlChanged, this,
            [this](KTextEditor::Document*) { syncTabs(); });
}

void EditorTabManager::pushHistory(KTextEditor::Document* doc)
{
    if (!doc || m_applyingHistory)
        return;

    QList<HistoryEntry>& hist = historyFor(doc);
    int& index = historyIndexFor(doc);
    if (hist.isEmpty()) {
        hist.append({ QString(), KTextEditor::Cursor(0, 0) });
        index = 0;
    }

    const QString text = doc->text();
    if (hist.at(index).text == text)
        return;                     // textChanged without a real change

    KTextEditor::Cursor caret(0, 0);
    if (KTextEditor::View* v = activeView()) {
        if (v->document() == doc)
            caret = v->cursorPosition();
    }

    // Drop the redo branch: the user edited after undoing.
    while (hist.size() > index + 1)
        hist.removeLast();
    hist.append({ text, caret });

    // Cap the depth so a long session cannot grow without bound.
    while (hist.size() > kMaxHistory)
        hist.removeFirst();
    index = hist.size() - 1;

    notifyHistory();
}

void EditorTabManager::applyHistoryStep(KTextEditor::Document* doc, int index)
{
    QList<HistoryEntry>& hist = historyFor(doc);
    if (index < 0 || index >= hist.size())
        return;

    const HistoryEntry entry = hist.at(index);
    m_applyingHistory = true;
    doc->setText(entry.text);
    if (KTextEditor::View* v = activeView()) {
        if (v->document() == doc) {
            // Clamp: the restored text may be shorter than the old caret.
            KTextEditor::Cursor c = entry.cursor;
            const KTextEditor::Range full = doc->documentRange();
            if (c.line() > full.end().line())
                c = full.end();
            else if (c.line() == full.end().line() && c.column() > full.end().column())
                c = full.end();
            v->setCursorPosition(c);
        }
    }
    m_applyingHistory = false;
    historyIndexFor(doc) = index;
    notifyHistory();
}

void EditorTabManager::undo()
{
    KTextEditor::Document* doc = activeDocument();
    if (!doc)
        return;
    const int index = historyIndexFor(doc);
    if (index <= 0)
        return;
    applyHistoryStep(doc, index - 1);
}

void EditorTabManager::redo()
{
    KTextEditor::Document* doc = activeDocument();
    if (!doc)
        return;
    const int index = historyIndexFor(doc);
    if (index + 1 >= historyFor(doc).size())
        return;
    applyHistoryStep(doc, index + 1);
}

bool EditorTabManager::canUndo() const
{
    KTextEditor::Document* doc = activeDocument();
    if (!doc)
        return false;
    return m_historyIndex.value(doc, 0) > 0;
}

bool EditorTabManager::canRedo() const
{
    KTextEditor::Document* doc = activeDocument();
    if (!doc)
        return false;
    return m_historyIndex.value(doc, 0) + 1 < m_history.value(doc).size();
}

void EditorTabManager::notifyHistory()
{
    emit historyChanged(canUndo(), canRedo());
}

void EditorTabManager::sortLines()
{
    KTextEditor::Document* doc = activeDocument();
    if (!doc)
        return;
    QStringList lines = doc->text().split(QLatin1Char('\n'));
    const bool trailing = lines.size() > 1 && lines.last().isEmpty();
    if (trailing)
        lines.removeLast();
    std::sort(lines.begin(), lines.end(),
              [](const QString& a, const QString& b) {
                  return a.localeAwareCompare(b) < 0;
              });
    QString out = lines.join(QLatin1Char('\n'));
    if (trailing)
        out += QLatin1Char('\n');
    doc->setText(out);
}

void EditorTabManager::selectAllActive()
{
    KTextEditor::View* v = activeView();
    KTextEditor::Document* doc = activeDocument();
    if (!v || !doc)
        return;
    // Whole document, not activeRange(): Select All must replace a smaller
    // selection rather than defer to it. An empty document yields the empty
    // range (0,0,0,0), which setSelection leaves unselected - harmless.
    const int lastLine = qMax(0, doc->lines() - 1);
    v->setSelection(KTextEditor::Range(0, 0, lastLine, doc->lineLength(lastLine)));
}

void EditorTabManager::deselectActive()
{
    if (KTextEditor::View* v = activeView())
        v->removeSelection();
}

QString EditorTabManager::affixLines(const QString& text, const QString& affix, bool atEnd)
{
    if (affix.isEmpty())
        return text;
    QStringList lines = text.split(QLatin1Char('\n'));
    // The segment after a trailing newline is not a line: it gets no affix.
    const int count = text.endsWith(QLatin1Char('\n')) ? lines.size() - 1
                                                       : lines.size();
    for (int i = 0; i < count; ++i) {
        QString& line = lines[i];
        // A CRLF's \r belongs to the terminator: the affix goes before it.
        QString cr;
        if (line.endsWith(QLatin1Char('\r'))) {
            cr = QLatin1Char('\r');
            line.chop(1);
        }
        line = atEnd ? line + affix + cr : affix + line + cr;
    }
    return lines.join(QLatin1Char('\n'));
}

QString EditorTabManager::stripEdgeText(const QString& text, const QString& needle,
                                        bool atEnd)
{
    if (needle.isEmpty())
        return text;
    QStringList lines = text.split(QLatin1Char('\n'));
    // Same phantom-segment rule as affixLines: a trailing \n's empty tail is
    // not a line, so it never gets stripped.
    const int count = text.endsWith(QLatin1Char('\n')) ? lines.size() - 1
                                                       : lines.size();
    for (int i = 0; i < count; ++i) {
        QString& line = lines[i];
        // A CRLF's \r belongs to the terminator: strip before it.
        QString cr;
        if (line.endsWith(QLatin1Char('\r'))) {
            cr = QLatin1Char('\r');
            line.chop(1);
        }
        if (atEnd) {
            if (line.endsWith(needle))
                line.chop(needle.size());
        } else if (line.startsWith(needle)) {
            line.remove(0, needle.size());
        }
        line += cr;
    }
    return lines.join(QLatin1Char('\n'));
}

QString EditorTabManager::removeLineBreaksText(const QString& text)
{
    if (text.isEmpty())
        return text;
    static const QRegularExpression breaks(QStringLiteral("(?:\\r\\n|\\r|\\n)+"));
    QString out = text;
    out.replace(breaks, QStringLiteral(" "));
    // A break at either edge must not leave the space it turned into behind.
    const QChar first = text.at(0);
    if ((first == QLatin1Char('\n') || first == QLatin1Char('\r'))
        && out.startsWith(QLatin1Char(' ')))
        out.remove(0, 1);
    const QChar last = text.at(text.size() - 1);
    if ((last == QLatin1Char('\n') || last == QLatin1Char('\r'))
        && out.endsWith(QLatin1Char(' ')))
        out.chop(1);
    return out;
}

bool EditorTabManager::wordWrap() const
{
    KTextEditor::View* v = activeView();
    if (!v)
        return false;
    return v->configValue(QStringLiteral("dynamic-word-wrap")).toBool();
}

void EditorTabManager::setWordWrap(bool on)
{
    KTextEditor::View* v = activeView();
    if (!v)
        return;
    v->setConfigValue(QStringLiteral("dynamic-word-wrap"), on);
    // setConfigValue updates the setting but does not relayout the view - the
    // "font" path in this file learned the same lesson.
    v->update();
}

namespace {
QAction* viewAction(KTextEditor::View* v, const char* name)
{
    if (!v)
        return nullptr;
    const QString n = QString::fromLatin1(name);
    // Ask the view's action collection FIRST. Measured: the folding actions
    // (folding_toplevel, folding_expandtoplevel, folding_toggle_current) live
    // ONLY there - findChild<QAction*> returns null for them, so a findChild-only
    // lookup silently turned every folding command into a no-op. The collection
    // lookup also works for the view toggles, and the child search stays as a
    // fallback for anything parented to the view rather than the collection.
    if (KActionCollection* ac = v->actionCollection()) {
        if (QAction* a = ac->action(n))
            return a;
    }
    return v->findChild<QAction*>(n);
}

// Kate's whitespace actions toggle on trigger(): the checked flag flips on
// every trigger (measured 0 -> 1 -> 0). Drive until the state matches, so a
// request for the state already in force costs nothing and a real one flips
// exactly once. setChecked is only a fallback for a build whose slot ignores
// the click - it moves the flag without going through Kate's rendering path.
void setActionOn(QAction* a, bool on)
{
    if (!a)
        return;
    for (int guard = 0; guard < 3 && a->isChecked() != on; ++guard)
        a->trigger();
    if (a->isChecked() != on)
        a->setChecked(on);
}
} // namespace

void EditorTabManager::setNonprintables(bool on)
{
    KTextEditor::View* v = activeView();
    setActionOn(viewAction(v, "view_show_whitespaces"), on);
    setActionOn(viewAction(v, "view_non_printable_spaces"), on);
}

bool EditorTabManager::nonprintablesVisible() const
{
    QAction* a = viewAction(activeView(), "view_show_whitespaces");
    return a && a->isChecked();
}

void EditorTabManager::setColouredBrackets(bool on)
{
    m_bracketsOn = on;
    if (m_brackets)
        m_brackets->setEnabled(on);
}

bool EditorTabManager::colouredBrackets() const
{
    return m_bracketsOn;
}

void EditorTabManager::collapseAll()
{
    // folding_toplevel is Kate's "Fold Toplevel Nodes": the outermost blocks
    // only. There is no API to walk the folding tree, so deeper levels are not
    // reachable from a command (probed: 10 visible lines -> 4).
    if (QAction* a = viewAction(activeView(), "folding_toplevel"))
        a->trigger();
}

void EditorTabManager::expandAll()
{
    if (QAction* a = viewAction(activeView(), "folding_expandtoplevel"))
        a->trigger();
}

void EditorTabManager::toggleCurrentNode()
{
    if (QAction* a = viewAction(activeView(), "folding_toggle_current"))
        a->trigger();
}

// ---- Block commands (2026-11-20) ------------------------------------------
//
// The right-click menu acts on the block whose OPENING line holds the caret.
// Kate has no public fold-region API, so BlockFinder computes the extent, and
// because Kate offers only a TOGGLE action, collapse/expand check the folded
// state first - otherwise a second "Collapse Block" would re-open the block.

BlockFinder::Block EditorTabManager::currentBlock() const
{
    BlockFinder::Block none;
    KTextEditor::View* v = activeView();
    if (!v || !v->document())
        return none;
    KTextEditor::Document* doc = v->document();

    QVector<QString> lines;
    lines.reserve(doc->lines());
    for (int i = 0; i < doc->lines(); ++i)
        lines.append(doc->line(i));

    return BlockFinder::findBlockStartingAt(lines, v->cursorPosition().line(),
                                            blockSkipProbe(doc));
}

// Same rule the colour passes use: text the highlighter owns (a string, a
// comment or a char literal) is not structure, so a '{' there opens nothing.
// Shared by currentBlock() and applyFoldMarks() so the two never disagree.
BlockFinder::SkipProbe EditorTabManager::blockSkipProbe(KTextEditor::Document* doc)
{
    return [doc](int line, int col) {
        const auto st = doc->defaultStyleAt(KTextEditor::Cursor(line, col));
        return st == KSyntaxHighlighting::Theme::TextStyle::Comment
               || st == KSyntaxHighlighting::Theme::TextStyle::String
               || st == KSyntaxHighlighting::Theme::TextStyle::Char;
    };
}

bool EditorTabManager::canActOnBlock() const
{
    return currentBlock().valid;
}

bool EditorTabManager::currentBlockIsFolded() const
{
    KTextEditor::View* v = activeView();
    const BlockFinder::Block b = currentBlock();
    if (!v || !b.valid || b.endLine <= b.startLine)
        return false;
    // A folded block hides the line after its opener.
    return !v->isLineVisible(b.startLine + 1);
}

void EditorTabManager::collapseBlock()
{
    if (!canActOnBlock() || currentBlockIsFolded())
        return;
    if (QAction* a = viewAction(activeView(), "folding_toggle_current"))
        a->trigger();
    refreshBlockActions();
    applyBlockHighlight();
    updateFoldGlyph();        // the opener's `▼` becomes a `▶` (glyph only)
}

void EditorTabManager::expandBlock()
{
    if (!canActOnBlock() || !currentBlockIsFolded())
        return;
    if (QAction* a = viewAction(activeView(), "folding_toggle_current"))
        a->trigger();
    refreshBlockActions();
    applyBlockHighlight();
    updateFoldGlyph();        // the opener's `▶` becomes a `▼` (glyph only)
}

// The gutter fold marker click path: act on the block that OPENS at `line`,
// not on the caret's current block. Move the caret there first (so the shared
// collapse/expand logic, which reads currentBlock(), sees the right line), then
// run it. A line with no foldable block is a no-op (currentBlock().invalid).
void EditorTabManager::collapseBlockAtLine(int line)
{
    KTextEditor::View* v = activeView();
    if (!v || line < 0)
        return;
    v->setCursorPosition(KTextEditor::Cursor(line, 0));
    collapseBlock();
}

void EditorTabManager::expandBlockAtLine(int line)
{
    KTextEditor::View* v = activeView();
    if (!v || line < 0)
        return;
    v->setCursorPosition(KTextEditor::Cursor(line, 0));
    expandBlock();
}

void EditorTabManager::clearBlockHighlightRange()
{
    delete m_blockHighlightRange;
    m_blockHighlightRange = nullptr;
}

void EditorTabManager::setBlockHighlight(bool on)
{
    if (m_blockHighlight == on) {
        if (on)
            applyBlockHighlight();      // still follow the caret
        return;
    }
    m_blockHighlight = on;
    if (m_ctxHighlightBlock)
        m_ctxHighlightBlock->setChecked(on);
    applyBlockHighlight();
}

void EditorTabManager::applyBlockHighlight()
{
    KTextEditor::View* v = activeView();
    if (!m_blockHighlight || !v || !v->document()) {
        clearBlockHighlightRange();
        return;
    }
    const BlockFinder::Block b = currentBlock();
    applyBlockRange(b);
}

// Paint `b` as one band over whole lines. Shared by the persistent Highlight
// Block toggle and the transient click-to-highlight so both use the same
// green-on-yellow palette.
void EditorTabManager::applyBlockRange(const BlockFinder::Block& b)
{
    clearBlockHighlightRange();
    KTextEditor::View* v = activeView();
    if (!b.valid || !v || !v->document())
        return;

    // Whole lines, so the band reads as one block rather than stopping mid-line.
    const int lastLine = qMin(b.endLine, v->document()->lines() - 1);
    const int lastLen = v->document()->line(lastLine).size();
    auto attr = KTextEditor::Attribute::Ptr(new KTextEditor::Attribute);
    // Dark DOS green behind bright DOS yellow. The user asked for a green block
    // band with yellow text; yellow on a DARK green stays legible, whereas
    // yellow on the brighter #00AA00 does not.
    attr->setBackground(QColor(QStringLiteral("#005500")));
    attr->setForeground(QColor(QStringLiteral("#ffff55")));
    auto* r = v->document()->newMovingRange(
        KTextEditor::Range(b.startLine, 0, lastLine, lastLen));
    r->setAttribute(attr);
    m_blockHighlightRange = r;
    v->repaint();
}

void EditorTabManager::highlightClickedBlock(int line)
{
    KTextEditor::View* v = activeView();
    if (!v || !v->document() || line < 0)
        return;
    QVector<QString> lines;
    lines.reserve(v->document()->lines());
    for (int i = 0; i < v->document()->lines(); ++i)
        lines.append(v->document()->line(i));
    const BlockFinder::Block b =
        BlockFinder::findBlockStartingAt(lines, line, blockSkipProbe(v->document()));
    applyBlockRange(b);
}

void EditorTabManager::clearClickedBlockHighlight()
{
    clearBlockHighlightRange();
}

void EditorTabManager::buildBlockActions()
{
    if (m_ctxCollapseBlock)
        return;
    m_ctxCollapseBlock = new QAction(tr("Collapse Block"), this);
    m_ctxCollapseBlock->setObjectName(QStringLiteral("actionCtxCollapseBlock"));
    connect(m_ctxCollapseBlock, &QAction::triggered, this, [this] { collapseBlock(); });

    m_ctxExpandBlock = new QAction(tr("Expand Block"), this);
    m_ctxExpandBlock->setObjectName(QStringLiteral("actionCtxExpandBlock"));
    connect(m_ctxExpandBlock, &QAction::triggered, this, [this] { expandBlock(); });

    m_ctxHighlightBlock = new QAction(tr("Highlight Block"), this);
    m_ctxHighlightBlock->setObjectName(QStringLiteral("actionCtxHighlightBlock"));
    m_ctxHighlightBlock->setCheckable(true);
    connect(m_ctxHighlightBlock, &QAction::toggled, this,
            [this](bool on) { setBlockHighlight(on); });
}

void EditorTabManager::refreshBlockActions()
{
    buildBlockActions();
    const bool onBlock = canActOnBlock();
    const bool folded = onBlock && currentBlockIsFolded();
    // Greyed out rather than hidden, so the menu keeps a stable shape - the same
    // convention the View menu uses for its folding commands.
    m_ctxCollapseBlock->setEnabled(onBlock && !folded);
    m_ctxExpandBlock->setEnabled(onBlock && folded);
    m_ctxHighlightBlock->setEnabled(onBlock);
}

QAction* EditorTabManager::contextBlockAction(const QString& name) const
{
    if (name == QLatin1String("actionCtxCollapseBlock"))
        return m_ctxCollapseBlock;
    if (name == QLatin1String("actionCtxExpandBlock"))
        return m_ctxExpandBlock;
    if (name == QLatin1String("actionCtxHighlightBlock"))
        return m_ctxHighlightBlock;
    return nullptr;
}

void EditorTabManager::refreshBlockMenuState()
{
    refreshBlockActions();
}

QAction* EditorTabManager::viewActionByName(const QString& name) const
{
    return viewAction(activeView(), name.toLatin1().constData());
}

void EditorTabManager::setBlockMarkersVisible(bool visible)
{
    // setActionOn drives the action with trigger() until the checked flag
    // matches, which is the only route that reaches Kate's rendering. A direct
    // setConfigValue("folding-bar", ...) is NOT used here: measured twice, it
    // segfaults when combined with a fold in the same turn.
    setActionOn(viewAction(activeView(), "view_folding_markers"), visible);
    // One switch, one meaning: the same flag also drives our `▶` gutter marks
    // and the icon strip that draws them (see refreshFoldIndicator).
    refreshFoldIndicator();
}

bool EditorTabManager::blockMarkersVisible() const
{
    QAction* a = viewAction(activeView(), "view_folding_markers");
    return a && a->isChecked();
}

QIcon EditorTabManager::foldIcon(bool folded) const
{
    // Built once and cached: a DOS-yellow triangle in the gutter, matching the
    // line numbers so it reads as gutter chrome rather than as text.
    //
    // Drawn as a QPainterPath triangle, NOT as a font glyph (`▶`/`▼`). Measured
    // defect (2026-11-21): the font-drawn `▶` antialiases its FLAT VERTICAL left
    // edge to ~alpha 72, which over the DOS-blue background composites to a dim
    // #5407AA bar immediately left of the solid triangle - the "whatever is to
    // the left of it" Xadium reported. Drawing the triangle geometrically with a
    // crisp vertical edge sitting ON a pixel boundary gives a fully opaque left
    // edge with a transparent neighbour (no sliver), and sidesteps font-atlas
    // resolution so the cached icon is always the triangle.
    static const QIcon rightPointing = [] {
        QPixmap pm(16, 16);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#ffff55")));
        // Right-pointing triangle: flat left edge at x=2 (a pixel boundary),
        // apex at x=13, centred vertically. Inset 2 keeps clear of the edge so
        // the antialiased tip does not spill either.
        QPainterPath path;
        path.moveTo(2, 3);
        path.lineTo(13, 8);
        path.lineTo(2, 13);
        path.closeSubpath();
        p.drawPath(path);
        return QIcon(pm);
    }();
    static const QIcon downPointing = [] {
        QPixmap pm(16, 16);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#ffff55")));
        // Down-pointing triangle: flat top edge at y=3, apex at y=13.
        QPainterPath path;
        path.moveTo(3, 3);
        path.lineTo(13, 3);
        path.lineTo(8, 13);
        path.closeSubpath();
        p.drawPath(path);
        return QIcon(pm);
    }();
    // Direction follows the user's reading (2026-11-21): a FOLDED block shows
    // the right-pointing `\u25b6` ("closed, click to open"), and an OPEN block shows
    // the down-pointing `\u25bc` ("open, content drops down"). This is the reverse of
    // the first implementation.
    return folded ? rightPointing : downPointing;
}

void EditorTabManager::applyFoldMarks()
{
    KTextEditor::View* v = activeView();
    KTextEditor::Document* doc = v ? v->document() : nullptr;
    if (!doc)
        return;
    // Folding needs a syntax mode: an untitled / None-mode document is not
    // foldable, so it never carries marks.
    if (!canFold()) {
        doc->clearMarks();
        return;
    }

    QVector<QString> lines;
    lines.reserve(doc->lines());
    for (int i = 0; i < doc->lines(); ++i)
        lines.append(doc->line(i));
    const QVector<int> foldable =
        FoldIndicator::foldableLines(lines, blockSkipProbe(doc));

    doc->clearMarks();
    // setMarkIcon is GLOBAL per mark TYPE (Mark stores only line + type - no
    // per-line icon), so the glyph cannot differ line by line. Set it ONCE from
    // the document's fold state: `▶` if ANY foldable block is collapsed, else
    // `▼`. Setting it inside the loop would let the last line's state overwrite
    // every mark (measured defect: collapsing block A left a stale ▼ when an
    // unfolded block B came later in the document).
    bool anyFolded = false;
    for (const int line : foldable) {
        if ((line + 1 < doc->lines()) && !v->isLineVisible(line + 1)) {
            anyFolded = true;
            break;
        }
    }
    doc->setMarkIcon(KTextEditor::Document::Bookmark, foldIcon(anyFolded));
    for (const int line : foldable)
        doc->setMark(line, KTextEditor::Document::Bookmark);
}

void EditorTabManager::refreshFoldIndicator()
{
    KTextEditor::View* v = activeView();
    KTextEditor::Document* doc = v ? v->document() : nullptr;
    if (!doc || !v)
        return;
    if (!blockMarkersVisible()) {
        doc->clearMarks();
        // Only write the config when it actually changes. A redundant
        // setConfigValue still routes through KateConfig::setValue ->
        // ViewPrivate::updateConfig -> KateLayoutCache::clear(), and that
        // clear crashes intermittently in ~QTextLayout (measured 2026-11-21).
        if (v->configValue(QStringLiteral("icon-bar")).toBool())
            v->setConfigValue(QStringLiteral("icon-bar"), false);
        return;
    }
    applyFoldMarks();
    if (!v->configValue(QStringLiteral("icon-bar")).toBool())
        v->setConfigValue(QStringLiteral("icon-bar"), true);
}

void EditorTabManager::updateFoldGlyph()
{
    KTextEditor::View* v = activeView();
    KTextEditor::Document* doc = v ? v->document() : nullptr;
    if (!doc || !v || !blockMarkersVisible() || !canFold())
        return;
    // The fold commands do not add or remove marks - the same opener lines stay
    // marked - so we only need to decide the shared glyph: `▶` if ANY marked
    // opener is folded, else `▼`. Iterate the existing marks (O(marks)) rather
    // than re-scanning every line, and never touch the config here.
    bool anyFolded = false;
    for (auto it = doc->marks().cbegin(); it != doc->marks().cend(); ++it) {
        const int line = it.key();
        if ((line + 1 < doc->lines()) && !v->isLineVisible(line + 1)) {
            anyFolded = true;
            break;
        }
    }
    doc->setMarkIcon(KTextEditor::Document::Bookmark, foldIcon(anyFolded));
}

void EditorTabManager::attachBlockTracking(KTextEditor::View* view)
{
    if (!view)
        return;
    // NOT View::contextMenuAboutToShow: that signal never fires in this app.
    // The shell is never mapped and ContentBridge synthesizes the right-button
    // press, so no QContextMenuEvent is ever produced and Kate never builds its
    // menu. The menu that appears is ShellWindow's BorlandMenu, which pulls
    // these same actions in via setBlockMenuActions(). All this hook has to do
    // is keep their enabled state and the highlight band following the caret.
    connect(view, &KTextEditor::View::cursorPositionChanged, this,
            [this](KTextEditor::View* v, KTextEditor::Cursor) {
                if (v != activeView())
                    return;
                refreshBlockActions();
                if (m_blockHighlight)
                    applyBlockHighlight();
            });
    // Re-apply the gutter fold marks whenever this document's text changes, so
    // a stale `▼` never sits next to a line that no longer opens a block.
    connect(view->document(), &KTextEditor::Document::textChanged, this,
            [this](KTextEditor::Document* doc) {
                if (doc == activeDocument())
                    refreshFoldIndicator();
            });
}

bool EditorTabManager::canFold() const
{
    KTextEditor::View* v = activeView();
    if (!v)
        return false;
    KTextEditor::Document* doc = v->document();
    if (!doc)
        return false;
    const QString mode = doc->highlightingMode();
    return !mode.isEmpty() && mode != QLatin1String("None");
}

void EditorTabManager::replaceWithFiles(const QVector<QUrl>& urls, int activeIndex)
{
    if (urls.isEmpty())
        return;

    // openFile() APPENDS, so remember what was already there.
    const int placeholders = m_docs.size();
    for (const QUrl& url : urls)
        openFile(url);

    // Drop the placeholders, newest first so the indices stay valid. Only a
    // pristine untitled document qualifies: an empty buffer with no URL that the
    // user has not modified. Anything else is real work and stays open.
    for (int i = placeholders - 1; i >= 0; --i) {
        KTextEditor::Document* doc = m_docs.value(i);
        if (!doc)
            continue;
        if (doc->url().isEmpty() && doc->text().isEmpty() && !doc->isModified())
            closeTab(i);
    }

    if (m_docs.isEmpty())
        return;                      // closeTab leaves a fresh tab in that case
    setActiveIndex(qBound(0, activeIndex, int(m_docs.size()) - 1));
}

void EditorTabManager::affixActiveLines(const QString& affix, bool atEnd)
{
    KTextEditor::View* v = activeView();
    KTextEditor::Document* doc = activeDocument();
    if (!v || !doc || affix.isEmpty())
        return;                        // empty affix: no edit, no undo entry
    const KTextEditor::Range range = activeRange(v, doc);
    const QString before = doc->text(range);
    const QString after = affixLines(before, affix, atEnd);
    if (after != before)
        doc->replaceText(range, after);
}

void EditorTabManager::addToLines(bool atEnd, QWidget* anchor)
{
    if (!activeView() || !activeDocument())
        return;
    LineAffixDialog dlg(atEnd ? LineAffixDialog::Mode::End
                              : LineAffixDialog::Mode::Start);
    if (TvModal::exec(anchor ? anchor : activeView(), &dlg) != TvModal::Accept)
        return;
    if (dlg.wantsRemove()) {
        // Remove mode: the typed string comes OFF each line's edge instead of
        // going on. (The field is disabled while Remove is ticked, so the text
        // is whatever was in it before the flip - that is the needle.)
        stripActiveLines(dlg.text(), atEnd);
        return;
    }
    affixActiveLines(dlg.wantsLineBreak() ? QStringLiteral("\n") : dlg.text(), atEnd);
}

void EditorTabManager::stripActiveLines(const QString& needle, bool atEnd)
{
    KTextEditor::View* v = activeView();
    KTextEditor::Document* doc = activeDocument();
    if (!v || !doc || needle.isEmpty())
        return;                        // empty needle: no edit, no undo entry
    const KTextEditor::Range range = activeRange(v, doc);
    const QString before = doc->text(range);
    const QString after = stripEdgeText(before, needle, atEnd);
    if (after != before)
        doc->replaceText(range, after);
}

void EditorTabManager::removeLineBreaks()
{
    KTextEditor::View* v = activeView();
    KTextEditor::Document* doc = activeDocument();
    if (!v || !doc)
        return;
    const KTextEditor::Range range = activeRange(v, doc);
    const QString before = doc->text(range);
    const QString after = removeLineBreaksText(before);
    if (after != before)
        doc->replaceText(range, after);
}

void EditorTabManager::goToLine(int oneBasedLine)
{
    KTextEditor::View* v = activeView();
    KTextEditor::Document* doc = activeDocument();
    if (!v || !doc)
        return;
    // 1-based in from the user, 0-based cursor out, floored and clamped (a paste
    // can bypass the dialog's validator, so this is not a duplicate of it).
    const int bounded = qBound(1, oneBasedLine, doc->lines());
    v->setCursorPosition(KTextEditor::Cursor(bounded - 1, 0));
}

void EditorTabManager::installActions(QWidget* host)
{
    if (!host)
        return;

    // Zoom shortcuts (Ctrl+= in, Ctrl+- out, Ctrl+0 reset).
    auto* zoomIn = new QAction(tr("Zoom &In"), host);
    zoomIn->setShortcut(QKeySequence(QStringLiteral("Ctrl+=")));
    connect(zoomIn, &QAction::triggered, this, [this] { setZoom(+1); });
    host->addAction(zoomIn);

    auto* zoomOut = new QAction(tr("Zoom &Out"), host);
    zoomOut->setShortcut(QKeySequence(QStringLiteral("Ctrl+-")));
    connect(zoomOut, &QAction::triggered, this, [this] { setZoom(-1); });
    host->addAction(zoomOut);

    auto* zoomReset = new QAction(tr("&Reset Zoom"), host);
    zoomReset->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    connect(zoomReset, &QAction::triggered, this, [this] { setZoom(0); });
    host->addAction(zoomReset);

    // Text tools.
    auto* upper = new QAction(tr("UPPERCASE"), host);
    connect(upper, &QAction::triggered, this, [this] { changeCase(Upper); });
    host->addAction(upper);

    auto* lower = new QAction(tr("lowercase"), host);
    connect(lower, &QAction::triggered, this, [this] { changeCase(Lower); });
    host->addAction(lower);

    auto* sort = new QAction(tr("Sort Lines"), host);
    connect(sort, &QAction::triggered, this, [this] { sortLines(); });
    host->addAction(sort);
}

bool EditorTabManager::saveActive()
{
    KTextEditor::Document* d = activeDocument();
    if (!d || !d->documentSave())
        return false;
    // Our own write fires the watcher; move the baseline now so the echo is
    // silent. Watcher events arrive later through the event loop, so this
    // always wins the race.
    armWatch(d);
    return true;
}

bool EditorTabManager::saveActiveAs()
{
    KTextEditor::Document* d = activeDocument();
    return d && d->documentSaveAs();
}

bool EditorTabManager::saveActiveAs(const QUrl& url)
{
    KTextEditor::Document* d = activeDocument();
    if (!d || url.isEmpty() || !d->saveAs(url))
        return false;
    // Save-as may have moved the document: armWatch drops the old path.
    armWatch(d);
    return true;
}

// ---------------------------------------------------------------------------
// External changes (2026-11-20)
//
// Kate has no file watcher in this build - libKF6TextEditor contains no
// QFileSystemWatcher, and modifiedOnDisk stayed silent when a file was
// rewritten underneath an open document - so the watch and the decision live
// here. FileChangeTracker decides, ReloadDialog asks, documentReload() acts.
// ---------------------------------------------------------------------------

void EditorTabManager::armWatch(KTextEditor::Document* doc)
{
    if (!doc)
        return;
    const QString path = doc->url().toLocalFile();
    if (path.isEmpty())
        return;   // untitled or remote: nothing to watch

    // Save-as can move a document; stop watching the path it left.
    const QStringList watched = m_watchedDoc.keys();
    for (const QString& old : watched) {
        if (old != path && m_watchedDoc.value(old) == doc) {
            if (m_watcher.files().contains(old))
                m_watcher.removePath(old);
            m_tracker.forget(old);
            m_watchedDoc.remove(old);
        }
    }

    // The baseline is the file as it is NOW. That is what keeps our own save
    // silent, and what a reload or a Keep-then-Save re-arms.
    m_tracker.setBaseline(path, FileSignature::fromPath(path));
    if (!m_watcher.files().contains(path))
        m_watcher.addPath(path);
    m_watchedDoc.insert(path, doc);

    // Watch the directory too: a temp-file rename drops the path from the
    // watcher, and only the directory tells us the file came back.
    const QString dir = QFileInfo(path).absolutePath();
    if (!dir.isEmpty() && !m_watcher.directories().contains(dir))
        m_watcher.addPath(dir);
}

void EditorTabManager::disarmWatch(KTextEditor::Document* doc)
{
    if (!doc)
        return;
    const QString path = doc->url().toLocalFile();
    if (path.isEmpty())
        return;
    if (m_watcher.files().contains(path))
        m_watcher.removePath(path);
    m_tracker.forget(path);
    m_watchedDoc.remove(path);
}

void EditorTabManager::onWatchedPathChanged(const QString& path)
{
    KTextEditor::Document* doc = m_watchedDoc.value(path);
    if (!doc)
        return;

    const FileChange change = m_tracker.takeAction(path, FileSignature::fromPath(path));

    // Qt drops a path that disappeared (a delete, or a rename-replace). Re-arm
    // whenever the file is back, so the NEXT change is still seen; while it is
    // gone, a save re-arms it because armWatch runs on every save.
    if (!m_watcher.files().contains(path) && QFile::exists(path))
        m_watcher.addPath(path);

    if (change == FileChange::None)
        return;

    emit externalChangeDetected(change, path);

    // One prompt at a time: an event that arrives while a dialog is up was
    // already consumed by the baseline above, so prompting again would only
    // stack a dialog on a dialog.
    if (m_externalPromptOpen)
        return;

    int role = TvModal::Reject;
    m_externalPromptOpen = true;
    if (m_externalHandler) {
        role = m_externalHandler(change, path);
    } else {
        // Heap-allocated: TvModal::exec reparents the modal to the anchor, and
        // a stack modal under a stack anchor is the known double-free.
        auto* dlg = new ReloadDialog(change == FileChange::Deleted
                                         ? ReloadDialog::Mode::Deleted
                                         : ReloadDialog::Mode::Modified,
                                     path);
        role = TvModal::exec(m_dialogAnchor ? m_dialogAnchor : activeView(), dlg);
        dlg->deleteLater();
    }
    m_externalPromptOpen = false;

    if (change == FileChange::Modified) {
        if (role == TvModal::Accept)
            reloadDocument(doc);
        return;
    }
    // Deleted: Keep leaves the buffer open and saveable; Close drops the tab.
    if (role != TvModal::Accept) {
        const int index = m_docs.indexOf(doc);
        if (index >= 0)
            closeTab(index);
    }
}

void EditorTabManager::reloadDocument(KTextEditor::Document* doc)
{
    if (!doc || doc->url().isEmpty())
        return;

    // documentReload() replaces the text and emits textChanged; without the
    // guard the pre-reload buffer would be pushed as an undo step.
    m_applyingHistory = true;
    const bool ok = doc->documentReload();
    m_applyingHistory = false;
    if (!ok)
        return;

    // The reloaded text is the new truth. Re-seed the snapshot stack so Undo
    // cannot resurrect the buffer the user chose to discard.
    QList<HistoryEntry>& hist = historyFor(doc);
    hist.clear();
    hist.append({ doc->text(), KTextEditor::Cursor(0, 0) });
    historyIndexFor(doc) = 0;
    notifyHistory();

    // A reload is not a change: re-baseline so the echo stays quiet.
    armWatch(doc);
}

void EditorTabManager::findReplace(QWidget* anchor, int whichField)
{
    // A child of the shell (TvModal::exec reparents), so it composites inside
    // the CRT pass like the file dock and the other house modals.
    FindReplaceDialog dlg(this);
    dlg.setInitialField(whichField);
    TvModal::exec(anchor, &dlg);
}

void EditorTabManager::findNext()
{
    // The dialog is gone, so its remembered pattern is the only state. With
    // nothing remembered there is nothing to repeat: open the dialog instead.
    if (FindReplaceDialog::lastPattern().isEmpty()) {
        findReplace(activeView(), 0);
        return;
    }
    // One matching path: build the dialog (it re-fills the pattern and the
    // remembered options) and let it run a Find Next without showing.
    FindReplaceDialog dlg(this);
    dlg.findNext();
}

void EditorTabManager::syncTabs()
{
    QStringList titles;
    for (KTextEditor::Document* d : m_docs) {
        QString t;
        if (d->url().isEmpty()) {
            // Kate's untitled-tab behaviour (user, Task 25): a document with no
            // file is named after what has been typed into it - the first line,
            // trimmed, cut to ten characters. It keeps following the buffer
            // until the document is saved, at which point the filename wins.
            const QString text = d->text();
            const int eol = text.indexOf(QLatin1Char('\n'));
            const QString firstLine = (eol < 0 ? text : text.left(eol)).trimmed();
            t = firstLine.isEmpty() ? QStringLiteral("untitled") : firstLine.left(10);
        } else {
            t = QFileInfo(d->url().toLocalFile()).fileName();
        }
        if (t.isEmpty())
            t = QStringLiteral("untitled");
        titles << t;
    }
    emit tabsChanged(titles, m_active);
}