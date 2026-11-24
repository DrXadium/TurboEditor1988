// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-20: single-quoted text must use #40A769 in files that HAVE a syntax
// mode, not just in mode-None documents.
//
// The user's requirement: "the text in single quotes should be #40A769". In a
// mode-None document our MovingRange pass does this. In a code file the
// highlighter owns the colour, and a range attribute cannot recolour text the
// highlighter has styled (measured: a magenta range over a string span changed
// nothing). The fix is therefore in the theme: Kate assigns Char to single-quoted
// literals, so crt_tv.theme's Char must be #40A769 while String stays #55FF55.
//
// This suite pins the theme file itself, so a silent revert fails the gate.
#include <QtTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QStyle>
#include <QToolButton>
#include <QWidget>
#include <QHBoxLayout>
#include "src/shell/BorlandStyle.h"

// 2026-11-22 (round 8): KTextEditor's crash-recovery prompt ("The file was not
// closed properly.") is NOT a dialog - it is an inline KateMessageWidget bar
// inside the view, so the house QDialog/QMessageBox rules never reached it and
// it inherited the global QWidget rule (blue #0000AA, editor-yellow #FFFF55).
// The test double carries the SAME class name because that is what a QSS type
// selector matches on: it proves the rule reaches the widget and its children.
class KateMessageWidget : public QWidget
{
    Q_OBJECT
public:
    using QWidget::QWidget;
};

class TstTheme : public QObject
{
    Q_OBJECT

