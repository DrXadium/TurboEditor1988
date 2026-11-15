// SPDX-License-Identifier: GPL-3.0-or-later
// Status-line selectors (F-row indicators + dropdowns).
// Task 1: the Sonnet dictionary list must be reachable (links KF6::SonnetCore).
#include <QtTest>
#include <sonnet/speller.h>
#include <QImage>
#include <QMenu>
#include "src/editor/StatusSelectors.h"
#include <ktexteditor/editor.h>
#include <ktexteditor/document.h>

namespace {
const QColor kOvrRed(0xAA, 0x00, 0x00);

// Fraction of pixels near `want` inside the widget's grab.
double inkFraction(const QImage& img, const QColor& want, int tol = 24)
{
    int hits = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (qAbs(c.red() - want.red()) <= tol && qAbs(c.green() - want.green()) <= tol &&
                qAbs(c.blue() - want.blue()) <= tol)
                ++hits;
        }
    return img.isNull() ? 0.0 : double(hits) / double(img.width() * img.height());
}
} // namespace

class TstStatusSel : public QObject
{
    Q_OBJECT
private slots:
    void spell_list_is_nonempty_when_dicts_exist()
    {
        Sonnet::Speller speller;
        const QMap<QString, QString> dicts = speller.availableDictionaries();
        QVERIFY2(!dicts.isEmpty(),
                 "no spell dictionaries found (hunspell dicts should be in /usr/share/hunspell)");
    }

    // Task 2: the INS/OVR indicator. OVR must be drawn RED (#AA0000); INS must
    // NOT be red (it uses the bar's normal ink).
    void ovr_indicator_is_red_and_ins_is_not()
    {
        StatusSelectors s;
        s.resize(240, 20);
        s.show();
        QVERIFY(QTest::qWaitForWindowExposed(&s));

        s.setOverwrite(true);
        QCoreApplication::processEvents();
        const QImage ovrImg = s.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const double ovrRed = inkFraction(ovrImg, kOvrRed);
        QVERIFY2(ovrRed > 0.0,
                 qPrintable(QStringLiteral("OVR indicator has no red ink (%1% of pixels)")
                                .arg(ovrRed * 100.0, 0, 'f', 2)));

        s.setOverwrite(false);
        QCoreApplication::processEvents();
        const QImage insImg = s.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const double insRed = inkFraction(insImg, kOvrRed);
        QVERIFY2(insRed == 0.0,
                 qPrintable(QStringLiteral("INS indicator is red (%1% of pixels) - only OVR is red")
                                .arg(insRed * 100.0, 0, 'f', 2)));
    }

    // Task 3: the doc-type label shows the document's highlighting mode.
    void doc_type_label_shows_current_mode()
    {
        KTextEditor::Document* doc = KTextEditor::Editor::instance()->createDocument(nullptr);
        QVERIFY(doc);
        doc->setText(QStringLiteral("int main() {}"));
        QVERIFY2(doc->setHighlightingMode(QStringLiteral("C++")),
                 "C++ highlighting mode not available");

        StatusSelectors s;
        s.setDocument(doc);
        QCOMPARE(s.docTypeText(), QStringLiteral("C++"));

        // Switch to a guaranteed-valid alternate mode (hardcoded names like
        // "Plain Text" are not what Kate reports; pick from the real list).
        QString alt;
        for (const QString& m : doc->highlightingModes())
            if (m != QStringLiteral("C++")) { alt = m; break; }
        QVERIFY2(!alt.isEmpty(), "no alternate highlighting mode available");
        QVERIFY2(doc->setHighlightingMode(alt), "could not switch highlighting mode");
        QCOMPARE(s.docTypeText(), alt);
        delete doc;
    }

