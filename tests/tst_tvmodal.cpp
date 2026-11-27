// SPDX-License-Identifier: GPL-3.0-or-later
// House-style modals (2026-11-18): one TvModal owns the Turbo Vision chrome
// every dialog in the app wears - gray panel, black double-line frame with the
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
#include "src/shell/ShellWindow.h"
#include "src/AppIdentity.h"
#include "src/settings/SettingsDialog.h"
#include "src/settings/CrtSettings.h"
#include "src/shell/TvCheckBox.h"
#include "src/shell/TvSpinBox.h"
#include "src/shell/TvTabBar.h"
#include <QLabel>
#include <QStackedWidget>
#include <QLineEdit>

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
    void the_plates_have_no_black_outline();

    // Reported live: the plates never animate. A pressed plate must go
    // brighter green and travel down-right onto its own shadow, as if pushed.
    void pressing_a_plate_moves_it_onto_its_shadow();
    void releasing_a_plate_returns_it_to_its_resting_place();

    // Reported live: "buttons don't return to an unpressed state / color if
    // released". Qt clears the pressed state from QAbstractButton::click() on a
    // release INSIDE the plate and from setDown(false) on a release OUTSIDE -
    // either way the paint must come back. A subclass that swallows the release
    // leaves the plate bright and shifted forever.
    void a_press_and_release_on_a_modal_plate_reverts_it();

    // The Find and Replace dialog needs plates that ACT without closing it
    // (Find Next / Replace / Replace All). addButton() always closes, so the
    // kit gains an action plate.
    void an_action_plate_acts_without_closing_the_modal();

    // About
    void about_shows_the_display_name_and_one_ok_plate();
    void the_about_text_is_not_empty();
    // User request 2026-11-24: the About copy is HIS text, and it is black
    // ink on the grey house panel instead of the editor yellow.
    void the_about_text_is_black_ink_with_the_edition_copy();

    // Settings
    void settings_uses_the_house_chrome();
    void the_two_tabs_switch_pages();
    void a_checkbox_toggle_reaches_the_settings();
    void cancel_restores_every_setting();
    void ok_keeps_the_changes();
    void escape_cancels_like_the_button();

    // Reported live (2026-11-19) through the find dialog: a modal measured
    // itself with the platform font, not the polished one, and came out
    // short.
    void a_modal_is_measured_with_the_polished_font();
    // 2026-11-21: keyboard ownership is now claimed by TvModal itself on Show
    // and released on Hide (not only by exec()'s round loop), because tests
    // and dev aids show modals directly and a keystroke then leaked to the
    // editor behind the dialog.
    void showing_a_modal_directly_claims_the_keyboard_and_hiding_releases_it();
};

// The chrome the file dock established, on a bare modal: gray panel inside a
// BLACK double frame (2026-11-26, Xadium: white trim and titles are illegible
// with the CRT effects on), a hard black shadow that reaches the widget edge
// on the right and bottom, a clear gutter above that shadow, and a closed
// corner.
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

    QVERIFY2(countColorIn(img, QColor(Qt::black), 20, QRect(f.left(), f.top(), f.width(), ch)) > 100,
             "the top row carries no black frame ink");
    QVERIFY2(countColorIn(img, QColor(Qt::black), 20,
                          QRect(f.left(), f.bottom() - ch + 1, f.width(), ch)) > 100,
             "the bottom row carries no black frame ink");
    QVERIFY2(countColorIn(img, QColor(Qt::black), 20, QRect(f.left(), f.top(), cw, f.height())) > 100,
             "the left column carries no black frame ink");
    QVERIFY2(countColorIn(img, QColor(Qt::black), 20,
                          QRect(f.right() - cw + 1, f.top(), cw, f.height())) > 100,
             "the right column carries no black frame ink");
    // ...and white chrome is GONE from the dialog (that was the bug).
    QVERIFY2(countColorIn(img, QColor(Qt::white), 20, QRect(f.left(), f.top(), f.width(), ch)) < 40,
             "the top row still carries white trim");

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
    titled.setTitle(QStringLiteral("About TURBO Editor 1998 Edition"));
    titled.resize(480, 300);
    titled.show();
    QVERIFY(QTest::qWaitForWindowExposed(&titled));
    const QImage b = titled.grab().toImage().convertToFormat(QImage::Format_RGB32);

    QCOMPARE(titled.title(), QStringLiteral("About TURBO Editor 1998 Edition"));

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

