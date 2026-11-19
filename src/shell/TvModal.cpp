// SPDX-License-Identifier: GPL-3.0-or-later
#include "TvModal.h"
#include "TvFrame.h"
#include "TvPlate.h"

#include <QEventLoop>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QResizeEvent>

namespace {

// The shadow is the panel's own rect offset down-right, so the panel insets by
// the same amount and the gutter around it stays unpainted (the CRT background
// shows through). Same model as TvFileDock - see its paintEvent.
const int kShadowOffset = 6;
const int kButtonShadow = 4;
const int kPad = 4;

const QColor kPanelGray(0xAA, 0xAA, 0xAA);
const QColor kBlack(Qt::black);

// The width a plate actually gets. setGeometry() is clamped by the widget's own
// minimum size, and the app-wide QSS font makes that minimum wider than the
// text alone - which is how the OK plate ended up covering the frame's right
// column. Ask the widget what it needs, then honour it.
int plateWidth(const QPushButton* button, int cellW, const QFontMetrics& fm)
{
    const int wanted = qMax(6 * cellW, fm.horizontalAdvance(button->text()) + 4 * cellW);
    return qMax(wanted, qMax(button->minimumSizeHint().width(), button->minimumSize().width()));
}

} // namespace

TvModal::TvModal(QWidget* parent)
    : QWidget(parent)
{
    setAutoFillBackground(false);   // paintEvent owns the panel
    setFocusPolicy(Qt::StrongFocus);

    // Every metric below - cellWidth(), cellHeight(), sizeHint(), the plate row
    // and the body's rect - comes from fontMetrics(), and a widget only wears
    // the app-wide QSS font (16 px VGA) once it has been polished. Measured
    // before that it reports the platform font (12 px cells), so the modal came
    // out one text line short of the body it had to hold and the body's grid
    // dumped the whole deficit on its only collapsible row: the find dialog's
    // option boxes were 4 px tall instead of 20. Polish here, once.
    ensurePolished();
}

void TvModal::setTitle(const QString& title)
{
    m_title = title;
    update();
}

void TvModal::setBody(QWidget* body)
{
    if (m_body == body)
        return;
    if (m_body)
        m_body->deleteLater();
    m_body = body;
    if (m_body) {
        m_body->setParent(this);
        // Every widget gets the TV desktop blue from the app-wide QSS; the
        // body has to let the gray panel through.
        //
        // The selector must be TYPESPECIFIC. A bare `background: transparent;`
        // has no type, so it cascades to every descendant and switches on Qt's
        // QStyleSheetStyle for the whole subtree - which silently changes how
        // the kit's children paint (the stepper triangles vanished behind it
        // and the checkbox frame changed shape) while a widget grabbed on its
        // own still looked correct. Scoping it to the body's own type keeps the
        // transparency on the body and leaves the children on normal painting.
        m_body->setStyleSheet(
            QStringLiteral("QWidget#%1 { background: transparent; }").arg(m_body->objectName()));
        m_body->show();
    }
    layoutChildren();
    updateGeometry();
}

void TvModal::addButton(const QString& text, int role)
{
    auto* button = new TvPlate(text, this);
    button->setObjectName(QStringLiteral("tvModalButton"));
    button->setInk(Qt::white);
    button->setBackdrop(kPanelGray);
    // The plate owns its block shadow and travels onto it when pressed, so this
    // is also the band the widget reserves to the right and below the plate.
    button->setShadowOffset(kButtonShadow);
    button->setCursor(Qt::ArrowCursor);
    connect(button, &QPushButton::clicked, this, [this, role] {
        if (role == Accept)
            accept();
        else
            reject();
    });
    m_buttons.append({button, role, QRect()});
    layoutChildren();
    updateGeometry();
}

QPushButton* TvModal::addActionButton(const QString& text)
{
    auto* button = new TvPlate(text, this);
    // A distinct name: role plates all share "tvModalButton", and tests (plus
    // the dialogs that own action plates) need to tell them apart.
    button->setObjectName(QStringLiteral("tvModalAction"));
    button->setInk(Qt::white);
    button->setBackdrop(kPanelGray);
    button->setShadowOffset(kButtonShadow);
    button->setCursor(Qt::ArrowCursor);
    // No role wiring on purpose: the caller connects clicked(), so the modal
    // stays open and the button can act again.
    m_buttons.append({button, -1, QRect()});
    layoutChildren();
    updateGeometry();
    return button;
}

QRect TvModal::buttonRect(int index) const
{
    if (index < 0 || index >= m_buttons.size())
        return QRect();
    return m_buttons.at(index).rect;
}

