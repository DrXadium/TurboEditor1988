// SPDX-License-Identifier: GPL-3.0-or-later
// CRT-Kate entry: Borland ShellWindow rendered offscreen by ContentBridge
// into the QML CRT scene (EffectChain), which is the visible window —
// CRT effects cover the entire display per the design spec.
#include <QApplication>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QKeySequence>
#include <QEvent>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QDir>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QtQuick/qquickitemgrabresult.h>
#include <QTimer>
#include "src/shell/ShellWindow.h"
#include "src/shell/TabStrip.h"
#include "src/shell/MenuBar.h"
#include "src/shell/Minimap.h"
#include "src/bridge/ContentBridge.h"
#include "src/editor/EditorTabManager.h"
#include "src/settings/CrtSettings.h"
#include "src/settings/SettingsDialog.h"
#include "src/session/SessionManager.h"
#include "src/autosave/AutosaveService.h"
#include <ktexteditor/view.h>
#include <ktexteditor/document.h>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CRT-Kate"));

    // The Borland chrome, rendered offscreen (never mapped to a real window).
    ShellWindow shell;
    shell.resize(1280, 800);

    // Composition root: host the KTextEditor tab container below the TabStrip,
    // sync the TabStrip to the documents, and install zoom/tools actions.
    EditorTabManager tabs;
    shell.setEditorWidget(tabs.containerWidget());
    tabs.installActions(&shell);
    QObject::connect(&tabs, &EditorTabManager::tabsChanged, &shell,
                     [&shell](const QStringList& titles, int active) {
                         if (TabStrip* strip = shell.tabStrip())
                             strip->setTabs(titles, active);
                     });
    QObject::connect(shell.tabStrip(), &TabStrip::tabActivated, &tabs,
                     &EditorTabManager::setActiveIndex);
    // Seed the strip with the initial tab (created by the manager ctor).
    if (TabStrip* strip0 = shell.tabStrip())
        strip0->setTabs(QStringList() << QStringLiteral("untitled"), 0);

    // Named sessions (Task 7): snapshot open files + cursor on quit, restore
    // the last session at startup (no-op on a fresh install: nothing saved yet).
    SessionManager sessions;
    // Collects the current tab set for persistence - shared by the quit
    // snapshot AND the Session > Save menu so both record identical state.
    auto collectSessionEntries = [&tabs]() {
        QVector<SessionManager::Entry> entries;
        for (int i = 0; i < tabs.viewCount(); ++i) {
            const QUrl url = tabs.viewFileUrl(i);
            if (url.isEmpty())
                continue;
            const QPoint p = tabs.viewCursorPosition(i);
            entries.append({ url, p.y(), p.x(), tabs.activeIndex() });
        }
        return entries;
    };
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &sessions, [&]() {
        const auto entries = collectSessionEntries();
        if (!entries.isEmpty())
            sessions.setEntries(entries);
    });
    // Opens a restored entry set (files, active tab, cursor) - shared by the
    // launch-time restore AND the Session > Restore Last menu.
    auto restoreEntries = [&tabs](const QVector<SessionManager::Entry>& entries) {
        for (const auto& e : entries)
            tabs.openFile(e.fileUrl);
        if (!entries.isEmpty()) {
            int want = 0;
            for (int i = 0; i < entries.size(); ++i)
                if (entries[i].activeIndex >= 0 && entries[i].activeIndex < entries.size())
                    want = entries[i].activeIndex;
            tabs.setActiveIndex(want);
            const auto& active = entries[want];
            if (KTextEditor::View* v = tabs.activeView())
                v->setCursorPosition(KTextEditor::Cursor(active.line, active.column));
        }
    };
    if (!sessions.lastSession().isEmpty() && sessions.restoreSession(sessions.lastSession()))
        restoreEntries(sessions.entries());

    ContentBridge bridge;
    bridge.setShellWindow(&shell);

    // CRT'd popup menus (Task 9): a QMenu opens as its own native top-level
    // window, which would sit ABOVE the CRT scene. Register it with the bridge
    // so its pixels are composited into the same texture the CRT samples.
    class PopupRegistrar : public QObject {
    public:
        explicit PopupRegistrar(ContentBridge* b, QObject* parent) : QObject(parent), m_b(b) {}
    protected:
        bool eventFilter(QObject* obj, QEvent* ev) override {
            if (auto* menu = qobject_cast<QMenu*>(obj)) {
                if (ev->type() == QEvent::Show)
                    m_b->registerPopup(menu);
                else if (ev->type() == QEvent::Hide)
                    m_b->unregisterPopup(menu);
            }
            return QObject::eventFilter(obj, ev);
        }
    private:
        ContentBridge* m_b;
    };
    auto* registrar = new PopupRegistrar(&bridge, &app);
    app.installEventFilter(registrar);

    // CRT settings (QSettings-backed single source of truth for effect knobs).
    // Loaded at startup; persisted on quit so the dialog's changes survive restart.
    CrtSettings crtSettings;
    crtSettings.load();
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &crtSettings,
                     [&crtSettings] { crtSettings.save(); });

    // Task 10: wire Kate-option menus (MenuBar builds the structure and the
    // Borland palette; here the actions get their real features). The session
    // collect/restore helpers above are reused so menu, quit and launch paths
    // record/restore identical state.
    if (auto* menuBar = shell.findChild<MenuBar*>()) {
        auto act = [menuBar](const char* name) -> QAction* {
            return menuBar->findChild<QAction*>(QLatin1String(name));
        };
        auto on = [](QAction* a, auto fn) {
            if (a)
                QObject::connect(a, &QAction::triggered, a,
                                 [fn = std::move(fn)](bool) { fn(); });
        };

        // File
        on(act("actionFileNew"), [&tabs] { tabs.newDocument(); });
        on(act("actionFileOpen"), [&shell, &tabs] {
            const QString f =
                QFileDialog::getOpenFileName(&shell, QObject::tr("Open File"));
            if (!f.isEmpty())
                tabs.openFile(QUrl::fromLocalFile(f));
        });
        on(act("actionFileSave"), [&tabs] { tabs.saveActive(); });
        on(act("actionFileSaveAs"), [&tabs] { tabs.saveActiveAs(); });

        // Edit: case/sort tools + on-the-fly spellcheck config toggle
        on(act("actionEditUpper"),
           [&tabs] { tabs.changeCase(EditorTabManager::Upper); });
        on(act("actionEditLower"),
           [&tabs] { tabs.changeCase(EditorTabManager::Lower); });
        on(act("actionEditSort"), [&tabs] { tabs.sortLines(); });
        if (QAction* spell = act("actionEditSpellcheck"))
            QObject::connect(spell, &QAction::triggered, spell, [&tabs, spell](bool) {
                if (auto* doc = tabs.activeDocument())
                    doc->setConfigValue(QStringLiteral("on-the-fly-spellcheck"),
                                        spell->isChecked());
            });

        // Search
        on(act("actionSearchFind"), [&tabs] { tabs.find(); });
        on(act("actionSearchFindNext"), [&tabs] { tabs.findNext(); });

        // View: zoom + checkable toggles
        on(act("actionViewZoomIn"), [&tabs] { tabs.setZoom(+1); });
        on(act("actionViewZoomOut"), [&tabs] { tabs.setZoom(-1); });
        on(act("actionViewZoomReset"), [&tabs] { tabs.setZoom(0); });
        if (QAction* mm = act("actionViewMinimap"))
            QObject::connect(mm, &QAction::triggered, mm, [&shell, mm](bool) {
                if (auto* mini = shell.minimap())
                    mini->setVisible(mm->isChecked());
            });
        if (QAction* fold = act("actionViewFoldBar"))
            QObject::connect(fold, &QAction::triggered, fold, [&tabs, fold](bool) {
                if (auto* v = tabs.activeView())
                    v->setConfigValue(QStringLiteral("folding-bar"), fold->isChecked());
            });

        // Session -> SessionManager (reuses collectSessionEntries/restoreEntries)
        on(act("actionSessionSave"),
           [&shell, &sessions, &collectSessionEntries] {
               bool ok = false;
               const QString name = QInputDialog::getText(
                   &shell, QObject::tr("Save Session"),
                   QObject::tr("Session name:"), QLineEdit::Normal,
                   sessions.lastSession(), &ok);
               if (!ok || name.trimmed().isEmpty())
                   return;
               const auto entries = collectSessionEntries();
               if (!entries.isEmpty())
                   sessions.setEntries(entries);
               sessions.saveSession(name);
           });
        on(act("actionSessionRestore"), [&shell, &sessions, &restoreEntries] {
            const QString name = sessions.lastSession();
            if (!name.isEmpty() && sessions.restoreSession(name))
                restoreEntries(sessions.entries());
        });

        // Settings -> SettingsDialog. Opens NATIVELY while exec()'d: do NOT pass
        // it through bridge.registerPopup - that WA_DontShowOnScreen hardening
        // would leave a modal dialog with no window and no way to dismiss it.
        on(act("actionSettingsConfigure"), [&shell, &crtSettings] {
            SettingsDialog dlg(&crtSettings, &shell);
            dlg.exec();
        });

        // Help
        on(act("actionHelpAbout"), [&shell] {
            QMessageBox::about(
                &shell, QObject::tr("About CRT-Kate"),
                QObject::tr("<b>CRT-Kate</b> - Kate-class editing in a "
                            "Borland-style shell with cool-retro-term CRT "
                            "effects.<br>Built on KTextEditor (KDE "
                            "Frameworks) and Qt."));
        });

        // Turbo Vision function keys (Task 12): the status line claims these,
        // so wire them live as extra shortcuts on the existing actions.
        // F2 Save, F3 Open, F5 Zoom reset; F6 next tab + F10 menu added below.
        auto addFKey = [](QAction* a, const char* key) {
            if (!a)
                return;
            QStringList keys;
            const QKeySequence cur = a->shortcut();
            keys << cur.toString();
            keys << QString::fromLatin1(key);
            QList<QKeySequence> seq;
            for (const QString& k : keys)
                if (!k.isEmpty())
                    seq << QKeySequence(k);
            a->setShortcuts(seq);
        };
        addFKey(act("actionFileSave"), "F2");
        addFKey(act("actionFileOpen"), "F3");
        addFKey(act("actionViewZoomReset"), "F5");
    }

    // F6: next tab (cycle). Wired after the action block (needs shell/tabs).
    {
        auto* nextTab = new QAction(&shell);
        nextTab->setObjectName(QStringLiteral("actionF6Next"));
        nextTab->setShortcut(QKeySequence(QStringLiteral("F6")));
        shell.addAction(nextTab);
        QObject::connect(nextTab, &QAction::triggered, nextTab, [&tabs] {
            const int n = tabs.tabCount();
            if (n > 0)
                tabs.setActiveIndex((tabs.activeIndex() + 1) % n);
        });
    }
    // F10: open the menu bar (first menu), the TV "F10 = Menu" convention.
    {
        auto* menuAct = new QAction(&shell);
        menuAct->setObjectName(QStringLiteral("actionF10Menu"));
        menuAct->setShortcut(QKeySequence(QStringLiteral("F10")));
        shell.addAction(menuAct);
        QObject::connect(menuAct, &QAction::triggered, menuAct, [&shell] {
            if (QMenuBar* mb = shell.menuBar()) {
                const auto tops = mb->actions();
                if (!tops.isEmpty())
                    mb->setActiveAction(tops.first());
                if (QMenu* m = mb->actions().first()->menu())
                    m->popup(mb->mapToGlobal(QPoint(0, mb->height())));
            }
        });
    }

    // Crash recovery (Task 8): snapshots modified buffers periodically; on
    // launch, surface any recovery files left by a previous crashed run as a
    // message box so unsaved work isn't silently lost.
    AutosaveService autosave;
    autosave.start(120000);
    if (const auto pending = autosave.pendingRecovery(); !pending.isEmpty()) {
        QStringList names;
        for (const auto& p : pending)
            names << (p.originalUrl.isEmpty() ? p.autosavePath
                                              : p.originalUrl.toLocalFile());
        QMessageBox::information(
            &shell, QObject::tr("Recovered unsaved work"),
            QObject::tr("%1 autosave file(s) from a previous session were found:\n\n%2\n\n"
                         "They are kept in:\n%3")
                .arg(pending.size())
                .arg(names.join(QLatin1Char('\n')))
                .arg(autosave.autosaveDir()));
    }

    QQmlApplicationEngine engine;
    // Locate QML at runtime so the binary is not tied to the build tree:
    // 1. CRT_KATE_QML_DIR env override,
    // 2. installed layout (AppImage: <appdir>/../share/crt-kate),
    // 3. build-time source dir (dev builds).
    const QString qmlRoot = []() -> QString {
        const QString env = qEnvironmentVariable("CRT_KATE_QML_DIR");
        if (!env.isEmpty())
            return env;
        const QString installed =
            QDir::cleanPath(QCoreApplication::applicationDirPath()
                            + QStringLiteral("/../share/crt-kate"));
        if (QFileInfo::exists(installed + QStringLiteral("/src/main.qml")))
            return installed;
        return QStringLiteral(CRT_KATE_SOURCE_DIR);
    }();
    engine.addImportPath(qmlRoot); // `import src`
    engine.addImageProvider(QStringLiteral("bridge"),
                             new BridgeImageProvider(&bridge));
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("crtSettings"),
                                             &crtSettings);

    // ~30 fps frame feed (spec's approved perf budget for QWidget::render).
    QTimer frameTimer;
    QObject::connect(&frameTimer, &QTimer::timeout, &bridge, &ContentBridge::update);
    frameTimer.start(33);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);

    // Render the first frame BEFORE the QML Image resolves its provider URL
    // (avoids a null-fetch at frameSerial=0).
    bridge.update();

    engine.load(QUrl::fromLocalFile(qmlRoot + QStringLiteral("/src/main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    // Dev aid (unset in normal runs): CRT_KATE_MENU=<title> opens a probe menu
    // over the shell before CRT_KATE_SHOT captures, proving a real popup gets
    // registered + composited into the CRT texture (Task 9 E2E). Opened via a
    // queued timer so the PopupRegistrar's event filter sees its Show event.
    if (const QByteArray menuTitle = qgetenv("CRT_KATE_MENU"); !menuTitle.isEmpty()) {
        QTimer::singleShot(700, &app, [&app, menuTitle]() {
            auto* probe = new QMenu(QString::fromUtf8(menuTitle));
            probe->addAction(QStringLiteral("First action"));
            probe->addAction(QStringLiteral("Second action"));
            probe->addAction(QStringLiteral("Third action"));
            probe->setObjectName(QStringLiteral("crtProbeMenu"));
            probe->move(40, 60);           // over the shell's top-left area
            probe->show();
        });
    }

    // Dev aid (unset in normal runs): CRT_KATE_BRIGHT=<0..3> forces the CRT
    // brightness at launch so a capture can reveal the underlying chrome
    // (used with CRT_KATE_SHOT for visual restyle verification).
    if (const QByteArray b = qgetenv("CRT_KATE_BRIGHT"); !b.isEmpty()) {
        bool ok = false;
        const double v = b.toDouble(&ok);
        if (ok)
            crtSettings.setBrightness(v);
    }

    // Dev aid (unset in normal runs): CRT_KATE_RAWSHOT=/tmp/x.png grabs the
    // RAW widget render (the true chrome, no CRT shader) and exits - used for
    // visual restyle verification / pixel proofs against the Turbo Vision spec.
    if (const QByteArray raw = qgetenv("CRT_KATE_RAWSHOT"); !raw.isEmpty()) {
        QTimer::singleShot(900, &app, [&shell, raw, &app]() {
            const QImage img = shell.grab().toImage();
            img.save(QString::fromUtf8(raw));
            QCoreApplication::exit(0);
        });
    }

    // Dev aid (unset in normal runs): CRT_KATE_SHOT=/tmp/x.png grabs the CRT
    // window after 1.5s and exits - used for headless visual verification.
    // Grabs via QQuickItem::grabToImage() on the EffectChain (full-window item).
    // NOTE: do NOT verify renders with QT_QPA_PLATFORM=offscreen — offscreen has
    // no GL, so Qt Quick falls back to the software backend where ShaderEffect
    // never compiles (status stays Uncompiled) and effects silently don't render.
    // Capture on a real display (DISPLAY/Wayland session) instead.
    if (const QByteArray shot = qgetenv("CRT_KATE_SHOT"); !shot.isEmpty()) {
        QTimer::singleShot(1500, &app, [&engine, shot, &app]() {
            if (auto* root = engine.rootObjects().value(0)) {
                QQuickItem *chain = root->findChild<QQuickItem*>(
                    QStringLiteral("crtEffectChain"));
                if (!chain)
                    chain = qobject_cast<QQuickItem*>(root);
                if (chain) {
                    auto result = chain->grabToImage();
                    if (result) {
                        QObject::connect(result.data(), &QQuickItemGrabResult::ready, &app,
                                         [result, shot, &app]() {
                            result->saveToFile(QString::fromUtf8(shot));
                            QCoreApplication::exit(0);
                        });
                        return;
                    }
                }
                if (auto* win = qobject_cast<QQuickWindow*>(root))
                    win->grabWindow().save(QString::fromUtf8(shot));
            }
            QCoreApplication::exit(0);
        });
    }

    return app.exec();
}