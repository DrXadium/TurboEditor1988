// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QProxyStyle>

// Borland Turbo-C++-era look: 3D beveled controls, navy desktop. Subclasses
// QProxyStyle (wrapping the base style) so we only override the bits that
// carry the Borland character rather than reimplementing all of QStyle.
class BorlandStyle : public QProxyStyle
{
    Q_OBJECT
public:
    explicit BorlandStyle(QStyle* base = nullptr);
    void drawControl(ControlElement ce, const QStyleOption* opt,
                     QPainter* p, const QWidget* w) const override;
    void drawPrimitive(PrimitiveElement pe, const QStyleOption* opt,
                       QPainter* p, const QWidget* w) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption* option,
                    const QWidget* widget) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption* opt,
                           const QSize& size, const QWidget* w) const override;
    // Horizontal padding each side of a menu-bar title. The OPEN title's green
    // plate must show the same amount of green before the text as after it, so
    // the item has to be SIZED wider, not just drawn further in (drawing alone
    // would clip the label against the plate edge).
    static int menuBarItemPad() { return 4; }
    // Grey gap reserved right of a message-bar action plate so its block
    // shadow never touches the neighbouring plate (2026-11-24 report).
    static int buttonGap() { return 8; }
};