    static QJsonObject loadTheme()
    {
        QFile f(QStringLiteral(CRT_KATE_SOURCE_DIR
                               "/src/editor/tvtheme/themes/crt_tv.theme"));
        if (!f.open(QIODevice::ReadOnly))
            return QJsonObject();
        return QJsonDocument::fromJson(f.readAll()).object();
    }

private slots:
    void initTestCase()
    {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    // The theme must be a readable KSyntaxHighlighting theme JSON document.
    void the_theme_file_is_valid_json()
    {
        const QJsonObject o = loadTheme();
        QVERIFY2(!o.isEmpty(), "crt_tv.theme could not be parsed");
        QCOMPARE(o.value(QStringLiteral("metadata")).toObject()
                     .value(QStringLiteral("name")).toString(),
                 QStringLiteral("CRTKTV"));
    }

    // THE assertion: single-quoted literals (Kate's Char style) are the user's
    // single-quote colour, and double-quoted text (String) is NOT.
    void single_quoted_text_uses_the_single_quote_colour()
    {
        const QJsonObject styles = loadTheme().value(QStringLiteral("text-styles"))
                                       .toObject();
        const QString single = styles.value(QStringLiteral("Char")).toObject()
                                   .value(QStringLiteral("text-color")).toString();
        const QString dbl = styles.value(QStringLiteral("String")).toObject()
                                .value(QStringLiteral("text-color")).toString();

        QCOMPARE(single.toLower(), QStringLiteral("#40a769"));
        // Double-quoted text keeps the green, so the two shades differ.
        QCOMPARE(dbl.toLower(), QStringLiteral("#55ff55"));
        QVERIFY2(single.compare(dbl, Qt::CaseInsensitive) != 0,
                 "both quote kinds share a colour - the nesting cue is lost");
    }

    // Related single-quoted styles follow Char so a literal reads as one shade.
    void related_single_quoted_styles_follow_char()
    {
        const QJsonObject styles = loadTheme().value(QStringLiteral("text-styles"))
                                       .toObject();
        const QString single = styles.value(QStringLiteral("Char")).toObject()
                                   .value(QStringLiteral("text-color")).toString().toLower();
        // SpecialChar is the escape-sequence flavour of a char literal.
        QCOMPARE(styles.value(QStringLiteral("SpecialChar")).toObject()
                     .value(QStringLiteral("text-color")).toString().toLower(),
                 single);
    }

    // Sanity: the palette the user sees must not be empty for strings either.
    void double_quoted_text_still_has_a_colour()
    {
        const QJsonObject styles = loadTheme().value(QStringLiteral("text-styles"))
                                       .toObject();
        QVERIFY(!styles.value(QStringLiteral("String")).toObject()
                   .value(QStringLiteral("text-color")).toString().isEmpty());
    }

    // Round 8: the recovery bar must read as a house panel (grey field, black
    // ink, TV plates on its action buttons) instead of the global blue/yellow.
    void the_crash_recovery_bar_is_styled_as_a_house_panel()
    {
        QFile f(QStringLiteral(CRT_KATE_SOURCE_DIR "/src/shell/borland.qss"));
        QVERIFY2(f.open(QIODevice::ReadOnly), "borland.qss must be readable");
        const QString qss = QString::fromUtf8(f.readAll());
        qApp->setStyleSheet(qss);

        KateMessageWidget bar;
        auto* text = new QLabel(QStringLiteral("The file was not closed properly."), &bar);
        auto* recover = new QToolButton(&bar);
        recover->setText(QStringLiteral("Recover Data"));
        bar.show();
        QCoreApplication::processEvents();

        QCOMPARE(bar.palette().color(QPalette::Window), QColor(0xAA, 0xAA, 0xAA));
        QCOMPARE(text->palette().color(QPalette::WindowText), QColor(0x00, 0x00, 0x00));
        // The bar's own label ink is black. The ACTION button's label ink is
        // black too, but it is painted by BorlandStyle::drawControl
        // (CE_ToolButtonLabel), not carried in QPalette::ButtonText - without a
        // QSS rule the widget palette keeps the app's editor yellow, so a
        // palette read would be misleading. Its black ink is pinned by pixel in
        // the notch test below (black glyphs on the green plate).

        // NO QSS rule may match the action button: any rule routes it through
        // QStyleSheetStyle, which owns the panel and never calls the style
        // engine (proved: PE_PanelButtonTool is not invoked while a rule exists),
        // so the notch would silently disappear.
        QVERIFY2(!qss.contains(QStringLiteral("KateMessageWidget QToolButton")),
                 "a QSS rule must not match the action button - it would bypass "
                 "BorlandStyle and drop the notch");
        QVERIFY2(!qss.contains(QStringLiteral("KMessageWidget QToolButton")),
                 "a QSS rule must not match the action button - it would bypass "
                 "BorlandStyle and drop the notch");

        qApp->setStyleSheet(QString());
    }

    // Round 9 (the report's two red arrows): the reference plate's shadow is a
    // black rect TRANSLATED (+4,+4) BEHIND the plate (TvPlate::paintEvent /
    // TvFileDock::paintEvent fillRect(cell.translated(4,4))). So the shadow is
    // hidden by the plate at the top-right and bottom-left - those two corners
    // are the PANE's grey, not black. The QSS border draws a full-height arm
    // flush to every plate edge instead, so it puts black in exactly those two
    // corners. This pins the reference notch by pixel: shadow present right and
    // below, plate green inside, and BOTH corner gutters grey.
    void the_recovery_bar_shadow_is_an_offset_notch_not_a_full_border()
    {
        QFile f(QStringLiteral(CRT_KATE_SOURCE_DIR "/src/shell/borland.qss"));
        QVERIFY2(f.open(QIODevice::ReadOnly), "borland.qss must be readable");
        qApp->setStyleSheet(QString::fromUtf8(f.readAll()));

        // Production paints this plate in BorlandStyle (QStyle runs DURING the
        // button's paint, the one place an event filter cannot reach - a filter
        // always runs before the handler and is overwritten). Install the same
        // style the app installs so this suite exercises the real mechanism.
        // setStyle(NEW ...) gives QApplication ownership, so the style outlives
        // this scope - a stack-local would dangle when it is destroyed.
        qApp->setStyle(new BorlandStyle);

        KateMessageWidget bar;
        bar.resize(400, 60);
        auto* btn = new QToolButton(&bar);
        btn->setText(QStringLiteral("Recover Data"));
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        // Button rect = PLATE + the 4px shadow band (TvPlate's model), so the
        // notch lives inside the widget and the probe points are deterministic.
        btn->setGeometry(40, 14, 124, 28);
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar));
        QCoreApplication::processEvents();

