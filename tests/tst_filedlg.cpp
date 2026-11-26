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
#include <QDebug>
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
            for (int x = list.left(); x < list.right() - 10; ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.red() < 24 && c.green() < 24 && c.blue() < 24)
                    ++black;
            }
        QVERIFY2(black > 20,
                 qPrintable(QStringLiteral("list names are not black ink (%1 black px)")
                                .arg(black)));

        // Round 16: the vertical scrollbar (and its green thumb) and the blue
        // column divider are BOTH gone - Xadium: "remove the blue divider" and
        // "remove the vertical scrollbar". The box is plain teal with black
        // names; the right edge carries no green thumb and the middle carries
        // no blue rule.
        int thumb = 0;
        for (int y = list.top(); y < list.bottom() - 7; ++y)
            for (int x = list.right() - 10; x <= list.right(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.green() > 120 && c.red() < 60 && c.blue() < 60)
                    ++thumb;
            }
        QVERIFY2(thumb == 0,
                 qPrintable(QStringLiteral("a green scroll thumb still sits on the box's right edge "
                                           "(%1 px)").arg(thumb)));

        int divider = 0;
        const QRect content = dock.listContentRect();
        const int mid = content.left() + content.width() / 2;
        // Scan the NAME band only (content.bottom() excludes the horizontal
        // scrollbar strip at the box's bottom, which is not a divider).
        for (int y = content.top() + 2; y <= content.bottom() - 2; ++y)
            for (int x = mid - 2; x <= mid + 3; ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.blue() > 120 && c.red() < 40 && c.green() < 40)
                    ++divider;
            }
        QVERIFY2(divider == 0,
                 qPrintable(QStringLiteral("a blue column divider still sits in the Files box (%1 px)")
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

    // Reported 2026-11-24: "the file picker scrollbar should be repositioned to
    // be vertical because right now it a) doesn't work and b) is
    // counterintuitive since the contents scroll down."
    //
    // The list scrolls VERTICALLY (two columns, top to bottom), so its control
    // is a vertical bar on the right edge of the Files box - a real QScrollBar
    // child, so dragging, the arrows and the track all behave like a scrollbar.
    // Round 16 (Xadium): the vertical scrollbar is REMOVED ("remove the
    // vertical scrollbar as it does nothing"). This test now pins the new
    // contract: no vertical bar widget exists, and vertical scrolling still
    // works by wheel.
    void the_file_list_has_no_vertical_bar_and_wheel_still_scrolls_it()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (int i = 0; i < 60; ++i)
            QFile(QDir(dir.path()).absoluteFilePath(
                      QStringLiteral("file_%1.txt").arg(i, 2, 10, QLatin1Char('0'))))
                .open(QIODevice::WriteOnly);

        TvFileDock dock;
        dock.resize(700, 500);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        dock.setDirectory(dir.path());
        QCoreApplication::processEvents();

        QVERIFY2(dock.findChild<QScrollBar*>(QStringLiteral("fileListVBar")) == nullptr,
                 "the Files box still has a vertical scrollbar");

        // Wheel scrolls the list vertically, with no bar to drive it.
        const QRect list = dock.listRect();
        const QPoint local(list.center());
        QWheelEvent down(QPointF(local), QPointF(dock.mapToGlobal(local)),
                         QPoint(0, 0), QPoint(0, -120), Qt::NoButton,
                         Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&dock, &down);
        QCoreApplication::processEvents();
        QVERIFY2(dock.scrollOffset() > 0, "wheel did not scroll the list down");

        dock.reject();
    }

    // ---- Places sidebar (user request 2026-11-24) --------------------------
    //
    // The dock carries a column of shortcut locations down its left side, in
    // the same teal field language as the Files box, and the whole content
    // block is shifted right to make room for it.
    void the_places_list_sits_on_the_left_in_house_style()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (const char* name : { "aaa.txt", "bbb.txt" }) {
            QFile f(dir.filePath(QLatin1String(name)));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("x");
        }

        TvFileDock dock;
        dock.setPlaces({ QStringLiteral("Alpha"), QStringLiteral("Beta") },
                       { dir.path(), QDir::homePath() });
        dock.setDirectory(dir.path());
        dock.resize(700, 500);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        QCoreApplication::processEvents();

        QVERIFY2(dock.placeCount() == 2, "the places list was not installed");
        const QRect places = dock.placesRect();
        const QRect list = dock.listRect();
        QVERIFY2(!places.isEmpty(), "no places column");
        QVERIFY2(places.right() < list.left(),
                 qPrintable(QStringLiteral("the Files box was not shifted right of the places "
                                           "column (%1 vs %2)")
                                .arg(list.left()).arg(places.right())));
        QVERIFY2(list.width() > 40, "the Files box lost its width");
        // The PATH bar follows the shifted content block (right of the column),
        // but the NAME/TYPE rows sit at the LEFT margin (2026-11-24: "the file
        // name / type inputs should move to the left") - they span under the
        // column rather than starting to its right.
        QVERIFY2(dock.pathRect().left() > places.right(),
                 "the path field still starts left of the places column");
        QVERIFY2(dock.nameRect().left() <= places.left() + 2,
                 qPrintable(QStringLiteral("the name field did not move to the left margin "
                                           "(starts at %1, margin %2)")
                                .arg(dock.nameRect().left()).arg(places.left())));

        // Rows are laid out top-down inside the column, one per place.
        const QRect r0 = dock.placeRect(0);
        const QRect r1 = dock.placeRect(1);
        QVERIFY2(!r0.isEmpty() && !r1.isEmpty(), "place rows have no geometry");
        QVERIFY2(r0.top() < r1.top(), "place rows are not stacked downwards");
        QVERIFY2(r1.top() >= r0.bottom(), "place rows overlap");

        // Styled to match the Files box: teal field, black names, and a yellow
        // "Places" caption over the column.
        const QImage img = dock.grab().toImage().convertToFormat(QImage::Format_RGB32);
        int teal = 0, black = 0;
        for (int y = places.top(); y <= places.bottom(); ++y)
            for (int x = places.left(); x <= places.right(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (qAbs(c.red() - 0x00) < 40 && qAbs(c.green() - 0xAA) < 40
                    && qAbs(c.blue() - 0xAA) < 40)
                    ++teal;
                if (c.red() < 24 && c.green() < 24 && c.blue() < 24)
                    ++black;
            }
        QVERIFY2(teal > 100,
                 qPrintable(QStringLiteral("places column is not the teal field (%1 px)")
                                .arg(teal)));
        QVERIFY2(black > 20,
                 qPrintable(QStringLiteral("places column shows no black names (%1 px)")
                                .arg(black)));

        dock.reject();
    }

    void clicking_a_place_moves_the_path_and_the_listing()
    {
        QTemporaryDir first, second;
        QVERIFY(first.isValid() && second.isValid());
        QFile(QDir(first.path()).absoluteFilePath(QStringLiteral("only_in_first.txt")))
            .open(QIODevice::WriteOnly);
        QFile(QDir(second.path()).absoluteFilePath(QStringLiteral("only_in_second.txt")))
            .open(QIODevice::WriteOnly);

        TvFileDock dock;
        dock.setPlaces({ QStringLiteral("First"), QStringLiteral("Second") },
                       { first.path(), second.path() });
        dock.setDirectory(first.path());
        dock.resize(700, 500);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        QCoreApplication::processEvents();

        QVERIFY2(dock.files().contains(QStringLiteral("only_in_first.txt")),
                 "did not start in the first directory");
        QCOMPARE(dock.currentPlace(), 0);

        // A real press inside the second place's row, forwarded by the bridge.
        const QRect row = dock.placeRect(1);
        QVERIFY(!row.isEmpty());
        const QPoint local = row.center();
        QMouseEvent press(QEvent::MouseButtonPress, local, dock.mapToGlobal(local),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&dock, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, local, dock.mapToGlobal(local),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&dock, &release);
        QCoreApplication::processEvents();

        QCOMPARE(QFileInfo(dock.directory()).canonicalFilePath(),
                 QFileInfo(second.path()).canonicalFilePath());
        QVERIFY2(dock.files().contains(QStringLiteral("only_in_second.txt")),
                 qPrintable(QStringLiteral("the listing did not change: %1")
                                .arg(dock.files().join(QLatin1Char(',')))));
        QVERIFY2(!dock.files().contains(QStringLiteral("only_in_first.txt")),
                 "the old directory's files are still listed");
        QVERIFY2(dock.pathText().contains(QFileInfo(second.path()).fileName()),
                 qPrintable(QStringLiteral("the path bar still says %1").arg(dock.pathText())));
        QCOMPARE(dock.currentPlace(), 1);

        dock.reject();
    }

    // The default list is what Kate shows: the user's own bookmarks from
    // ~/.local/share/user-places.xbel when one exists (2026-11-24: "the places
    // display doesn't match what kate can see"), otherwise Home, the XDG
    // folders that actually exist, then the root. Either way every entry is a
    // real directory (an entry that is not a directory would strand the user).
    void the_default_places_are_the_standard_locations()
    {
        const QStringList labels = TvFileDock::defaultPlaceLabels();
        const QStringList paths = TvFileDock::defaultPlacePaths();
        QVERIFY2(labels.size() >= 6, "too few default places");
        QCOMPARE(labels.size(), paths.size());
        QVERIFY2(!paths.isEmpty(), "no default places at all");
        // When a bookmark file exists its entries replace the XDG set, so the
        // Home/Root specifics are not asserted here - only that every offered
        // place is a directory the user can actually open.
        for (const QString& p : paths)
            QVERIFY2(QFileInfo(p).isDir(),
                     qPrintable(QStringLiteral("default place is not a directory: %1").arg(p)));
    }

    // The bar is a child widget, so the bridge's childAt() hit-test is what
    // makes a click on it land: pressed on the track below the thumb it pages
    // the list, pressed on an arrow it steps by one row.
    // Round 16: the vertical bar is gone, so there is nothing to click. This
    // replaces the old "clicking the vertical bar pages the list" test, whose
    // subject no longer exists.
    void there_is_no_vertical_bar_to_click()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (int i = 0; i < 60; ++i)
            QFile(QDir(dir.path()).absoluteFilePath(
                      QStringLiteral("file_%1.txt").arg(i, 2, 10, QLatin1Char('0'))))
                .open(QIODevice::WriteOnly);

        TvFileDock dock;
        dock.resize(700, 500);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        dock.setDirectory(dir.path());
        QCoreApplication::processEvents();

        QVERIFY2(dock.findChild<QScrollBar*>(QStringLiteral("fileListVBar")) == nullptr,
                 "the Files box still has a vertical scrollbar to click");
        dock.reject();
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

    // ---- Round 15 (Xadium's five-issue list) -------------------------------
    //
    // (1) The file name / file type rows must sit at the dock's LEFT margin,
    // under the Places column - they were shifted right along with the rest of
    // the content, so the two fields drifted away from the label column.
    void name_and_type_rows_sit_at_the_left_margin()
    {
        TvFileDock dock;
        dock.setDirectory(QStringLiteral("/tmp"));
        dock.resize(700, 500);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        QCoreApplication::processEvents();

        const QRect places = dock.placesRect();
        QVERIFY2(!places.isEmpty(), "no Places column");
        // The left margin is the column's own left edge; the name/type rows
        // start there (they may start slightly inside the column, never to
        // its right).
        QVERIFY2(dock.nameRect().left() <= places.left() + 2,
                 qPrintable(QStringLiteral("file name row starts at %1, left margin is %2")
                                .arg(dock.nameRect().left()).arg(places.left())));
        QVERIFY2(dock.typeRect().left() <= places.left() + 2,
                 qPrintable(QStringLiteral("file type row starts at %1, left margin is %2")
                                .arg(dock.typeRect().left()).arg(places.left())));
        QVERIFY2(dock.nameRect().left() < places.right(),
                 "the file name row still starts to the right of the Places column");
        QVERIFY2(dock.typeRect().left() < places.right(),
                 "the file type row still starts to the right of the Places column");
        dock.reject();
    }

    // (2) OK / Cancel belong in the LOWER right, stacked close together and
    // clear of the picker (Files box), so they fit under it instead of sitting
    // at the top of the list.
    void ok_and_cancel_sit_in_the_lower_right_close_together()
    {
        TvFileDock dock;
        dock.setDirectory(QStringLiteral("/tmp"));
        dock.resize(700, 500);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        QCoreApplication::processEvents();

        const QRect ok = dock.okRect();
        const QRect cancel = dock.cancelRect();
        const QRect list = dock.listRect();
        QVERIFY2(!ok.isNull() && !cancel.isNull(), "a button has no geometry");

        // Lower right: both buttons are in the bottom half of the dock and
        // hug the right margin.
        QVERIFY2(ok.top() > dock.rect().height() / 2,
                 qPrintable(QStringLiteral("OK sits at y=%1, expected the lower half")
                                .arg(ok.top())));
        QVERIFY2(cancel.top() > dock.rect().height() / 2,
                 qPrintable(QStringLiteral("Cancel sits at y=%1, expected the lower half")
                                .arg(cancel.top())));
        QVERIFY2(ok.right() > dock.rect().width() * 3 / 4,
                 qPrintable(QStringLiteral("OK ends at x=%1, expected the right quarter")
                                .arg(ok.right())));

        // Stacked, close together: a tight vertical gap, not the old 58px void.
        const int gap = cancel.top() - ok.bottom() - 1;
        QVERIFY2(gap >= 0 && gap <= 12,
                 qPrintable(QStringLiteral("OK/Cancel are %1px apart, expected 0..12").arg(gap)));

        // Clear of the picker: neither plate overlaps the Files box.
        QVERIFY2(!ok.intersects(list), "the OK plate overlaps the picker");
        QVERIFY2(!cancel.intersects(list), "the Cancel plate overlaps the picker");
        QVERIFY2(dock.rect().contains(ok) && dock.rect().contains(cancel),
                 "a button escapes the dock");
        dock.reject();
    }

    // (3) The Places column must show what Kate shows - the user's bookmarks
    // from ~/.local/share/user-places.xbel (GFX Work, Blockbuster Video,
    // scratch, ...) - not a hardcoded XDG eight. With a populated xbel on disk
    // the dock offers those entries; with none it falls back to the XDG set.
    void the_places_column_reads_the_users_kate_bookmarks()
    {
        // A real xbel with two bookmarks, in the location Kate reads.
        QTemporaryDir dataDir;
        QVERIFY(dataDir.isValid());
        const QString xbelPath = dataDir.filePath(QStringLiteral("user-places.xbel"));
        {
            QFile f(xbelPath);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
            f.write(
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<!DOCTYPE xbel>\n"
                "<xbel>\n"
                " <bookmark href=\"file:///tmp/alpha-place\">\n"
                "  <title>Alpha Place</title>\n"
                " </bookmark>\n"
                " <bookmark href=\"file:///tmp/beta-place\">\n"
                "  <title>Beta Place</title>\n"
                " </bookmark>\n"
                "</xbel>\n");
        }
        QVERIFY(QDir().mkpath(QStringLiteral("/tmp/alpha-place")));
        QVERIFY(QDir().mkpath(QStringLiteral("/tmp/beta-place")));

        // Point the loader at our temp XDG data home for this test only.
        const QByteArray oldXdg = qgetenv("XDG_DATA_HOME");
        qputenv("XDG_DATA_HOME", dataDir.path().toUtf8());

        TvFileDock dock;
        const QStringList labels = dock.defaultPlaceLabels();
        const QStringList paths = dock.defaultPlacePaths();

        if (oldXdg.isEmpty())
            qunsetenv("XDG_DATA_HOME");
        else
            qputenv("XDG_DATA_HOME", oldXdg);

        QVERIFY2(paths.contains(QStringLiteral("/tmp/alpha-place")),
                 qPrintable(QStringLiteral("bookmarks not read; places = %1")
                                .arg(paths.join(QLatin1Char(',')))));
        QVERIFY2(labels.contains(QStringLiteral("Alpha Place")),
                 qPrintable(QStringLiteral("bookmark title missing; labels = %1")
                                .arg(labels.join(QLatin1Char(',')))));
        QVERIFY2(labels.contains(QStringLiteral("Beta Place")),
                 "second bookmark title missing");
        QVERIFY2(!paths.contains(QStringLiteral("/")),  // no hardcoded Root
                 "Root was injected despite a populated xbel");
        QVERIFY(!xbelPath.isEmpty());
        QDir().rmpath(QStringLiteral("/tmp/alpha-place"));
        QDir().rmpath(QStringLiteral("/tmp/beta-place"));
        dock.reject();
    }

    // (4) The picker's scrollbars must be the same size as the edit view's
    // RightScrollBar (one character cell), and a click delivered THROUGH the
    // bridge (the production path) must actually reach them. Both were broken:
    // the width was hardcoded to 11px, and the hand-rolled strip was a painted
    // rect the bridge's childAt() could not route a press to.
    void picker_scrollbars_match_the_edit_view_and_take_a_bridge_click()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // Long names so the horizontal bar owns a real scroll range (the whole
        // point of this test is that a bridge click on it moves the names).
        for (int i = 0; i < 80; ++i) {
            QFile f(QDir(dir.path()).absoluteFilePath(
                QStringLiteral("a_name_long_enough_to_need_side_scroll_%1.txt")
                    .arg(i, 2, 10, QLatin1Char('0'))));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("x");
        }

        ShellWindow shell;
        shell.resize(900, 700);
        shell.show();
        QCoreApplication::processEvents();
        ContentBridge bridge;
        bridge.setShellWindow(&shell);

        TvFileDock* dock = TvFileDock::openOn(&shell, TvFileDock::Open,
                                              QStringLiteral("Open File"), dir.path());
        QVERIFY(dock);
        dock->resize(700, 500);
        QCoreApplication::processEvents();

        // Round 16 contract: the VERTICAL bar is gone (it did nothing); the
        // HORIZONTAL bar stays, one character cell tall like the edit view's
        // RightScrollBar, and a bridge click must still reach it.
        QScrollBar* vbar = dock->findChild<QScrollBar*>(QStringLiteral("fileListVBar"));
        QScrollBar* hbar = dock->findChild<QScrollBar*>(QStringLiteral("fileListHBar"));
        QVERIFY2(vbar == nullptr, "the Files box still has a vertical scrollbar");
        QVERIFY2(hbar, "the Files box has no horizontal scrollbar");

        const int cell = dock->cellWidth();
        QVERIFY2(hbar->height() == cell,
                 qPrintable(QStringLiteral("horizontal bar is %1px tall, edit view uses %2px")
                                .arg(hbar->height()).arg(cell)));

        // A bridge click (production path) on the horizontal bar's right-arrow
        // end must scroll the names sideways - childAt() has to resolve the bar.
        const int before = dock->horizontalOffset();
        QVERIFY2(hbar->maximum() > hbar->minimum(), "the horizontal bar has no scroll range");
        const QPoint barPt = dock->mapTo(&shell,
                                         QPoint(hbar->geometry().right() - 3,
                                                hbar->geometry().center().y()));
        QSignalSpy barSpy(hbar, &QScrollBar::valueChanged);
        bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                 Qt::LeftButton, Qt::NoModifier, barPt.x(), barPt.y());
        bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton,
                                 Qt::NoButton, Qt::NoModifier, barPt.x(), barPt.y());
        QCoreApplication::processEvents();
        QVERIFY2(dock->horizontalOffset() > before || barSpy.count() > 0,
                 qPrintable(QStringLiteral("the bridge click on the horizontal bar did nothing "
                                           "(offset %1 -> %2, valueChanged x%3)")
                                .arg(before).arg(dock->horizontalOffset()).arg(barSpy.count())));

        dock->reject();
        QCoreApplication::processEvents();
    }

    // Round 16 (Xadium): four file-list fixes, all observed on the rendered
    // picker. Issue A - the dark-blue vertical divider between the two name
    // columns must be gone.
    void the_file_list_has_no_blue_divider_between_the_columns()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (int i = 0; i < 20; ++i) {
            QFile f(QDir(dir.path()).absoluteFilePath(
                QStringLiteral("col_%1.txt").arg(i)));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("x");
        }
        TvFileDock dock;
        dock.resize(700, 500);
        dock.show();
        dock.setDirectory(dir.path());
        QCoreApplication::processEvents();
        // The selected row also paints kBlue, so scan ONLY the divider's own
        // 2px column at content.left()+half - the x-range the selected row
        // (left column, ending at half-4) never reaches. A vertical rule of
        // blue there is the divider.
        const QRect content = dock.listContentRect();
        QVERIFY(!content.isEmpty());
        const int half = content.width() / 2;
        const QRect dividerBand(content.left() + half, content.top(),
                                2, content.height());
        const QImage img = dock.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const QColor blue(0x00, 0x00, 0xAA);
        const int blueInBand = countColorIn(img, blue, 6, dividerBand);
        QVERIFY2(blueInBand == 0,
                 qPrintable(QStringLiteral("a %1px blue vertical divider still sits between the columns")
                                .arg(blueInBand)));
        dock.reject();
    }

    // Issue B - scrolling names sideways must not let a left-column name bleed
    // into the right column. Round 16 clipped each name INSIDE its fixed cell
    // (a per-name offset), which Xadium rejected on 2026-11-26: "the columns
    // stay fixed and the names slide WITHIN them (acts like a split pane)."
    //
    // Round 18 moves the WHOLE strip: rowRect() shifts every cell by
    // m_hOffset and the ink is drawn at the cell's own origin, so a name
    // travels WITH its cell. The gutter is not a hard boundary any more - a
    // name that fills its cell slides its tail across it as the strip moves
    // (that is the group move). What must hold: a name never ORIGINATES in the
    // gutter - for a short name (which fits its cell) the ink's left edge is
    // the cell's own left edge, whatever the offset. That is the geometry/
    // paint agreement the split-pane bug broke.
    void horizontal_scroll_does_not_bleed_a_name_into_the_neighbour_column()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // One SHORT name per row (fits its cell) so its ink origin is exactly
        // its cell's left edge; rows fill left, right, left, ...
        for (int i = 0; i < 40; ++i) {
            QFile f(QDir(dir.path()).absoluteFilePath(
                QStringLiteral("n%1.txt").arg(i, 2, 10, QLatin1Char('0'))));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("x");
        }

        TvFileDock dock;
        dock.resize(700, 500);
        dock.show();
        dock.setDirectory(dir.path());
        QCoreApplication::processEvents();

        // Scroll the names well to the right so the cells move under the ink.
        dock.setHorizontalOffset(60);
        QCoreApplication::processEvents();
        QVERIFY2(dock.horizontalOffset() > 0, "names did not scroll sideways");

        const QRect content = dock.listContentRect();
        const int half = content.width() / 2;
        const QImage img = dock.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const int rowH = dock.cellHeight();
        const int gutterL = content.left() + half - 4;   // gutter band starts
        const int gutterR = content.left() + half + 2;   // right cell starts (= gutter end)
        // Both cells at their SHIFTED origins (rowRect()'s arithmetic). A cell
        // entirely out of view contributes nothing to check.
        int originsInGutter = 0;
        for (int row = 0; row * rowH + rowH <= content.height(); ++row) {
            const int y0 = content.top() + row * rowH;
            for (const int cellLeft : {content.left() - dock.horizontalOffset(),
                                       gutterR - dock.horizontalOffset()}) {
                const QRect vis = QRect(cellLeft, y0, half - 4, rowH)
                                      .intersected(QRect(content.left(), y0,
                                                         content.width(), rowH));
                if (vis.isEmpty())
                    continue;   // this cell slid entirely out of view
                // First ink in the visible part of this cell.
                int ink = -1;
                for (int x = vis.left(); x <= vis.right() && ink < 0; ++x)
                    for (int y = vis.top(); y < vis.bottom(); ++y) {
                        const QColor c = img.pixelColor(x, y);
                        if (qAbs(c.red()) < 60 && qAbs(c.green()) < 60 && qAbs(c.blue()) < 60) {
                            ink = x;
                            break;
                        }
                    }
                if (ink < 0)
                    continue;   // this short name's ink slid out of the visible part
                // The name must ORIGINATE at its own cell's left edge (+ the 2px
                // padding), NOT in the gutter band [gutterL, gutterR). Ink sitting
                // in the gutter that is NOT the cell's own origin means the text
                // slid across the boundary (the split-pane bug).
                const bool inGutter = ink >= gutterL && ink < gutterR;
                const bool atOwnOrigin = qAbs(ink - (cellLeft + 2)) <= 1
                                         && cellLeft + 2 >= gutterR;   // right cell's origin
                if (inGutter && !atOwnOrigin)
                    ++originsInGutter;
            }
        }
        QVERIFY2(originsInGutter == 0,
                 qPrintable(QStringLiteral(
                                "%1 name(s) originate in the column gutter during horizontal scroll")
                                .arg(originsInGutter)));
        dock.reject();
    }

    // 2026-11-26 (round 18): horizontal scrolling must move the COLUMNS as a
    // group. Xadium: the two columns "should shift sideways as a group
    // (carrying their names)" to reveal what is to their right, but round 16's
    // model slid each name INSIDE its fixed cell (drawText(r.adjusted(
    // 2 - m_hOffset ...))) - the names slid within the columns while the
    // columns themselves stayed put.
    //
    // Contract (the whole fix in two rules):
    //   1. LEFT column: every name's ink origin must shift left by exactly
    //      m_hOffset (a group move, proven by ink starting a shifted row rect).
    //   2. RIGHT column: a name must leave its home cell when the offset is
    //      large - it must NOT stay pinned at its cell's left edge sliding only
    //      its first characters out of view (the old split-pane behaviour).
    void horizontal_scroll_shifts_the_columns_as_a_group()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // A long name in the LEFT column (no ".." and no subdirs, so index 0
        // is a file and the columns fill row-major left, right, ...). Names
        // sorted by QDir::Name end up in BOTH columns, so the WIDEST one also
        // sets the horizontal cap.
        QVERIFY(QFile(QDir(dir.path()).absoluteFilePath(
                          QStringLiteral("left_column_long_name_shows_how_the_strip_must_move.txt")))
                    .open(QIODevice::WriteOnly));
        // Names long enough to overflow the two-cell strip - the horizontal
        // cap only opens up when one name is wider than both columns, and the
        // group-move rules need ink to measure on both sides.
        const QString first = QStringLiteral(
            "a_rather_long_left_column_name_that_will_not_fit_inside_a_single_cell_"
            "so_it_needs_a_side_scroll_to_read_all_of_it_%1.txt");
        QVERIFY(QFile(QDir(dir.path()).absoluteFilePath(first.arg(0)))
                    .open(QIODevice::WriteOnly));
        for (int i = 1; i < 60; ++i) {
            QVERIFY(QFile(QDir(dir.path()).absoluteFilePath(first.arg(i)))
                        .open(QIODevice::WriteOnly));
        }

        TvFileDock dock;
        dock.resize(700, 500);
        dock.show();
        dock.setDirectory(dir.path());
        QCoreApplication::processEvents();
        QCOMPARE(dock.files().size(), 62);   // ".." + long name + 60 fills

        const QRect content = dock.listContentRect();
        QVERIFY(content.width() > 100);
        const int half = content.width() / 2;

        // Ink bounding box of one row band, across the whole content width.
        auto rowInkBox = [&](const QImage& img, int row) {
            const int top = content.top() + row * dock.cellHeight();
            QRect box;
            for (int x = content.left(); x <= content.right(); ++x)
                for (int y = top; y < top + dock.cellHeight(); ++y) {
                    const QColor c = img.pixelColor(x, y);
                    if (qAbs(c.red()) < 60 && qAbs(c.green()) < 60 && qAbs(c.blue()) < 60)
                        box = box.united(QRect(x, y, 1, 1));
                }
            return box;
        };

        // ---- offset 0: the long left name (row 1: ".." is row 0) must START
        // inside the left cell. (It OVERFLOWS to the right - there is no clip
        // now - but its origin is what a horizontal scroll moves.)
        QCoreApplication::processEvents();
        const QImage base = dock.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const QRect ink1_0 = rowInkBox(base, 1);
        QVERIFY2(!ink1_0.isEmpty(), "row 1 draws no ink at offset 0");
        QVERIFY2(ink1_0.left() < content.left() + half - 4,
                 qPrintable(QStringLiteral("the left name starts at x=%1, not inside the left cell (%2)")
                                .arg(ink1_0.left()).arg(content.left() + half - 4)));

        // ---- RULE 1 (the group move): the name must travel left by EXACTLY
        // m_hOffset. The text ORIGIN cannot be measured directly - the content
        // clip pins every visible left edge at the viewport - so the band's
        // RIGHT edge is the honest metric: it moves with the text, and under
        // the old model (text sliding inside a FIXED cell) it stayed put.
        const int shift = 24;   // small enough that the strip stays in view
        dock.setHorizontalOffset(shift);
        QCoreApplication::processEvents();
        QVERIFY2(dock.horizontalOffset() == shift,
                 qPrintable(QStringLiteral("the horizontal bar refused the offset: cap=%1, bar range=%2..%3")
                                .arg(dock.maxHorizontalOffset())
                                .arg(dock.horizontalScrollBarWidget()->minimum())
                                .arg(dock.horizontalScrollBarWidget()->maximum())));
        const QImage moved = dock.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const QRect ink1_s = rowInkBox(moved, 1);
        QVERIFY2(!ink1_s.isEmpty(), "the left long name drew no ink after the shift");
        const int movedPx = ink1_0.right() - ink1_s.right();
        QVERIFY2(qAbs(movedPx - shift) <= 2,
                 qPrintable(QStringLiteral(
                                "the left column did not shift as a group: its name moved %1px, "
                                "want ~%2px (m_hOffset); names are still sliding within fixed cells")
                                .arg(movedPx).arg(shift)));

        // ---- RULE 2 (the columns leave their cells too): at the CAP the strip
        // is so far left that the right column's home cell - where a name sits
        // at offset 0 - is OUT of the content rect. If the columns moved as a
        // group there is nothing but teal there; if they stayed fixed the
        // pinned names would still be drawn (the split-pane bug).
        const int cap = dock.maxHorizontalOffset();
        QVERIFY2(cap > half,
                 qPrintable(QStringLiteral("cap %1 cannot shift even one column (%2 wide) off its home cell")
                                .arg(cap).arg(half)));
        dock.setHorizontalOffset(cap);
        QCoreApplication::processEvents();
        QVERIFY2(dock.horizontalOffset() == cap, "the horizontal bar refused the cap");
        const QImage atCap = dock.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const QRect rightHome(content.left() + half + 2, content.top(),
                              8, dock.cellHeight() * 3);
        const int pinned = countColorIn(atCap, QColor(0, 0, 0), 60, rightHome);
        const int pinnedBase = countColorIn(base, QColor(0, 0, 0), 60, rightHome);
        QVERIFY2(pinnedBase > 0, "the right column's names drew no ink at offset 0");
        QVERIFY2(pinned == 0,
                 qPrintable(QStringLiteral(
                                "the right column is pinned: %1 px of names still start at their home "
                                "cells after a full %2px horizontal scroll")
                                .arg(pinned).arg(cap)));
        dock.reject();
    }

    // Issue C - the Files box's vertical scrollbar does nothing useful and is
    // to be removed. It is gone; vertical scrolling still works by wheel.
    void the_file_list_has_no_vertical_scrollbar()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (int i = 0; i < 80; ++i) {
            QFile f(QDir(dir.path()).absoluteFilePath(
                QStringLiteral("v_%1.txt").arg(i, 2, 10, QLatin1Char('0'))));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("x");
        }
        TvFileDock dock;
        dock.resize(700, 500);
        dock.show();
        dock.setDirectory(dir.path());
        QCoreApplication::processEvents();

        QVERIFY2(dock.findChild<QScrollBar*>(QStringLiteral("fileListVBar")) == nullptr,
                 "the Files box still has a vertical scrollbar");
        // The horizontal bar stays.
        QVERIFY2(dock.findChild<QScrollBar*>(QStringLiteral("fileListHBar")) != nullptr,
                 "the horizontal scrollbar was removed too");

        // Vertical scrolling still works by wheel.
        const int before = dock.scrollOffset();
        const QPoint local(dock.listRect().center());
        QWheelEvent wheel(QPointF(local), QPointF(dock.mapToGlobal(local)),
                          QPoint(0, 0), QPoint(0, -120), Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&dock, &wheel);
        QCoreApplication::processEvents();
        QVERIFY2(dock.scrollOffset() != before || dock.files().size() <= 2 * 1,
                 "wheel no longer scrolls the list vertically");
        dock.reject();
    }

    // Issue D - visual space above the Cancel button. OK and Cancel are stacked
    // in the lower right; there must be a clear gap between them so Cancel is
    // not glued to OK.
    void the_cancel_button_has_visual_space_above_it()
    {
        TvFileDock dock;
        dock.resize(700, 500);
        dock.show();
        dock.setDirectory(QStringLiteral("/tmp"));
        QCoreApplication::processEvents();

        const QRect ok = dock.okRect();
        const QRect cancel = dock.cancelRect();
        QVERIFY(!ok.isEmpty() && !cancel.isEmpty());
        QVERIFY2(cancel.top() >= ok.bottom(),
                 "Cancel overlaps or sits above OK");
        const int gap = cancel.top() - ok.bottom() - 1;
        QVERIFY2(gap >= 8,
                 qPrintable(QStringLiteral("only %1px between OK and Cancel; want >= 8px of space")
                                .arg(gap)));
        dock.reject();
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstFiledlg tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_filedlg.moc"
