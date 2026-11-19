// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "src/shell/TvModal.h"

#include <QString>

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

protected:
    bool eventFilter(QObject* watched, QEvent* ev) override;

private:
    void setActiveField(QLineEdit* field);

    EditorTabManager* m_tabs = nullptr;
    QLineEdit* m_findField = nullptr;
    QLineEdit* m_replaceField = nullptr;
    QWidget* m_findCell = nullptr;
    QWidget* m_replaceCell = nullptr;
    TvCheckBox* m_caseBox = nullptr;
    TvCheckBox* m_wholeWordsBox = nullptr;
    TvCheckBox* m_regexBox = nullptr;
    TvTabBar* m_scopeBar = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_findNext = nullptr;
    QPushButton* m_replace = nullptr;
    QPushButton* m_replaceAll = nullptr;
};
