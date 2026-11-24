#include <QtTest>
#include <QSignalSpy>
#include <ktexteditor/editor.h>
#include <ktexteditor/document.h>
#include <ktexteditor/view.h>
#include <KActionCollection>
#include "src/editor/SwatchApply.h"
#include "src/shell/HexColorScan.h"

// Applying a picked colour to the document (2026-11-23).
//
// The click path ends here: whatever the picker says must land on EXACTLY the
// token that was clicked, as ONE undoable edit, and a pick that changes nothing
// must not touch the document at all (an empty edit would still dirty a saved
// file and push an undo step the user never asked for).
class TstSwatchApply : public QObject
{
    Q_OBJECT
private slots:
    void replaces_exactly_the_token()
    {
        KTextEditor::Document* doc = KTextEditor::Editor::instance()->createDocument(this);
        doc->setText(QStringLiteral("a #ff0000 b"));
        const auto tokens = scanHexColors(doc->line(0));
        QCOMPARE(tokens.size(), 1);

        QVERIFY(applySwatchColor(doc, 0, tokens.at(0), QColor(0x00, 0xff, 0x00)));
        QCOMPARE(doc->line(0), QStringLiteral("a #00ff00 b"));
    }

    void token_at_end_of_line_is_replaced()
    {
        KTextEditor::Document* doc = KTextEditor::Editor::instance()->createDocument(this);
        doc->setText(QStringLiteral("color:#ff0000"));
        const auto tokens = scanHexColors(doc->line(0));

        QVERIFY(applySwatchColor(doc, 0, tokens.at(0), QColor(0x12, 0x34, 0x56)));
        QCOMPARE(doc->line(0), QStringLiteral("color:#123456"));
    }

    void the_edit_is_one_undo_step()
    {
        KTextEditor::Document* doc = KTextEditor::Editor::instance()->createDocument(this);
        doc->setText(QStringLiteral("a #ff0000 b"));
        const auto tokens = scanHexColors(doc->line(0));
        QVERIFY(applySwatchColor(doc, 0, tokens.at(0), QColor(0x00, 0x00, 0xff)));
        QCOMPARE(doc->line(0), QStringLiteral("a #0000ff b"));

        // KTextEditor keeps undo on the VIEW (its edit_undo action), and Kate
        // leaves those actions disabled until a view gets focus - the app
        // enables them on the same reasoning (EditorTabManager::
        // triggerActiveViewShortcut).
        KTextEditor::View* view = doc->createView(nullptr);
        QAction* undo = view->actionCollection()->action(QStringLiteral("edit_undo"));
        QVERIFY2(undo, "no edit_undo action");
        undo->setEnabled(true);
        undo->trigger();
        QCOMPARE(doc->line(0), QStringLiteral("a #ff0000 b"));
    }

    void an_unchanged_colour_touches_nothing()
    {
        KTextEditor::Document* doc = KTextEditor::Editor::instance()->createDocument(this);
        doc->setText(QStringLiteral("a #ff0000 b"));
        const auto tokens = scanHexColors(doc->line(0));

        QSignalSpy changes(doc, &KTextEditor::Document::textChanged);
        QVERIFY(!applySwatchColor(doc, 0, tokens.at(0), QColor(0xff, 0x00, 0x00)));
        QCOMPARE(changes.count(), 0);
        QCOMPARE(doc->line(0), QStringLiteral("a #ff0000 b"));
    }

    void a_short_token_is_rewritten_as_six_digits()
    {
        KTextEditor::Document* doc = KTextEditor::Editor::instance()->createDocument(this);
        doc->setText(QStringLiteral("c:#abc"));
        const auto tokens = scanHexColors(doc->line(0));

        QVERIFY(applySwatchColor(doc, 0, tokens.at(0), QColor(0x00, 0x11, 0x22)));
        QCOMPARE(doc->line(0), QStringLiteral("c:#001122"));
    }
};

QTEST_MAIN(TstSwatchApply)
#include "tst_swatchapply.moc"
