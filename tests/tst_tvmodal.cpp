// SPDX-License-Identifier: GPL-3.0-or-later
// House-style modals (2026-11-18): one TvModal owns the Turbo Vision chrome
// every dialog in the app wears - gray panel, white double-line frame with the
// title in the top row, hard black offset shadow, green plates with block
// shadows - and the local event-loop round trip the file dock proved works in
// a shell that is never an active window.
//
// TDD: written before TvModal exists.
#include <QtTest>
#include <QApplication>
#include <QImage>
#include <QSignalSpy>
#include <QTimer>
#include <QWidget>
#include <QPushButton>
#include "src/shell/TvModal.h"
#include "src/shell/AboutModal.h"
#include "src/AppIdentity.h"
#include <QLabel>

namespace {

int countColorIn(const QImage& img, const QColor& want, int tol, const QRect& r)
{
    int n = 0;
    const QRect clip = r.intersected(img.rect());
    for (int y = clip.top(); y <= clip.bottom(); ++y)
        for (int x = clip.left(); x <= clip.right(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (qAbs(c.red() - want.red()) <= tol
                && qAbs(c.green() - want.green()) <= tol
                && qAbs(c.blue() - want.blue()) <= tol)
                ++n;
        }
    return n;
}

bool isPanelGrey(const QColor& c)
{
    return qAbs(c.red() - 0xAA) <= 10 && qAbs(c.green() - 0xAA) <= 10
           && qAbs(c.blue() - 0xAA) <= 10;
}

bool isBlack(const QColor& c)
{
    return c.red() < 16 && c.green() < 16 && c.blue() < 16;
}

QString rgb(const QColor& c)
{
    return QStringLiteral("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue());
}

} // namespace

class TstTvModal : public QObject
{
    Q_OBJECT

private slots:
    void chrome_is_the_house_style();
    void the_title_changes_the_top_row();
    void escape_rejects_and_return_accepts();
    void exec_centres_the_modal_over_its_anchor();
    void a_modal_larger_than_its_anchor_is_clamped();
    void buttons_get_a_block_shadow();
    void the_button_row_stays_inside_the_frame();

    // About
    void about_shows_the_display_name_and_one_ok_plate();
    void the_about_text_is_not_empty();
};

// The chrome the file dock established, on a bare modal: gray panel inside a
// white double frame, a hard black shadow that reaches the widget edge on the
// right and bottom, a clear gutter above that shadow, and a closed corner.
void TstTvModal::chrome_is_the_house_style()
{
    TvModal m;
    m.setTitle(QStringLiteral("House Style"));
    m.resize(480, 300);
    m.show();
    QVERIFY(QTest::qWaitForWindowExposed(&m));

    const QImage img = m.grab().toImage().convertToFormat(QImage::Format_RGB32);
    const QRect f = m.frameRect();
    const int cw = m.cellWidth();
    const int ch = m.cellHeight();
    QVERIFY2(f.width() > 0 && cw > 0 && ch > 0, "the modal has no frame or no cell metrics");

    const QColor panel = img.pixelColor(f.left() + cw + 1, f.center().y());
    QVERIFY2(isPanelGrey(panel), qPrintable(QStringLiteral("panel is %1").arg(rgb(panel))));

    QVERIFY2(countColorIn(img, QColor(Qt::white), 20, QRect(f.left(), f.top(), f.width(), ch)) > 100,
             "the top row carries no white frame ink");
    QVERIFY2(countColorIn(img, QColor(Qt::white), 20,
                          QRect(f.left(), f.bottom() - ch + 1, f.width(), ch)) > 100,
             "the bottom row carries no white frame ink");
    QVERIFY2(countColorIn(img, QColor(Qt::white), 20, QRect(f.left(), f.top(), cw, f.height())) > 100,
             "the left column carries no white frame ink");
    QVERIFY2(countColorIn(img, QColor(Qt::white), 20,
                          QRect(f.right() - cw + 1, f.top(), cw, f.height())) > 100,
             "the right column carries no white frame ink");

    const QColor right = img.pixelColor(f.right() + 3, f.center().y());
    QVERIFY2(isBlack(right),
             qPrintable(QStringLiteral("right shadow is %1 - it must reach the edge").arg(rgb(right))));
    const QColor bottom = img.pixelColor(f.center().x(), f.bottom() + 3);
    QVERIFY2(isBlack(bottom),
             qPrintable(QStringLiteral("bottom shadow is %1 - it must reach the edge").arg(rgb(bottom))));

    const QColor gutter = img.pixelColor(f.right() + 3, f.top() + 2);
    QVERIFY2(!isPanelGrey(gutter),
             qPrintable(QStringLiteral("the gutter above the shadow is panel grey (%1)").arg(rgb(gutter))));

    const QColor corner = img.pixelColor(f.right() + 2, f.bottom() + 2);
    QVERIFY2(isBlack(corner),
             qPrintable(QStringLiteral("the bottom-right corner is %1 - the L is open").arg(rgb(corner))));
}

// A title is not decoration: it must reach the pixels of the top row.
void TstTvModal::the_title_changes_the_top_row()
{
    TvModal blank;
    blank.resize(480, 300);
    blank.show();
    QVERIFY(QTest::qWaitForWindowExposed(&blank));
    const QImage a = blank.grab().toImage().convertToFormat(QImage::Format_RGB32);

    TvModal titled;
    titled.setTitle(QStringLiteral("About TURBO Kate"));
    titled.resize(480, 300);
    titled.show();
    QVERIFY(QTest::qWaitForWindowExposed(&titled));
    const QImage b = titled.grab().toImage().convertToFormat(QImage::Format_RGB32);

    QCOMPARE(titled.title(), QStringLiteral("About TURBO Kate"));

    const QRect topRow(0, titled.frameRect().top(), b.width(), titled.cellHeight());
    int diff = 0;
    for (int y = topRow.top(); y <= topRow.bottom(); ++y)
        for (int x = topRow.left(); x <= topRow.right(); ++x)
            if (a.pixelColor(x, y) != b.pixelColor(x, y))
                ++diff;
    QVERIFY2(diff > 50,
             qPrintable(QStringLiteral("only %1 px of the top row changed with the title").arg(diff)));
}

void TstTvModal::escape_rejects_and_return_accepts()
{
    TvModal m;
    m.resize(320, 200);
    m.show();
    QVERIFY(QTest::qWaitForWindowExposed(&m));

    QSignalSpy spy(&m, &TvModal::finished);
    QTest::keyClick(&m, Qt::Key_Escape);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), int(TvModal::Reject));
    QVERIFY2(!m.isVisible(), "Escape left the modal on screen");