// Reported live: "the green buttons have black outlines which they should not".
// A TV plate is a flat green field with a black shadow BEHIND it - the outline
// belongs to the shadow, never to the plate. Probed at the plate's edge ring,
// so label ink in the middle cannot mask (or fake) an outline.
void TstTvModal::the_plates_have_no_black_outline()
{
    TvModal m;
    m.resize(480, 320);
    m.addButton(QStringLiteral("OK"), TvModal::Accept);
    m.addButton(QStringLiteral("Cancel"), TvModal::Reject);
    m.show();
    QVERIFY(QTest::qWaitForWindowExposed(&m));

    const QImage img = m.grab().toImage().convertToFormat(QImage::Format_RGB32);
    for (int i = 0; i < m.buttonCount(); ++i) {
        const QRect b = m.buttonRect(i);
        QVERIFY(!b.isNull());
        const QPoint probes[] = {
            QPoint(b.left(), b.center().y()),
            QPoint(b.right(), b.center().y()),
            QPoint(b.center().x(), b.top()),
            QPoint(b.center().x(), b.bottom()),
        };
        for (const QPoint& pt : probes) {
            const QColor px = img.pixelColor(pt);
            QVERIFY2(!(px.red() < 40 && px.green() < 40 && px.blue() < 40),
                     qPrintable(QStringLiteral("plate %1 edge at %2,%3 is black (%4,%5,%6)")
                                    .arg(i).arg(pt.x()).arg(pt.y())
                                    .arg(px.red()).arg(px.green()).arg(px.blue())));
        }
    }
}

// Reported live: pressing a plate did nothing - no movement, no flash. In Turbo
// Vision a pushed button travels down-right ONTO its own block shadow, which is
// exactly what "pressed" looks like: the shadow disappears under the plate, so
// the button reads as sunk into the panel.
//
// Asserted on PIXELS, not on the widget's geometry: the widget deliberately
// never moves (it owns its shadow band, so a release can never miss it). What
// travels is the painted plate.
void TstTvModal::pressing_a_plate_moves_it_onto_its_shadow()
{
    const int kOffset = 4;   // the plate's block shadow (TvModal's kButtonShadow)
    TvModal m;
    m.resize(480, 320);
    m.addButton(QStringLiteral("OK"), TvModal::Accept);
    m.addButton(QStringLiteral("Cancel"), TvModal::Reject);
    m.show();
    QVERIFY(QTest::qWaitForWindowExposed(&m));

    const QRect home = m.buttonRect(0);
    QVERIFY(!home.isNull());
    auto* plate = m.findChild<QPushButton*>(QStringLiteral("tvModalButton"));
    QVERIFY2(plate, "the modal has no plate widget");

    // Two probes, clear of the centred label ink:
    //  - the shadow band just past the plate's right edge (black at rest),
    //  - the plate's own top-left corner (green at rest).
    const QPoint shadowProbe(home.right() + kOffset / 2, home.top() + 6);
    const QPoint cornerProbe(home.left() + 2, home.top() + 2);

    const QImage idle = m.grab().toImage().convertToFormat(QImage::Format_RGB32);
    QVERIFY2(isBlack(idle.pixelColor(shadowProbe)),
             qPrintable(QStringLiteral("no shadow at %1,%2 to cover - got %3")
                            .arg(shadowProbe.x()).arg(shadowProbe.y())
                            .arg(rgb(idle.pixelColor(shadowProbe)))));
    QVERIFY2(!isPanelGrey(idle.pixelColor(cornerProbe)), "the plate corner is not plate-coloured");

    QTest::mousePress(plate, Qt::LeftButton);
    QCoreApplication::processEvents();

    const QImage down = m.grab().toImage().convertToFormat(QImage::Format_RGB32);

    // The plate has travelled onto its shadow: the band is now plate, not black.
    const QColor covered = down.pixelColor(shadowProbe);
    QVERIFY2(!isBlack(covered),
             "the pressed plate left its shadow showing - it did not travel onto it");
    // ...and it flashes BRIGHTER green than the resting plate (#00AA00).
    QVERIFY2(covered.green() > 0xAA && covered.red() > 0x20,
             qPrintable(QStringLiteral("the pressed plate is not the brighter green: %1")
                            .arg(rgb(covered))));

    // The band it vacated shows the panel again - the plate really moved.
    const QColor vacated = down.pixelColor(cornerProbe);
    QVERIFY2(isPanelGrey(vacated),
             qPrintable(QStringLiteral("the vacated corner is %1, not the panel grey")
                            .arg(rgb(vacated))));
}

