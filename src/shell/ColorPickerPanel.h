// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QColor>
#include <QRect>
#include <QWidget>

class TvLineEdit;

// Full-range colour picker, as the body of the house modal (2026-11-23).
//
// Xadium: "full range since it may look retro but we'll be doing modern work" -
// so the model is HSV with a saturation/value square and a hue strip, not a
// palette grid, and a hex field for exact entry.
//
// The widget is a CHILD of the shell (hosted by TvModal), so ContentBridge's
// render pass composites it into the CRT texture and the synthesized mouse
// routing drives it. It never opens a window of its own (design doc
// 2026-11-23-color-swatch-picker-design.md).
//
// Colour bookkeeping: `color()` returns the EXACT colour that was set or picked.
// HSV is kept only to place the markers, because RGB->HSV->RGB is lossy in
// integer arithmetic and a picker that silently shifts a colour by one is a
// picker that rewrites your document wrong.
class ColorPickerPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ColorPickerPanel(QWidget* parent = nullptr);

    QColor color() const { return m_color; }
    // Seeds the HSV markers, the hex field and the preview. Emits colorChanged.
    void setColor(const QColor& c);
    // The colour the picker OPENED with: the preview chip shows it beside the
    // candidate so the change is visible before it is committed. Does not emit.
    void setOriginalColor(const QColor& c);
    QColor originalColor() const { return m_original; }

    // Geometry hooks: the painter, the hit-testing and the tests all use these,
    // so a click can never land on the wrong part of the picker.
    QRect svRect() const;
    QRect hueRect() const;
    QRect previewRect() const;
    TvLineEdit* hexField() const { return m_hex; }

signals:
    void colorChanged(const QColor& c);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;

private:
    void layoutChildren();
    // Marker position in the SV square for the current saturation/value.
    QPoint svMarker() const;
    // Marker position on the hue strip for the current hue.
    int hueMarkerY() const;
    void applySvAt(const QPoint& p);
    void applyHueAt(const QPoint& p);
    void syncFieldAndEmit(bool emitChange);

    QColor m_color = QColor(0xff, 0xff, 0xff);
    QColor m_original;
    int m_hue = 0;      // 0..359
    int m_sat = 0;      // 0..255
    int m_val = 255;    // 0..255
    QRect m_sv;
    QRect m_hueRect;
    QRect m_preview;
    QRect m_hexRect;
    TvLineEdit* m_hex = nullptr;
    bool m_draggingSv = false;
    bool m_draggingHue = false;
};
