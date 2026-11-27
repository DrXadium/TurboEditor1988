// SPDX-License-Identifier: GPL-3.0-or-later
#include "TvSelect.h"
#include "BorlandMenu.h"
#include "TvFrame.h"

#include <QAction>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPolygon>
#include <QResizeEvent>

namespace {
const QColor kFieldBlue(0x00, 0x00, 0xAA);
const QColor kInk(Qt::white);
const QColor kPlate(0x00, 0xAA, 0x00);
const QColor kPlatePressed(0x55, 0xFF, 0x55);
const QColor kBlack(Qt::black);
const int kButtonShadow = 3;
const int kCellW = 18;
} // namespace

TvSelect::TvSelect(QWidget* parent) : QWidget(parent)
{
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(1, 1);
    updateGeometry();
}

void TvSelect::addItem(const QString& label, const QString& value)
{
    m_items.append({label, value.isEmpty() ? label : value});
    if (m_index < 0) {
        m_index = 0;
        emit currentIndexChanged(m_index);
        emit currentValueChanged(currentValue());
    }
    update();
}

void TvSelect::clear()
{
    if (m_items.isEmpty() && m_index < 0)
        return;
    m_items.clear();
    m_index = -1;
    update();
    emit currentIndexChanged(-1);
    emit currentValueChanged(QString());
}

QString TvSelect::currentText() const
{
    return (m_index >= 0 && m_index < m_items.size()) ? m_items.at(m_index).label
                                                       : QString();
}

QString TvSelect::currentValue() const
{
    return (m_index >= 0 && m_index < m_items.size()) ? m_items.at(m_index).value
                                                       : QString();
}

void TvSelect::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_items.size())
        index = -1;
    if (m_index == index)
        return;
    m_index = index;
    update();
    emit currentIndexChanged(m_index);
    emit currentValueChanged(currentValue());
}

void TvSelect::setCurrentValue(const QString& value)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).value == value) {
            setCurrentIndex(i);
            return;
        }
    }
    setCurrentIndex(-1);
}

QRect TvSelect::fieldRect() const
{
    return m_fieldRect;
}

QRect TvSelect::dropRect() const
{
    return m_dropRect;
}

QSize TvSelect::sizeHint() const
{
    const QFontMetrics fm(font());
    int textW = 0;
    for (const Item& item : m_items)
        textW = qMax(textW, fm.horizontalAdvance(item.label));
    return QSize(qMax(textW + 16, 12 * fm.horizontalAdvance(QLatin1Char('0'))) + kCellW,
                 fm.height() + 4);
}

void TvSelect::updateGeometry()
{
    const int w = width();
    const int h = height();
    m_dropRect = QRect(qMax(0, w - kCellW), 0, kCellW, h);
    m_fieldRect = QRect(0, 0, qMax(0, w - kCellW - 1), h);
}

void TvSelect::resizeEvent(QResizeEvent* ev)
{
    updateGeometry();
    QWidget::resizeEvent(ev);
}

void TvSelect::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    p.fillRect(m_fieldRect, kFieldBlue);

    QFont inkFont = font();
    inkFont.setStyleStrategy(QFont::NoAntialias);
    p.setFont(inkFont);
    p.setPen(kInk);
    p.drawText(m_fieldRect.adjusted(3, 0, -3, 0),
               Qt::AlignLeft | Qt::AlignVCenter, currentText());

    // Drop cell: block shadow, then the green face (bright and shifted onto the
    // shadow while held), then the black triangle - the file dock's treatment.
    TvFrame::drawPlateShadow(p, m_dropRect, kButtonShadow);
    const QRect face = m_pressed ? m_dropRect.translated(kButtonShadow, kButtonShadow)
                                 : m_dropRect;
    p.fillRect(face, m_pressed ? kPlatePressed : kPlate);
    const QPoint c = face.center();
    QPolygon tri;
    tri << QPoint(c.x() - 3, c.y() - 2) << QPoint(c.x() + 3, c.y() - 2)
        << QPoint(c.x(), c.y() + 2);
    p.save();
    p.setPen(kBlack);
    p.setBrush(kBlack);
    p.drawPolygon(tri);
    p.restore();
}

void TvSelect::showMenu()
{
    if (m_items.isEmpty())
        return;

    auto* menu = new BorlandMenu(this);
    for (int i = 0; i < m_items.size(); ++i) {
        QAction* action = menu->addAction(m_items.at(i).label);
        action->setData(i);
        if (i == m_index)
            action->setCheckable(true), action->setChecked(true);
    }
    connect(menu, &QMenu::triggered, this, [this](QAction* action) {
        if (action)
            setCurrentIndex(action->data().toInt());
    });
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    menu->popup(mapToGlobal(QPoint(0, height())));
}

void TvSelect::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(ev);
        return;
    }
    if (rect().contains(ev->pos())) {
        m_pressed = true;
        update();
        ev->accept();
        return;
    }
    QWidget::mousePressEvent(ev);
}

void TvSelect::mouseReleaseEvent(QMouseEvent* ev)
{
    if (!m_pressed) {
        QWidget::mouseReleaseEvent(ev);
        return;
    }
    m_pressed = false;
    update();
    if (rect().contains(ev->pos()))
        showMenu();
    ev->accept();
}

void TvSelect::keyPressEvent(QKeyEvent* ev)
{
    if (m_items.isEmpty()) {
        QWidget::keyPressEvent(ev);
        return;
    }
    if (ev->key() == Qt::Key_Down || ev->key() == Qt::Key_Right) {
        setCurrentIndex(qMin(m_index + 1, m_items.size() - 1));
        ev->accept();
        return;
    }
    if (ev->key() == Qt::Key_Up || ev->key() == Qt::Key_Left) {
        setCurrentIndex(qMax(m_index - 1, 0));
        ev->accept();
        return;
    }
    if (ev->key() == Qt::Key_Space || ev->key() == Qt::Key_Return
        || ev->key() == Qt::Key_Enter) {
        showMenu();
        ev->accept();
        return;
    }
    QWidget::keyPressEvent(ev);
}