void TstTvModal::releasing_a_plate_returns_it_to_its_resting_place()
{
    const int kOffset = 4;
    TvModal m;
    m.resize(480, 320);
    m.addButton(QStringLiteral("OK"), TvModal::Accept);
    m.show();
    QVERIFY(QTest::qWaitForWindowExposed(&m));

    const QRect home = m.buttonRect(0);
    auto* plate = m.findChild<QPushButton*>(QStringLiteral("tvModalButton"));
    QVERIFY(plate);
    const QPoint shadowProbe(home.right() + kOffset / 2, home.top() + 6);

    QTest::mousePress(plate, Qt::LeftButton);
    QCoreApplication::processEvents();
    QVERIFY2(!isBlack(m.grab().toImage().pixelColor(shadowProbe)), "the plate never went down");

    QTest::mouseRelease(plate, Qt::LeftButton);
    QCoreApplication::processEvents();
    const QColor back = m.grab().toImage().pixelColor(shadowProbe);
    QVERIFY2(isBlack(back),
             qPrintable(QStringLiteral("the plate did not come back up - the shadow band is %1")
                            .arg(rgb(back))));
}

void TstTvModal::a_press_and_release_on_a_modal_plate_reverts_it()
{
    const int kOffset = 4;
    TvModal m;
    m.resize(480, 320);
    m.addButton(QStringLiteral("OK"), TvModal::Accept);
    m.show();
    QVERIFY(QTest::qWaitForWindowExposed(&m));

    const QRect home = m.buttonRect(0);
    auto* plate = m.findChild<QPushButton*>(QStringLiteral("tvModalButton"));
    QVERIFY(plate);
    const QPoint shadowProbe(home.right() + kOffset / 2, home.top() + 6);
    const QPoint cornerProbe(home.left() + 2, home.top() + 2);

    QTest::mousePress(plate, Qt::LeftButton);
    QCoreApplication::processEvents();

    const QImage down = m.grab().toImage().convertToFormat(QImage::Format_RGB32);
    QVERIFY2(!isBlack(down.pixelColor(shadowProbe)), "the plate never went down");

    QTest::mouseRelease(plate, Qt::LeftButton);
    QCoreApplication::processEvents();

    const QImage up = m.grab().toImage().convertToFormat(QImage::Format_RGB32);
    const QColor band = up.pixelColor(shadowProbe);
    QVERIFY2(isBlack(band),
             qPrintable(QStringLiteral("the plate is still down after release - shadow band is %1")
                            .arg(rgb(band))));

    const QColor corner = up.pixelColor(cornerProbe);
    QVERIFY2(!(corner.green() > 0xAA && corner.red() > 0x20),
             qPrintable(QStringLiteral("the plate is still shifted after release - its corner is %1")
                            .arg(rgb(corner))));
}

