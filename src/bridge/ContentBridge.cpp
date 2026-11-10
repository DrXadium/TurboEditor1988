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
    painter.end();

    m_frame = frame;
    ++m_frameSerial;
    emit frameChanged();
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