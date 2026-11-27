// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QString>
#include <QList>
#include <QRect>

class QPushButton;

// The house-style Turbo Vision modal: gray panel, black double-line frame with
// the title in the top row, a hard black drop shadow offset down-right, and a
// row of green plates with their own block shadows along the bottom-right.
//
// Like TvFileDock it is an ordinary CHILD of the shell window, so
// ContentBridge's QWidget::render pass composites it into the CRT texture and
// the synthesized mouse/key routing drives it. A child widget has no popup
// grab, so exec() runs a local QEventLoop and quits on finished() - the shell
// is WA_DontShowOnScreen and never becomes an active window, so a native
// QDialog::exec() on it would never return.
//
// The frame and plate painting is shared with TvFileDock through TvFrame.
class TvModal : public QWidget
{
    Q_OBJECT
public:
    enum Role { Reject = 0, Accept = 1 };
    Q_ENUM(Role)

    explicit TvModal(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    QString title() const { return m_title; }

    // The caller's content. Reparented into the modal and given a transparent
    // background so the gray panel shows through (the app-wide QSS otherwise
    // paints every widget TV blue).
    void setBody(QWidget* body);
    QWidget* body() const { return m_body; }

    // Buttons are laid out right to left from the bottom-right corner, so
    // index 0 is the rightmost plate. Accept/Reject close the modal; any other
    // role closes it with that role.
    void addButton(const QString& text, int role);

    // A plate that ACTS instead of closing: laid out exactly like the others
    // (right to left, same width rules) but not wired to accept()/reject(). The
    // caller connects clicked(). Find Next / Replace / Replace All need this -
    // they act on the document and keep the dialog open.
    QPushButton* addActionButton(const QString& text);
    int buttonCount() const { return m_buttons.size(); }
    QRect buttonRect(int index) const;

    // Character cell and chrome geometry (tests and hit-testing).
    int cellWidth() const;
    int cellHeight() const;
    QRect frameRect() const { return m_frameRect; }

    virtual void accept();
    virtual void reject();

    // Show `modal` over `anchor`, run the round trip, return the role. The
    // modal is clamped to the anchor so it can never hang off the edge.
    static int exec(QWidget* anchor, TvModal* modal);

    QSize sizeHint() const override;

signals:
    void finished(int role);

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    // Claims/releases the shell's keyboard on Show/Hide (see .cpp).
    bool eventFilter(QObject* watched, QEvent* ev) override;

private:
    struct Button {
        QPushButton* widget = nullptr;
        int role = Reject;
        QRect rect;
    };

    void layoutChildren();

    QString m_title;
    QWidget* m_body = nullptr;
    QList<Button> m_buttons;
    QRect m_frameRect;
};
