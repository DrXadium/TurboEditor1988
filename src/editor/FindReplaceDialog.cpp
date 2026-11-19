// SPDX-License-Identifier: GPL-3.0-or-later
#include "FindReplaceDialog.h"

#include "EditorTabManager.h"
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
#include <ktexteditor/view.h>

namespace {

const QColor kFieldBlue(0x00, 0x00, 0xAA);
const QColor kYellow(0xFF, 0xFF, 0x55);
const int kRing = 2;
const int kFieldMinWidth = 220;

// The flat TV field: blue box, white text, NO border - the rule that removed
// the white-border defect the user reported on the settings fields.
const char* kFieldQss =
    "QLineEdit { background-color: #0000AA; color: #FFFFFF;"
    " border: none; padding: 0px 3px; }";

// A TV field cell. The QLineEdit sits inset by kRing inside it, and the cell
// paints the ring itself: the shell is never an active window, so a QLineEdit
// never draws a caret here - the yellow ring is the "you are typing here" mark,
// the same treatment the file dock gives its fields.
class FieldCell : public QWidget
{
public:
    explicit FieldCell(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAutoFillBackground(false);
    }

    // The field must be tall enough for a full line of VGA text plus the ring,
    // or the grid squeezes it to 12 px and the glyphs are clipped.
    QSize sizeHint() const override
    {
        return QSize(kFieldMinWidth, fontMetrics().height() + 2 * kRing);
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

    void setEdit(QLineEdit* edit) { m_edit = edit; }
    QLineEdit* edit() const { return m_edit; }
    bool isActive() const { return m_active; }
    void setActive(bool on)
    {
        if (m_active == on)
            return;
        m_active = on;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), kFieldBlue);
        if (m_active) {
            // Four hard 2 px bands: exact pixels, no pen-join surprises.
            p.fillRect(QRect(0, 0, width(), kRing), kYellow);
            p.fillRect(QRect(0, height() - kRing, width(), kRing), kYellow);
            p.fillRect(QRect(0, 0, kRing, height()), kYellow);
            p.fillRect(QRect(width() - kRing, 0, kRing, height()), kYellow);
        }
    }

    void resizeEvent(QResizeEvent*) override
    {
        if (m_edit)
            m_edit->setGeometry(rect().adjusted(kRing, kRing, -kRing, -kRing));
    }

private:
    QLineEdit* m_edit = nullptr;
    bool m_active = false;
};

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
    edit->setStyleSheet(QString::fromLatin1(kFieldQss));
    edit->setFrame(false);
    edit->setAutoFillBackground(true);
    cell->setEdit(edit);
    return cell;
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
    for (QWidget* cell : {m_findCell, m_replaceCell}) {
        auto* cellWidget = static_cast<FieldCell*>(cell);
        if (cellWidget)
            cellWidget->setActive(cellWidget->edit() == field);
    }
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

// Bodies land in Tasks 5 and 6; the plates exist so the chrome can be reviewed
// first.
void FindReplaceDialog::findNext() {}

void FindReplaceDialog::replaceOne() {}

void FindReplaceDialog::replaceAll() {}
