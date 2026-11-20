// SPDX-License-Identifier: GPL-3.0-or-later
#include "ContentBridge.h"
#include "src/shell/ShellWindow.h"
#include "src/shell/FKeyStatusBar.h"
#include <QPainter>
#include <QLayout>
#include <QMenu>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QMenuBar>
#include "../shell/MenuBar.h"
#include <QKeyEvent>
#include <QApplication>
#include <QFile>
#include <QAction>

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
    QWidget* target = popupAt(shellLocal);
    const QPoint shellGlobal = m_shell->mapToGlobal(shellLocal);
    // A press that MISSES an open popup must still reach the popup: QMenu hides
    // itself when it receives a press outside its rect. This app synthesizes
    // mouse events rather than relying on the popup's mouse grab, so without
    // this the dropdown stayed open when the user clicked the editor.
    if (static_cast<QEvent::Type>(type) == QEvent::MouseButtonPress) {
        for (QWidget* popup : std::as_const(m_popups)) {
            if (!popup || !popup->isVisible())
                continue;
            const QPoint inPopup = popup->mapFromGlobal(shellGlobal);
            if (popup->rect().contains(inPopup))
                continue;               // inside: handled as a normal target
            auto* ev = new QMouseEvent(QEvent::MouseButtonPress, QPointF(inPopup),
                                       QPointF(shellGlobal),
                                       static_cast<Qt::MouseButton>(button),
                                       static_cast<Qt::MouseButtons>(buttons),
                                       static_cast<Qt::KeyboardModifiers>(modifiers));
            QApplication::sendEvent(popup, ev);
            delete ev;
            update();
            // Fall through: the SAME click also goes to whatever is under the
            // cursor, so a click that dismisses the menu still places the caret
            // in the editor (and clicking another title switches menus). The
            // popup is hidden by now, so target resolution skips it.
            break;
        }
    }
    // Resolve the widget under the point ONCE. (An earlier version assigned
    // `target` here and then tested `!target` for the right-click branch, so
    // inside the editor - where childAt is never null - the branch could not
    // run and right-click did nothing.)
    QWidget* hit = target ? target : m_shell->childAt(shellLocal);
    // Editor right-click opens the shell's BorlandMenu context menu. Qt's
    // native context-menu path never runs here: the shell is never mapped and
    // these events are synthesized, so no QContextMenuEvent is ever produced.
    // Raised directly, the same way the menu bar is driven. A press that landed
    // in an open popup is left alone so dropdown dismissal still works.
    if (static_cast<QEvent::Type>(type) == QEvent::MouseButtonPress
        && static_cast<Qt::MouseButton>(button) == Qt::RightButton
        && !target
        && m_shell->isEditorArea(hit)) {
        // Ask the shell for the word under the pointer so the spell section
        // can be built (the resolver is installed by the composition root).
        m_shell->showEditorContextMenu(shellLocal, m_shell->spellWordAt(shellLocal));
        update();
        return;
    }
    target = hit ? hit : m_shell;
    // Dev aid (unset in normal runs): CRT_KATE_MOUSELOG=<file> records what a
    // forwarded press resolved to and where focus went. A click that "does
    // nothing" is almost always a hit-test disagreement, and the shell is never
    // mapped so there is no other way to see it.
    if (static_cast<QEvent::Type>(type) == QEvent::MouseButtonPress
        || static_cast<QEvent::Type>(type) == QEvent::MouseButtonRelease) {
        const QByteArray logPath = qgetenv("CRT_KATE_MOUSELOG");
        if (!logPath.isEmpty()) {
            if (QFile f(QString::fromLocal8Bit(logPath));
                f.open(QIODevice::Append | QIODevice::Text)) {
                // QApplication::focusWidget() is null for a window that was
                // never activated (the shell is WA_DontShowOnScreen), so report
                // the shell's own focus child - that is what keyTarget() uses.
                const QWidget* focused = m_shell->focusWidget();
                f.write(QStringLiteral("%1 shell=%2,%3 button=%4 buttons=%5 hit=%6 focus=%7\n")
                            .arg(static_cast<QEvent::Type>(type) == QEvent::MouseButtonPress
                                     ? QStringLiteral("press")
                                     : QStringLiteral("release"))
                            .arg(shellLocal.x()).arg(shellLocal.y())
                            .arg(button).arg(buttons)
                            .arg(hit ? (hit->objectName().isEmpty()
                                            ? QString::fromLatin1(hit->metaObject()->className())
                                            : hit->objectName())
                                     : QStringLiteral("<null>"))
                            .arg(focused ? (focused->objectName().isEmpty()
                                                ? QString::fromLatin1(focused->metaObject()->className())
                                                : focused->objectName())
                                         : QStringLiteral("<null>"))
                            .toUtf8());
            }
        }
    }
    // Convert through GLOBAL, never mapFrom(parent, pos): a popup is a top-level
    // window that merely happens to have a parent widget. QWidget::mapFrom walks
    // the parent chain subtracting each pos(), so for a status-bar dropdown
    // (parented to StatusSelectors, far from the origin) every event arrived
    // offset by the parent's position - no row was ever hit, which is why only
    // status-bar popups lost their hover highlight. Menu-bar menus are parented
    // at the origin, so the same bug was invisible there.
    const QPoint local = target->mapFromGlobal(shellGlobal);
    const QPoint global = target->mapToGlobal(local);
    auto* ev = new QMouseEvent(static_cast<QEvent::Type>(type),
                               QPointF(local), QPointF(global),
                               static_cast<Qt::MouseButton>(button),
                               static_cast<Qt::MouseButtons>(buttons),
                               static_cast<Qt::KeyboardModifiers>(modifiers));
    QApplication::sendEvent(target, ev);
    delete ev;
    // A press also moves keyboard focus to the widget under the cursor - the
    // synthesized click does not run Qt's automatic focus-on-click, so without
    // this, clicking from the menu into the editor left keys going to whatever
    // held focus before.
    if (static_cast<QEvent::Type>(type) == QEvent::MouseButtonPress) {
        for (QWidget* w2 = target; w2; w2 = w2->parentWidget()) {
            if (w2->focusPolicy() & Qt::ClickFocus) {
                w2->setFocus(Qt::MouseFocusReason);
                break;
            }
        }
    }
    update();   // re-render now so the click shows without waiting the 33ms tick
}

