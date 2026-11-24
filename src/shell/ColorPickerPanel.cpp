// SPDX-License-Identifier: GPL-3.0-or-later
#include "ColorPickerPanel.h"
#include "TvLineEdit.h"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>

#include "HexColorScan.h"

namespace {
// Turbo Vision chrome: the panel is the gray dialog face, the wells are the
// window blue, and every edge is 1 pixel of black or white - never a blend.
const QColor kFace(0xAA, 0xAA, 0xAA);
const QColor kWell(0x00, 0x00, 0xAA);
const QColor kBlack(0x00, 0x00, 0x00);
const QColor kWhite(0xFF, 0xFF, 0xFF);
const QColor kGreen(0x00, 0xAA, 0x00);

const int kPad = 10;
const int kSquare = 150;   // SV square side
const int kStrip = 18;     // hue strip width
const int kChipH = 22;
const int kFieldH = 20;
} // namespace

ColorPickerPanel::ColorPickerPanel(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("colorPickerPanel"));
    // The app-wide QSS paints every QWidget window-blue; the panel must show
    // the dialog face instead, and it paints its own wells.
    setAttribute(Qt::WA_NoSystemBackground, true);

    m_hex = new TvLineEdit(this);
    m_hex->setObjectName(QStringLiteral("colorPickerHex"));
    m_hex->setMaxLength(7);
    m_hex->setPlaceholderText(QStringLiteral("#rrggbb"));
    connect(m_hex, &QLineEdit::textChanged, this, [this](const QString& text) {
        QColor parsed;
        if (!parseHexColor(text, &parsed))
            return;               // junk: leave the model exactly as it was
        if (parsed == m_color)
            return;
        m_color = parsed;
        const QColor hsv = parsed.toHsv();
        m_hue = qBound(0, hsv.hsvHue(), 359);
        m_sat = hsv.hsvSaturation();
        m_val = hsv.value();
        update();
        emit colorChanged(m_color);
    });

    layoutChildren();
}

QRect ColorPickerPanel::svRect() const { return m_sv; }
QRect ColorPickerPanel::hueRect() const { return m_hueRect; }
QRect ColorPickerPanel::previewRect() const { return m_preview; }

void ColorPickerPanel::layoutChildren()
{
    m_sv = QRect(kPad, kPad, kSquare, kSquare);
    m_hueRect = QRect(m_sv.right() + kPad, kPad, kStrip, kSquare);
    m_preview = QRect(kPad, m_sv.bottom() + kPad, kSquare, kChipH);
    m_hexRect = QRect(m_preview.right() + kPad, m_preview.top(),
                      qMax(kStrip, width() - m_preview.right() - 2 * kPad), kFieldH);
    if (m_hex)
        m_hex->setGeometry(m_hexRect);
    setMinimumSize(kPad * 3 + kSquare + kStrip, m_preview.bottom() + kPad + kFieldH);
}

void ColorPickerPanel::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    layoutChildren();
}

QPoint ColorPickerPanel::svMarker() const
{
    const int x = m_sv.left() + (m_sat * (m_sv.width() - 1)) / 255;
    const int y = m_sv.top() + ((255 - m_val) * (m_sv.height() - 1)) / 255;
    return QPoint(x, y);
}

int ColorPickerPanel::hueMarkerY() const
{
    return m_hueRect.top() + (m_hue * (m_hueRect.height() - 1)) / 359;
}

void ColorPickerPanel::setColor(const QColor& c)
{
    if (!c.isValid())
        return;
    m_color = c;
    const QColor hsv = c.toHsv();
    m_hue = qBound(0, hsv.hsvHue(), 359);
    m_sat = hsv.hsvSaturation();
    m_val = hsv.value();
    syncFieldAndEmit(true);
    update();
}

void ColorPickerPanel::syncFieldAndEmit(bool emitChange)
{
    if (m_hex) {
        const QString text = m_color.name(QColor::HexRgb);   // "#rrggbb"
        if (m_hex->text().compare(text, Qt::CaseInsensitive) != 0) {
            const QSignalBlocker block(m_hex);
            m_hex->setText(text);
        }
    }
    if (emitChange)
        emit colorChanged(m_color);
}

void ColorPickerPanel::applySvAt(const QPoint& p)
{
    const int x = qBound(m_sv.left(), p.x(), m_sv.right());
    const int y = qBound(m_sv.top(), p.y(), m_sv.bottom());
    m_sat = ((x - m_sv.left()) * 255) / qMax(1, m_sv.width() - 1);
    m_val = 255 - ((y - m_sv.top()) * 255) / qMax(1, m_sv.height() - 1);
    // toRgb(): QColor equality compares the colour SPEC as well as the
    // components, so a fromHsv() colour never equals the same RGB literal.
    m_color = QColor::fromHsv(m_hue, m_sat, m_val).toRgb();
    syncFieldAndEmit(true);
    update();
}

