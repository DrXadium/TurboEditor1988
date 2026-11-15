// SPDX-License-Identifier: GPL-3.0-or-later
#include "StatusSelectors.h"
#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QMenu>
#include <QStringConverter>
#include "src/shell/BorlandMenu.h"
#include <sonnet/speller.h>
#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

namespace {
// Bar ink: the F-row's own black text color (TV status line words are black).
const QColor kInk(0x00, 0x00, 0x00);
// OVR red: the same #AA0000 the F-key names use, so it reads as part of the
// bar's language (spec: INS normal, OVR red).
const QColor kOvrRed(0xAA, 0x00, 0x00);
// One space between indicators, in pixels (a cell's worth of daylight).
const int kGap = 8;
// Boundary between two cells: prev.right() + 1 + kGap.
const int kBoundary = kGap + 1;
} // namespace

StatusSelectors::StatusSelectors(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("statusSelectors"));
    setAutoFillBackground(false);   // FKeyStatusBar's gray shows through
    setAttribute(Qt::WA_TranslucentBackground, true);
}

void StatusSelectors::setOverwrite(bool ovr)
{
    if (m_ovr == ovr)
        return;
    m_ovr = ovr;
    update();
}

void StatusSelectors::setDocument(KTextEditor::Document* doc)
{
    if (m_doc == doc)
        return;
    m_doc = doc;
    update();
    updateGeometry();   // docType/encoding text feeds sizeHint - re-measure
}

void StatusSelectors::setView(KTextEditor::View* view)
{
    if (m_view == view)
        return;
    m_view = view;
    update();
}

void StatusSelectors::setSpellLanguage(const QString& code)
{
    if (m_spell == code)
        return;
    m_spell = code;
    update();
    updateGeometry();   // the spell description can be long - re-measure
}

void StatusSelectors::setSpeller(Sonnet::Speller* speller)
{
    if (m_speller == speller)
        return;
    m_speller = speller;
    update();
    updateGeometry();   // dictionary description changes the label - re-measure
}

QString StatusSelectors::overwriteText() const
{
    return m_ovr ? QStringLiteral("OVR") : QStringLiteral("INS");
}

QString StatusSelectors::docTypeText() const
{
    if (!m_doc)
        return QStringLiteral("-");
    const QString mode = m_doc->highlightingMode();
    return mode.isEmpty() ? QStringLiteral("-") : mode;
}

QString StatusSelectors::encodingText() const
{
    if (!m_doc)
        return QStringLiteral("-");
    const QString enc = m_doc->encoding();
    return enc.isEmpty() ? QStringLiteral("Default") : enc;
}

QString StatusSelectors::spellText() const
{
    if (m_spell.isEmpty())
        return QStringLiteral("-");
    // Show the human-readable description ("English (United States)"), not the
    // raw code ("en_US"), when the speller can map it.
    if (m_speller) {
        const auto dicts = m_speller->availableDictionaries();
        const QString desc = dicts.key(m_spell);
        if (!desc.isEmpty())
            return desc;
    }
    return m_spell;
}

QSize StatusSelectors::sizeHint() const
{
    const QFontMetrics fm(font());
    // spell + encoding + docType + overwrite, each with a gap between.
    const int w = fm.horizontalAdvance(spellText()) + fm.horizontalAdvance(encodingText()) +
                  fm.horizontalAdvance(docTypeText()) + fm.horizontalAdvance(overwriteText()) +
                  3 * kBoundary;
    return QSize(w, qMax(10, fm.height()));
}

// The status bar must NEVER squeeze us below our content: a stretch-0 widget
// is laid out at its minimumSizeHint when space is tight, and a 60px minimum
// let QStatusBar crush all four labels into a sliver (they rendered as tiny
// clipped clusters). Reserve the full natural sizeHint instead - the F-key
// label (stretch 1) elides first on narrow windows, exactly as the bar's own
// comment promises.
QSize StatusSelectors::minimumSizeHint() const
{
    return sizeHint();
}

QPoint StatusSelectors::docTypeCenter() const
{
    return docTypeRect().center();
}

QPoint StatusSelectors::encodingCenter() const
{
    return encodingRect().center();
}

QPoint StatusSelectors::spellCenter() const
{
    return spellRect().center();
}

