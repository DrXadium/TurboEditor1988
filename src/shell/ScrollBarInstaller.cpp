// SPDX-License-Identifier: GPL-3.0-or-later
#include "ScrollBarInstaller.h"
#include "TextScrollBar.h"
#include <QEvent>
#include <QLayout>
#include <QPointer>
#include <QScrollBar>
#include <QTimer>

ScrollBarInstaller::ScrollBarInstaller(QObject* parent) : QObject(parent)
{
}

void ScrollBarInstaller::link(QScrollBar* source, TextScrollBar* proxy)
{
    if (!source || !proxy)
        return;

    // Value, both ways. setValue() with an unchanged value emits nothing, so
    // the two connections converge instead of ping-ponging.
    QObject::connect(proxy, &QScrollBar::valueChanged, source,
                     [source](int v) {
                         if (source->value() != v)
                             source->setValue(v);
                     });
    QObject::connect(source, &QScrollBar::valueChanged, proxy,
                     [proxy](int v) {
                         if (proxy->value() != v)
                             proxy->setValue(v);
                     });

    // Range / page step, both ways: these set the thumb length and where the
    // arrows can reach, so a growing document must propagate.
    QObject::connect(proxy, &QScrollBar::rangeChanged, source,
                     [source](int mn, int mx) {
                         if (source->minimum() != mn || source->maximum() != mx)
                             source->setRange(mn, mx);
                     });
    QObject::connect(source, &QScrollBar::rangeChanged, proxy,
                     [proxy](int mn, int mx) {
                         if (proxy->minimum() != mn || proxy->maximum() != mx)
                             proxy->setRange(mn, mx);
                     });

    QObject::connect(proxy, &QScrollBar::actionTriggered, source,
                     [source](int action) { source->triggerAction(
                         static_cast<QAbstractSlider::SliderAction>(action)); });
}

bool ScrollBarInstaller::eventFilter(QObject* watched, QEvent* ev)
{
    // Swap only on Show, and DEFER the actual widget swap to the next
    // event-loop turn (QTimer::singleShot(0,...)) — swapping a widget while
    // its own Show event is being dispatched (KTextEditor/QScrollArea are mid
    // bookkeeping) segfaults. Running later is safe.
    const auto t = ev->type();
    if (t != QEvent::Show)
        return QObject::eventFilter(watched, ev);

    auto* sb = qobject_cast<QScrollBar*>(watched);
    if (!sb || qobject_cast<TextScrollBar*>(sb))
        return QObject::eventFilter(watched, ev);
    if (sb->property("textScrollBarDone").toBool())
        return QObject::eventFilter(watched, ev);

    // Capture state now; perform the swap on a queued invocation.
    QPointer<QScrollBar> guard(sb);
    QTimer::singleShot(0, sb, [guard]() {
        if (!guard)
            return;   // original already gone
        QScrollBar* oldSb = guard.data();
        oldSb->setProperty("textScrollBarDone", true);

        auto* parent = oldSb->parentWidget();
        const auto orient = oldSb->orientation();
        const auto geom = oldSb->geometry();
        const int mn = oldSb->minimum(), mx = oldSb->maximum(),
                  val = oldSb->value(), page = oldSb->pageStep();
        const auto sp = oldSb->sizePolicy();
        const QString objName = oldSb->objectName();

        auto* text = new TextScrollBar(orient, parent);
        text->setObjectName(objName);
        text->setRange(mn, mx);
        text->setPageStep(page);
        text->setValue(val);
        text->setSizePolicy(sp);

        if (parent) {
            if (QLayout* lay = parent->layout()) {
                if (QLayoutItem* oldItem = lay->replaceWidget(oldSb, text)) {
                    delete oldItem;
                } else {
                    text->setGeometry(geom);
                }
            } else {
                text->setGeometry(geom);
            }
            oldSb->setParent(nullptr);
        } else {
            text->setGeometry(geom);
        }
        text->show();

        // Keep the real scrollbar ALIVE as the state owner: KTextEditor holds
        // connections to it that drive the actual view scrolling. Deleting it
        // (as this used to) left the painted bar inert - dragging it scrolled
        // nothing, and scrolling the view never moved it.
        oldSb->hide();
        oldSb->setParent(parent ? static_cast<QWidget*>(parent) : nullptr);
        ScrollBarInstaller::link(oldSb, text);
    });
    return QObject::eventFilter(watched, ev);
}