int TvModal::cellWidth() const
{
    return fontMetrics().horizontalAdvance(QLatin1Char('M'));
}

int TvModal::cellHeight() const
{
    return fontMetrics().height();
}

void TvModal::accept()
{
    emit finished(Accept);
    hide();
}

void TvModal::reject()
{
    emit finished(Reject);
    hide();
}

QSize TvModal::sizeHint() const
{
    const int cw = cellWidth();
    const int ch = cellHeight();

    QSize bodyHint(40 * cw, 10 * ch);
    if (m_body) {
        bodyHint = m_body->sizeHint().expandedTo(m_body->minimumSize());
        if (!bodyHint.isValid())
            bodyHint = QSize(40 * cw, 10 * ch);
    }

    int buttonsW = 0;
    const QFontMetrics fm = fontMetrics();
    for (const Button& b : m_buttons)
        buttonsW += plateWidth(b.widget, cw, fm) + 2 * cw;

    const int w = qMax(bodyHint.width(), buttonsW) + 2 * (cw + kPad) + kShadowOffset;
    const int h = bodyHint.height()
                  + (m_buttons.isEmpty() ? 0 : ch + 6 + ch)
                  + 2 * (ch + kPad) + kShadowOffset;
    return QSize(qMax(w, 320), qMax(h, 200));
}

void TvModal::layoutChildren()
{
    const int cw = cellWidth();
    const int ch = cellHeight();
    m_frameRect = rect().adjusted(0, 0, -kShadowOffset, -kShadowOffset);

    const QRect inner = m_frameRect.adjusted(cw + kPad, ch + kPad, -(cw + kPad), -(ch + kPad));
    if (inner.width() <= 0 || inner.height() <= 0)
        return;

    const int buttonH = ch + 6;
    const QFontMetrics fm = fontMetrics();
    int x = inner.right();
    for (Button& b : m_buttons) {
        const int w = plateWidth(b.widget, cw, fm);
        b.rect = QRect(x - w + 1, inner.bottom() - buttonH + 1, w, buttonH);
        // The widget carries the plate's shadow band too (it paints the shadow
        // and the press travel itself), so it is offset-sized around the plate.
        b.widget->setGeometry(b.rect.adjusted(0, 0, kButtonShadow, kButtonShadow));
        x = b.rect.left() - 2 * cw;
    }

    if (m_body) {
        const int bottom = m_buttons.isEmpty() ? inner.bottom() : inner.bottom() - buttonH - ch;
        m_body->setGeometry(QRect(inner.left(), inner.top(), inner.width(),
                                  qMax(0, bottom - inner.top() + 1)));
    }
}

void TvModal::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // Shadow first, then the panel over it: only the offset L survives.
    p.fillRect(m_frameRect.translated(kShadowOffset, kShadowOffset), kBlack);
    p.fillRect(m_frameRect, kPanelGray);

    // The plates own their block shadows (TvPlate paints shadow + plate + the
    // press travel), so this pass draws none of them.
    TvFrame::drawDoubleFrame(p, m_frameRect, m_title, cellWidth(), cellHeight());
}

void TvModal::keyPressEvent(QKeyEvent* ev)
{
    if (ev->key() == Qt::Key_Escape) {
        reject();
        return;
    }
    if (ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Enter) {
        accept();
        return;
    }
    QWidget::keyPressEvent(ev);
}

void TvModal::resizeEvent(QResizeEvent* ev)
{
    layoutChildren();
    QWidget::resizeEvent(ev);
}

int TvModal::exec(QWidget* anchor, TvModal* modal)
{
    if (!modal)
        return Reject;
    if (anchor)
        modal->setParent(anchor);

    // Clamp to the anchor: a modal that does not fit is worse than a small one.
    const QSize room = anchor ? anchor->size() - QSize(2 * kShadowOffset + 4, 2 * kShadowOffset + 4)
                              : QSize(760, 560);
    QSize want = modal->sizeHint().expandedTo(modal->minimumSize()).boundedTo(room);
    const QSize floorSize = QSize(320, 200).boundedTo(room);
    want.setWidth(qMax(want.width(), floorSize.width()));
    want.setHeight(qMax(want.height(), floorSize.height()));
    modal->resize(want);
    if (anchor)
        modal->move((anchor->width() - want.width()) / 2, (anchor->height() - want.height()) / 2);

    int result = Reject;
    QEventLoop loop;
    QObject::connect(modal, &TvModal::finished, &loop, [&result, &loop](int role) {
        result = role;
        loop.quit();
    });
    modal->show();
    modal->raise();
    modal->setFocus(Qt::OtherFocusReason);
    loop.exec();
    modal->hide();
    return result;
}
