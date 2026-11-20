// SPDX-License-Identifier: GPL-3.0-or-later
#include "FindReplaceDialog.h"

#include "EditorTabManager.h"
#include "src/shell/FieldCell.h"
#include "src/shell/TvCheckBox.h"
#include "src/shell/TvTabBar.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

namespace {

// Row labels: the dialog's yellow ink on the gray panel.
QLabel* tvLabel(const QString& text)
{
    auto* label = new QLabel(text);
    label->setStyleSheet(QStringLiteral("color: #FFFF55; background: transparent;"));
    return label;
}

FieldCell* makeField(QLineEdit*& edit, const char* name, QWidget* parent)
{
    auto* cell = new FieldCell(parent);
    edit = new QLineEdit(cell);
    edit->setObjectName(QLatin1String(name));
    styleAsTvField(edit);
    cell->setEdit(edit);
    return cell;
}

// Offset of a KTextEditor cursor in the document's text. FindReplace works in
// offsets (that is what QRegularExpression reports), KTextEditor in line/column.
int offsetAt(const QString& text, const KTextEditor::Cursor& cursor)
{
    int line = 0;
    int offset = 0;
    while (line < cursor.line()) {
        const int newline = text.indexOf(QLatin1Char('\n'), offset);
        if (newline < 0)
            return text.size();
        offset = newline + 1;
        ++line;
    }
    return qBound(0, offset + cursor.column(), int(text.size()));
}

QString countText(int n)
{
    return n == 1 ? QObject::tr("1 match") : QObject::tr("%1 matches").arg(n);
}

} // namespace

QString FindReplaceDialog::s_lastPattern;

FindReplaceDialog::FindReplaceDialog(EditorTabManager* tabs, QWidget* parent)
    : TvModal(parent)
    , m_tabs(tabs)
{
    setObjectName(QStringLiteral("findReplaceDialog"));
    setWindowTitle(tr("Find and Replace"));
    setTitle(tr("Find and Replace"));

    auto* body = new QWidget;
    body->setObjectName(QStringLiteral("findBody"));
    auto* grid = new QGridLayout(body);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(2 * cellWidth());
    grid->setVerticalSpacing(cellHeight());
    grid->setColumnStretch(1, 1);

    m_findCell = makeField(m_findField, "findField", body);
    m_replaceCell = makeField(m_replaceField, "replaceField", body);

    grid->addWidget(tvLabel(tr("Find what:")), 0, 0);
    grid->addWidget(m_findCell, 0, 1);
    grid->addWidget(tvLabel(tr("Replace with:")), 1, 0);
    grid->addWidget(m_replaceCell, 1, 1);

    // Options: the [ ] / [■] idiom, left to right.
    auto* options = new QWidget(body);
    options->setObjectName(QStringLiteral("findOptions"));
    auto* optionRow = new QHBoxLayout(options);
    optionRow->setContentsMargins(0, 0, 0, 0);
    optionRow->setSpacing(3 * cellWidth());
    m_caseBox = new TvCheckBox(tr("Case sensitive"), options);
    m_wholeWordsBox = new TvCheckBox(tr("Whole words"), options);
    m_regexBox = new TvCheckBox(tr("Regular expression"), options);
    optionRow->addWidget(m_caseBox);
    optionRow->addWidget(m_wholeWordsBox);
    optionRow->addWidget(m_regexBox);
    optionRow->addStretch(1);
    grid->addWidget(options, 2, 0, 1, 2);

    // Scope: three chips, "Current document" first.
    m_scopeBar = new TvTabBar(body);
    m_scopeBar->setObjectName(QStringLiteral("findScope"));
    m_scopeBar->addTab(tr("Current document"));
    m_scopeBar->addTab(tr("All documents"));
    m_scopeBar->addTab(tr("Selection"));
    m_scopeBar->setCurrentIndex(0);
    grid->addWidget(tvLabel(tr("Search in:")), 3, 0);
    grid->addWidget(m_scopeBar, 3, 1);

    // Status: yellow ink, one line, blank until something happens.
    m_status = new QLabel(body);
    m_status->setObjectName(QStringLiteral("findStatus"));
    m_status->setStyleSheet(QStringLiteral("color: #FFFF55; background: transparent;"));
    grid->addWidget(m_status, 4, 0, 1, 2);

    setBody(body);

    // Plates: right to left, so Cancel is added first and sits rightmost.
    addButton(tr("Cancel"), Reject);
    m_replaceAll = addActionButton(tr("Replace All"));
    m_replace = addActionButton(tr("Replace"));
    m_findNext = addActionButton(tr("Find Next"));

    // The fields report their clicks so the ring and the selection show at
    // once: with no active window Qt never delivers a real FocusIn here.
    for (QLineEdit* field : {m_findField, m_replaceField})
        field->installEventFilter(this);

    // Options: a regex search cannot also be a whole-word one.
    connect(m_regexBox, &TvCheckBox::toggled, this,
            [this](bool on) { m_wholeWordsBox->setEnabled(!on); });

    // An empty pattern has nothing to look for, so the action plates go dead.
    connect(m_findField, &QLineEdit::textChanged, this, [this](const QString& text) {
        s_lastPattern = text;
        const bool has = !text.isEmpty();
        for (QPushButton* plate : {m_findNext, m_replace, m_replaceAll})
            if (plate)
                plate->setEnabled(has);
    });

    // The Selection scope is offered only when the view HAS a selection, and
    // the range is captured NOW: Find Next selects the hit it finds, and a
    // scope that re-read the live selection would then search that hit.
    if (m_tabs) {
        if (KTextEditor::View* view = m_tabs->activeView())
            m_capturedSelection = view->selectionRange();
    }
    m_scopeBar->setTabEnabled(2, m_capturedSelection.isValid() && !m_capturedSelection.isEmpty());

    m_findField->setText(s_lastPattern);   // pre-fill, and seed the plates' state
    m_findNext->setEnabled(!s_lastPattern.isEmpty());
    m_replace->setEnabled(!s_lastPattern.isEmpty());
    m_replaceAll->setEnabled(!s_lastPattern.isEmpty());

    setInitialField(0);
}

