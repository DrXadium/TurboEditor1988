// SPDX-License-Identifier: GPL-3.0-or-later
#include "TvModal.h"
#include "TvFrame.h"

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

// Turbo Vision green plate. The app-wide QSS rule does not reach these, so the
// style is set here, matching the file dock's buttons.
const char* kPlateQss =
    "QPushButton { background-color: #00AA00; color: #FFFFFF;"
    " border: none; padding: 2px 10px; }"
    "QPushButton:hover { background-color: #1FBE1F; }"
    "QPushButton:pressed { background-color: #148A14; }";

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
        m_body->setStyleSheet(QStringLiteral("background: transparent;"));
        m_body->show();
    }
    layoutChildren();
    updateGeometry();
}

void TvModal::addButton(const QString& text, int role)
{
    auto* button = new QPushButton(text, this);
    button->setObjectName(QStringLiteral("tvModalButton"));
    button->setStyleSheet(QString::fromLatin1(kPlateQss));
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
        b.widget->setGeometry(b.rect);
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

    // The plates are child widgets and paint after this pass, so their shadow
    // shows only where the plate does not cover it.
    for (const Button& b : m_buttons)
        if (!b.rect.isNull())
            TvFrame::drawPlateShadow(p, b.rect, kButtonShadow);

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
