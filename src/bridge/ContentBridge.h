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