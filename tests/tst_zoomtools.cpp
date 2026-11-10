// SPDX-License-Identifier: GPL-3.0-or-later
// Task 5: KTextEditor tabs + zoom + tools contract test.
// Proves EditorTabManager opens documents, drives zoom (font point size via
// the view's "font" config), and applies text tools (change case, sort lines).
#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QAction>
#include <QWidget>
#include <ktexteditor/editor.h>
#include <ktexteditor/view.h>
#include <ktexteditor/document.h>
#include "src/editor/EditorTabManager.h"

class TstZoomTools : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        // Headless-safe (spike pattern): fall back to offscreen if no display.
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
        QVERIFY2(KTextEditor::Editor::instance(),
                 "KTextEditor::Editor::instance() returned null");
    }

    void new_document_creates_view() {
        EditorTabManager m;
        m.newDocument();
        QVERIFY2(m.activeView(), "no active view after newDocument()");
        QVERIFY(m.activeView()->document());
        QCOMPARE(m.activeView()->document()->text(), QString());
    }

    void open_file_reads_content() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("sample.txt");
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream out(&f);
            out << "hello\nworld\n";
        }
        EditorTabManager m;
        m.openFile(QUrl::fromLocalFile(path));
        QVERIFY2(m.activeView(), "no active view after openFile()");
        QCOMPARE(m.activeView()->document()->text(),
                 QStringLiteral("hello\nworld\n"));
    }

    void zoom_in_increases_font_point_size() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        const int base = v->configValue("font").value<QFont>().pointSize();
        QVERIFY2(base > 0, "initial font point size <= 0");

        m.setZoom(+1);
        const int up = v->configValue("font").value<QFont>().pointSize();
        QVERIFY2(up > base,
                 qPrintable(QStringLiteral("zoom-in: %1 <= base %2").arg(up).arg(base)));

        m.setZoom(-1);
        const int down = v->configValue("font").value<QFont>().pointSize();
        QVERIFY2(down < up,
                 qPrintable(QStringLiteral("zoom-out: %1 >= up %2").arg(down).arg(up)));
    }

    void change_case_upper_uppercases_selection() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        v->document()->setText(QStringLiteral("lower case words"));
        // Select the whole first line.
        v->setSelection(KTextEditor::Range(0, 0, 0, 16));
        QVERIFY(v->selection());

        m.changeCase(EditorTabManager::Upper);
        QCOMPARE(v->document()->text(), QStringLiteral("LOWER CASE WORDS"));
    }

    void container_hosts_active_view() {
        EditorTabManager m;
        QVERIFY(m.containerWidget());
        // Views are reparented into the container's stacked widget.
        QCOMPARE(m.containerWidget()->count(), m.tabCount());
        QVERIFY(m.activeView()->parentWidget());
    }

    void tabs_changed_signal_fires_on_new_document() {
        EditorTabManager m;
        QSignalSpy spy(&m, &EditorTabManager::tabsChanged);
        m.newDocument();
        QVERIFY2(spy.count() >= 1, "tabsChanged not emitted on newDocument");
        const QStringList titles = spy.last().at(0).toStringList();
        QCOMPARE(titles.size(), m.tabCount());
        QVERIFY(m.tabCount() >= 2);   // initial + new
    }

    void install_actions_creates_zoom_and_tool_shortcuts() {
        QWidget host;
        EditorTabManager m;
        m.newDocument();
        m.installActions(&host);

        // Zoom shortcuts: Ctrl+= (in), Ctrl+- (out), Ctrl+0 (reset).
        auto hasShortcut = [&host](const QString& seq) {
            const auto acts = host.findChildren<QAction*>();
            for (QAction* a : acts)
                if (a->shortcut().toString() == seq)
                    return true;
            return false;
        };
        QVERIFY2(hasShortcut(QStringLiteral("Ctrl+=")), "no Ctrl+= zoom-in action");
        QVERIFY2(hasShortcut(QStringLiteral("Ctrl+-")), "no Ctrl+- zoom-out action");
        QVERIFY2(hasShortcut(QStringLiteral("Ctrl+0")), "no Ctrl+0 zoom-reset action");
        // Tool actions: change case + sort.
        QVERIFY2(host.findChildren<QAction*>(
                     QString(), Qt::FindDirectChildrenOnly).size() >= 5,
                 "expected >=5 actions (3 zoom + case/sort)");
    }

    void zoom_shortcut_actually_changes_font() {
        QWidget host;
        EditorTabManager m;
        m.newDocument();
        m.installActions(&host);
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        const int base = v->configValue("font").value<QFont>().pointSize();

        // Trigger the Ctrl+= action; font must grow.
        for (QAction* a : host.findChildren<QAction*>()) {
            if (a->shortcut().toString() == QStringLiteral("Ctrl+=")) {
                a->trigger();
                break;
            }
        }
        const int up = v->configValue("font").value<QFont>().pointSize();
        QVERIFY2(up > base, "Ctrl+= action did not zoom in");
    }

    void sort_lines_orders_document() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        v->document()->setText(QStringLiteral("cherry\napple\nbanana\n"));

        m.sortLines();
        QCOMPARE(v->document()->text(), QStringLiteral("apple\nbanana\ncherry\n"));
    }
};

int main(int argc, char** argv)
{
    if (qgetenv("QT_QPA_PLATFORM").isEmpty())
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TstZoomTools tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_zoomtools.moc"