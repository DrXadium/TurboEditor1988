// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QUrl>
#include <QList>
#include <QPoint>
#include <QStackedWidget>

namespace KTextEditor { class Document; class View; }

// Owns the KTextEditor documents/views behind the Borland TabStrip.
// One KTextEditor::Document per tab; the active document's view is the editor.
// Provides zoom (Ctrl+=/-/0) and text tools (change case, sort lines).
class EditorTabManager : public QObject
{
    Q_OBJECT
public:
    enum CaseKind { Upper, Lower, Title };
    Q_ENUM(CaseKind)

    explicit EditorTabManager(QObject* parent = nullptr);
    ~EditorTabManager() override;

    KTextEditor::View* activeView() const;
    KTextEditor::Document* activeDocument() const;
    int activeIndex() const { return m_active; }
    int tabCount() const { return m_docs.size(); }
    // Container hosting the per-tab KTextEditor::Views (replaces ShellWindow's
    // central placeholder; main.cpp installs it via ShellWindow::setCentralWidget).
    QStackedWidget* containerWidget() const { return m_container; }

    void newDocument();
    void openFile(const QUrl& url);
    void setActiveIndex(int index);

    // Session snapshot accessors (Task 7): per-tab file URL + cursor position
    // and the active tab, so the composition root can persist a named session.
    int viewCount() const { return m_views.size(); }
    QUrl viewFileUrl(int index) const;
    QPoint viewCursorPosition(int index) const;

    // Zoom: +1 in / -1 out / 0 reset, applied to the active view's "font"
    // config key (KTextEditor::View::configValue/setConfigValue).
    void setZoom(int delta);

    // Text tools on the active view (selection if present, else whole doc).
    void changeCase(CaseKind kind);
    void sortLines();

    // File ops for the File menu (Task 10): documentSave/documentSaveAs on the
    // active document. NOTE: both can open a native file dialog (save-as, or
    // save of an untitled buffer) - never call them from automated tests.
    bool saveActive();
    bool saveActiveAs();

    // Search menu (Task 10): find() prompts for a pattern (QInputDialog) and
    // jumps to the first match after the cursor; findNext() repeats the last
    // pattern. Core matching lives in jumpToNextMatch().
    void find();
    void findNext();

    // Installs zoom shortcuts (Ctrl+= / Ctrl+- / Ctrl+0) and text-tool actions
    // (change case, sort) as children of \p host (typically the ShellWindow).
    void installActions(QWidget* host);

signals:
    void tabsChanged(const QStringList& titles, int active);

private:
    KTextEditor::Document* createDoc();
    void syncTabs();
    void applyZoom(int delta);
    bool jumpToNextMatch(const QString& pattern);

    QList<KTextEditor::Document*> m_docs;
    QList<KTextEditor::View*> m_views;
    int m_active = -1;
    int m_basePointSize = -1;   // font point size at first view (zoom baseline)
    QStackedWidget* m_container = nullptr;
    QString m_lastFind;   // last Search-menu pattern (for Find Next)
};