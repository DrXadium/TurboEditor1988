// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtGlobal>   // qEnvironmentVariableIsSet (noSweep verification aid)
#include <QObject>
#include <QImage>
#include <QQuickImageProvider>
#include <QQuickItem>   // full type required for the sourceItem property's moc meta-type

class ShellWindow;
class QWidget;

// Renders the ShellWindow widget tree into a frame the QML CRT scene samples.
// Mechanism (spec's approved fallback path): QWidget::render() -> QImage at
// ~30fps, served to QML through BridgeImageProvider ("image://bridge/frame").
class ContentBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuickItem* sourceItem READ sourceItem WRITE setSourceItem
               NOTIFY sourceItemChanged)
    Q_PROPERTY(int frameSerial READ frameSerial NOTIFY frameChanged)
    // Verification aid: when set, the QML sweep clock stops advancing so a
    // capture is not caught mid-band (a vertical band lingers ~0.6s per row on
    // a 800px-tall window vs ~0.06s per column horizontally).
    Q_PROPERTY(bool noSweep READ noSweep CONSTANT)
    // CRT effects master switch. OFF by default while the UI is being nailed
    // down (the shader's chroma shift/scanlines make chrome defects hard to
    // read). CRT_KATE_FX=1 turns the effect chain back on.
    Q_PROPERTY(bool effectsEnabled READ effectsEnabled CONSTANT)
public:
    explicit ContentBridge(QObject* parent = nullptr);
    void setShellWindow(ShellWindow* shell);

    bool noSweep() const { return qEnvironmentVariableIsSet("CRT_KATE_NOSWEEP"); }
    bool effectsEnabled() const { return qEnvironmentVariableIsSet("CRT_KATE_FX"); }

    // Renders the shell into frame(); bumps frameSerial; emits frameChanged.
    Q_INVOKABLE void update();
    // Keeps the shell sized to the visible QML window (spec: view fits window).
    Q_INVOKABLE void resizeShell(int w, int h);

    // Input forwarding (the CRT glass captures mouse/keys and posts them at the
    // offscreen shell, 1:1 coords — see input-forwarding design doc).
    // type: QEvent::Type (MouseButtonPress/Release/Move, KeyPress/Release).
    // x,y are shell-local (== QML local, since shell renders 1:1 to the window).
    Q_INVOKABLE void forwardMouseEvent(int type, int button, int buttons,
                                       int modifiers, double x, double y);
    Q_INVOKABLE void forwardKeyEvent(int type, int key, int modifiers,
                                     const QString& text);
    // Wheel: the QML input layer forwards no wheel at all, so a scrolled
    // dropdown never received a QWheelEvent and could not be scrolled by the
    // mouse. dx/dy are the wheel deltas (QPoint angleDelta components).
    Q_INVOKABLE void forwardWheelEvent(int dx, int dy, int buttons,
                                       int modifiers, double x, double y);
    // Where a key goes. An open popup owns the keyboard (its arrow keys walk the
    // list); otherwise it is the focused child (the editor). Keys used to go to
    // keyFocusWidget() unconditionally, so arrows never reached an open dropdown.
    QWidget* keyTarget() const;
    // The visible popup containing this shell-local point, else nullptr.
    QWidget* popupAt(const QPoint& shellLocal) const;
    // Trigger the QAction whose shortcut matches this key combo. Needed because
    // the unmapped shell has no active window, so Qt's own shortcut map is
    // bypassed entirely. Returns true when an action fired.
    bool dispatchShortcut(int key, int modifiers);
    // Same problem one level down: KTextEditor binds cursor movement and
    // deletion to QActions with BARE-key shortcuts (Left/Right/Up/Down,
    // Backspace, Delete, PageUp/PageDown), so with no shortcut map those keys
    // reach the view and do nothing. Walks from the focused widget up to the
    // shell and triggers the first enabled action whose shortcut matches.
    bool dispatchFocusedShortcut(QWidget* target, int key, int modifiers);

    // Popup bridging (Task 9): an open QMenu/QWidget popup is captured into the
    // same composited frame so CRT effects cover it (instead of leaving it a
    // separate native window above the CRT scene).
    void registerPopup(QWidget* popup);
    void unregisterPopup(QWidget* popup);
    QList<QWidget*> registeredPopups() const { return m_popups; }

    // TV-revision2 spec 7: the status line is REPLACED by a context hint while
    // a dropdown is open. The bridge owns the popup lifecycle, so it is the
    // natural place to relay hovered actions to the shell's FKeyStatusBar.
    void setStatusHint(const QString& hint);

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