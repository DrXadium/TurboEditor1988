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
// The press flash: DOS bright green, like the plates (TvPlate's kPlatePressed).
const QColor kPlatePressed(0x55, 0xFF, 0x55);
const QColor kPlateDark(0x00, 0x77, 0x00);
const QColor kBlack(Qt::black);
const int kButtonShadow = 3;
const int kCellW = 18;   // stepper column width

// The field's own look. The app-wide QSS would otherwise paint it TV blue with
// the dialog font; this keeps it a field inside the modal's panel.
//
// No border: the field is a flat blue field on the gray panel, the same rule
// the green plates follow (their black belongs only to the shadow behind them).
// A white ring here was the "blue boxes should not have white borders" report.
const char* kFieldQss =
    "QLineEdit { background-color: #0000AA; color: #FFFFFF;"
    " border: none; padding: 0px 3px; }";
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

    // Stepper cells: shadow first, then the green plate. Pressed, the cell
    // turns the bright green and travels ONTO that shadow - the same push the
    // plates make (no NEW shadow: the cells already own one at rest).
    auto cellFace = [this](const QRect& cell, Cell which) {
        return (m_pressed == which) ? cell.translated(kButtonShadow, kButtonShadow) : cell;
    };
    TvFrame::drawPlateShadow(p, m_upRect, kButtonShadow);
    TvFrame::drawPlateShadow(p, m_downRect, kButtonShadow);
    p.fillRect(cellFace(m_upRect, Cell::Up),
               m_pressed == Cell::Up ? kPlatePressed : kPlate);
    p.fillRect(cellFace(m_downRect, Cell::Down),
               m_pressed == Cell::Down ? kPlatePressed : kPlate);
    // No outline: the cells are flat green fields, the ink is the triangle.

    // Painted triangles (the CP437 face has no U+25B2/U+25BC).
    //
    // The inset is ADAPTIVE: a fixed +3/-3 (and +4/-4) leaves only 3px of
    // height on a 9px cell - below this guard's own <4 floor - so the arrow
    // silently vanished in the real dialog's 82x18 spin box while still
    // drawing in a taller standalone grab. Scale the inset to the cell so the
    // arrow always keeps at least 4px of height and a usable width.
    auto triangle = [&p](const QRect& cell, bool up) {
        const int insetX = qMax(1, (cell.width() - 6) / 3);
        const int insetY = qMax(1, cell.height() / 6);
        const QRect r = cell.adjusted(insetX, insetY, -insetX, -insetY);
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
    triangle(cellFace(m_upRect, Cell::Up), true);
    triangle(cellFace(m_downRect, Cell::Down), false);

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
    fprintf(stderr, "SPINPRESS pos=%d,%d up=%d,%d down=%d,%d field=%d,%d\n",
            pos.x(), pos.y(), m_upRect.x(), m_upRect.y(), m_downRect.x(), m_downRect.y(),
            m_fieldRect.x(), m_fieldRect.y());
    if (m_upRect.contains(pos)) {
        m_pressed = Cell::Up;
        update();
        stepUp();
        ev->accept();
        return;
    }
    if (m_downRect.contains(pos)) {
        m_pressed = Cell::Down;
        update();
        stepDown();
        ev->accept();
        return;
    }
    QWidget::mousePressEvent(ev);
}

void TvSpinBox::mouseReleaseEvent(QMouseEvent* ev)
{
    // Any release reverts the highlight - the cell pops back up even if the
    // pointer has left it, so a cell can never stay lit.
    if (m_pressed != Cell::None) {
        m_pressed = Cell::None;
        update();
        ev->accept();
        return;
    }
    QWidget::mouseReleaseEvent(ev);
}
