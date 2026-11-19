// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "FindReplace.h"
#include "src/shell/TvModal.h"

#include <QString>
#include <ktexteditor/range.h>
#include <ktexteditor/view.h>

// Defined in FindReplaceDialog.cpp; the dialog's members point at the field
// cells it builds there.
class FieldCell;

class QLabel;
class QLineEdit;
class QPushButton;
class TvCheckBox;
class TvTabBar;
class EditorTabManager;

// Search > Find / Replace: the house-style modal that replaces the native
// QInputDialog this used to open (which landed outside the CRT scene entirely).
//
// The matching itself lives in FindReplace.h - this class is the surface: two
// fields, three options, a three-chip scope row, a status line, and plates that
// act without closing (Find Next / Replace / Replace All) next to Cancel.
class FindReplaceDialog : public TvModal
{
    Q_OBJECT
public:
    enum class Scope { CurrentDocument, AllDocuments, Selection };

    explicit FindReplaceDialog(EditorTabManager* tabs, QWidget* parent = nullptr);

    void setScope(Scope scope);
    Scope scope() const;

    // 0 = Find what, 1 = Replace with: which field takes the caret on open.
    void setInitialField(int which);

    QLineEdit* findField() const { return m_findField; }
    QLineEdit* replaceField() const { return m_replaceField; }
    TvCheckBox* caseBox() const { return m_caseBox; }
    TvCheckBox* wholeWordsBox() const { return m_wholeWordsBox; }
    TvCheckBox* regexBox() const { return m_regexBox; }
    TvTabBar* scopeBar() const { return m_scopeBar; }
    QString statusText() const;
    QPushButton* findNextButton() const { return m_findNext; }
    QPushButton* replaceButton() const { return m_replace; }
    QPushButton* replaceAllButton() const { return m_replaceAll; }

    void findNext();
    void replaceOne();
    void replaceAll();

    // What the three boxes currently say. Whole words is off whenever regex is
    // on: KTextEditor's whole-word matching is plaintext-only.
    FindReplace::Options options() const;

    // The pattern the last dialog left behind - Search > Find Next uses it when
    // the dialog is closed.
    static QString lastPattern();

protected:
    bool eventFilter(QObject* watched, QEvent* ev) override;

private:
    void setActiveField(QLineEdit* field);
    void setStatus(const QString& text);
    void findNextInAllDocuments(const QString& pattern, const FindReplace::Options& options);

    EditorTabManager* m_tabs = nullptr;
    QLineEdit* m_findField = nullptr;
    QLineEdit* m_replaceField = nullptr;
    FieldCell* m_findCell = nullptr;
    FieldCell* m_replaceCell = nullptr;
    TvCheckBox* m_caseBox = nullptr;
    TvCheckBox* m_wholeWordsBox = nullptr;
    TvCheckBox* m_regexBox = nullptr;
    TvTabBar* m_scopeBar = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_findNext = nullptr;
    QPushButton* m_replace = nullptr;
    QPushButton* m_replaceAll = nullptr;

    // The selection as it was when the dialog opened. Find Next selects the hit
    // it finds; if the Selection scope re-read the live selection, choosing it
    // after a Find Next would silently search the hit instead of the user's
    // range.
    KTextEditor::Range m_capturedSelection;

    static QString s_lastPattern;
};
