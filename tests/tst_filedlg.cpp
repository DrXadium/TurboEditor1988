// SPDX-License-Identifier: GPL-3.0-or-later
// Task 1 (2026-11-17 GUI pass): Turbo Vision file dialogs drawn ON the app
// screen (not a separate native popup). Reference: the TV "Save File As"
// dialog - gray panel, white double-line frame, black drop shadow, yellow
// labels, blue entry field, teal Files list box, green OK/Cancel/Help, blue
// path status row.
//
// TDD: these tests are written FIRST and must fail because TvFileDock /
// TvFilePicker do not exist yet.
#include <QtTest>
#include <QApplication>
#include <QImage>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include "src/shell/ShellWindow.h"
#include "src/shell/TvFileDock.h"

namespace {
// Count pixels within tol of want.
int countColor(const QImage& img, const QColor& want, int tol)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (qAbs(c.red() - want.red()) <= tol
                && qAbs(c.green() - want.green()) <= tol
                && qAbs(c.blue() - want.blue()) <= tol)
                ++n;
        }
    return n;
}
} // namespace

class TstFiledlg : public QObject
{
    Q_OBJECT
private slots:
    // The dock must look like the reference TV modal: gray panel, white
    // double-line frame, black drop shadow, teal Files box, green buttons.
    void file_dock_is_a_tv_modal_with_the_reference_panels()
    {
        TvFileDock dock;
        dock.setMode(TvFileDock::Save);
        dock.setDirectory(QStringLiteral("/tmp"));
        dock.resize(560, 400);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));

        const QImage img = dock.grab().toImage().convertToFormat(QImage::Format_RGB32);
        QVERIFY(img.width() > 0);

        // Gray panel background (#AAAAAA) dominates.
        const int gray = countColor(img, QColor(0xAA, 0xAA, 0xAA), 6);
        QVERIFY2(gray > img.width() * img.height() / 4,
                 qPrintable(QStringLiteral("only %1 gray px - panel is not TV gray").arg(gray)));

        // White double-line frame (#FFFFFF) present.
        const int white = countColor(img, QColor(0xFF, 0xFF, 0xFF), 6);
        QVERIFY2(white > 40,
                 qPrintable(QStringLiteral("only %1 white px - no white frame").arg(white)));

        // Black drop shadow (#000000) present.
        const int black = countColor(img, QColor(0x00, 0x00, 0x00), 6);
        QVERIFY2(black > 40,
                 qPrintable(QStringLiteral("only %1 black px - no drop shadow").arg(black)));

        // Teal Files list box (#00AAAA) present.
        const int teal = countColor(img, QColor(0x00, 0xAA, 0xAA), 6);
        QVERIFY2(teal > 200,
                 qPrintable(QStringLiteral("only %1 teal px - no Files list box").arg(teal)));

        // Green OK / Cancel / Help buttons (#00AA00) present.
        const int green = countColor(img, QColor(0x00, 0xAA, 0x00), 6);
        QVERIFY2(green > 100,
                 qPrintable(QStringLiteral("only %1 green px - no green buttons").arg(green)));

        // Yellow labels (#FFFF55) present ("Save file as", "Files").
        const int yellow = countColor(img, QColor(0xFF, 0xFF, 0x55), 6);
        QVERIFY2(yellow > 30,
                 qPrintable(QStringLiteral("only %1 yellow px - no yellow labels").arg(yellow)));

        dock.reject();
    }

    // The Files list mirrors the current directory; selecting a row fills the
    // edit box, and typing a name in the edit box selects the matching row.
    void files_list_and_edit_box_follow_each_other()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        {
            QFile f(dir.filePath(QStringLiteral("alpha.txt")));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("a");
        }
        {
            QFile f(dir.filePath(QStringLiteral("beta.log")));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("b");
        }
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("subdir")));

        TvFileDock dock;
        dock.setDirectory(dir.path());
        QVERIFY2(dock.files().contains(QStringLiteral("alpha.txt")),
                 qPrintable(QStringLiteral("files() missing alpha.txt: %1")
                                .arg(dock.files().join(QStringLiteral(", ")))));
        QVERIFY2(dock.files().contains(QStringLiteral("beta.log")),
                 "files() missing beta.log");
        QVERIFY2(dock.files().contains(QStringLiteral("subdir")),
                 "files() missing subdir");

        // Selecting a row fills the edit box.
        dock.selectFile(QStringLiteral("alpha.txt"));
        QCOMPARE(dock.editText(), QStringLiteral("alpha.txt"));

        // Typing a name selects the matching row.
        dock.setEditText(QStringLiteral("beta.log"));
        QCOMPARE(dock.selectedFile(), QStringLiteral("beta.log"));
    }

    // OK commits the picked path; Cancel discards (empty).
    void ok_commits_cancel_discards()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        {
            QFile f(dir.filePath(QStringLiteral("notes.txt")));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("hi");
        }

        TvFileDock dock;
        dock.setDirectory(dir.path());
        dock.setMode(TvFileDock::Save);
        dock.selectFile(QStringLiteral("notes.txt"));

        QSignalSpy spy(&dock, &TvFileDock::dialogFinished);
        dock.accept();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), dir.filePath(QStringLiteral("notes.txt")));

        TvFileDock dock2;
        dock2.setDirectory(dir.path());
        QSignalSpy spy2(&dock2, &TvFileDock::dialogFinished);
        dock2.reject();
        QCOMPARE(spy2.count(), 1);
        QVERIFY2(spy2.first().at(0).toString().isEmpty(),
                 "Cancel must return an empty path");
    }

    // Turbo Vision list language: unselected names are BLACK on the teal field
    // (a blue-on-teal list reads as a link, not a DOS list box), and the field
    // ends in a teal scroll strip whose thumb is GREEN.
    void list_ink_and_scroll_strip_follow_the_reference()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (const char* name : { "aaa.txt", "bbb.txt", "ccc.txt" }) {
            QFile f(dir.filePath(QLatin1String(name)));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("x");
        }

        TvFileDock dock;
        dock.setDirectory(dir.path());
        dock.resize(560, 400);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        QCoreApplication::processEvents();

        const QRect list = dock.listRect();
        QVERIFY2(list.width() > 40 && list.height() > 40, "Files box has no area");
        const QImage img = dock.grab().toImage().convertToFormat(QImage::Format_RGB32);

        // Black ink inside the list (the file names), excluding the selection
        // plate: the selected row is the only blue-on-teal row.
        int black = 0;
        for (int y = list.top(); y < list.bottom() - 7; ++y)
            for (int x = list.left(); x < list.right(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.red() < 24 && c.green() < 24 && c.blue() < 24)
                    ++black;
            }
        QVERIFY2(black > 20,
                 qPrintable(QStringLiteral("list names are not black ink (%1 black px)")
                                .arg(black)));

        // Green thumb in the bottom scroll strip of the box.
        int thumb = 0;
        for (int y = list.bottom() - 6; y <= list.bottom(); ++y)
            for (int x = list.left(); x < list.right(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.green() > 120 && c.red() < 60 && c.blue() < 60)
                    ++thumb;
            }
        QVERIFY2(thumb > 10,
                 qPrintable(QStringLiteral("no green scroll thumb under the list (%1 px)")
                                .arg(thumb)));

        // The two-column divider: a solid blue rule down the middle of the box.
        // It is what makes the reference's Files field read as two columns.
        int divider = 0;
        const int mid = list.left() + list.width() / 2;
        for (int y = list.top() + 2; y < list.bottom() - 8; ++y)
            for (int x = mid - 2; x <= mid + 3; ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.blue() > 120 && c.red() < 40 && c.green() < 40)
                    ++divider;
            }
        QVERIFY2(divider > 30,
                 qPrintable(QStringLiteral("no column divider in the Files box (%1 px)")
                                .arg(divider)));

        dock.reject();
    }

    // Save mode pre-fills the default extension so the user does not have to
    // remember it.
    void save_dock_offers_the_default_extension()
    {
        TvFileDock dock;
        dock.setMode(TvFileDock::Save);
        dock.setDirectory(QStringLiteral("/tmp"));
        QVERIFY2(!dock.defaultExtension().isEmpty(),
                 "Save mode must offer a default extension");
        dock.reject();
    }

    // The composition root must open OUR dock on the shell, not a native
    // QFileDialog (the whole point: drawn on the app screen).
    void wiring_opens_the_dock_not_qfiledialog()
    {
        ShellWindow shell;
        shell.resize(800, 600);
        shell.show();
        QCoreApplication::processEvents();

        TvFileDock* dock = TvFileDock::openOn(&shell, TvFileDock::Open,
                                              QStringLiteral("Open File"),
                                              QStringLiteral("/tmp"));
        QVERIFY2(dock, "openOn returned no dock");
        QVERIFY2(dock->parentWidget() == &shell, "dock is not parented to the shell");
        QVERIFY2(dock->isVisible(), "dock did not show");
        QVERIFY2(shell.findChild<TvFileDock*>(QStringLiteral("tvFileDock")),
                 "dock not findable under the shell");
        dock->reject();
        QCoreApplication::processEvents();
    }

    // The reference modal casts a hard black block shadow under every green
    // plate (image 2 of the user's report). The buttons are child widgets and
    // paint over the parent's pass, so the shadow is drawn by the dock and only
    // the offset L survives.
    void buttons_cast_the_reference_block_shadow()
    {
        TvFileDock dock;
        dock.setMode(TvFileDock::Save);
        dock.setDirectory(QStringLiteral("/tmp"));
        dock.resize(760, 560);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));

        const QImage img = dock.grab().toImage().convertToFormat(QImage::Format_RGB32);

        for (const QRect& r : {dock.okRect(), dock.cancelRect(), dock.helpRect()}) {
            QVERIFY2(r.isValid(), "button rect is empty");

            const QColor right = img.pixelColor(r.right() + 2, r.center().y());
            QVERIFY2(right.red() < 12 && right.green() < 12 && right.blue() < 12,
                     qPrintable(QStringLiteral("no black shadow right of %1..%2: %3,%4,%5")
                                    .arg(r.left()).arg(r.right())
                                    .arg(right.red()).arg(right.green()).arg(right.blue())));

            const QColor below = img.pixelColor(r.center().x(), r.bottom() + 2);
            QVERIFY2(below.red() < 12 && below.green() < 12 && below.blue() < 12,
                     qPrintable(QStringLiteral("no black shadow under %1..%2: %3,%4,%5")
                                    .arg(r.top()).arg(r.bottom())
                                    .arg(below.red()).arg(below.green()).arg(below.blue())));

            // ...and the plate itself is still green, not swallowed by the shadow.
            const QColor inside = img.pixelColor(r.left() + 6, r.center().y());
            QVERIFY2(qAbs(inside.green() - 0xAA) < 12 && inside.red() < 12,
                     qPrintable(QStringLiteral("plate is not green: %1,%2,%3")
                                    .arg(inside.red()).arg(inside.green()).arg(inside.blue())));
        }
        dock.reject();
    }

    // The modern layout: a typeable path bar on top, the Files box in the
    // middle, then the file-name row (with its recents dropdown) and the
    // file-type row stacked at the bottom - without disturbing the panels.
    void layout_stacks_path_on_top_and_name_type_at_the_bottom()
    {
        TvFileDock dock;
        dock.setDirectory(QStringLiteral("/tmp"));
        dock.resize(760, 560);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));

        QVERIFY2(dock.pathRect().isValid(), "no path bar");
        QVERIFY2(dock.nameRect().isValid(), "no file-name box");
        QVERIFY2(dock.typeRect().isValid(), "no file-type box");

        QVERIFY2(dock.pathRect().top() < dock.listRect().top(),
                 "path bar is not above the Files list");
        QVERIFY2(dock.nameRect().top() > dock.listRect().bottom(),
                 "file-name box is not below the Files list");
        QVERIFY2(dock.typeRect().top() > dock.nameRect().bottom(),
                 "file-type box is not under the file-name box");

        QVERIFY2(!dock.nameRect().intersects(dock.listRect()), "name box overlaps the list");
        QVERIFY2(!dock.typeRect().intersects(dock.listRect()), "type box overlaps the list");
        QVERIFY2(!dock.nameRect().intersects(dock.typeRect()), "name and type rows collide");

        for (const QRect& plate : {dock.okRect(), dock.cancelRect(), dock.helpRect()}) {
            QVERIFY2(!dock.nameRect().intersects(plate), "name box runs into a button");
            QVERIFY2(!dock.typeRect().intersects(plate), "type box runs into a button");
        }

        // The recents cell hangs off the right end of the name box, inside the dock.
        QVERIFY2(dock.nameDropRect().left() > dock.nameRect().right(),
                 "dropdown cell is not right of the name box");
        QVERIFY2(dock.rect().contains(dock.nameDropRect()),
                 "dropdown cell escapes the dock");
        QVERIFY2(dock.rect().contains(dock.typeDropRect()),
                 "type dropdown cell escapes the dock");
        dock.reject();
    }

    // The dock can be asked for 420x300 (openOn clamps to that floor): the new
    // rows must not collide or squeeze the Files box to nothing.
    void layout_survives_the_minimum_dock_size()
    {
        TvFileDock dock;
        dock.setDirectory(QStringLiteral("/tmp"));
        dock.resize(420, 300);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));

        QVERIFY2(dock.pathRect().isValid(), "no path bar at the minimum size");
        QVERIFY2(dock.nameRect().isValid(), "no name box at the minimum size");
        QVERIFY2(dock.typeRect().isValid(), "no type box at the minimum size");
        QVERIFY2(dock.listRect().height() >= 24, "Files box collapsed at the minimum size");
        QVERIFY2(!dock.nameRect().intersects(dock.typeRect()), "rows collide at the minimum size");
        QVERIFY2(!dock.typeRect().intersects(dock.listRect()), "type row eats the list");
        QVERIFY2(!dock.nameRect().intersects(dock.listRect()), "name row eats the list");
        dock.reject();
    }

    // The file-type box starts on "all files" and offers era presets.
    void type_box_defaults_to_all_files()
    {
        TvFileDock dock;
        QCOMPARE(dock.typeText(), QStringLiteral("All files (*)"));
        QVERIFY(!dock.typePresets().isEmpty());
        QCOMPARE(dock.typePresets().first(), QStringLiteral("All files (*)"));
    }

    // Typing a directory jumps the list there; typing a file jumps to its
    // directory AND drops the file name into the name box.
    void path_box_navigates_and_fills_the_name()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("sub")));
        {
            QFile f(dir.filePath(QStringLiteral("sub/alpha.txt")));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("a");
        }
        const QString sub = QFileInfo(dir.filePath(QStringLiteral("sub"))).absoluteFilePath();

        TvFileDock dock;
        dock.setDirectory(dir.path());

        dock.setPathText(sub);
        dock.navigateToTypedPath();
        QCOMPARE(QFileInfo(dock.directory()).canonicalFilePath(),
                 QFileInfo(sub).canonicalFilePath());
        QVERIFY2(dock.files().contains(QStringLiteral("alpha.txt")),
                 "navigating did not re-list the target directory");

        dock.setPathText(QFileInfo(dir.filePath(QStringLiteral("sub/alpha.txt")))
                             .absoluteFilePath());
        dock.navigateToTypedPath();
        QCOMPARE(QFileInfo(dock.directory()).canonicalFilePath(),
                 QFileInfo(sub).canonicalFilePath());
        QCOMPARE(dock.editText(), QStringLiteral("alpha.txt"));
        QCOMPARE(dock.selectedFile(), QStringLiteral("alpha.txt"));
    }

    // Save As into a name that does not exist yet: split it, do not lose it.
    void path_box_accepts_a_not_yet_existing_save_path()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        TvFileDock dock;
        dock.setMode(TvFileDock::Save);
        dock.setDirectory(dir.path());

        dock.setPathText(dir.filePath(QStringLiteral("brand-new.txt")));
        dock.navigateToTypedPath();
        QCOMPARE(QFileInfo(dock.directory()).canonicalFilePath(),
                 QFileInfo(dir.path()).canonicalFilePath());
        QCOMPARE(dock.editText(), QStringLiteral("brand-new.txt"));
    }

    // The era's home shorthand.
    void path_box_expands_a_leading_tilde()
    {
        TvFileDock dock;
        dock.setPathText(QStringLiteral("~"));
        dock.navigateToTypedPath();
        QCOMPARE(QFileInfo(dock.directory()).canonicalFilePath(),
                 QFileInfo(QDir::homePath()).canonicalFilePath());
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstFiledlg tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_filedlg.moc"
