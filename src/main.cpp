// SPDX-License-Identifier: GPL-3.0-or-later
// CRT-Kate entry: Borland ShellWindow rendered offscreen by ContentBridge
// into the QML CRT scene (EffectChain), which is the visible window —
// CRT effects cover the entire display per the design spec.
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include "src/shell/ShellWindow.h"
#include "src/bridge/ContentBridge.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CRT-Kate"));

    // The Borland chrome, rendered offscreen (never mapped to a real window).
    ShellWindow shell;
    shell.resize(1280, 800);

    ContentBridge bridge;
    bridge.setShellWindow(&shell);

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral(CRT_KATE_SOURCE_DIR)); // `import src`
    engine.addImageProvider(QStringLiteral("bridge"),
                             new BridgeImageProvider(&bridge));
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

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
    // window after 1.5s and exits — used for headless visual verification.
    if (const QByteArray shot = qgetenv("CRT_KATE_SHOT"); !shot.isEmpty()) {
        QTimer::singleShot(1500, &app, [&engine, shot]() {
            if (auto* root = engine.rootObjects().value(0))
                if (auto* win = qobject_cast<QQuickWindow*>(root))
                    win->grabWindow().save(QString::fromUtf8(shot));
            QCoreApplication::exit(0);
        });
    }

    return app.exec();
}