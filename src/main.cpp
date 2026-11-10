// SPDX-License-Identifier: GPL-3.0-or-later
// CRT-Kate entry: Borland ShellWindow rendered offscreen by ContentBridge
// into the QML CRT scene (EffectChain), which is the visible window —
// CRT effects cover the entire display per the design spec.
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QtQuick/qquickitemgrabresult.h>
#include <QTimer>
#include "src/shell/ShellWindow.h"
#include "src/shell/TabStrip.h"
#include "src/bridge/ContentBridge.h"
#include "src/editor/EditorTabManager.h"
#include "src/settings/CrtSettings.h"
#include "src/session/SessionManager.h"
#include <ktexteditor/view.h>

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
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &sessions, [&]() {
        QVector<SessionManager::Entry> entries;
        for (int i = 0; i < tabs.viewCount(); ++i) {
            const QUrl url = tabs.viewFileUrl(i);
            if (url.isEmpty())
                continue;
            const QPoint p = tabs.viewCursorPosition(i);
            entries.append({ url, p.y(), p.x(), tabs.activeIndex() });
        }
        if (!entries.isEmpty())
            sessions.setEntries(entries);
    });
    if (!sessions.lastSession().isEmpty() && sessions.restoreSession(sessions.lastSession())) {
        const auto entries = sessions.entries();
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
    }

    ContentBridge bridge;
    bridge.setShellWindow(&shell);

    // CRT settings (QSettings-backed single source of truth for effect knobs).
    // Loaded at startup; persisted on quit so the dialog's changes survive restart.
    CrtSettings crtSettings;
    crtSettings.load();
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &crtSettings,
                     [&crtSettings] { crtSettings.save(); });

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral(CRT_KATE_SOURCE_DIR)); // `import src`
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

    engine.load(QUrl::fromLocalFile(
        QStringLiteral(CRT_KATE_SOURCE_DIR "/src/main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

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