void TstTvModal::an_action_plate_acts_without_closing_the_modal()
{
    TvModal m;
    m.resize(460, 260);
    m.addButton(QStringLiteral("Cancel"), TvModal::Reject);
    QPushButton* action = m.addActionButton(QStringLiteral("Find Next"));
    QVERIFY2(action, "addActionButton returned no plate");
    m.show();
    QVERIFY(QTest::qWaitForWindowExposed(&m));

    auto* closer = m.findChild<QPushButton*>(QStringLiteral("tvModalButton"));
    QVERIFY(closer);

    QSignalSpy finished(&m, &TvModal::finished);
    QSignalSpy acted(action, &QPushButton::clicked);

    QTest::mouseClick(action, Qt::LeftButton);
    QCOMPARE(acted.count(), 1);
    QCOMPARE(finished.count(), 0);   // the action plate does NOT close the modal

    QTest::mouseClick(closer, Qt::LeftButton);
    QCOMPARE(finished.count(), 1);   // ...while a role plate still does
    QCOMPARE(finished.at(0).at(0).toInt(), int(TvModal::Reject));
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

// User request 2026-11-24: the About copy is the user's own writing - a
// Borland-style editor built on KTextEditor (KDE Frameworks) and Qt, with
// cool-retro-term's graphics - and it is BLACK ink on the grey house panel.
// Measured in PIXELS: a stylesheet colour that never painted would leave the
// palette right and the screen yellow, which is exactly the report.
void TstTvModal::the_about_text_is_black_ink_with_the_edition_copy()
{
    AboutModal m;
    m.resize(640, 360);
    m.show();
    QVERIFY(QTest::qWaitForWindowExposed(&m));

    auto* label = m.findChild<QLabel*>(QStringLiteral("tvAboutText"));
    QVERIFY(label);

    // The copy itself. Whitespace is normalised first: the label word-wraps, so
    // the source line breaks are layout, not content.
    const QString flat = label->text().simplified();
    QVERIFY2(flat.contains(QStringLiteral("Borland-style text editor")),
             qPrintable(label->text()));
    QVERIFY2(flat.contains(QStringLiteral("built on KTextEditor")),
             qPrintable(label->text()));
    QVERIFY2(flat.contains(QStringLiteral("leveraging graphical effects from "
                                          "cool-retro-term")),
             qPrintable(label->text()));
    QVERIFY2(flat.contains(QStringLiteral("With love to the early days of the "
                                          "internet, when anything was possible. "
                                          "We need to get there again.")),
             qPrintable(label->text()));
    QVERIFY2(flat.contains(QStringLiteral("Dr. Xadium")), qPrintable(label->text()));

    // Ink, in pixels inside the label's own rect.
    const QImage img = m.grab().toImage().convertToFormat(QImage::Format_RGB32);
    const QRect r = label->geometry().intersected(img.rect());
    QVERIFY2(!r.isEmpty(), "the about label has no geometry");
    int black = 0, yellow = 0;
    for (int y = r.top(); y <= r.bottom(); ++y)
        for (int x = r.left(); x <= r.right(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (c.red() < 40 && c.green() < 40 && c.blue() < 40)
                ++black;
            if (qAbs(c.red() - 0xFF) < 12 && qAbs(c.green() - 0xFF) < 12
                && qAbs(c.blue() - 0x55) < 16)
                ++yellow;
        }
    QVERIFY2(black > 30,
             qPrintable(QStringLiteral("the about text paints only %1 black px").arg(black)));
    QVERIFY2(yellow == 0,
             qPrintable(QStringLiteral("the about text still paints %1 yellow px").arg(yellow)));
}

// --- Settings ----------------------------------------------------------------

void TstTvModal::settings_uses_the_house_chrome()
{
    CrtSettings s;
    SettingsDialog dlg(&s);
    QVERIFY2(dlg.title().contains(AppIdentity::displayName()),
             qPrintable(QStringLiteral("settings title is %1").arg(dlg.title())));
    QCOMPARE(dlg.buttonCount(), 2);
    QVERIFY2(dlg.frameRect().width() > 0, "the settings modal has no frame");
}

void TstTvModal::the_two_tabs_switch_pages()
{
    CrtSettings s;
    SettingsDialog dlg(&s);

    auto* tabs = dlg.findChild<TvTabBar*>();
    QVERIFY2(tabs, "no TvTabBar in the settings modal");
    QCOMPARE(tabs->count(), 2);

    auto* stack = dlg.findChild<QStackedWidget*>();
    QVERIFY2(stack, "no page stack in the settings modal");
    QCOMPARE(stack->count(), 2);
    QCOMPARE(stack->currentIndex(), 0);

    tabs->setCurrentIndex(1);
    QCOMPARE(stack->currentIndex(), 1);
    // The second page really carries the Editor controls.
    QVERIFY2(stack->widget(1)->findChild<TvSpinBox*>(QStringLiteral("fontSizeSpin")),
             "the Editor page has no font size control");
}

void TstTvModal::a_checkbox_toggle_reaches_the_settings()
{
    CrtSettings s;
    SettingsDialog dlg(&s);
    auto* scan = dlg.findChild<TvCheckBox*>(QStringLiteral("scanlinesCheck"));
    QVERIFY2(scan, "scanlinesCheck is missing");
    const bool before = s.scanlinesEnabled();
    scan->setChecked(!before);
    QCOMPARE(s.scanlinesEnabled(), !before);
}

// Review Focus 2: Cancel must undo every edit, not just the visible one.
void TstTvModal::cancel_restores_every_setting()
{
    CrtSettings s;
    s.setBrightness(0.7);
    s.setContrast(1.1);
    s.setScanlinesEnabled(true);
    const double b0 = s.brightness();
    const double c0 = s.contrast();

    SettingsDialog dlg(&s);
    auto* bright = dlg.findChild<TvSpinBox*>(QStringLiteral("brightnessSpin"));
    auto* contrast = dlg.findChild<TvSpinBox*>(QStringLiteral("contrastSpin"));
    auto* scan = dlg.findChild<TvCheckBox*>(QStringLiteral("scanlinesCheck"));
    QVERIFY(bright && contrast && scan);

    bright->setValue(0.2);
    contrast->setValue(0.5);
    scan->setChecked(false);

    // Edits apply live (the CRT preview is the point of the dialog) ...
    QCOMPARE(s.brightness(), 0.2);
    QCOMPARE(s.contrast(), 0.5);
    QCOMPARE(s.scanlinesEnabled(), false);

    // ... and Cancel puts all of them back.
    dlg.reject();
    QCOMPARE(s.brightness(), b0);
    QCOMPARE(s.contrast(), c0);
    QCOMPARE(s.scanlinesEnabled(), true);
}

void TstTvModal::ok_keeps_the_changes()
{
    CrtSettings s;
    s.setBrightness(0.7);
    SettingsDialog dlg(&s);
    auto* bright = dlg.findChild<TvSpinBox*>(QStringLiteral("brightnessSpin"));
    QVERIFY(bright);
    bright->setValue(0.25);
    dlg.accept();
    QCOMPARE(s.brightness(), 0.25);
}

// Review Focus 3: Escape while a field has focus must still cancel.
void TstTvModal::escape_cancels_like_the_button()
{
    CrtSettings s;
    s.setBrightness(0.7);
    SettingsDialog dlg(&s);
    dlg.resize(520, 400);
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    auto* bright = dlg.findChild<TvSpinBox*>(QStringLiteral("brightnessSpin"));
    QVERIFY(bright);
    bright->editor()->setFocus();
    bright->setValue(0.15);
    QCOMPARE(s.brightness(), 0.15);

    QTest::keyClick(bright->editor(), Qt::Key_Escape);
    QCOMPARE(s.brightness(), 0.7);
}

// Reported live (2026-11-19): the Find and Replace dialog's option row was
// clipped. Root cause: TvModal measured its chrome with the widget's font
// BEFORE polish, so cellHeight() was the platform font's 12 px while every
// child already wore the app-wide QSS 16 px face - the dialog came out 16 px
// short of the body it had to hold. A modal must measure with the font its
// children actually use.
void TstTvModal::a_modal_is_measured_with_the_polished_font()
{
    // A font size the platform default cannot be mistaken for.
    const QString old = qApp->styleSheet();
    qApp->setStyleSheet(QStringLiteral("QWidget { font-size: 33px; }"));

    TvModal m;
    auto* body = new QWidget;
    body->setObjectName(QStringLiteral("probeBody"));
    auto* line = new QLabel(QStringLiteral("one text line"), body);
    line->adjustSize();
    const int lineH = line->sizeHint().height();
    body->setFixedHeight(3 * lineH);
    m.setBody(body);

    const int childCell = line->fontMetrics().height();
    QVERIFY2(m.cellHeight() == childCell,
             qPrintable(QStringLiteral("the modal measures a %1 px cell but its children use %2")
                            .arg(m.cellHeight()).arg(childCell)));

    // ...and the body therefore gets the height it asked for.
    m.resize(m.sizeHint());
    m.show();
    QVERIFY(QTest::qWaitForWindowExposed(&m));
    QVERIFY2(body->height() >= body->sizeHint().height(),
             qPrintable(QStringLiteral("body got %1 px, needs %2")
                            .arg(body->height()).arg(body->sizeHint().height())));

    qApp->setStyleSheet(old);
}

// Reported 2026-11-21: Find/Replace and Go to Line "pass backspace back to
// the editor". The house modal must claim the shell's keyboard the moment it
// is SHOWN - through exec() or directly (tests and the CRT_KATE_MODAL dev aid
// show modals without the round loop) - and release it on Hide, so
// ContentBridge stops offering keystrokes to the editor behind the dialog.
void TstTvModal::showing_a_modal_directly_claims_the_keyboard_and_hiding_releases_it()
{
    ShellWindow shell;
    shell.resize(800, 600);
    shell.show();
    QCoreApplication::processEvents();
    QVERIFY2(!shell.keyboardIsOwned(), "the shell starts owned?!");

    {
        // Parented to the shell, like production: TvModal::exec reparents the
        // modal to its anchor, and the Show/Hide filter looks the shell up via
        // window() - an unparented modal's window() is ITSELF, so it has no
        // shell to claim (correctly: there is no shell in sight).
        TvModal m(&shell);
        m.resize(320, 200);
        m.show();
        QCoreApplication::processEvents();
        QVERIFY2(shell.keyboardIsOwned(),
                 "showing a modal directly did not claim the keyboard");
        QVERIFY2(shell.keyboardOwner() == &m,
                 "the claimed owner is not the modal itself");

        m.hide();
        QCoreApplication::processEvents();
        QVERIFY2(!shell.keyboardIsOwned(),
                 "hiding the modal left the keyboard claimed");
    }

    // A second modal over the same shell: ownership moves with the visible
    // modal and clears when it goes, so a stack of dialogs cannot strand the
    // owner pointer on a dead widget.
    {
        TvModal second(&shell);
        second.resize(320, 200);
        second.show();
        QCoreApplication::processEvents();
        QVERIFY(shell.keyboardOwner() == &second);
        second.hide();
        QCoreApplication::processEvents();
        QVERIFY(!shell.keyboardIsOwned());
    }
}

QTEST_MAIN(TstTvModal)
#include "tst_tvmodal.moc"
