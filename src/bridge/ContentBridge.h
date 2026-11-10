// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QImage>
#include <QQuickImageProvider>
#include <QQuickItem>   // full type required for the sourceItem property's moc meta-type

class ShellWindow;

// Renders the ShellWindow widget tree into a frame the QML CRT scene samples.
// Mechanism (spec's approved fallback path): QWidget::render() -> QImage at
// ~30fps, served to QML through BridgeImageProvider ("image://bridge/frame").
class ContentBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuickItem* sourceItem READ sourceItem WRITE setSourceItem
               NOTIFY sourceItemChanged)
    Q_PROPERTY(int frameSerial READ frameSerial NOTIFY frameChanged)
public:
    explicit ContentBridge(QObject* parent = nullptr);
    void setShellWindow(ShellWindow* shell);

    // Renders the shell into frame(); bumps frameSerial; emits frameChanged.
    Q_INVOKABLE void update();
    // Keeps the shell sized to the visible QML window (spec: view fits window).
    Q_INVOKABLE void resizeShell(int w, int h);

    // Popup bridging (Task 9): an open QMenu/QWidget popup is captured into the
    // same composited frame so CRT effects cover it (instead of leaving it a
    // separate native window above the CRT scene).
    void registerPopup(QWidget* popup);
    void unregisterPopup(QWidget* popup);
    QList<QWidget*> registeredPopups() const { return m_popups; }

    QImage frame() const { return m_frame; }
    int frameSerial() const { return m_frameSerial; }
    QQuickItem* sourceItem() const { return m_sourceItem; }
    void setSourceItem(QQuickItem* item);

signals:
    void frameChanged();
    void sourceItemChanged();

private:
    ShellWindow* m_shell = nullptr;
    QImage m_frame;
    int m_frameSerial = 0;
    QQuickItem* m_sourceItem = nullptr;
    QList<QWidget*> m_popups;
};

// Serves ContentBridge::frame() to QML as "image://bridge/<id>".
class BridgeImageProvider : public QQuickImageProvider
{
public:
    explicit BridgeImageProvider(ContentBridge* bridge)
        : QQuickImageProvider(QQuickImageProvider::Image), m_bridge(bridge) {}
    QImage requestImage(const QString& id, QSize* size,
                        const QSize& requestedSize) override;

private:
    ContentBridge* m_bridge;
};