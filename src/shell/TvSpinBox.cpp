// SPDX-License-Identifier: GPL-3.0-or-later
#include "TvSpinBox.h"
#include "TvFrame.h"

#include <QFontMetrics>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPolygon>
#include <QResizeEvent>

namespace {
const QColor kFieldBlue(0x00, 0x00, 0xAA);
const QColor kInk(Qt::white);
const QColor kPlate(0x00, 0xAA, 0x00);
const QColor kPlateDark(0x00, 0x77, 0x00);
const QColor kBlack(Qt::black);
const int kButtonShadow = 3;
const int kCellW = 18;   // stepper column width

// The field's own look. The app-wide QSS would otherwise paint it TV blue with
// the dialog font; this keeps it a field inside the modal's panel.
const char* kFieldQss =
    "QLineEdit { background-color: #0000AA; color: #FFFFFF;"
    " border: 1px solid #FFFFFF; padding: 0px 3px; }";
} // namespace

TvSpinBox::TvSpinBox(QWidget* parent)
    : QWidget(parent)
{
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);

    m_edit = new QLineEdit(this);
    m_edit->setObjectName(QStringLiteral("tvSpinField"));
    m_edit->setStyleSheet(QString::fromLatin1(kFieldQss));
    m_edit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_edit->setFrame(false);
    connect(m_edit, &QLineEdit::editingFinished, this, &TvSpinBox::commitTyped);
    connect(m_edit, &QLineEdit::returnPressed, this, &TvSpinBox::commitTyped);

    refreshText();
    layoutCells();
}

void TvSpinBox::setRange(double lo, double hi)
{
    if (hi < lo)
        std::swap(lo, hi);
    m_min = lo;
    m_max = hi;
    setValue(m_value);
}

void TvSpinBox::setDecimals(int decimals)
{
    m_decimals = qBound(0, decimals, 6);
    refreshText();
}

void TvSpinBox::setSuffix(const QString& suffix)
{
    m_suffix = suffix;
    refreshText();
}

double TvSpinBox::clamped(double value) const
{
    return qBound(m_min, value, m_max);
}

void TvSpinBox::setValue(double value)
{
    const double next = clamped(value);
    if (qFuzzyCompare(next, m_value) && !m_edit->text().isEmpty())
        return;
    m_value = next;
    refreshText();
    emit valueChanged(m_value);
}

void TvSpinBox::stepUp()
{
    setValue(m_value + m_step);
}

void TvSpinBox::stepDown()
{
    setValue(m_value - m_step);
}

void TvSpinBox::commitTyped()
{
    QString text = m_edit->text();
    if (!m_suffix.isEmpty() && text.endsWith(m_suffix))
        text.chop(m_suffix.size());
    text = text.trimmed();
    bool ok = false;
    const double typed = text.toDouble(&ok);
    if (!ok) {
        refreshText();      // snap back to the value we already hold
        return;
    }
    setValue(typed);        // clamps; no-ops (and stays silent) if unchanged
    refreshText();          // always re-render the clamped form
}

QString TvSpinBox::formatted() const
{
    return QString::number(m_value, 'f', m_decimals) + m_suffix;
}

void TvSpinBox::refreshText()
{
    if (!m_edit)
        return;
    const QString text = formatted();
    if (m_edit->text() != text)
        m_edit->setText(text);
}

QSize TvSpinBox::sizeHint() const
{
    const QFontMetrics fm(font());
    const int textW = fm.horizontalAdvance(formatted()) + 16;
    return QSize(qMax(textW, 8 * fm.horizontalAdvance(QLatin1Char('0'))) + kCellW, fm.height() + 4);
}

void TvSpinBox::layoutCells()
{
    const int cellW = kCellW;
    const int h = height();
    const int half = h / 2;
    m_upRect = QRect(width() - cellW, 0, cellW, half);
    m_downRect = QRect(width() - cellW, half, cellW, h - half);
    m_fieldRect = QRect(0, 0, qMax(0, width() - cellW - 1), h);
    if (m_edit)
        m_edit->setGeometry(m_fieldRect.adjusted(1, 1, -1, -1));
}

void TvSpinBox::resizeEvent(QResizeEvent* ev)
{
    layoutCells();
    QWidget::resizeEvent(ev);
}

void TvSpinBox::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // Stepper cells: shadow first, then the green plate.
    TvFrame::drawPlateShadow(p, m_upRect, kButtonShadow);
    TvFrame::drawPlateShadow(p, m_downRect, kButtonShadow);
    p.fillRect(m_upRect, kPlate);
    p.fillRect(m_downRect, kPlate);
    // No outline: the cells are flat green fields, the ink is the triangle.

    // Painted triangles (the CP437 face has no U+25B2/U+25BC).
    auto triangle = [&p](const QRect& cell, bool up) {
        const QRect r = cell.adjusted(4, 3, -4, -3);
        if (r.width() < 4 || r.height() < 4)
            return;
        QPolygon poly;
        if (up) {
            poly << QPoint(r.center().x(), r.top())
                 << QPoint(r.left(), r.bottom())
                 << QPoint(r.right(), r.bottom());
        } else {
            poly << QPoint(r.center().x(), r.bottom())
                 << QPoint(r.left(), r.top())
                 << QPoint(r.right(), r.top());
        }
        p.setPen(kBlack);
        p.setBrush(kBlack);
        p.drawPolygon(poly);
        p.setBrush(Qt::NoBrush);
    };
    triangle(m_upRect, true);
    triangle(m_downRect, false);

    // Divider between the two cells.
    p.fillRect(QRect(m_upRect.left(), m_upRect.bottom(), m_upRect.width(), 1), kPlateDark);
}

void TvSpinBox::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(ev);
        return;
    }
    const QPoint pos = ev->pos();
    if (m_upRect.contains(pos)) {
        stepUp();
        ev->accept();
        return;
    }
    if (m_downRect.contains(pos)) {
        stepDown();
        ev->accept();
        return;
    }
    QWidget::mousePressEvent(ev);
}
