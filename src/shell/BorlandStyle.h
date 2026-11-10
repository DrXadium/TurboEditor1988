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
};
