// SPDX-License-Identifier: GPL-3.0-or-later
// Task 1 (2026-11-17 GUI pass): Turbo Vision file dialogs drawn ON the app
// screen (not a separate native popup). Reference: the TV "Save File As"
// dialog - gray panel, white double-line frame, black drop shadow, yellow
// labels, blue entry field, teal Files list box, green OK/Cancel, blue
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
#include "src/shell/TvLineEdit.h"
#include "src/bridge/ContentBridge.h"
#include <QLineEdit>
#include <QScrollBar>
#include <QWheelEvent>
#include <QApplication>

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

// Same, restricted to a rectangle (the focus ring lives around one field).
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

bool isWhite(const QColor& c)
{
    return c.red() > 240 && c.green() > 240 && c.blue() > 240;
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

        // Green OK / Cancel buttons (#00AA00) present.
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

        for (const QRect& r : {dock.okRect(), dock.cancelRect()}) {
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

        for (const QRect& plate : {dock.okRect(), dock.cancelRect()}) {
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

    // The type box is the listing's filter: a glob narrows it, and the two
    // "everything" idioms (bare "*", DOS "*.*", or an empty box) show all.
    void type_box_filters_the_listing()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (const char* name : {"notes.txt", "readme.md", "Makefile"}) {
            QFile f(dir.filePath(QString::fromLatin1(name)));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("x");
        }

        TvFileDock dock;
        dock.setDirectory(dir.path());
        QVERIFY(dock.files().contains(QStringLiteral("notes.txt")));
        QVERIFY(dock.files().contains(QStringLiteral("readme.md")));

        dock.setTypeText(QStringLiteral("*.md"));
        QVERIFY2(dock.files().contains(QStringLiteral("readme.md")),
                 "*.md hid the markdown file");
        QVERIFY2(!dock.files().contains(QStringLiteral("notes.txt")),
                 "*.md did not hide the text file");
        QCOMPARE(dock.defaultExtension(), QStringLiteral("md"));

        dock.setTypeText(QStringLiteral("All files (*.*)"));
        QVERIFY2(dock.files().contains(QStringLiteral("Makefile")),
                 "the DOS all-files idiom hid an extensionless file");
        QVERIFY(dock.files().contains(QStringLiteral("notes.txt")));

        dock.setTypeText(QString());
        QVERIFY2(dock.files().contains(QStringLiteral("Makefile")),
                 "an empty type box hid files");
        QVERIFY(dock.files().contains(QStringLiteral("readme.md")));
    }

    // Directories are navigation, never filtered away.
    void type_box_never_filters_directories()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("subdir")));
        TvFileDock dock;
        dock.setDirectory(dir.path());
        dock.setTypeText(QStringLiteral("*.md"));
        QVERIFY2(dock.files().contains(QStringLiteral("subdir")),
                 "filtering hid a directory");
        QVERIFY2(dock.files().contains(QStringLiteral("..")),
                 "filtering hid the parent entry");
    }

    // Reported live: the green cells that open the dropdowns must animate like
    // the plates. Press lights the cell brighter and travels it onto its own
    // block shadow; the menu opens on RELEASE, the way a button activates -
    // acting on press meant the flash was never seen at all.
    void pressing_a_drop_cell_lights_it_and_the_menu_waits_for_release()
    {
        TvFileDock dock;
        dock.resize(760, 560);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));

        const QRect cell = dock.nameDropRect();
        QVERIFY2(!cell.isNull(), "the dock has no recents cell");
        const int kOffset = 4;   // the cell's block shadow
        // Just past the cell's right edge, above the centred arrow glyph.
        const QPoint probe(cell.right() + kOffset / 2, cell.top() + 5);

        const QImage idle = dock.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const QColor rest = idle.pixelColor(probe);
        QVERIFY2(rest.red() < 16 && rest.green() < 16 && rest.blue() < 16,
                 qPrintable(QStringLiteral("no block shadow beside the cell - got %1,%2,%3")
                                .arg(rest.red()).arg(rest.green()).arg(rest.blue())));

        QSignalSpy spy(&dock, &TvFileDock::recentsRequested);
        QTest::mousePress(&dock, Qt::LeftButton, Qt::NoModifier, cell.center());
        QCoreApplication::processEvents();
        QCOMPARE(spy.count(), 0);   // the menu waits for the release

        const QImage down = dock.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const QColor lit = down.pixelColor(probe);
        QVERIFY2(!(lit.red() < 16 && lit.green() < 16 && lit.blue() < 16),
                 "the pressed cell left its shadow showing - it did not travel onto it");
        QVERIFY2(lit.green() > 0xAA && lit.red() > 0x20,
                 qPrintable(QStringLiteral("the pressed cell is not the brighter green: "
                                           "%1,%2,%3")
                                .arg(lit.red()).arg(lit.green()).arg(lit.blue())));

        QTest::mouseRelease(&dock, Qt::LeftButton, Qt::NoModifier, cell.center());
        QCOMPARE(spy.count(), 1);
        dock.reject();
    }

    // Reported live: "buttons don't return to an unpressed state / color if
    // released". The hand-fed press/release pair above passes while the real
    // app sticks - clicks arrive as SYNTHESIZED events through ContentBridge,
    // and the dock only sees them if childAt() resolves the plate. Assert the
    // full round trip: press lights the cell, and the release both fires the
    // menu and leaves the cell dark again.
    void a_bridge_click_lights_the_drop_cell_and_reverts_it()
    {
        ShellWindow shell;
        shell.resize(800, 600);
        shell.show();
        QCoreApplication::processEvents();

        ContentBridge bridge;
        bridge.setShellWindow(&shell);
        TvFileDock* dock = TvFileDock::openOn(&shell, TvFileDock::Open,
                                              QStringLiteral("Open File"),
                                              QStringLiteral("/tmp"));
        QVERIFY(dock);
        QCoreApplication::processEvents();

        const QRect cell = dock->nameDropRect();
        QVERIFY2(!cell.isNull(), "the dock has no recents cell");
        const int kOffset = 4;
        const QPoint probe(cell.right() + kOffset / 2, cell.top() + 5);
        const QPoint shellPt = cell.center() + dock->pos();
        const QPoint platePt = shellPt - dock->pos();   // the cell is a painted
        // rect: its centre in shell coordinates, then in dock coordinates.
        Q_UNUSED(platePt);

        QSignalSpy spy(dock, &TvFileDock::recentsRequested);

        bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                 Qt::LeftButton, Qt::NoModifier,
                                 shellPt.x(), shellPt.y());
        QCoreApplication::processEvents();

        const QImage down = dock->grab().toImage().convertToFormat(QImage::Format_RGB32);
        const QColor lit = down.pixelColor(probe);
        QVERIFY2(lit.green() > 0xAA && lit.red() > 0x20,
                 qPrintable(QStringLiteral("the press through the bridge did not light the cell: "
                                           "%1,%2,%3")
                                .arg(lit.red()).arg(lit.green()).arg(lit.blue())));
        QCOMPARE(spy.count(), 0);

        bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton,
                                 Qt::NoButton, Qt::NoModifier,
                                 shellPt.x(), shellPt.y());
        QCoreApplication::processEvents();

        QCOMPARE(spy.count(), 1);

        // THE BUG: after the release the cell must be back at rest - flat
        // green over its black shadow, not still shifted onto it.
        const QImage up = dock->grab().toImage().convertToFormat(QImage::Format_RGB32);
        const QColor rest = up.pixelColor(probe);
        QVERIFY2(rest.red() < 16 && rest.green() < 16 && rest.blue() < 16,
                 qPrintable(QStringLiteral("the cell is still down after release - shadow probe "
                                           "is %1,%2,%3")
                                .arg(rest.red()).arg(rest.green()).arg(rest.blue())));

        const QPoint cellTopLeft = cell.topLeft();
        const QColor corner = up.pixelColor(cellTopLeft + QPoint(2, 2));
        QVERIFY2(!(corner.green() > 0xAA && corner.red() > 0x20),
                 qPrintable(QStringLiteral("the cell is still shifted after release - its corner "
                                           "is %1,%2,%3")
                                .arg(corner.red()).arg(corner.green()).arg(corner.blue())));

        dock->reject();
        QCoreApplication::processEvents();
    }

    // The dropdown keeps at most ten entries and the green cell asks for it.
    void recents_are_capped_at_ten_and_the_cell_requests_the_menu()
    {
        TvFileDock dock;
        dock.resize(760, 560);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));

        QStringList many;
        for (int i = 0; i < 12; ++i)
            many << QStringLiteral("/tmp/f%1.txt").arg(i);
        dock.setRecents(many);
        QCOMPARE(dock.recents().size(), 10);
        QCOMPARE(dock.recents().first(), QStringLiteral("/tmp/f0.txt"));

        QSignalSpy spy(&dock, &TvFileDock::recentsRequested);
        QTest::mouseClick(&dock, Qt::LeftButton, Qt::NoModifier,
                          dock.nameDropRect().center());
        QCOMPARE(spy.count(), 1);
        dock.reject();
    }

    // Picking a recent navigates to it and names it.
    void applying_a_recent_navigates_and_fills_the_name()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString file = dir.filePath(QStringLiteral("picked.txt"));
        {
            QFile f(file);
            QVERIFY(f.open(QIODevice::WriteOnly));
        }

        TvFileDock dock;
        dock.setDirectory(QStringLiteral("/tmp"));
        dock.setRecents({file});
        dock.applyRecent(file);
        QCOMPARE(QFileInfo(dock.directory()).canonicalFilePath(),
                 QFileInfo(dir.path()).canonicalFilePath());
        QCOMPARE(dock.editText(), QStringLiteral("picked.txt"));
        QCOMPARE(dock.selectedFile(), QStringLiteral("picked.txt"));
    }

    // A recent whose file has since been deleted must not take the dock down.
    void applying_a_deleted_recent_does_not_crash()
    {
        TvFileDock dock;
        dock.setDirectory(QStringLiteral("/tmp"));
        dock.applyRecent(QStringLiteral("/definitely/not/here/gone.txt"));
        QCOMPARE(dock.editText(), QStringLiteral("gone.txt"));
    }

    // The type row's green cell asks for the preset list.
    void type_cell_requests_the_preset_menu()
    {
        TvFileDock dock;
        dock.resize(760, 560);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        QSignalSpy spy(&dock, &TvFileDock::typePresetsRequested);
        QTest::mouseClick(&dock, Qt::LeftButton, Qt::NoModifier,
                          dock.typeDropRect().center());
        QCOMPARE(spy.count(), 1);
        dock.reject();
    }

    // Reported live: the drop shadow stopped short of the widget edge (a grey
    // stripe down the right), its corner was missing, and the gutter above it
    // was panel grey instead of clear. The shadow is the panel's own rect
    // offset down-right, with the panel painted over it.
    void the_drop_shadow_is_a_complete_offset_L()
    {
        TvFileDock dock;
        dock.setDirectory(QStringLiteral("/tmp"));
        dock.resize(760, 560);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));

        const QImage img = dock.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const QRect f = dock.frameRect();
        const int midY = (f.top() + f.bottom()) / 2;

        // Right arm: black from the frame's edge out to the widget's edge.
        for (int x = f.right() + 1; x <= dock.width() - 1; ++x) {
            const QColor c = img.pixelColor(x, midY);
            QVERIFY2(c.red() < 12 && c.green() < 12 && c.blue() < 12,
                     qPrintable(QStringLiteral("x=%1 y=%2 is %3,%4,%5 - not shadow")
                                    .arg(x).arg(midY)
                                    .arg(c.red()).arg(c.green()).arg(c.blue())));
        }
        // Bottom arm: same, under the panel.
        const int midX = (f.left() + f.right()) / 2;
        for (int y = f.bottom() + 1; y <= dock.height() - 1; ++y) {
            const QColor c = img.pixelColor(midX, y);
            QVERIFY2(c.red() < 12 && c.green() < 12 && c.blue() < 12,
                     qPrintable(QStringLiteral("x=%1 y=%2 is %3,%4,%5 - not shadow")
                                    .arg(midX).arg(y)
                                    .arg(c.red()).arg(c.green()).arg(c.blue())));
        }
        // ...and the corner where the two arms meet.
        const QColor corner = img.pixelColor(dock.width() - 2, dock.height() - 2);
        QVERIFY2(corner.red() < 12 && corner.green() < 12 && corner.blue() < 12,
                 qPrintable(QStringLiteral("corner is %1,%2,%3 - the shadow L is open")
                                .arg(corner.red()).arg(corner.green()).arg(corner.blue())));
        dock.reject();
    }

    // The gutter outside the shadow is NOT painted: whatever is behind the dock
    // shows through, which is what makes the L read as a shadow. Above the
    // right arm (upper right) and left of the bottom arm (lower left) must stay
    // clear.
    void the_gutter_outside_the_shadow_is_left_clear()
    {
        QWidget host;
        host.resize(900, 700);
        host.setAutoFillBackground(true);
        QPalette pal = host.palette();
        pal.setColor(QPalette::Window, QColor(0xFF, 0x00, 0x00));   // marker colour
        host.setPalette(pal);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));

        auto* dock = new TvFileDock(&host);
        dock->setDirectory(QStringLiteral("/tmp"));
        dock->resize(760, 560);
        dock->move(10, 10);
        dock->show();
        QCoreApplication::processEvents();

        const QImage img = host.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const QRect f = dock->frameRect();

        // Upper right: right of the panel, above where the shadow starts.
        // Whatever the CRT background is there (the app-wide QSS paints widget
        // backgrounds TV blue), it must not be the panel's grey or the frame's
        // white - either would mean the panel bleeds into the shadow gutter.
        const QPoint upperRight(dock->x() + f.right() + 3, dock->y() + f.top() + 2);
        const QColor upper = img.pixelColor(upperRight);
        QVERIFY2(!isPanelGrey(upper) && !isWhite(upper),
                 qPrintable(QStringLiteral("upper-right gutter is %1,%2,%3 - the panel bled into it")
                                .arg(upper.red()).arg(upper.green()).arg(upper.blue())));

        // Lower left: under the panel, left of where the shadow starts.
        const QPoint lowerLeft(dock->x() + f.left() + 2, dock->y() + f.bottom() + 3);
        const QColor lower = img.pixelColor(lowerLeft);
        QVERIFY2(!isPanelGrey(lower) && !isWhite(lower),
                 qPrintable(QStringLiteral("lower-left gutter is %1,%2,%3 - the panel bled into it")
                                .arg(lower.red()).arg(lower.green()).arg(lower.blue())));

        dock->reject();
        delete dock;
    }

    // Refactor guard (house-style round, Task 1): the frame drawing moves into
    // TvFrame::drawDoubleFrame, so pin what it must keep producing. Green
    // before AND after the extraction - a red here means the frame was already
    // broken and the refactor must not start.
    void the_frame_is_a_white_double_line_box_with_a_centred_title()
    {
        TvFileDock dock;
        dock.setTitle(QStringLiteral("Save File As"));
        dock.setDirectory(QStringLiteral("/tmp"));
        dock.resize(760, 560);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));

        const QImage img = dock.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const QRect f = dock.frameRect();
        const int cw = dock.cellWidth();
        const int ch = dock.cellHeight();
        QVERIFY2(cw > 0 && ch > 0, "the dock has no character cell");

        QVERIFY2(countColorIn(img, QColor(Qt::white), 20,
                              QRect(f.left(), f.top(), f.width(), ch)) > 100,
                 "the top row carries no white frame ink");
        QVERIFY2(countColorIn(img, QColor(Qt::white), 20,
                              QRect(f.left(), f.bottom() - ch + 1, f.width(), ch)) > 100,
                 "the bottom row carries no white frame ink");
        QVERIFY2(countColorIn(img, QColor(Qt::white), 20,
                              QRect(f.left(), f.top(), cw, f.height())) > 100,
                 "the left column carries no white frame ink");
        QVERIFY2(countColorIn(img, QColor(Qt::white), 20,
                              QRect(f.right() - cw + 1, f.top(), cw, f.height())) > 100,
                 "the right column carries no white frame ink");

        // The too-small guard inside the frame drawing is unreachable through
        // the dock (it enforces a minimum size), so it is covered directly on
        // TvFrame::drawDoubleFrame in tst_tvkit.
    }

    // Reported live: "the green buttons have black outlines which they should
    // not". A TV plate is a flat green field with the shadow BEHIND it. The
    // probe is the plate's EDGE RING, not its interior: these plates carry
    // black label ink by design, so a whole-rect count would measure the text.
    void the_plates_have_no_black_outline()
    {
        TvFileDock dock;
        dock.setDirectory(QStringLiteral("/tmp"));
        dock.resize(760, 560);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));

        const QImage img = dock.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const QRect plates[] = {dock.okRect(), dock.cancelRect()};
        for (const QRect& p : plates) {
            QVERIFY2(!p.isNull(), "a plate was never laid out");
            const QPoint probes[] = {
                QPoint(p.left(), p.center().y()),
                QPoint(p.right(), p.center().y()),
                QPoint(p.center().x(), p.top()),
                QPoint(p.center().x(), p.bottom()),
            };
            for (const QPoint& pt : probes) {
                const QColor px = img.pixelColor(pt);
                QVERIFY2(!(px.red() < 40 && px.green() < 40 && px.blue() < 40),
                         qPrintable(QStringLiteral("plate edge at %1,%2 is black (%3,%4,%5)")
                                        .arg(pt.x()).arg(pt.y())
                                        .arg(px.red()).arg(px.green()).arg(px.blue())));
            }
        }
    }

    // Reported live: "there's no indication anything worked". The shell window
    // is never active, so a plain QLineEdit never draws a caret - the click has
    // to show itself. Clicking a field draws a yellow frame around it and puts
    // the block caret where you clicked (2026-11-20: it used to select the whole
    // content instead, which made it impossible to edit part of a path);
    // clicking another field moves both.
    void clicking_a_field_marks_it_active()
    {
        TvFileDock dock;
        dock.setDirectory(QStringLiteral("/tmp"));
        dock.resize(760, 560);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));

        auto* path = dock.findChild<QLineEdit*>(QStringLiteral("tvFileDockPath"));
        auto* name = dock.findChild<QLineEdit*>(QStringLiteral("tvFileDockEdit"));
        QVERIFY2(path && name, "dock is missing its fields");
        QVERIFY2(dock.activeField() == nullptr, "a field is marked active before any click");

        QTest::mouseClick(path, Qt::LeftButton, Qt::NoModifier, path->rect().center());
        QCOMPARE(dock.activeField(), static_cast<QWidget*>(path));
        // A click places the caret; it must NOT select the whole string, or a
        // second click could never edit part of it.
        QVERIFY2(path->selectedText().isEmpty(),
                 qPrintable(QStringLiteral("clicking selected the whole field (selected=%1)")
                                .arg(path->selectedText())));

        const QImage img = dock.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const int ring = countColorIn(img, QColor(0xFF, 0xFF, 0x55), 6,
                                      dock.pathRect().adjusted(-1, -1, 1, 1));
        QVERIFY2(ring > 200,
                 qPrintable(QStringLiteral("only %1 yellow px around the active field").arg(ring)));

        // The mark follows the click.
        QTest::mouseClick(name, Qt::LeftButton, Qt::NoModifier, name->rect().center());
        QCOMPARE(dock.activeField(), static_cast<QWidget*>(name));
        const QImage img2 = dock.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const int left = countColorIn(img2, QColor(0xFF, 0xFF, 0x55), 6,
                                      dock.pathRect().adjusted(-1, -1, 1, 1));
        QVERIFY2(left < 20,
                 qPrintable(QStringLiteral("the mark stayed on the path field (%1 px)").arg(left)));
        const int moved = countColorIn(img2, QColor(0xFF, 0xFF, 0x55), 6,
                                       dock.nameRect().adjusted(-1, -1, 1, 1));
        QVERIFY2(moved > 200,
                 qPrintable(QStringLiteral("the mark did not move to the name field (%1 px)").arg(moved)));
        dock.reject();
    }

    // Reported live: "I cannot type in the path bar; clicking into it does
    // nothing." Clicks arrive through ContentBridge (the shell is never mapped),
    // so the field must be reachable by childAt() AND end up focused.
    void clicking_the_path_bar_focuses_it()
    {
        ShellWindow shell;
        shell.resize(800, 600);
        shell.show();
        QCoreApplication::processEvents();

        ContentBridge bridge;
        bridge.setShellWindow(&shell);
        TvFileDock* dock = TvFileDock::openOn(&shell, TvFileDock::Open,
                                              QStringLiteral("Open File"),
                                              QStringLiteral("/tmp"));
        QVERIFY(dock);
        QCoreApplication::processEvents();

        auto* path = dock->findChild<QLineEdit*>(QStringLiteral("tvFileDockPath"));
        QVERIFY2(path, "no path edit in the dock");
        QVERIFY2(path->isVisible(), "path edit exists but is not visible");
        QVERIFY2(path->geometry().width() > 20 && path->geometry().height() > 8,
                 "path edit has no usable geometry");

        const QPoint shellPt = path->mapTo(&shell, path->rect().center());
        QWidget* hit = shell.childAt(shellPt);
        QVERIFY2(hit == path,
                 qPrintable(QStringLiteral("childAt resolved %1, not the path edit")
                                .arg(hit ? hit->objectName() : QStringLiteral("<null>"))));

        bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                 Qt::LeftButton, Qt::NoModifier,
                                 shellPt.x(), shellPt.y());
        QCoreApplication::processEvents();
        // Assert the shell's own focus child, not hasFocus(): hasFocus() is
        // false whenever no window is "active", which is always the case for
        // the never-mapped shell. focusWidget() is what keyTarget() uses.
        QVERIFY2(shell.focusWidget() == path,
                 qPrintable(QStringLiteral("clicking the path bar left focus on %1")
                                .arg(shell.focusWidget()
                                         ? shell.focusWidget()->objectName()
                                         : QStringLiteral("<null>"))));

        // ...and then the keys must actually land in the field.
        bridge.forwardKeyEvent(QEvent::KeyPress, Qt::Key_Slash, Qt::NoModifier,
                               QStringLiteral("/"));
        QCoreApplication::processEvents();
        QVERIFY2(path->text().contains(QLatin1Char('/')),
                 qPrintable(QStringLiteral("typing did not reach the path bar (text=%1)")
                                .arg(path->text())));

        dock->reject();
        QCoreApplication::processEvents();
    }

    // ---- 2026-11-20: typing, caret and scrolling in the dock --------------
    // User report: "the text highlights when i initially select the field but
    // the highlight doesn't leave when i click on it again to edit part of the
    // string". Every click used to consume the release and selectAll(), so a
    // second click could never place the caret inside the text.
    void clicking_a_field_places_the_caret_instead_of_selecting_everything()
    {
        ShellWindow shell;
        shell.resize(800, 600);
        shell.show();
        ContentBridge bridge;
        bridge.setShellWindow(&shell);
        TvFileDock* dock = TvFileDock::openOn(&shell, TvFileDock::Open,
                                             QStringLiteral("Open File"),
                                             QStringLiteral("/tmp"));
        QVERIFY(dock);
        QCoreApplication::processEvents();

        auto* path = dock->findChild<QLineEdit*>(QStringLiteral("tvFileDockPath"));
        QVERIFY(path);
        path->setText(QStringLiteral("/home/user/projects"));
        path->setCursorPosition(0);
        path->deselect();

        // Click a few characters into the string.
        const int clickX = path->rect().left()
                           + path->fontMetrics().horizontalAdvance(QStringLiteral("/home"));
        const QPoint local(clickX, path->rect().center().y());
        const QPoint shellPt = path->mapTo(&shell, local);
        bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                 Qt::LeftButton, Qt::NoModifier,
                                 shellPt.x(), shellPt.y());
        bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton,
                                 Qt::NoButton, Qt::NoModifier,
                                 shellPt.x(), shellPt.y());
        QCoreApplication::processEvents();

        QVERIFY2(path->selectedText().isEmpty(),
                 qPrintable(QStringLiteral("click left the whole field selected: '%1'")
                                .arg(path->selectedText())));
        QVERIFY2(path->cursorPosition() > 0
                     && path->cursorPosition() < path->text().size(),
                 qPrintable(QStringLiteral("caret was not placed inside the text (pos=%1 of %2)")
                                .arg(path->cursorPosition()).arg(path->text().size())));
    }

    // Typing after that click must EDIT the string, not replace it - the exact
    // behaviour the select-all workaround destroyed.
    void typing_after_clicking_mid_string_edits_in_place()
    {
        ShellWindow shell;
        shell.resize(800, 600);
        shell.show();
        ContentBridge bridge;
        bridge.setShellWindow(&shell);
        TvFileDock* dock = TvFileDock::openOn(&shell, TvFileDock::Open,
                                             QStringLiteral("Open File"),
                                             QStringLiteral("/tmp"));
        QVERIFY(dock);
        QCoreApplication::processEvents();

        auto* path = dock->findChild<QLineEdit*>(QStringLiteral("tvFileDockPath"));
        QVERIFY(path);
        path->setText(QStringLiteral("abcdef"));
        // Click three characters in - the real gesture the user described.
        const int clickX = path->rect().left()
                           + path->fontMetrics().horizontalAdvance(QStringLiteral("abc"));
        const QPoint shellPt = path->mapTo(&shell,
                                           QPoint(clickX, path->rect().center().y()));
        bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                 Qt::LeftButton, Qt::NoModifier,
                                 shellPt.x(), shellPt.y());
        bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton,
                                 Qt::NoButton, Qt::NoModifier,
                                 shellPt.x(), shellPt.y());
        QCoreApplication::processEvents();
        QCOMPARE(path->selectedText(), QString());
        QCOMPARE(path->cursorPosition(), 3);

        bridge.forwardKeyEvent(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier,
                               QStringLiteral("x"));
        QCoreApplication::processEvents();

        QCOMPARE(path->text(), QStringLiteral("abcxdef"));
    }

    // "I can't see where the mouse cursor is in the input line": the field
    // paints a DOS block caret at the insertion point.
    void field_paints_a_block_caret_at_the_insertion_point()
    {
        ShellWindow shell;
        shell.resize(800, 600);
        shell.show();
        ContentBridge bridge;
        bridge.setShellWindow(&shell);
        TvFileDock* dock = TvFileDock::openOn(&shell, TvFileDock::Open,
                                             QStringLiteral("Open File"),
                                             QStringLiteral("/tmp"));
        QVERIFY(dock);
        QCoreApplication::processEvents();

        auto* path = qobject_cast<TvLineEdit*>(
            dock->findChild<QLineEdit*>(QStringLiteral("tvFileDockPath")));
        QVERIFY2(path, "the path field is not a TvLineEdit");
        QVERIFY(path->caretRect().isEmpty());      // not focused: no caret

        const QPoint shellPt = path->mapTo(&shell, path->rect().center());
        bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                 Qt::LeftButton, Qt::NoModifier,
                                 shellPt.x(), shellPt.y());
        QCoreApplication::processEvents();

        QVERIFY(path->holdsKeyFocus());
        const QRect caret = path->caretRect();
        QVERIFY2(!caret.isEmpty(), "focused field reports no caret cell");
        QVERIFY2(path->rect().contains(caret.topLeft()),
                 "caret cell lies outside the field");
        QVERIFY2(caret.width() >= 2,
                 qPrintable(QStringLiteral("caret is %1 px wide - not a block")
                                .arg(caret.width())));

        // ...and it is actually INK on screen. Blink off so the probe is
        // deterministic (the app leaves it blinking; tests must not race it).
        path->setCaretBlinkEnabled(false);
        path->setCaretColors(QColor(0xFF, 0xFF, 0xFF), QColor(0x00, 0x00, 0xAA));
        const QImage img = path->grab().toImage();
        int white = 0;
        for (int y = caret.top(); y <= caret.bottom() && y < img.height(); ++y)
            for (int x = caret.left(); x <= caret.right() && x < img.width(); ++x)
                if (isWhite(img.pixelColor(x, y)))
                    ++white;
        QVERIFY2(white >= 4,
                 qPrintable(QStringLiteral("no block caret painted (%1 white px in the cell)")
                                .arg(white)));
    }

    // "the horizontal scroll does not work in the file list": the strip is a
    // real draggable thumb and its track pages.
    void strip_drag_scrolls_the_list()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (int i = 0; i < 40; ++i)
            QFile(QDir(dir.path()).absoluteFilePath(
                      QStringLiteral("file_%1_with_a_fairly_long_name.txt").arg(i, 2, 10, QLatin1Char('0'))))
                .open(QIODevice::WriteOnly);

        TvFileDock dock;
        dock.resize(700, 500);
        dock.show();
        dock.setDirectory(dir.path());
        QCoreApplication::processEvents();

        QVERIFY2(dock.scrollOffset() == 0, "list does not start at the top");
        const QRect thumb = dock.stripTrackRect();
        QVERIFY2(!thumb.isEmpty(), "no scroll strip");

        // Press on the thumb, drag to the far right, release.
        const QRect grab(thumb.left() + 2, thumb.center().y(), 6, 2);
        QMouseEvent press(QEvent::MouseButtonPress, grab.center(), dock.mapToGlobal(grab.center()),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&dock, &press);
        const QPoint far(thumb.right() - 1, thumb.center().y());
        QMouseEvent move(QEvent::MouseMove, far, dock.mapToGlobal(far),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&dock, &move);
        QMouseEvent release(QEvent::MouseButtonRelease, far, dock.mapToGlobal(far),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&dock, &release);
        QCoreApplication::processEvents();

        QVERIFY2(dock.scrollOffset() > 0,
                 "dragging the strip did not scroll the list");
    }

    void shift_wheel_scrolls_long_names_sideways()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile(QDir(dir.path()).absoluteFilePath(
                  QStringLiteral("a_name_long_enough_to_be_clipped_by_the_column.txt")))
            .open(QIODevice::WriteOnly);

        TvFileDock dock;
        dock.resize(700, 500);
        dock.show();
        dock.setDirectory(dir.path());
        QCoreApplication::processEvents();
        QVERIFY2(dock.horizontalOffset() == 0, "names start scrolled");

        const QPoint local(60, dock.listRect().center().y());
        QWheelEvent shiftWheel(QPointF(local), QPointF(dock.mapToGlobal(local)),
                               QPoint(0, 0), QPoint(0, -120), Qt::NoButton,
                               Qt::ShiftModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&dock, &shiftWheel);
        QCoreApplication::processEvents();
        QVERIFY2(dock.horizontalOffset() > 0,
                 "Shift+wheel did not scroll the names sideways");

        // A plain wheel must still scroll the LIST, not the names.
        dock.setHorizontalOffset(0);
        QWheelEvent plainWheel(QPointF(local), QPointF(dock.mapToGlobal(local)),
                               QPoint(0, 0), QPoint(0, -120), Qt::NoButton,
                               Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&dock, &plainWheel);
        QCoreApplication::processEvents();
        QCOMPARE(dock.horizontalOffset(), 0);
    }

    // 2026-11-21: user removed the inert Help plate from both file dialogs.
    // Only OK and Cancel remain, both live, neither overlapping.
    void the_dock_has_no_help_plate()
    {
        TvFileDock dock;
        dock.setDirectory(QStringLiteral("/tmp"));
        dock.resize(760, 560);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));

        QVERIFY2(dock.findChild<QWidget*>(QStringLiteral("tvFileDockHelp")) == nullptr,
                 "the Help plate is still on the dock");
        QVERIFY2(!dock.okRect().isNull(), "OK lost its geometry");
        QVERIFY2(!dock.cancelRect().isNull(), "Cancel lost its geometry");
        QVERIFY2(!dock.okRect().intersects(dock.cancelRect()),
                 "OK and Cancel now overlap after Help's removal");

        // No Help TEXT: scan only the plate BAND (the rows OK and Cancel
        // occupy), not the whole lower half - the recents/type dropdown cells
        // are green too and drown the signal (they put 3800 green px in this
        // window). Two plates live in exactly two rects.
        const QImage img = dock.grab().toImage().convertToFormat(QImage::Format_RGB32);
        int green = 0;
        for (const QRect& plate : { dock.okRect(), dock.cancelRect() }) {
            const QRect clip = plate.intersected(img.rect());
            for (int y = clip.top(); y <= clip.bottom(); ++y)
                for (int x = clip.left(); x <= clip.right(); ++x) {
                    const QColor c = img.pixelColor(x, y);
                    if (c.green() > 100 && c.red() < 80 && c.blue() < 80)
                        ++green;
                }
        }
        // Two plates' worth of green. A Help plate back on screen would add a
        // THIRD rect of its own that neither OK nor Cancel covers, so this
        // bound only ever moves if the two plates themselves change - and the
        // sibling-overlap check above already forbids that.
        QVERIFY2(green > 150,
                 qPrintable(QStringLiteral("the two plates barely paint (%1 green px)")
                                .arg(green)));
        dock.reject();
    }

    // Reported 2026-11-21: double-clicking ".." must move UP a directory.
    void double_clicking_parent_moves_up_a_directory()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("sub")));
        const QString sub = QFileInfo(dir.filePath(QStringLiteral("sub"))).absoluteFilePath();

        ShellWindow shell;
        shell.resize(800, 600);
        shell.show();
        ContentBridge bridge;
        bridge.setShellWindow(&shell);
        QCoreApplication::processEvents();

        TvFileDock* dock = TvFileDock::openOn(&shell, TvFileDock::Open,
                                             QStringLiteral("Open File"), sub);
        QVERIFY(dock);
        QCoreApplication::processEvents();
        QVERIFY2(dock->files().contains(QStringLiteral("..")), "no .. entry listed");

        const int parent = dock->files().indexOf(QStringLiteral(".."));
        QVERIFY(parent >= 0);
        const QRect cell = dock->rowRect(parent);
        QVERIFY2(!cell.isNull(), ".. cell has no geometry");
        // Bridge coordinates are SHELL-local: the cell lives in the dock.
        const QPoint p = cell.center() + dock->pos();

        // Through the BRIDGE (the production path): QML only ever sends
        // press/release pairs, so the bridge itself must turn the second press
        // into MouseButtonDblClick - a raw DblClick event would test the dock
        // but not the gesture the user actually performs.
        auto press = [&bridge, dock, p] {
            bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                     Qt::LeftButton, Qt::NoModifier, p.x(), p.y());
            bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton,
                                     Qt::NoButton, Qt::NoModifier, p.x(), p.y());
        };
        press();   // first click: selects the row
        QCoreApplication::processEvents();
        QCOMPARE(dock->editText(), QStringLiteral(".."));

        press();   // second click, same spot, inside the interval: double
        QCoreApplication::processEvents();

        QCOMPARE(QFileInfo(dock->directory()).canonicalFilePath(),
                 QFileInfo(dir.path()).canonicalFilePath());
        QVERIFY2(dock->editText().isEmpty(),
                 qPrintable(QStringLiteral("name box not cleared after up-nav (text=%1)")
                                .arg(dock->editText())));
        dock->reject();
    }

    // Reported 2026-11-21: the file list has no horizontal scrollbar widget,
    // so sideways scrolling is impossible without knowing the Shift+wheel
    // trick. A real horizontal bar must exist, own a range, and drag.
    void the_file_list_has_a_working_horizontal_scrollbar()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (int i = 0; i < 60; ++i) {
            QFile f(dir.filePath(QStringLiteral("a_name_very_long_enough_to_need_side_scroll_%1.txt")
                                     .arg(i, 2, 10, QLatin1Char('0'))));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("x");
        }

        TvFileDock dock;
        dock.setDirectory(dir.path());
        dock.resize(700, 500);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        QCoreApplication::processEvents();

        QScrollBar* bar = dock.findChild<QScrollBar*>(QStringLiteral("fileListHBar"));
        QVERIFY2(bar, "no horizontal scrollbar on the file list");
        QVERIFY2(bar->isVisible(), "the horizontal scrollbar is hidden");
        QVERIFY2(bar->width() > 40, "the horizontal scrollbar is too thin to grab");
        QVERIFY2(bar->orientation() == Qt::Horizontal, "wrong orientation");
        QVERIFY2(bar->maximum() > bar->minimum(),
                 "the horizontal scrollbar has no range to scroll");

        // Drag the thumb: the names must move sideways.
        const int before = dock.horizontalOffset();
        const int mid = (bar->minimum() + bar->maximum()) / 2;
        bar->setValue(mid);
        QCoreApplication::processEvents();
        QVERIFY2(dock.horizontalOffset() != before,
                 qPrintable(QStringLiteral("setting the bar to %1 did not move the names (offset=%2)")
                                .arg(mid).arg(dock.horizontalOffset())));
        QVERIFY2(dock.horizontalOffset() > 0, "names still at 0 after dragging to mid-range");
        dock.reject();
    }

    // 2026-11-21 (round 5, FAIL 1): live File > Open showed no caret and
    // Backspace edited the editor behind the dock. The test suite passed
    // because EVERY existing test calls shell.show() before
    // bridge.setShellWindow(&shell); production main.cpp never did. Without a
    // live parent window, dock->show() delivers no QShowEvent, TvFileDock::
    // showEvent never runs, the keyboard is never claimed, and focus never
    // moves into a field. This test reproduces the PRODUCTION ORDER (bridge
    // first, no prior show) and pins the chain: keyboard claimed, field gets
    // key focus, caret paints, Backspace stays in the field.
    void the_production_order_still_claims_the_keyboard_and_focuses_a_field()
    {
        ShellWindow shell;
        shell.resize(800, 600);
        // NOTE: no shell.show() here - that is the production bug.
        ContentBridge bridge;
        bridge.setShellWindow(&shell);   // the bridge must show() internally
        QCoreApplication::processEvents();

        TvFileDock* dock = TvFileDock::openOn(&shell, TvFileDock::Open,
                                             QStringLiteral("Open File"),
                                             QStringLiteral("/tmp"));
        QVERIFY(dock);
        QCoreApplication::processEvents();

        QVERIFY2(shell.keyboardIsOwned(),
                 "opening the dock in the production order did not claim the keyboard");

        auto* path = qobject_cast<TvLineEdit*>(
            dock->findChild<QLineEdit*>(QStringLiteral("tvFileDockPath")));
        QVERIFY2(path, "the path field is not a TvLineEdit");

        const QPoint shellPt = path->mapTo(&shell, path->rect().center());
        bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                 Qt::LeftButton, Qt::NoModifier,
                                 shellPt.x(), shellPt.y());
        bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton,
                                 Qt::NoButton, Qt::NoModifier,
                                 shellPt.x(), shellPt.y());
        QCoreApplication::processEvents();

        QVERIFY2(path->holdsKeyFocus(),
                 "clicking the path field in the production order did not give it key focus");
        QVERIFY2(!path->caretRect().isEmpty(),
                 "focused path field reports no caret cell");

        dock->reject();
        QCoreApplication::processEvents();
        QVERIFY2(!shell.keyboardIsOwned(),
                 "closing the dock left the keyboard claimed");
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstFiledlg tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_filedlg.moc"