        const QImage img = bar.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const QRect g = btn->geometry();
        // The green plate is the button rect inset by the 4px shadow band on
        // right+bottom, plus the 8px grey gap the style reserves right of the
        // plate (2026-11-24: neighbouring plates must not touch). The black
        // shadow band fills the vacated 4px, offset so it hides behind the
        // plate at the top-right and bottom-left.
        const QRect plate = g.adjusted(0, 0, -12, -4);
        const auto isGreen = [](const QColor& c) {
            return c.green() > 120 && c.red() < 120 && c.blue() < 120;
        };
        const auto isBlack = [](const QColor& c) {
            return c.red() < 12 && c.green() < 12 && c.blue() < 12;
        };
        const auto isGrey = [](const QColor& c) {
            return qAbs(c.red() - 0xAA) < 16 && qAbs(c.green() - 0xAA) < 16
                   && qAbs(c.blue() - 0xAA) < 16;
        };

        // The plate is green inside (not swallowed by the shadow, no label ink
        // at the left edge).
        const QColor inside = img.pixelColor(plate.left() + 3, plate.center().y());
        QVERIFY2(isGreen(inside),
                 qPrintable(QStringLiteral("plate not green: %1,%2,%3")
                                .arg(inside.red()).arg(inside.green()).arg(inside.blue())));

        // Shadow present: black in the band right of and below the plate. The
        // right band starts at plate.right()+1 (the rect also carries the grey
        // gap after it, so g.right() is no longer inside the shadow).
        const QColor right = img.pixelColor(plate.right() + 2, plate.center().y());
        QVERIFY2(isBlack(right),
                 qPrintable(QStringLiteral("no shadow in right band: %1,%2,%3")
                                .arg(right.red()).arg(right.green()).arg(right.blue())));
        const QColor below = img.pixelColor(plate.center().x(), g.bottom() - 2);
        QVERIFY2(isBlack(below),
                 qPrintable(QStringLiteral("no shadow in bottom band: %1,%2,%3")
                                .arg(below.red()).arg(below.green()).arg(below.blue())));

        // THE NOTCH (the report's two red arrows): the shadow is offset +4,+4 so
        // it is hidden BEHIND the plate at the top-right and bottom-left - those
        // corners are the pane's grey, not black. A full-height QSS border arm
        // fails both of these.
        const QColor topRight = img.pixelColor(plate.right() + 2, plate.top() + 1);
        QVERIFY2(isGrey(topRight),
                 qPrintable(QStringLiteral("top-right gutter is %1,%2,%3 - want pane grey "
                                           "(shadow offset down, hidden behind plate)")
                                .arg(topRight.red()).arg(topRight.green()).arg(topRight.blue())));
        const QColor bottomLeft = img.pixelColor(g.left() + 1, g.bottom() - 2);
        QVERIFY2(isGrey(bottomLeft),
                 qPrintable(QStringLiteral("bottom-left gutter is %1,%2,%3 - want pane grey "
                                           "(shadow offset right, hidden behind plate)")
                                .arg(bottomLeft.red()).arg(bottomLeft.green()).arg(bottomLeft.blue())));