// Each indicator is a cell sized to its text, laid out left to right with a
// gap. When the widget is narrower than the content (the F-row squeezed on a
// small window), the cells shrink PROPORTIONALLY so every label stays inside
// the row - the paint step then elides each label to its cell. Natural width
// (wide widget) is unchanged.
std::array<int, 4> StatusSelectors::cellWidths() const
{
    const QFontMetrics fm(font());
    std::array<int, 4> nat{fm.horizontalAdvance(spellText()),
                           fm.horizontalAdvance(encodingText()),
                           fm.horizontalAdvance(docTypeText()),
                           fm.horizontalAdvance(overwriteText())};
    const int boundaries = 3 * kBoundary;
    const int content = nat[0] + nat[1] + nat[2] + nat[3];
    const int avail = qMax(0, width() - boundaries);
    if (content <= avail || content == 0)
        return nat;
    // Shrink proportionally, floor at 0.
    const double scale = double(avail) / double(content);
    std::array<int, 4> out;
    for (int i = 0; i < 4; ++i)
        out[i] = qRound(nat[i] * scale);
    return out;
}

// Cell origins: cumulative widths + boundaries. Returns are widget-local.
QRect StatusSelectors::spellRect() const
{
    const auto w = cellWidths();
    return QRect(0, 0, w[0], height());
}

QRect StatusSelectors::encodingRect() const
{
    const auto w = cellWidths();
    return QRect(spellRect().right() + kBoundary, 0, w[1], height());
}

QRect StatusSelectors::docTypeRect() const
{
    const auto w = cellWidths();
    return QRect(encodingRect().right() + kBoundary, 0, w[2], height());
}

QRect StatusSelectors::overwriteRect() const
{
    const auto w = cellWidths();
    return QRect(docTypeRect().right() + kBoundary, 0, w[3], height());
}

void StatusSelectors::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, false);
    const QFontMetrics fm(font());
    // Elide each label to its (possibly shrunk) cell so nothing overflows.
    auto elide = [&fm](const QRect& r, const QString& t) {
        return fm.elidedText(t, Qt::ElideRight, r.width());
    };

    // Spell language (leftmost).
    p.setPen(kInk);
    p.drawText(spellRect(), Qt::AlignCenter, elide(spellRect(), spellText()));
    // Encoding.
    p.drawText(encodingRect(), Qt::AlignCenter, elide(encodingRect(), encodingText()));
    // Document type (highlighting mode).
    p.drawText(docTypeRect(), Qt::AlignCenter, elide(docTypeRect(), docTypeText()));
    // INS/OVR: red only when OVR.
    p.setPen(m_ovr ? kOvrRed : kInk);
    p.drawText(overwriteRect(), Qt::AlignCenter, elide(overwriteRect(), overwriteText()));
}

void StatusSelectors::mousePressEvent(QMouseEvent* ev)
{
    if (overwriteRect().contains(ev->pos())) {
        emit overwriteToggled(!m_ovr);
        return;
    }
    if (docTypeRect().contains(ev->pos())) {
        openDocTypeMenu();
        return;
    }
    if (encodingRect().contains(ev->pos())) {
        openEncodingMenu();
        return;
    }
    if (spellRect().contains(ev->pos())) {
        openSpellMenu();
        return;
    }
    QWidget::mousePressEvent(ev);
}

// The doc-type dropdown: every highlighting mode the document can take, as a
// TV-styled scrollable menu. popup() (never exec() - the shell is never mapped
// and exec()'s nested loop hangs forever).
void StatusSelectors::openDocTypeMenu()
{
    if (!m_doc)
        return;
    delete m_menu;
    m_menu = new BorlandMenu(this);   // TV dropdown, same as the menu bar's
    const QStringList modes = m_doc->highlightingModes();
    const QString current = m_doc->highlightingMode();
    for (const QString& m : modes) {
        QAction* a = m_menu->addAction(m);
        a->setCheckable(true);
        a->setChecked(m == current);
        a->setData(m);
    }
    connect(m_menu, &QMenu::triggered, this, [this](QAction* a) {
        if (m_doc && a)
            m_doc->setHighlightingMode(a->data().toString());
    });
    // Anchor below the indicator, clamped inside the top-level window.
    const QPoint global = mapToGlobal(docTypeRect().topLeft());
    const QRect shell = window() ? QRect(window()->mapToGlobal(QPoint(0, 0)), window()->size())
                                 : QRect(global, size());
    QPoint pos = global;
    const QSize ms = m_menu->sizeHint();
    if (pos.x() + ms.width() > shell.right())
        pos.setX(shell.right() - ms.width());
    if (pos.x() < shell.left())
        pos.setX(shell.left());
    if (pos.y() + ms.height() > shell.bottom())
        pos.setY(global.y() - ms.height());
    attachHint(m_menu, QStringLiteral("Select document type"));
    m_menu->popup(pos);
}

