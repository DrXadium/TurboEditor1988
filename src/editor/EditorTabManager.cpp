// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorTabManager.h"
#include <ktexteditor/editor.h>
#include <ktexteditor/document.h>
#include <ktexteditor/view.h>
#include <ktexteditor/cursor.h>
#include <ktexteditor/range.h>
#include <QFileInfo>
#include <QAction>
#include <QWidget>

EditorTabManager::EditorTabManager(QObject* parent) : QObject(parent)
{
    m_container = new QStackedWidget;
    m_container->setObjectName(QStringLiteral("editorStack"));
    // Ensure a first tab exists so activeView() is meaningful immediately.
    if (m_docs.isEmpty())
        newDocument();
}

EditorTabManager::~EditorTabManager() = default;

KTextEditor::Document* EditorTabManager::createDoc()
{
    auto* editor = KTextEditor::Editor::instance();
    if (!editor)
        return nullptr;
    return editor->createDocument(this);
}

KTextEditor::Document* EditorTabManager::activeDocument() const
{
    if (m_active < 0 || m_active >= m_docs.size())
        return nullptr;
    return m_docs.at(m_active);
}

KTextEditor::View* EditorTabManager::activeView() const
{
    if (m_active < 0 || m_active >= m_views.size())
        return nullptr;
    return m_views.at(m_active);
}

void EditorTabManager::newDocument()
{
    KTextEditor::Document* doc = createDoc();
    if (!doc)
        return;
    KTextEditor::View* view = doc->createView(nullptr);
    m_docs.append(doc);
    m_views.append(view);
    m_container->addWidget(view);       // host in the stacked container
    m_active = m_docs.size() - 1;
    m_container->setCurrentIndex(m_active);
    syncTabs();
}

void EditorTabManager::openFile(const QUrl& url)
{
    KTextEditor::Document* doc = createDoc();
    if (!doc)
        return;
    doc->openUrl(url);
    KTextEditor::View* view = doc->createView(nullptr);
    m_docs.append(doc);
    m_views.append(view);
    m_container->addWidget(view);
    m_active = m_docs.size() - 1;
    m_container->setCurrentIndex(m_active);
    syncTabs();
}

void EditorTabManager::setActiveIndex(int index)
{
    if (index < 0 || index >= m_docs.size() || index == m_active)
        return;
    m_active = index;
    m_container->setCurrentIndex(index);
    syncTabs();
}

QUrl EditorTabManager::viewFileUrl(int index) const
{
    if (index < 0 || index >= m_views.size())
        return {};
    return m_views[index]->document()->url();
}

QPoint EditorTabManager::viewCursorPosition(int index) const
{
    if (index < 0 || index >= m_views.size())
        return {};
    const KTextEditor::Cursor c = m_views[index]->cursorPosition();
    return { c.column(), c.line() };
}

void EditorTabManager::setZoom(int delta)
{
    if (delta == 0) {
        applyZoom(0);   // reset handled via stored baseline
        return;
    }
    applyZoom(delta);
}

void EditorTabManager::applyZoom(int delta)
{
    KTextEditor::View* v = activeView();
    if (!v)
        return;
    QFont f = v->configValue(QStringLiteral("font")).value<QFont>();
    if (f.pointSize() <= 0)
        f.setPointSize(10);

    if (delta == 0) {                       // reset to baseline
        if (m_basePointSize > 0)
            f.setPointSize(m_basePointSize);
    } else {
        if (m_basePointSize < 0)
            m_basePointSize = f.pointSize(); // lock baseline on first zoom
        int next = f.pointSize() + delta;
        if (next < 4)  next = 4;
        if (next > 96) next = 96;
        f.setPointSize(next);
    }
    v->setConfigValue(QStringLiteral("font"), f);
}

// Case/sort operate on the selection if one exists, else the whole document.
// Uses document->text()/setText() with a cursor range so undo works via setText.
void EditorTabManager::changeCase(CaseKind kind)
{
    KTextEditor::View* v = activeView();
    KTextEditor::Document* doc = activeDocument();
    if (!v || !doc)
        return;

    KTextEditor::Range range(0, 0, 0, 0);
    bool hasSel = v->selection() && v->selectionRanges().size() == 1;
    if (hasSel)
        range = v->selectionRanges().first();
    else {
        const int lastLine = doc->lines() - 1;
        range = KTextEditor::Range(0, 0, lastLine, doc->lineLength(lastLine));
    }

    QString text = doc->text(range);
    switch (kind) {
    case Upper: text = text.toUpper(); break;
    case Lower: text = text.toLower(); break;
    case Title: {
        // Title-case each whitespace-delimited word's first letter.
        bool atWordStart = true;
        for (int i = 0; i < text.size(); ++i) {
            if (text[i].isSpace()) { atWordStart = true; continue; }
            if (atWordStart) { text[i] = text[i].toUpper(); atWordStart = false; }
            else             { text[i] = text[i].toLower(); }
        }
        break;
    }
    }
    doc->replaceText(range, text);
}

void EditorTabManager::sortLines()
{
    KTextEditor::Document* doc = activeDocument();
    if (!doc)
        return;
    QStringList lines = doc->text().split(QLatin1Char('\n'));
    const bool trailing = lines.size() > 1 && lines.last().isEmpty();
    if (trailing)
        lines.removeLast();
    std::sort(lines.begin(), lines.end(),
              [](const QString& a, const QString& b) {
                  return a.localeAwareCompare(b) < 0;
              });
    QString out = lines.join(QLatin1Char('\n'));
    if (trailing)
        out += QLatin1Char('\n');
    doc->setText(out);
}

void EditorTabManager::installActions(QWidget* host)
{
    if (!host)
        return;

    // Zoom shortcuts (Ctrl+= in, Ctrl+- out, Ctrl+0 reset).
    auto* zoomIn = new QAction(tr("Zoom &In"), host);
    zoomIn->setShortcut(QKeySequence(QStringLiteral("Ctrl+=")));
    connect(zoomIn, &QAction::triggered, this, [this] { setZoom(+1); });
    host->addAction(zoomIn);

    auto* zoomOut = new QAction(tr("Zoom &Out"), host);
    zoomOut->setShortcut(QKeySequence(QStringLiteral("Ctrl+-")));
    connect(zoomOut, &QAction::triggered, this, [this] { setZoom(-1); });
    host->addAction(zoomOut);

    auto* zoomReset = new QAction(tr("&Reset Zoom"), host);
    zoomReset->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    connect(zoomReset, &QAction::triggered, this, [this] { setZoom(0); });
    host->addAction(zoomReset);

    // Text tools.
    auto* upper = new QAction(tr("UPPERCASE"), host);
    connect(upper, &QAction::triggered, this, [this] { changeCase(Upper); });
    host->addAction(upper);

    auto* lower = new QAction(tr("lowercase"), host);
    connect(lower, &QAction::triggered, this, [this] { changeCase(Lower); });
    host->addAction(lower);

    auto* sort = new QAction(tr("Sort Lines"), host);
    connect(sort, &QAction::triggered, this, [this] { sortLines(); });
    host->addAction(sort);
}

void EditorTabManager::syncTabs()
{
    QStringList titles;
    for (KTextEditor::Document* d : m_docs) {
        QString t = d->url().isEmpty()
            ? QStringLiteral("untitled")
            : QFileInfo(d->url().toLocalFile()).fileName();
        if (t.isEmpty())
            t = QStringLiteral("untitled");
        titles << t;
    }
    emit tabsChanged(titles, m_active);
}