    TvModal n;
    n.resize(320, 200);
    n.show();
    QVERIFY(QTest::qWaitForWindowExposed(&n));
    QSignalSpy spy2(&n, &TvModal::finished);
    QTest::keyClick(&n, Qt::Key_Return);
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(spy2.at(0).at(0).toInt(), int(TvModal::Accept));
    QVERIFY2(!n.isVisible(), "Return left the modal on screen");
}

void TstTvModal::exec_centres_the_modal_over_its_anchor()
{
    QWidget anchor;
    anchor.resize(800, 600);
    anchor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&anchor));

    TvModal m;
    m.setTitle(QStringLiteral("Centred"));
    QRect seen;
    QTimer::singleShot(0, &m, [&m, &seen] {
        seen = m.geometry();
        m.reject();
    });
    const int role = TvModal::exec(&anchor, &m);

    QCOMPARE(role, int(TvModal::Reject));
    QVERIFY2(seen.isValid(), "exec never showed the modal");
    QVERIFY2(qAbs(seen.left() - (anchor.width() - seen.width()) / 2) <= 2,
             qPrintable(QStringLiteral("left margin %1 is not centred in %2")
                            .arg(seen.left()).arg(anchor.width())));
    QVERIFY2(qAbs(seen.top() - (anchor.height() - seen.height()) / 2) <= 2,
             qPrintable(QStringLiteral("top margin %1 is not centred in %2")
                            .arg(seen.top()).arg(anchor.height())));
}

// Review Focus 4: a modal that wants more room than the anchor has must be
// clamped inside it, never pushed off the right or bottom edge.
void TstTvModal::a_modal_larger_than_its_anchor_is_clamped()
{
    QWidget anchor;
    anchor.resize(400, 300);
    anchor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&anchor));

    TvModal m;
    auto* big = new QWidget;
    big->setMinimumSize(900, 800);
    m.setBody(big);

    QRect seen;
    QTimer::singleShot(0, &m, [&m, &seen] {
        seen = m.geometry();
        m.reject();
    });
    TvModal::exec(&anchor, &m);

    QVERIFY2(seen.isValid(), "exec never showed the modal");
    QVERIFY2(seen.width() <= anchor.width(),
             qPrintable(QStringLiteral("modal is %1 wide in a %2 anchor")
                            .arg(seen.width()).arg(anchor.width())));
    QVERIFY2(seen.height() <= anchor.height(),
             qPrintable(QStringLiteral("modal is %1 tall in a %2 anchor")
                            .arg(seen.height()).arg(anchor.height())));
    QVERIFY2(seen.left() >= 0 && seen.top() >= 0,
             qPrintable(QStringLiteral("modal starts at %1,%2 - off the top-left")
                            .arg(seen.left()).arg(seen.top())));
}