void ColorPickerPanel::applyHueAt(const QPoint& p)
{
    const int y = qBound(m_hueRect.top(), p.y(), m_hueRect.bottom());
    m_hue = ((y - m_hueRect.top()) * 359) / qMax(1, m_hueRect.height() - 1);
    // toRgb(): QColor equality compares the colour SPEC as well as the
    // components, so a fromHsv() colour never equals the same RGB literal.
    m_color = QColor::fromHsv(m_hue, m_sat, m_val).toRgb();
    syncFieldAndEmit(true);
    update();
}

void ColorPickerPanel::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(ev);
        return;
    }
    if (m_sv.contains(ev->pos())) {
        m_draggingSv = true;
        applySvAt(ev->pos());
    } else if (m_hueRect.contains(ev->pos())) {
        m_draggingHue = true;
        applyHueAt(ev->pos());
    }
}

void ColorPickerPanel::mouseMoveEvent(QMouseEvent* ev)
{
    if (m_draggingSv)
        applySvAt(ev->pos());
    else if (m_draggingHue)
        applyHueAt(ev->pos());
}

void ColorPickerPanel::mouseReleaseEvent(QMouseEvent*)
{
    m_draggingSv = false;
    m_draggingHue = false;
}

void ColorPickerPanel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    // Hard edges everywhere: the CRT pass and the VGA face are both 1-bit, and
    // a blended picker immediately reads as an alien window.
    p.setRenderHint(QPainter::Antialiasing, false);

    p.fillRect(rect(), kFace);

    // --- Saturation/value square -------------------------------------------
    // Left to right: unsaturated -> the current hue; top to bottom: bright ->
    // black. Two linear gradients, which is exactly what the mapping above
    // inverts.
    p.fillRect(m_sv, QColor::fromHsv(m_hue, 255, 255));
    QLinearGradient toWhite(m_sv.topLeft(), m_sv.topRight());
    toWhite.setColorAt(0.0, kWhite);
    toWhite.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillRect(m_sv, toWhite);
    QLinearGradient toBlack(m_sv.topLeft(), m_sv.bottomLeft());
    toBlack.setColorAt(0.0, QColor(0, 0, 0, 0));
    toBlack.setColorAt(1.0, kBlack);
    p.fillRect(m_sv, toBlack);

    // Marker: a hard white square with a black outline, two pixels bigger than
    // the cell grid so it is visible on any colour underneath.
    const QPoint m = svMarker();
    const QRect marker(m.x() - 3, m.y() - 3, 7, 7);
    p.setPen(kBlack);
    p.drawRect(marker);
    p.setPen(kWhite);
    p.drawRect(marker.adjusted(1, 1, -1, -1));

    // --- Hue strip ----------------------------------------------------------
    QLinearGradient hues(m_hueRect.topLeft(), m_hueRect.bottomLeft());
    for (int i = 0; i <= 6; ++i)
        hues.setColorAt(i / 6.0, QColor::fromHsv(qMin(359, i * 60), 255, 255));
    p.fillRect(m_hueRect, hues);
    const int hy = hueMarkerY();
    p.setPen(kWhite);
    p.drawLine(m_hueRect.left(), hy, m_hueRect.right(), hy);
    p.setPen(kBlack);
    p.drawLine(m_hueRect.left(), hy + 1, m_hueRect.right(), hy + 1);

    // --- Preview chip -------------------------------------------------------
    // The current colour on the left, the colour as it was when the picker
    // opened on the right, so the change is visible before committing.
    const QRect leftHalf(m_preview.left(), m_preview.top(),
                         m_preview.width() / 2, m_preview.height());
    const QRect rightHalf(leftHalf.right() + 1, m_preview.top(),
                          m_preview.width() - leftHalf.width() - 1, m_preview.height());
    p.fillRect(leftHalf, m_color);
    p.fillRect(rightHalf, m_original.isValid() ? m_original : m_color);
    p.setPen(kBlack);
    p.drawRect(m_preview.adjusted(0, 0, -1, -1));

    // --- Field frame --------------------------------------------------------
    p.setPen(kWell);
    p.drawRect(m_hexRect.adjusted(0, 0, -1, -1));
}

void ColorPickerPanel::setOriginalColor(const QColor& c)
{
    m_original = c;
    update();
}