void FindReplaceDialog::setScope(Scope scope)
{
    if (scope == Scope::Selection && !m_scopeBar->isTabEnabled(2))
        return;   // nothing to search in: stay where we are
    m_scopeBar->setCurrentIndex(int(scope));
}

FindReplaceDialog::Scope FindReplaceDialog::scope() const
{
    const int index = m_scopeBar->currentIndex();
    if (index == 1)
        return Scope::AllDocuments;
    if (index == 2)
        return Scope::Selection;
    return Scope::CurrentDocument;
}

void FindReplaceDialog::setInitialField(int which)
{
    QLineEdit* field = (which == 1) ? m_replaceField : m_findField;
    if (!field)
        return;
    setActiveField(field);
    field->setFocus();
    field->selectAll();
}

QString FindReplaceDialog::statusText() const
{
    return m_status ? m_status->text() : QString();
}

FindReplace::Options FindReplaceDialog::options() const
{
    FindReplace::Options o;
    o.caseSensitive = m_caseBox && m_caseBox->isChecked();
    o.regex = m_regexBox && m_regexBox->isChecked();
    // Whole words only exists for plaintext matching.
    o.wholeWords = !o.regex && m_wholeWordsBox && m_wholeWordsBox->isChecked();
    return o;
}

QString FindReplaceDialog::lastPattern()
{
    return s_lastPattern;
}

void FindReplaceDialog::setActiveField(QLineEdit* field)
{
    if (!field)
        return;
    m_findCell->setActive(m_findField == field);
    m_replaceCell->setActive(m_replaceField == field);
}

bool FindReplaceDialog::eventFilter(QObject* watched, QEvent* ev)
{
    auto* field = qobject_cast<QLineEdit*>(watched);
    if (!field || (field != m_findField && field != m_replaceField))
        return TvModal::eventFilter(watched, ev);

    if (ev->type() == QEvent::MouseButtonPress) {
        // Mark the field at once - the click has to show itself.
        setActiveField(field);
        return false;   // the field still places its own caret
    }
    if (ev->type() == QEvent::MouseButtonRelease) {
        // Highlight the content: with no active window the caret never blinks,
        // so the selection is the visible proof that the click landed.
        setActiveField(field);
        field->selectAll();
        return true;    // consume, or the field clears the selection again
    }
    if (ev->type() == QEvent::KeyPress) {
        auto* kev = static_cast<QKeyEvent*>(ev);
        if (kev->key() == Qt::Key_Escape) {
            reject();
            return true;
        }
        if (kev->key() == Qt::Key_Return || kev->key() == Qt::Key_Enter) {
            findNext();
            return true;
        }
    }
    return TvModal::eventFilter(watched, ev);
}