    // Task 3: the dropdown lists the available modes and picking one applies it.
    void doc_type_dropdown_lists_modes_and_applies_pick()
    {
        KTextEditor::Document* doc = KTextEditor::Editor::instance()->createDocument(nullptr);
        QVERIFY(doc);
        doc->setText(QStringLiteral("int main() {}"));

        StatusSelectors s;
        s.setDocument(doc);

        // Open the dropdown by clicking the doc-type indicator.
        QTest::mouseClick(&s, Qt::LeftButton, Qt::NoModifier, s.docTypeCenter());
        QCoreApplication::processEvents();

        // The popup must be open and offer every mode the document can take.
        QMenu* menu = s.openMenu();
        QVERIFY2(menu, "clicking the doc-type indicator did not open a menu");
        QVERIFY(menu->isVisible());
        const QStringList modes = doc->highlightingModes();
        QVERIFY2(modes.size() > 1, "document reports too few highlighting modes");
        QStringList menuTexts;
        for (QAction* a : menu->actions())
            menuTexts << a->text();
        for (const QString& m : modes)
            QVERIFY2(menuTexts.contains(m),
                     qPrintable(QStringLiteral("mode '%1' missing from the dropdown").arg(m)));

        // Pick a mode different from the current one; the document must switch.
        const QString before = doc->highlightingMode();
        QString pick;
        for (const QString& m : modes)
            if (m != before) { pick = m; break; }
        QVERIFY2(!pick.isEmpty(), "no alternate mode to pick");
        menu->actions().at(menuTexts.indexOf(pick))->trigger();
        QCoreApplication::processEvents();
        QCOMPARE(doc->highlightingMode(), pick);

        menu->hide();
        delete doc;
    }

    // Task 4: the encoding label shows the document's encoding.
    void encoding_label_shows_current_encoding()
    {
        KTextEditor::Document* doc = KTextEditor::Editor::instance()->createDocument(nullptr);
        QVERIFY(doc);
        doc->setText(QStringLiteral("hello"));
        QVERIFY2(doc->setEncoding(QStringLiteral("UTF-8")), "could not set UTF-8");

        StatusSelectors s;
        s.setDocument(doc);
        QCOMPARE(s.encodingText(), QStringLiteral("UTF-8"));
        delete doc;
    }

    // Task 4 (data-loss guard): picking an encoding RECORDS it for the next
    // save/reload but must NOT touch the open buffer. Kate's setEncoding only
    // takes effect on reload; re-decoding here would drop unsaved edits.
    void encoding_pick_records_without_touching_text()
    {
        KTextEditor::Document* doc = KTextEditor::Editor::instance()->createDocument(nullptr);
        QVERIFY(doc);
        const QString seeded = QStringLiteral("unsaved edits stay put");
        doc->setText(seeded);
        // Pin a known starting encoding so the pick is unambiguous (a fresh
        // doc's encoding() is "" and Kate may normalize it after setText).
        QVERIFY2(doc->setEncoding(QStringLiteral("UTF-8")), "could not set UTF-8");

        StatusSelectors s;
        s.setDocument(doc);

        QTest::mouseClick(&s, Qt::LeftButton, Qt::NoModifier, s.encodingCenter());
        QCoreApplication::processEvents();
        QMenu* menu = s.openMenu();
        QVERIFY2(menu && menu->isVisible(), "clicking the encoding indicator opened no menu");

        // Pick an encoding different from the current one.
        const QString before = doc->encoding();
        QString pick;
        QAction* pickAction = nullptr;
        for (QAction* a : menu->actions()) {
            const QString t = a->text();
            // Skip "Default" (its stored value is "", not its display text);
            // pick a real encoding whose display text equals its stored value.
            if (t != before && !t.isEmpty() && t != QStringLiteral("Default")) {
                pick = t; pickAction = a; break;
            }
        }
        QVERIFY2(!pick.isEmpty(), "no alternate encoding to pick");
        pickAction->trigger();
        QCoreApplication::processEvents();

        // The encoding was recorded...
        QCOMPARE(doc->encoding(), pick);
        // ...and the buffer is byte-for-byte untouched (no reload, no re-decode).
        QCOMPARE(doc->text(), seeded);

        menu->hide();
        delete doc;
    }
};

QTEST_MAIN(TstStatusSel)
#include "tst_statussel.moc"