// Spec 7: while a dropdown is open the F-key row shows a context hint and the
// four indicators step aside; when it closes, both are restored. Wire that swap
// to the menu's own show/hide so it happens no matter how the menu dismisses
// (pick, Escape, click-away).
void StatusSelectors::attachHint(QMenu* menu, const QString& hint)
{
    if (!menu)
        return;
    connect(menu, &QMenu::aboutToShow, this, [this, hint]() { emit dropdownOpened(hint); });
    connect(menu, &QMenu::aboutToHide, this, [this]() { emit dropdownClosed(); });
}

// The encoding switcher: every encoding Qt can name, as a scrollable menu.
// Pick RECORDS the encoding via Document::setEncoding() - Kate applies it on
// the next save/reload. It deliberately does NOT re-decode the open buffer
// (that would drop unsaved edits); see the spec's locked decisions.
void StatusSelectors::openEncodingMenu()
{
    if (!m_doc)
        return;
    delete m_menu;
    m_menu = new BorlandMenu(this);   // TV dropdown, same as the menu bar's
    const QString current = m_doc->encoding();
    // Qt6 exposes the encoding enum, not a "list all" - walk the named values.
    static const struct { QStringConverter::Encoding e; const char* name; } kEncodings[] = {
        { QStringConverter::Utf8, "UTF-8" },
        { QStringConverter::Latin1, "ISO-8859-1" },
        { QStringConverter::System, "System" },
    };
    // "Default" = the document uses the editor's own default (empty name).
    QAction* def = m_menu->addAction(QStringLiteral("Default"));
    def->setData(QString());
    def->setCheckable(true);
    def->setChecked(current.isEmpty());
    for (const auto& enc : kEncodings) {
        const QString name = QString::fromUtf8(enc.name);
        QAction* a = m_menu->addAction(name);
        a->setData(name);
        a->setCheckable(true);
        a->setChecked(current == name);
    }
    connect(m_menu, &QMenu::triggered, this, [this](QAction* a) {
        // Record only: setEncoding() takes effect on save/reload, never here.
        if (m_doc && a)
            m_doc->setEncoding(a->data().toString());
    });
    const QPoint global = mapToGlobal(encodingRect().topLeft());
    const QRect shell = window() ? QRect(window()->mapToGlobal(QPoint(0, 0)), window()->size())
                                 : QRect(global, size());
    QPoint pos = global;
    const QSize ms = m_menu->sizeHint();
    if (pos.x() + ms.width() > shell.right())
        pos.setX(shell.right() - ms.width());
    if (pos.x() < shell.left())
        pos.setX(shell.left());
    if (pos.y() + ms.height() > shell.bottom())
        pos.setY(global.y() - ms.height());
    attachHint(m_menu, QStringLiteral("Select character encoding"));
    m_menu->popup(pos);
}

// The spellcheck-language dropdown: every dictionary Sonnet can find, by its
// human-readable description. Picking one sets the speller's language code.
void StatusSelectors::openSpellMenu()
{
    if (!m_speller)
        return;
    delete m_menu;
    m_menu = new BorlandMenu(this);   // TV dropdown, same as the menu bar's
    const auto dicts = m_speller->availableDictionaries();   // description -> code
    const QString currentCode = m_speller->language();
    for (auto it = dicts.constBegin(); it != dicts.constEnd(); ++it) {
        QAction* a = m_menu->addAction(it.key());
        a->setData(it.value());
        a->setCheckable(true);
        a->setChecked(it.value() == currentCode);
    }
    connect(m_menu, &QMenu::triggered, this, [this](QAction* a) {
        if (m_speller && a) {
            m_speller->setLanguage(a->data().toString());
            emit spellLanguageChanged(a->data().toString());
        }
    });
    const QPoint global = mapToGlobal(spellRect().topLeft());
    const QRect shell = window() ? QRect(window()->mapToGlobal(QPoint(0, 0)), window()->size())
                                 : QRect(global, size());
    QPoint pos = global;
    const QSize ms = m_menu->sizeHint();
    if (pos.x() + ms.width() > shell.right())
        pos.setX(shell.right() - ms.width());
    if (pos.x() < shell.left())
        pos.setX(shell.left());
    if (pos.y() + ms.height() > shell.bottom())
        pos.setY(global.y() - ms.height());
    attachHint(m_menu, QStringLiteral("Select spellcheck language"));
    m_menu->popup(pos);
}