        qApp->setStyleSheet(QString());
    }

    // Reported 2026-11-24: the crash-recovery bar's action plates (View
    // Changes / Recover Data / Discard) were laid out flush, so one plate's
    // 4px black block shadow ran right into the next plate's green edge. The
    // reference bar separates every plate from its neighbour with pane grey.
    void the_recovery_bar_action_plates_are_separated()
    {
        QFile f(QStringLiteral(CRT_KATE_SOURCE_DIR "/src/shell/borland.qss"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        qApp->setStyleSheet(QString::fromUtf8(f.readAll()));
        qApp->setStyle(new BorlandStyle);   // the same style the app installs

        // Laid out for real, exactly as KMessageWidget does it: a left-to-right
        // box, spacing 0, buttons sized from their size hints. The style has to
        // reserve the room - asking the buttons to overlap would prove nothing.
        KateMessageWidget bar;
        auto* lay = new QHBoxLayout(&bar);
        lay->setContentsMargins(10, 16, 10, 16);
        lay->setSpacing(0);
        auto* a = new QToolButton(&bar);
        a->setText(QStringLiteral("View Changes"));
        a->setToolButtonStyle(Qt::ToolButtonTextOnly);
        auto* b = new QToolButton(&bar);
        b->setText(QStringLiteral("Recover Data"));
        b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        auto* c = new QToolButton(&bar);
        c->setText(QStringLiteral("Discard"));
        c->setToolButtonStyle(Qt::ToolButtonTextOnly);
        lay->addWidget(a);
        lay->addWidget(b);
        lay->addWidget(c);
        lay->addStretch(1);
        bar.resize(bar.sizeHint());
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar));
        QCoreApplication::processEvents();

        // The plates are the green inset by the 4px shadow band (BorlandStyle
        // PE_PanelButtonTool), so the gap that matters is grey BETWEEN the
        // shadow band of one plate and the green of the next.
        const QRect plateA = a->geometry().adjusted(0, 0, -12, -4);
        const QRect plateB = b->geometry().adjusted(0, 0, -12, -4);
        const QRect plateC = c->geometry().adjusted(0, 0, -12, -4);
        QVERIFY(plateA.right() < plateB.left());
        QVERIFY(plateB.right() < plateC.left());

        const QImage img = bar.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const auto isBlack = [](const QColor& c) {
            return c.red() < 12 && c.green() < 12 && c.blue() < 12;
        };
        const auto isGrey = [](const QColor& c) {
            return qAbs(c.red() - 0xAA) < 16 && qAbs(c.green() - 0xAA) < 16
                   && qAbs(c.blue() - 0xAA) < 16;
        };

        // Shadow arm ends 4px right of plate A; everything after it up to
        // plate B must be grey.
        int greyAB = 0;
        for (int x = plateA.right() + 5; x <= plateB.left() - 1; ++x) {
            const QColor px = img.pixelColor(x, plateB.center().y());
            if (isGrey(px))
                ++greyAB;
        }
        QVERIFY2(greyAB >= 4,
                 qPrintable(QStringLiteral("only %1 grey px between plate A's shadow and plate "
                                           "B - they touch").arg(greyAB)));
        QVERIFY2(!isBlack(img.pixelColor(plateB.left() - 1, plateB.center().y())),
                 "plate B's left edge is black: plate A's shadow runs into it");

        int greyBC = 0;
        for (int x = plateB.right() + 5; x <= plateC.left() - 1; ++x) {
            const QColor px = img.pixelColor(x, plateC.center().y());
            if (isGrey(px))
                ++greyBC;
        }
        QVERIFY2(greyBC >= 4,
                 qPrintable(QStringLiteral("only %1 grey px between plate B's shadow and plate "
                                           "C - they touch").arg(greyBC)));

        // The shadows themselves survive: they must not be deleted to make room.
        QVERIFY2(isBlack(img.pixelColor(plateA.right() + 2, plateA.center().y())),
                 "plate A lost its right shadow band");
        QVERIFY2(isBlack(img.pixelColor(plateA.center().x(), a->geometry().bottom() - 2)),
                 "plate A lost its bottom shadow band");

        qApp->setStyleSheet(QString());
    }
};

QTEST_MAIN(TstTheme)
#include "tst_theme.moc"
