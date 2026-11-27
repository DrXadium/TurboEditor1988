// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QString>
#include <QVector>

// A Turbo Vision select: a read-only blue field showing the current item, with
// the file dock's green drop cell against its right edge. Clicking the cell (or
// the field) opens a BorlandMenu; choosing a row changes the value. There is no
// editable text - this is a select, not a combo box.
class TvSelect : public QWidget
{
    Q_OBJECT
public:
    explicit TvSelect(QWidget* parent = nullptr);

    void addItem(const QString& label, const QString& value = QString());
    void clear();

    int count() const { return m_items.size(); }
    int currentIndex() const { return m_index; }
    QString currentText() const;
    QString currentValue() const;

    void setCurrentIndex(int index);
    void setCurrentValue(const QString& value);

    // Geometry (tests and hit-testing).
    QRect fieldRect() const;
    QRect dropRect() const;

    QSize sizeHint() const override;

signals:
    void currentIndexChanged(int index);
    void currentValueChanged(const QString& value);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    struct Item {
        QString label;
        QString value;
    };

    void showMenu();
    void updateGeometry();

    QVector<Item> m_items;
    int m_index = -1;
    bool m_pressed = false;
    QRect m_fieldRect;
    QRect m_dropRect;
};