namespace {

void selectHit(KTextEditor::View* view, const QString& text, int offset, int length)
{
    const KTextEditor::Cursor start = FindReplace::cursorAt(text, offset);
    view->setSelection(KTextEditor::Range(start, FindReplace::cursorAt(text, offset + length)));
    view->setCursorPosition(start);
}

// Find Next has nothing to show for a zero-length hit ("^" matches everywhere
// and nowhere), so those are dropped from the walk but still replaced.
QList<FindReplace::Hit> selectable(const QList<FindReplace::Hit>& hits)
{
    QList<FindReplace::Hit> out;
    for (const FindReplace::Hit& hit : hits)
        if (hit.length > 0)
            out.append(hit);
    return out;
}

int firstAtOrAfter(const QList<FindReplace::Hit>& hits, int from)
{
    for (int i = 0; i < hits.size(); ++i)
        if (hits.at(i).offset >= from)
            return i;
    return -1;
}

} // namespace

void FindReplaceDialog::setStatus(const QString& text)
{
    if (m_status)
        m_status->setText(text);
}

void FindReplaceDialog::keyPressEvent(QKeyEvent* ev)
{
    if (ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Enter) {
        findNext();
        ev->accept();
        return;
    }
    TvModal::keyPressEvent(ev);
}

void FindReplaceDialog::findNext()
{
    const QString pattern = m_findField->text();
    if (pattern.isEmpty() || !m_tabs)
        return;

    const FindReplace::Options opts = options();
    if (!FindReplace::isValid(pattern, opts)) {
        setStatus(tr("Bad pattern"));
        return;
    }
    s_lastPattern = pattern;

    if (scope() == Scope::AllDocuments) {
        findNextInAllDocuments(pattern, opts);
        return;
    }

    KTextEditor::Document* doc = m_tabs->activeDocument();
    KTextEditor::View* view = m_tabs->activeView();
    if (!doc || !view) {
        setStatus(tr("Not found"));
        return;
    }

    const QString text = doc->text();
    int regionStart = 0;
    int regionEnd = text.size();
    if (scope() == Scope::Selection && m_capturedSelection.isValid()) {
        regionStart = offsetAt(text, m_capturedSelection.start());
        regionEnd = offsetAt(text, m_capturedSelection.end());
    }
    const QString region = text.mid(regionStart, qMax(0, regionEnd - regionStart));

    const QList<FindReplace::Hit> hits = selectable(FindReplace::findAll(region, pattern, opts));
    if (hits.isEmpty()) {
        setStatus(tr("Not found"));
        return;
    }

    // Continue AFTER whatever is selected, so repeated Find Next walks the
    // document instead of re-selecting the hit it just made.
    int from = 0;
    if (view->selection()) {
        const int end = offsetAt(text, view->selectionRange().end());
        if (end >= regionStart && end <= regionEnd)
            from = end - regionStart;
    } else {
        const int cursor = offsetAt(text, view->cursorPosition());
        if (cursor > regionStart && cursor <= regionEnd)
            from = cursor - regionStart;
    }

    const int index = FindReplace::nextHitIndex(hits, from);
    selectHit(view, text, regionStart + hits.at(index).offset, hits.at(index).length);
    setStatus(countText(hits.size()));
}

void FindReplaceDialog::findNextInAllDocuments(const QString& pattern,
                                               const FindReplace::Options& opts)
{
    const QList<KTextEditor::Document*> docs = m_tabs->documents();
    if (docs.isEmpty()) {
        setStatus(tr("Not found"));
        return;
    }
    const int active = qBound(0, m_tabs->activeIndex(), int(docs.size()) - 1);

    QList<QList<FindReplace::Hit>> perDocument;
    int total = 0;
    for (KTextEditor::Document* doc : docs) {
        perDocument.append(selectable(FindReplace::findAll(doc->text(), pattern, opts)));
        total += perDocument.last().size();
    }
    if (total == 0) {
        setStatus(tr("Not found"));
        return;
    }

    // The active document from the cursor onwards, then each tab in turn.
    KTextEditor::View* view = m_tabs->activeView();
    int from = 0;
    if (view) {
        const QString text = docs.at(active)->text();
        from = view->selection() ? offsetAt(text, view->selectionRange().end())
                                 : offsetAt(text, view->cursorPosition());
    }
    int here = firstAtOrAfter(perDocument.at(active), from);
    int chosen = active;
    if (here < 0) {
        // Nothing after the cursor in the active tab: walk the rest in order,
        // and only if THEY are empty wrap to the top of the active one.
        here = 0;
        chosen = -1;
        for (int step = 1; step < docs.size(); ++step) {
            const int index = (active + step) % int(docs.size());
            if (!perDocument.at(index).isEmpty()) {
                chosen = index;
                break;
            }
        }
        if (chosen < 0)
            chosen = active;   // wrap
    }
    if (chosen != m_tabs->activeIndex())
        m_tabs->setActiveIndex(chosen);

    KTextEditor::View* target = m_tabs->activeView();
    if (!target) {
        setStatus(tr("Not found"));
        return;
    }
    const QList<FindReplace::Hit>& hits = perDocument.at(chosen);
    selectHit(target, docs.at(chosen)->text(), hits.at(here).offset, hits.at(here).length);
    setStatus(countText(total));
}