void ContentBridge::forwardKeyEvent(int type, int key, int modifiers,
                                    const QString& text)
{
    if (!m_shell)
        return;
    // An open popup owns the keyboard: its own key handler walks the list with
    // the arrows, pages with PageUp/PageDown, and commits with Return. Keys used
    // to go to keyFocusWidget() unconditionally, so with a dropdown open the
    // arrows still moved the editor's caret behind it.
    QWidget* target = keyTarget();
    if (!target)
        return;
    const auto evType = static_cast<QEvent::Type>(type);

    // Dev aid (unset in normal runs): CRT_KATE_MOUSELOG also records where each
    // keystroke went. "I cannot type in X" is either a key-target problem or a
    // widget problem, and this tells them apart.
    {
        const QByteArray logPath = qgetenv("CRT_KATE_MOUSELOG");
        if (!logPath.isEmpty()) {
            if (QFile f(QString::fromLocal8Bit(logPath));
                f.open(QIODevice::Append | QIODevice::Text)) {
                f.write(QStringLiteral("key type=%1 target=%2 text=%3\n")
                            .arg(type)
                            .arg(target->objectName().isEmpty()
                                     ? QString::fromLatin1(target->metaObject()->className())
                                     : target->objectName())
                            .arg(text)
                            .toUtf8());
            }
        }
    }
    auto* ev = new QKeyEvent(evType, static_cast<Qt::Key>(key),
                             static_cast<Qt::KeyboardModifiers>(modifiers), text);

    QApplication::sendEvent(target, ev);
    delete ev;

    // A dock or modal that owns the keyboard keeps the WHOLE keystroke. Without
    // this the event was also offered to the menu's actions and to the editor's
    // action collection, so with File > Open on screen Ctrl+S saved the
    // document, Ctrl+X cut it and Ctrl+F opened Find behind the dialog. The
    // owner's own widgets (line edits, lists) have already handled the key
    // above; nothing else may see it.
    if (m_shell->keyboardIsOwned()) {
        update();       // the field repaint must reach the CRT texture
        return;
    }

    // With no active window Qt's shortcut map never runs, so dispatch the
    // matching action directly. The menu's own actions win (Ctrl+X/C/V, F5...);
    // otherwise the focused widget's actions get their turn. That second step
    // is what makes arrow keys, Backspace and Delete work at all: KTextEditor
    // binds them to bare-key QAction shortcuts, and its key handler only types
    // text. Modifier chords carry an empty `text`, so this must not be gated on
    // non-empty text.
    //
    // SKIPPED for a popup: while a dropdown is open the keystroke belongs to it
    // (otherwise Ctrl+A inside a menu would also reach the editor's Select All).
    if (evType == QEvent::KeyPress && !qobject_cast<QMenu*>(target)) {
        const QKeySequence seq(static_cast<Qt::Key>(key)
                               | static_cast<Qt::KeyboardModifiers>(modifiers));
        const bool menuFired = dispatchShortcut(key, modifiers);
        const bool editorFired = m_shell->triggerEditorShortcut(seq);
        if (!menuFired && !editorFired)
            dispatchFocusedShortcut(target, key, modifiers);
    }
    update();
}

