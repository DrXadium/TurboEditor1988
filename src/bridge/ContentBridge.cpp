// SPDX-License-Identifier: GPL-3.0-or-later
#include "ContentBridge.h"
#include "src/shell/ShellWindow.h"
#include "src/shell/FKeyStatusBar.h"
#include <QPainter>
#include <QLayout>
#include <QMenu>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>

ContentBridge::ContentBridge(QObject* parent) : QObject(parent) {}

void ContentBridge::setShellWindow(ShellWindow* shell)
{
    m_shell = shell;
    if (m_shell) {
        // Offscreen: the shell is rendered into frames, never mapped to a
        // real window (the visible window is the QML CRT scene).
        m_shell->setAttribute(Qt::WA_DontShowOnScreen);
        m_shell->ensurePolished();
        // Create the platform window WITHOUT mapping it. QMenuBar's popup
        // path (QMenuBarPrivate::popupAction) reads
        // window()->windowHandle()->screen() when a menu opens; on a shell
        // that was never shown, windowHandle() is null and clicking a menu
        // SIGSEGV'd (null QWindow). winId() forces the unmapped handle so
        // screen() resolves, while WA_DontShowOnScreen keeps it off-screen.
        m_shell->winId();
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
        // Spec 6/8: hard black drop shadow, offset (+2 cols, +1 row):
        // 2 cells wide along the right edge starting one row below the top,
        // 1 row tall along the bottom edge starting 2 cells right of the left
        // edge. Popups are composited here, so the shadow is drawn into the
        // same pass (a widget cannot paint outside its own rect).
        if (popup->inherits("QMenu")) {
            const QFontMetrics fm(popup->font());
            const int cellH = fm.height();
            const int cellW = qMax(1, fm.horizontalAdvance(QLatin1Char('M')));
            const int pw = popup->width();
            const int ph = popup->height();
            painter.fillRect(QRect(pw, cellH, 2 * cellW, ph), Qt::black);
            painter.fillRect(QRect(2 * cellW, ph, pw, cellH), Qt::black);
        }
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
    //
    // QMenu-ONLY: an exec()'d modal dialog (SettingsDialog, QMessageBox) needs
    // a real window to receive input; WA_DontShowOnScreen would leave it visible
    // but uninteractable. Native top-level windows map fine on Wayland and stay
    // isVisible() after show, so they composite without hardening.
    if (auto* menu = qobject_cast<QMenu*>(popup)) {
        // Spec 7: while this dropdown is open the status line shows the
        // highlighted item's hint; closing restores the F-key row.
        connect(menu, &QMenu::hovered, this, [this](QAction* a) {
            if (!a) {
                setStatusHint(QString());
                return;
            }
            const QString hint = a->statusTip().isEmpty()
                ? a->text().remove(QLatin1Char('&'))
                : a->statusTip();
            setStatusHint(hint);
        });
        connect(menu, &QMenu::aboutToHide, this, [this] { setStatusHint(QString()); });
        if (!menu->testAttribute(Qt::WA_DontShowOnScreen))
            menu->setAttribute(Qt::WA_DontShowOnScreen);
        if (!menu->isVisible())
            menu->show();
    }

    m_popups.append(popup);
}

void ContentBridge::unregisterPopup(QWidget* popup)
{
    m_popups.removeAll(popup);
    if (m_popups.isEmpty())
        setStatusHint(QString());
}

void ContentBridge::setStatusHint(const QString& hint)
{
    if (!m_shell)
        return;
    if (auto* bar = m_shell->fkeyStatusBar()) {
        if (hint.isEmpty())
            bar->clearContextHint();
        else
            bar->setContextHint(hint);
    }
}

void ContentBridge::resizeShell(int w, int h)
{
    if (m_shell && w > 0 && h > 0 && m_shell->size() != QSize(w, h))
        m_shell->resize(w, h);
}

void ContentBridge::forwardMouseEvent(int type, int button, int buttons,
                                      int modifiers, double x, double y)
{
    if (!m_shell)
        return;
    // x,y arrive in SHELL-local coords (== QML-local, 1:1). Resolve the child
    // under the point, then convert to THAT child's local frame — a child
    // widget's mousePressEvent hit-tests in its own coordinates, not the
    // shell's. (Posting at the shell makes QMainWindow::event() swallow it, so
    // we must send to the child with child-local coords + matching global.)
    const QPoint shellLocal{int(x), int(y)};
    // Popups are top-level windows, not shell children, so childAt() can never
    // reach them. Resolve visible popups FIRST: a move over an open dropdown
    // must land in the popup (QMenu's hover highlight + the spec-7 status
    // hint depend on it).
    QWidget* target = nullptr;
    const QPoint shellGlobal = m_shell->mapToGlobal(shellLocal);
    for (QWidget* popup : std::as_const(m_popups)) {
        if (!popup || !popup->isVisible())
            continue;
        const QPoint inPopup = popup->mapFromGlobal(shellGlobal);
        if (popup->rect().contains(inPopup)) {
            target = popup;
            break;
        }
    }
    if (!target)
        target = m_shell->childAt(shellLocal);
    if (!target)
        target = m_shell;
    const QPoint local = target->mapFrom(m_shell, shellLocal);
    const QPoint global = target->mapToGlobal(local);
    auto* ev = new QMouseEvent(static_cast<QEvent::Type>(type),
                               QPointF(local), QPointF(global),
                               static_cast<Qt::MouseButton>(button),
                               static_cast<Qt::MouseButtons>(buttons),
                               static_cast<Qt::KeyboardModifiers>(modifiers));
    QApplication::sendEvent(target, ev);
    delete ev;
    update();   // re-render now so the click shows without waiting the 33ms tick
}

void ContentBridge::forwardKeyEvent(int type, int key, int modifiers,
                                    const QString& text)
{
    if (!m_shell)
        return;
    // Keys go to the focused child (editor/line-edit), or the shell if nothing
    // holds focus offscreen. (QWidget::keyPressEvent routes from the focused
    // widget; posting at the shell would be swallowed by QMainWindow::event.)
    QWidget* target = m_shell->focusWidget();
    if (!target)
        target = m_shell;
    auto* ev = new QKeyEvent(static_cast<QEvent::Type>(type),
                             static_cast<Qt::Key>(key),
                             static_cast<Qt::KeyboardModifiers>(modifiers),
                             text);
    QApplication::sendEvent(target, ev);
    delete ev;
    update();
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