void TstTvModal::buttons_get_a_block_shadow()
{
    TvModal m;
    m.resize(480, 320);
    m.addButton(QStringLiteral("OK"), TvModal::Accept);
    m.addButton(QStringLiteral("Cancel"), TvModal::Reject);
    m.show();
    QVERIFY(QTest::qWaitForWindowExposed(&m));

    QCOMPARE(m.buttonCount(), 2);
    const QRect ok = m.buttonRect(0);
    const QRect cancel = m.buttonRect(1);
    QVERIFY2(!ok.isNull() && !cancel.isNull(), "the buttons were never laid out");
    QVERIFY2(ok.left() > cancel.left(), "buttons are not laid out right to left from the corner");

    const QImage img = m.grab().toImage().convertToFormat(QImage::Format_RGB32);
    const QColor shadow = img.pixelColor(ok.right() + 3, ok.center().y());
    QVERIFY2(isBlack(shadow),
             qPrintable(QStringLiteral("the OK plate has no block shadow (%1)").arg(rgb(shadow))));
}

// Caught live: the OK plate covered the frame's right column and the shadow
// gutter. setGeometry() is clamped by the widget's own minimum size, so a
// button that cannot shrink to the width layout computed overflows to the
// right - which is what the app-wide QSS font does to these plates.
void TstTvModal::the_button_row_stays_inside_the_frame()
{
    TvModal m;
    m.addButton(QStringLiteral("OK"), TvModal::Accept);
    m.addButton(QStringLiteral("Cancel"), TvModal::Reject);
    // Force the condition: a plate that cannot shrink to the computed width.
    for (auto* b : m.findChildren<QPushButton*>())
        b->setMinimumWidth(150);

    // A size that is NOT the sizeHint, the way exec() and the dev aid set it.
    m.resize(520, 300);
    m.show();
    QVERIFY(QTest::qWaitForWindowExposed(&m));

    const QRect f = m.frameRect();
    const int cw = m.cellWidth();
    const auto buttons = m.findChildren<QPushButton*>();
    QCOMPARE(buttons.size(), 2);
    for (QPushButton* b : buttons) {
        const QRect g(b->geometry());
        QVERIFY2(g.right() <= f.right() - cw,
                 qPrintable(QStringLiteral("a plate ends at %1, past the frame column at %2")
                                .arg(g.right()).arg(f.right() - cw)));
        QVERIFY2(g.left() >= f.left() + cw,
                 qPrintable(QStringLiteral("a plate starts at %1, left of the frame column at %2")
                                .arg(g.left()).arg(f.left() + cw)));
        QVERIFY2(g.bottom() <= f.bottom() - 1,
                 qPrintable(QStringLiteral("a plate ends at %1, below the frame").arg(g.bottom())));
        QVERIFY2(g.width() >= b->minimumWidth(),
                 "the plate is narrower than its own minimum");
    }
}

// Help > About used to be a host-themed QMessageBox above the CRT scene.
void TstTvModal::about_shows_the_display_name_and_one_ok_plate()
{
    AboutModal m;
    QVERIFY2(m.title().contains(AppIdentity::displayName()),
             qPrintable(QStringLiteral("the title is %1").arg(m.title())));
    QCOMPARE(m.buttonCount(), 1);

    auto* label = m.findChild<QLabel*>(QStringLiteral("tvAboutText"));
    QVERIFY2(label, "the about text is missing");
    QVERIFY2(label->text().contains(AppIdentity::displayName()),
             "the about text does not name the app");
}

void TstTvModal::the_about_text_is_not_empty()
{
    AboutModal m;
    auto* label = m.findChild<QLabel*>(QStringLiteral("tvAboutText"));
    QVERIFY(label);
    QVERIFY2(label->text().trimmed().size() > 40,
             qPrintable(QStringLiteral("the about text is only %1 chars")
                            .arg(label->text().trimmed().size())));
}

QTEST_MAIN(TstTvModal)
#include "tst_tvmodal.moc"
