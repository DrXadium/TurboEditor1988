// SPDX-License-Identifier: GPL-3.0-or-later
#include "ContentBridge.h"
#include "src/shell/ShellWindow.h"
#include <QPainter>
#include <QLayout>

ContentBridge::ContentBridge(QObject* parent) : QObject(parent) {}

void ContentBridge::setShellWindow(ShellWindow* shell)
{
    m_shell = shell;
    if (m_shell) {
        // Offscreen: the shell is rendered into frames, never mapped to a
        // real window (the visible window is the QML CRT scene).
        m_shell->setAttribute(Qt::WA_DontShowOnScreen);
        m_shell->ensurePolished();
    }
}

void ContentBridge::update()
{
    if (!m_shell)
        return;
    if (m_shell->layout())
        m_shell->layout()->activate();
    const QSize sz = m_shell->size();
    if (sz.isEmpty())
        return;

    QImage frame(sz, QImage::Format_RGB32);
    frame.fill(Qt::black);
    QPainter painter(&frame);
    m_shell->render(&painter);   // the spec's QWidget::render fallback path

    // Composite registered popups (menus) on top, at their position relative to
    // the shell, so they're part of the texture the CRT samples.
    for (QWidget* popup : std::as_const(m_popups)) {
        if (!popup || !popup->isVisible())
            continue;
        // Popups are top-level (not shell children), so convert from global
        // coordinates via the shell's own top-level position.
        const QPoint rel = popup->pos() - m_shell->pos();
        painter.save();
        painter.translate(rel);
        popup->render(&painter);
        painter.restore();
    }
    painter.end();

    m_frame = frame;
    ++m_frameSerial;
    emit frameChanged();
}

void ContentBridge::registerPopup(QWidget* popup)
{
    if (!popup || m_popups.contains(popup))
        return;

    // Wayland has no soft-disable for popup grabbing: a QMenu whose transient
    // parent has no native surface (our shell is WA_DontShowOnScreen) fails to
    // map as a grabbing popup, so isVisible() reverts to false once events are
    // dispatched and the visibility guard above would skip compositing it.
    // Force the popup into the spec's non-native-menu fallback (policy 2 in the
    // design spec, "render menus as non-native widgets"): it never maps to a
    // native window, stays visible, and QWidget::render() captures it reliably
    // on every platform. The menu is drawn only into the composited CRT texture,
    // never as a stray native window above the scene.
    if (!popup->testAttribute(Qt::WA_DontShowOnScreen))
        popup->setAttribute(Qt::WA_DontShowOnScreen);
    if (!popup->isVisible())
        popup->show();

    m_popups.append(popup);
}

void ContentBridge::unregisterPopup(QWidget* popup)
{
    m_popups.removeAll(popup);
}

void ContentBridge::resizeShell(int w, int h)
{
    if (m_shell && w > 0 && h > 0 && m_shell->size() != QSize(w, h))
        m_shell->resize(w, h);
}

void ContentBridge::setSourceItem(QQuickItem* item)
{
    if (m_sourceItem == item)
        return;
    m_sourceItem = item;
    emit sourceItemChanged();
}

QImage BridgeImageProvider::requestImage(const QString& id, QSize* size,
                                         const QSize& requestedSize)
{
    QImage img = m_bridge ? m_bridge->frame() : QImage();
    if (size)
        *size = img.size();
    if (requestedSize.isValid() && !img.isNull())
        img = img.scaled(requestedSize, Qt::IgnoreAspectRatio);
    return img;
}