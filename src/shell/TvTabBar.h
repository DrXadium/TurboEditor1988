// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QString>
#include <QList>
#include <QRect>

// Turbo Vision tab chips: the active chip is the panel's own grey with a white
// frame around it, the inactive ones are field blue with white labels. A
// QTabBar cannot produce that (the style engine owns the chip's palette), and
// QTabWidget drags a whole styled frame in with it, so this is painted.
class TvTabBar : public QWidget
{
    Q_OBJECT
public:
    explicit TvTabBar(QWidget* parent = nullptr);

    void addTab(const QString& text);
    int count() const { return m_tabs.size(); }
    void setCurrentIndex(int index);
    int currentIndex() const { return m_current; }
    QRect tabRect(int index) const;

    QSize sizeHint() const override;

signals:
    void currentChanged(int index);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    int cellW() const;
    int cellH() const;
    void layoutChips();

    QList<QString> m_tabs;
    QList<QRect> m_rects;
    int m_current = -1;
};