void FindReplaceDialog::replaceOne()
{
    const QString pattern = m_findField->text();
    if (pattern.isEmpty() || !m_tabs)
        return;
    KTextEditor::Document* doc = m_tabs->activeDocument();
    KTextEditor::View* view = m_tabs->activeView();
    if (!doc || !view || !view->selection())
        return;
    const FindReplace::Options opts = options();
    if (!FindReplace::isValid(pattern, opts))
        return;
    const KTextEditor::Range range = view->selectionRange();
    // Replace swaps a MATCH, not whatever happens to be selected: a selection
    // the pattern does not describe must survive untouched, so this behaves
    // like Find Next instead of destroying the user's text.
    const QString selected = doc->text(range);
    const QList<FindReplace::Hit> onSelection = FindReplace::findAll(selected, pattern, opts);
    const bool isMatch = onSelection.size() == 1 && onSelection.first().offset == 0
                         && onSelection.first().length == selected.size();
    if (!isMatch) {
        findNext();
        return;
    }

    const int start = offsetAt(doc->text(), range.start());
    const QString replacement = m_replaceField->text();
    doc->replaceText(range, replacement);
    s_lastPattern = pattern;
    // Advance to the next hit after the replacement.
    const QString text = doc->text();
    const int nextFrom = start + replacement.length();
    const QList<FindReplace::Hit> hits =
        selectable(FindReplace::findAll(text.mid(nextFrom), pattern, opts));
    if (!hits.isEmpty()) {
        const int hitStart = nextFrom + hits.first().offset;
        selectHit(view, text, hitStart, hits.first().length);
    }
    setStatus(tr("Replaced 1"));
}

void FindReplaceDialog::replaceAll()
{
    const QString pattern = m_findField->text();
    if (pattern.isEmpty() || !m_tabs)
        return;
    const FindReplace::Options opts = options();
    if (!FindReplace::isValid(pattern, opts)) {
        setStatus(tr("Bad pattern"));
        return;
    }
    s_lastPattern = pattern;
    const QString replacement = m_replaceField->text();
    const Scope scope = this->scope();
    QList<KTextEditor::Document*> docs;
    if (scope == Scope::AllDocuments) {
        docs = m_tabs->documents();
    } else {
        KTextEditor::Document* doc = m_tabs->activeDocument();
        if (doc)
            docs.append(doc);
    }
    int total = 0;
    int docsChanged = 0;
    for (KTextEditor::Document* doc : docs) {
        const QString original = doc->text();
        const KTextEditor::Range searchRange =
            (scope == Scope::Selection && m_capturedSelection.isValid())
                ? m_capturedSelection
                : KTextEditor::Range(KTextEditor::Cursor(0, 0), doc->documentEnd());
        const QString region = doc->text(searchRange);
        const QList<FindReplace::Hit> hits = FindReplace::findAll(region, pattern, opts);
        if (hits.isEmpty())
            continue;
        int replaced = 0;
        const QString newText = FindReplace::applyAll(region, pattern, replacement, opts, &replaced);
        doc->replaceText(searchRange, newText);
        total += replaced;
        ++docsChanged;
    }
    if (scope == Scope::AllDocuments && docsChanged > 1)
        setStatus(tr("Replaced %1 in %2 documents").arg(total).arg(docsChanged));
    else
        setStatus(tr("Replaced %1").arg(total));
}