QWidget* ContentBridge::keyTarget() const
{
    for (QWidget* p : m_popups)
        if (p && p->isVisible())
            return p;
    return m_shell ? m_shell->keyFocusWidget() : nullptr;
}

QWidget* ContentBridge::popupAt(const QPoint& shellLocal) const
{
    if (!m_shell)
        return nullptr;
    const QPoint g = m_shell->mapToGlobal(shellLocal);
    for (QWidget* p : m_popups) {
        if (!p || !p->isVisible())
            continue;
        if (p->rect().contains(p->mapFromGlobal(g)))
            return p;
    }
    return nullptr;
}

void ContentBridge::forwardWheelEvent(int dx, int dy, int buttons, int modifiers,
                                      double x, double y)
{
    if (!m_shell)
        return;
    const QPoint shellLocal{int(x), int(y)};
    QWidget* target = popupAt(shellLocal);
    if (!target)
        target = m_shell->childAt(shellLocal);
    if (!target)
        target = m_shell;
    const QPoint local = target->mapFrom(m_shell, shellLocal);
    const QPoint global = target->mapToGlobal(local);
    QWheelEvent ev(QPointF(local), QPointF(global), QPoint(0, 0), QPoint(dx, dy),
                   static_cast<Qt::MouseButtons>(buttons),
                   static_cast<Qt::KeyboardModifiers>(modifiers),
                   Qt::NoScrollPhase, false);
    QApplication::sendEvent(target, &ev);
    update();
}

// QShortcutMap only fires when the key's owning window is ACTIVE and a widget
// holds focus. Neither is true here: the shell is never mapped, so during a
// real keypress QApplication::activeWindow() is null and no QAction shortcut
// ever matches. Fall back to finding the action ourselves.
bool ContentBridge::dispatchShortcut(int key, int modifiers)
{
    if (!m_shell)
        return false;
    MenuBar* mb = qobject_cast<MenuBar*>(m_shell->menuBar());
    if (!mb)
        return false;
    const QKeySequence seq(static_cast<Qt::Key>(key)
                           | static_cast<Qt::KeyboardModifiers>(modifiers));
    for (QAction* a : mb->findChildren<QAction*>()) {
        if (!a || a->shortcut().isEmpty() || !a->isEnabled())
            continue;
        if (a->shortcut() == seq) {
            a->trigger();
            update();
            return true;
        }
    }
    return false;
}

// Qt resolves a key to an action by walking the widget hierarchy from the focus
// widget upward, and only while the window is ACTIVE. The shell never is, so
// this is the same walk done by hand. Widget-level actions (addAction()) and
// actions owned by a child object (KTextEditor keeps its set in a
// KActionCollection, a child of the view) both count.
bool ContentBridge::dispatchFocusedShortcut(QWidget* target, int key, int modifiers)
{
    if (!target || !m_shell)
        return false;
    const QKeySequence seq(static_cast<Qt::Key>(key)
                           | static_cast<Qt::KeyboardModifiers>(modifiers));
    // Stop BELOW the shell: its actions belong to the menu, which
    // dispatchShortcut() has already had its say on, and scanning the whole
    // window here would also reach the other tabs' views.
    for (QWidget* w = target; w && w != m_shell; w = w->parentWidget()) {
        QList<QAction*> candidates = w->actions();
        candidates += w->findChildren<QAction*>();
        for (QAction* a : std::as_const(candidates)) {
            if (!a || !a->isEnabled())
                continue;
            if (a->shortcuts().contains(seq)) {
                a->trigger();
                return true;
            }
        }
    }
    return false;
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