// SPDX-License-Identifier: GPL-3.0-or-later
#include "TvFileDock.h"
#include "TvFrame.h"
#include "TabListMenu.h"
#include "TvPlate.h"
#include "TvLineEdit.h"
#include "TextScrollBar.h"
#include "ShellWindow.h"
#include <QAction>
#include <QMenu>
#include <QLineEdit>
#include <QPushButton>
#include <QPainter>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QEvent>
#include <QDir>
#include <QFileInfo>
#include <QEventLoop>
#include <QRegularExpression>
#include <QPolygon>
#include <QVBoxLayout>
#include <QStyle>

namespace {
const QColor kPanelGray(0xAA, 0xAA, 0xAA);
const QColor kWhite(0xFF, 0xFF, 0xFF);
const QColor kBlack(0x00, 0x00, 0x00);
const QColor kYellow(0xFF, 0xFF, 0x55);
const QColor kTeal(0x00, 0xAA, 0xAA);
const QColor kGreen(0x00, 0xAA, 0x00);
// DOS bright green: the press flash on a plate or a dropdown cell.
const QColor kGreenBright(0x55, 0xFF, 0x55);
const QColor kBlue(0x00, 0x00, 0xAA);
const int kScrollStrip = 7;      // height of the teal scroll strip under the list
// 2026-11-21, horizontal scrollbar: the strip is now a widget whose geometry
// lives in layoutChildren (bar + strip under the list), so the NAME ROWS end
// where the strip begins: listBottom - kScrollStrip. Every read of the row
// band goes through these two helpers - inlining the old arithmetic is how
// the strip and the rows drift apart.
// Row band = the list rect minus the strip at its bottom.
inline int listBodyBottom(const QRect& list) { return list.bottom() - kScrollStrip; }
// The drop shadow is the panel's own rect offset down-right by this much, so
// the panel also insets by it: the shadow then reaches the widget edge exactly
// and the gutter around it is left unpainted (the CRT background shows through,
// which is what makes the L read as a shadow instead of a grey border).
const int kShadowOffset = 6;
const int kButtonShadow = 4;     // black block shadow behind each green plate
const int kPad = 6;
const int kMaxRecents = 10;      // the dropdown lists the last ten files
} // namespace

TvFileDock::TvFileDock(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("tvFileDock"));
    setAutoFillBackground(false);          // paintEvent fills the panel
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_DeleteOnClose, false);

    // The fields are TvLineEdit, not QLineEdit: the shell is never an active
    // window, so Qt paints no caret in a plain QLineEdit (see TvLineEdit.h).
    m_edit = new TvLineEdit(this);
    m_edit->setObjectName(QStringLiteral("tvFileDockEdit"));
    m_edit->setAutoFillBackground(true);
    m_edit->installEventFilter(this);

    // Path bar (top): where the Files box is looking.
    m_pathEdit = new TvLineEdit(this);
    m_pathEdit->setObjectName(QStringLiteral("tvFileDockPath"));
    m_pathEdit->setAutoFillBackground(true);
    m_pathEdit->installEventFilter(this);

    // File-type box (bottom): free text, seeded with the era default.
    m_typeEdit = new TvLineEdit(this);
    m_typeEdit->setObjectName(QStringLiteral("tvFileDockType"));
    m_typeEdit->setAutoFillBackground(true);
    m_typeEdit->installEventFilter(this);
    m_typeEdit->setText(typePresets().first());
    // DOS block caret: white cell on the blue entry field (borland.qss).
    for (TvLineEdit* f : { m_edit, m_pathEdit, m_typeEdit })
        f->setCaretColors(kWhite, kBlue);
    connect(m_typeEdit, &QLineEdit::textChanged, this,
            [this](const QString& text) { setFilter(text); });

    // Horizontal scrollbar under the Files box (2026-11-21: "the horizontal
    // scrollbar doesn't work in the file list" - there was no bar at all, only
    // a Shift+wheel trick). BorlandStyle already repaints every QScrollBar as
    // a TextScrollBar, so it wears the TV glyph language for free.
    m_hBar = new TextScrollBar(Qt::Horizontal, this);
    m_hBar->setObjectName(QStringLiteral("fileListHBar"));
    m_hBar->setSingleStep(qMax(8, fontMetrics().horizontalAdvance(QLatin1Char('M'))));
    connect(m_hBar, &QScrollBar::valueChanged, this,
            [this](int v) { setHorizontalOffset(v); });
    m_hBar->show();
    syncHorizontalBar();

    auto makeButton = [this](const QString& text, const char* name) {
        auto* b = new TvPlate(text, this);
        b->setObjectName(QLatin1String(name));
        b->setFocusPolicy(Qt::NoFocus);
        // TV green plate with BLACK ink (the app-wide QSS rule paints white).
        b->setInk(Qt::black);
        b->setBackdrop(kPanelGray);
        b->setShadowOffset(kButtonShadow);
        return b;
    };
    m_ok = makeButton(QStringLiteral("OK"), "tvFileDockOk");
    m_cancel = makeButton(QStringLiteral("Cancel"), "tvFileDockCancel");
    connect(m_ok, &QPushButton::clicked, this, &TvFileDock::accept);
    connect(m_cancel, &QPushButton::clicked, this, &TvFileDock::reject);
    connect(m_edit, &QLineEdit::returnPressed, this, &TvFileDock::accept);
    connect(m_edit, &QLineEdit::textChanged, this, [this] { syncSelectionFromEdit(); });
    connect(m_pathEdit, &QLineEdit::returnPressed, this, &TvFileDock::navigateToTypedPath);

    m_dir = QDir::currentPath();
    setFilter(m_typeEdit->text());
    refreshFiles();
}

int TvFileDock::cellW() const
{
    return qMax(6, fontMetrics().horizontalAdvance(QLatin1Char('M')));
}

int TvFileDock::cellH() const
{
    return qMax(10, fontMetrics().height());
}

void TvFileDock::setMode(Mode m)
{
    m_mode = m;
    if (m_title.isEmpty() || m_title == QLatin1String("Open File")
        || m_title == QLatin1String("Save File As")) {
        m_title = (m == Save) ? QStringLiteral("Save File As")
                              : QStringLiteral("Open File");
    }
    if (m == Save && m_edit->text().trimmed().isEmpty() && !m_defaultExt.isEmpty())
        m_edit->setText(QStringLiteral("untitled.") + m_defaultExt);
    update();
}

void TvFileDock::setTitle(const QString& title)
{
    m_title = title;
    update();
}

void TvFileDock::setDirectory(const QString& dir)
{
    const QFileInfo fi(dir.isEmpty() ? QDir::currentPath() : dir);
    m_dir = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
    m_scroll = 0;
    // Keep the path bar in step with the list. Only the bar's Return key
    // navigates, so writing the text here can never re-enter this function.
    if (m_pathEdit && m_pathEdit->text() != QDir::toNativeSeparators(m_dir))
        m_pathEdit->setText(QDir::toNativeSeparators(m_dir));
    refreshFiles();
    if (m_mode == Save && m_edit->text().trimmed().isEmpty() && !m_defaultExt.isEmpty())
        m_edit->setText(QStringLiteral("untitled.") + m_defaultExt);
}

void TvFileDock::setFilter(const QString& filter)
{
    m_filter = filter;
    // "Text files (*.txt)" or "*.txt" -> the first extension becomes the
    // default for Save completion.
    const int star = m_filter.indexOf(QStringLiteral("*."));
    if (star >= 0) {
        QString ext = m_filter.mid(star + 2);
        const int end = ext.indexOf(QRegularExpression(QStringLiteral("[^A-Za-z0-9]")));
        if (end >= 0)
            ext.truncate(end);
        if (!ext.isEmpty())
            m_defaultExt = ext;
    }
    refreshFiles();
}

// "Text files (*.txt)" -> {"*.txt"}; a bare "*.md" stays as it is.
QStringList TvFileDock::filterGlobs() const
{
    QString s = m_filter;
    const int open = s.indexOf(QLatin1Char('('));
    const int close = s.lastIndexOf(QLatin1Char(')'));
    if (open >= 0 && close > open)
        s = s.mid(open + 1, close - open - 1);
    return s.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}

bool TvFileDock::matchesFilter(const QString& name) const
{
    const QStringList globs = filterGlobs();
    // An empty box, a bare "*" and the DOS "*.*" all mean "everything".
    if (globs.isEmpty() || globs.contains(QStringLiteral("*"))
        || globs.contains(QStringLiteral("*.*")))
        return true;
    return QDir::match(globs, name);
}

void TvFileDock::refreshFiles()
{
    m_files.clear();
    QDir d(m_dir);
    const QFileInfo parentInfo(m_dir);
    if (!parentInfo.absolutePath().isEmpty() && parentInfo.absolutePath() != m_dir)
        m_files << QStringLiteral("..");

    QStringList dirs, plain;
    const QFileInfoList entries =
        d.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                        QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& fi : entries) {
        if (fi.isDir())
            dirs << fi.fileName();
        else if (matchesFilter(fi.fileName()))
            plain << fi.fileName();
    }
    m_files << dirs << plain;

    if (m_selected >= m_files.size())
        m_selected = m_files.isEmpty() ? -1 : 0;
    if (m_selected < 0 && !m_files.isEmpty())
        m_selected = 0;
    layoutChildren();
    update();
}

QString TvFileDock::selectedFile() const
{
    if (m_selected < 0 || m_selected >= m_files.size())
        return QString();
    return m_files.at(m_selected);
}

void TvFileDock::selectFile(const QString& name)
{
    const int idx = m_files.indexOf(name);
    if (idx >= 0) {
        m_selected = idx;
        ensureVisible(idx);
        if (m_edit->text() != name)
            m_edit->setText(name);
        update();
    }
}

void TvFileDock::setEditText(const QString& text)
{
    m_edit->setText(text);
    syncSelectionFromEdit();
}

QString TvFileDock::editText() const
{
    return m_edit->text();
}

void TvFileDock::setPathText(const QString& text)
{
    if (m_pathEdit)
        m_pathEdit->setText(text);
}

QString TvFileDock::pathText() const
{
    return m_pathEdit ? m_pathEdit->text() : QString();
}

void TvFileDock::setTypeText(const QString& text)
{
    if (m_typeEdit)
        m_typeEdit->setText(text);
}

QString TvFileDock::typeText() const
{
    return m_typeEdit ? m_typeEdit->text() : QString();
}

// The era's own filter idioms, offered by the type box's dropdown.
QStringList TvFileDock::typePresets()
{
    return {
        QStringLiteral("All files (*)"),
        QStringLiteral("Text files (*.txt)"),
        QStringLiteral("C/C++ (*.cpp *.h)"),
        QStringLiteral("Markdown (*.md)"),
        QStringLiteral("Python (*.py)"),
        QStringLiteral("JSON (*.json)"),
    };
}

void TvFileDock::navigateToTypedPath()
{
    QString text = m_pathEdit ? m_pathEdit->text().trimmed() : QString();
    if (text.isEmpty())
        return;
    // QFileInfo does not expand the shell's home shorthand.
    if (text == QLatin1String("~") || text.startsWith(QLatin1String("~/")))
        text = QDir::homePath() + text.mid(1);

    const QFileInfo fi(text);
    if (fi.isDir()) {                       // an existing directory: just go there
        setDirectory(fi.absoluteFilePath());
        return;
    }
    if (text.endsWith(QLatin1Char('/'))) {  // a directory that may not exist yet
        setDirectory(text);
        return;
    }
    // A file (or a Save name): split it into directory + file name.
    setDirectory(fi.absolutePath());
    setEditText(fi.fileName());
    selectFile(fi.fileName());
}

void TvFileDock::setRecents(const QStringList& paths)
{
    m_recents.clear();
    for (const QString& p : paths) {
        if (p.trimmed().isEmpty() || m_recents.contains(p))
            continue;
        m_recents << p;
        if (m_recents.size() >= kMaxRecents)
            break;
    }
}

void TvFileDock::applyRecent(const QString& path)
{
    const QFileInfo fi(path);
    // A recent whose directory has since disappeared still fills the name;
    // only navigate when the directory is actually there.
    if (!fi.absolutePath().isEmpty() && QFileInfo(fi.absolutePath()).isDir())
        setDirectory(fi.absolutePath());
    setEditText(fi.fileName());
    selectFile(fi.fileName());
}

void TvFileDock::showRecentsMenu()
{
    emit recentsRequested();
    if (m_recents.isEmpty())
        return;

    QMenu* menu = TabListMenu::build(this, m_recents, -1);
    QObject::connect(menu, &QMenu::triggered, this, [this](QAction* a) {
        const int idx = a ? a->data().toInt() : -1;
        if (idx >= 0 && idx < m_recents.size())
            applyRecent(m_recents.at(idx));
    });
    QObject::connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);

    // Same anchoring contract as the tab list: global cell rect, clamped to the
    // window so the list can never open off-screen.
    QWidget* top = window();
    const QRect shellRect(top->mapToGlobal(QPoint(0, 0)), top->size());
    const QRect cellGlobal(mapToGlobal(m_nameDropRect.topLeft()), m_nameDropRect.size());
    menu->popup(TabListMenu::anchoredPos(cellGlobal, menu->sizeHint(), shellRect));
}

void TvFileDock::showTypeMenu()
{
    emit typePresetsRequested();
    const QStringList presets = typePresets();

    QMenu* menu = TabListMenu::build(this, presets, -1);
    QObject::connect(menu, &QMenu::triggered, this, [this, presets](QAction* a) {
        const int idx = a ? a->data().toInt() : -1;
        if (idx >= 0 && idx < presets.size())
            setTypeText(presets.at(idx));
    });
    QObject::connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);

    QWidget* top = window();
    const QRect shellRect(top->mapToGlobal(QPoint(0, 0)), top->size());
    const QRect cellGlobal(mapToGlobal(m_typeDropRect.topLeft()), m_typeDropRect.size());
    menu->popup(TabListMenu::anchoredPos(cellGlobal, menu->sizeHint(), shellRect));
}

void TvFileDock::syncSelectionFromEdit()
{
    const int idx = m_files.indexOf(m_edit->text());
    if (idx >= 0 && idx != m_selected) {
        m_selected = idx;
        ensureVisible(idx);
        update();
    }
}

QString TvFileDock::resolvedPath() const
{
    QString name = m_edit->text().trimmed();
    if (name.isEmpty())
        return QString();
    if (QFileInfo(name).isAbsolute())
        return name;
    if (m_mode == Save && !name.contains(QLatin1Char('.')) && !m_defaultExt.isEmpty())
        name += QLatin1Char('.') + m_defaultExt;
    return QDir(m_dir).absoluteFilePath(name);
}

void TvFileDock::accept()
{
    const QString name = m_edit->text().trimmed();
    if (name.isEmpty())
        return;
    const QString path = resolvedPath();
    const QFileInfo fi(path);
    if (fi.isDir()) {                 // a directory name means "go there"
        setDirectory(fi.absoluteFilePath());
        m_edit->clear();
        return;
    }
    if (m_mode == Open && !fi.exists())
        return;                       // nothing to open: stay put
    hide();
    emit dialogFinished(path);
}

void TvFileDock::reject()
{
    hide();
    emit dialogFinished(QString());
}

// --- geometry ---------------------------------------------------------------

void TvFileDock::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    layoutChildren();
}

void TvFileDock::showEvent(QShowEvent* ev)
{
    QWidget::showEvent(ev);
    // The dock owns the keyboard while it is up, so the bridge stops offering
    // the keystroke to the menu and the editor behind it.
    if (auto* shell = qobject_cast<ShellWindow*>(window()))
        shell->setKeyboardOwner(this);
}

void TvFileDock::hideEvent(QHideEvent* ev)
{
    if (auto* shell = qobject_cast<ShellWindow*>(window()))
        if (shell->keyboardOwner() == this)
            shell->setKeyboardOwner(nullptr);
    QWidget::hideEvent(ev);
}

void TvFileDock::changeEvent(QEvent* ev)
{
    QWidget::changeEvent(ev);
    if (ev->type() == QEvent::FontChange) {
        layoutChildren();
        update();
    }
}

void TvFileDock::layoutChildren()
{
    const int cw = cellW();
    const int ch = cellH();
    // Flush to the top-left: the panel IS the modal, and the only margin it
    // needs is the shadow gutter down the right and along the bottom.
    m_frameRect = rect().adjusted(0, 0, -kShadowOffset, -kShadowOffset);

    // Content area: inside the frame's own cell-thick border.
    const QRect inner = m_frameRect.adjusted(cw, ch, -cw, -ch);
    if (inner.width() < 40 || inner.height() < 60)
        return;

    m_statusRect = QRect(inner.left(), inner.bottom() - ch + 1, inner.width(), ch);

    const int x0 = inner.left() + kPad;
    const int usable = inner.width() - 2 * kPad;
    const int listW = qMax(cw * 12, int(usable * 0.62));
    const int btnW = qMax(cw * 9, 76);
    const int btnX = inner.right() - kPad - btnW + 1;

    const int entryH = ch + 6;

    // Bottom block, laid out UPWARD from the status row so the new rows grow
    // into the space the list gives up, never past the frame.
    const int dropW = qMax(cw * 2, 14);          // a real click target, not a sliver
    const int fieldW = qMax(cw * 8, listW - dropW - 5);
    const int typeTop = m_statusRect.top() - 6 - entryH;
    m_typeRect = QRect(x0, typeTop, fieldW, entryH);
    m_typeLabelRect = QRect(x0, typeTop - 2 - ch, listW, ch);
    m_typeDropRect = QRect(m_typeRect.right() + 5, typeTop, dropW, entryH);

    const int nameTop = m_typeLabelRect.top() - 6 - entryH;
    m_nameRect = QRect(x0, nameTop, fieldW, entryH);
    m_nameDropRect = QRect(m_nameRect.right() + 5, nameTop, dropW, entryH);
    m_nameLabelRect = QRect(x0, nameTop - 2 - ch, listW, ch);

    // Top block: the path bar.
    int y = inner.top() + 2;
    m_pathLabelRect = QRect(x0, y, listW, ch);
    y += ch + 2;
    m_pathRect = QRect(x0, y, listW, entryH);
    y += entryH + 6;

    // Files label, then the list fills whatever is left in the middle.
    m_filesLabelRect = QRect(x0, y, listW, ch);
    y += ch + 2;

    const int listBottom = m_nameLabelRect.top() - 6;
    m_listRect = QRect(x0, y, listW, qMax(ch * 3, listBottom - y));

    const int bh = ch + 6;
    m_okRect = QRect(btnX, m_listRect.top(), btnW, bh);
    // Cancel sits under OK, kept clear of the list's bottom edge (the old
    // third Help slot anchored it there; with Help gone the pair is spaced
    // from OK and clamped to the frame).
    m_cancelRect = QRect(btnX, m_listRect.top() + 3 * (bh + 4), btnW, bh);
    const int lowest = m_listRect.bottom() - bh + 1;
    if (m_cancelRect.bottom() > lowest)
        m_cancelRect.moveTop(qMax(m_okRect.bottom() + 4, lowest - bh + 1));

    if (m_pathEdit)
        m_pathEdit->setGeometry(m_pathRect.adjusted(2, 2, -2, -2));
    if (m_edit)
        m_edit->setGeometry(m_nameRect.adjusted(2, 2, -2, -2));
    if (m_typeEdit)
        m_typeEdit->setGeometry(m_typeRect.adjusted(2, 2, -2, -2));
    if (m_ok)
        m_ok->setGeometry(m_okRect.adjusted(0, 0, kButtonShadow, kButtonShadow));
    if (m_cancel)
        m_cancel->setGeometry(m_cancelRect.adjusted(0, 0, kButtonShadow, kButtonShadow));
    // Horizontal bar sits in the strip's ROW, to the right of the vertical
    // strip's clickable slice (see stripTrackRect), so the box's bottom row
    // carries both scroll controls without stealing height from the rows.
    if (m_hBar) {
        const int barH = kScrollStrip;
        const int slice = kScrollStrip + 1;
        m_hBar->setGeometry(m_listRect.left() + slice,
                            m_listRect.bottom() - barH + 1,
                            qMax(24, m_listRect.width() - slice - slice), barH);
        syncHorizontalBar();
    }
}

int TvFileDock::visibleRows() const
{
    const int rowH = cellH();
    if (rowH <= 0)
        return 1;
    // Row band excludes the scroll strip at the box's bottom edge (the band
    // helper keeps paint, hit-test and this count on the same arithmetic).
    return qMax(1, (listBodyBottom(m_listRect) - m_listRect.top() + 1) / rowH);
}

QRect TvFileDock::stripRect() const
{
    if (m_listRect.isEmpty())
        return QRect();
    // The strip's ROW: teal track (vertical scroll) on the left slice, the
    // horizontal bar takes the rest - both live in the list's bottom row, so
    // neither steals height from the name rows.
    return QRect(m_listRect.left(), m_listRect.bottom() - kScrollStrip + 1,
                 m_listRect.width(), kScrollStrip);
}

QRect TvFileDock::stripTrackRect() const
{
    // The vertical strip's own clickable slice, left of the horizontal bar.
    const QRect row = stripRect();
    if (row.isEmpty())
        return QRect();
    return QRect(row.left(), row.top(), kScrollStrip, row.height());
}

QRect TvFileDock::stripThumbRect() const
{
    const QRect track = stripTrackRect();   // the vertical slice only
    if (track.isEmpty() || m_files.isEmpty())
        return QRect();
    const int perCol = visibleRows();
    const int total = qMax(1, m_files.size());
    // Same rules as before the horizontal bar split the strip row: a thumb
    // proportional to the visible share, moved by m_scroll.
    const int thumbW = qMax(8, track.width() * qMin(total, 2 * perCol) / total);
    const int maxScroll = qMax(1, total - 2 * perCol);
    const int x = track.left()
                  + (track.width() - thumbW) * qBound(0, m_scroll, maxScroll) / maxScroll;
    return QRect(x, track.top() + 1, thumbW, qMax(2, track.height() - 2));
}

int TvFileDock::maxHorizontalOffset() const
{
    const int colW = qMax(0, m_listRect.width() / 2 - 4);
    if (colW <= 0)
        return 0;
    int widest = 0;
    for (const QString& name : m_files)
        widest = qMax(widest, fontMetrics().horizontalAdvance(name));
    return qMax(0, widest + 4 - colW);
}

void TvFileDock::setHorizontalOffset(int px)
{
    const int next = qBound(0, px, maxHorizontalOffset());
    if (next == m_hOffset)
        return;
    m_hOffset = next;
    if (m_hBar && m_hBar->value() != m_hOffset)
        m_hBar->setValue(m_hOffset);   // echo back; valueChanged no-ops if equal
    update();
}

// Range/value for the bar: 0..maxHorizontalOffset(), value = m_hOffset.
// Called after any listing change (setDirectory/setFilter/refreshFiles) and
// from layoutChildren(), so a bar born after a listing starts consistent.
void TvFileDock::syncHorizontalBar()
{
    if (!m_hBar)
        return;
    m_hBar->setRange(0, maxHorizontalOffset());
    m_hBar->setPageStep(qMax(1, m_listRect.width() / 4));
    if (m_hBar->value() != m_hOffset)
        m_hBar->setValue(m_hOffset);
}

void TvFileDock::scrollByPage(int pages)
{
    const int perCol = visibleRows();
    const int maxScroll = qMax(0, m_files.size() - 2 * perCol);
    const int next = qBound(0, m_scroll + pages * perCol, maxScroll);
    if (next != m_scroll) {
        m_scroll = next;
        update();
    }
}

int TvFileDock::rowIndex(int column, int row) const
{
    const int perCol = visibleRows();
    if (row < 0 || row >= perCol)
        return -1;
    return column == 0 ? m_scroll + row : m_scroll + perCol + row;
}

QRect TvFileDock::rowRect(int index) const
{
    const int perCol = visibleRows();
    const int rowH = cellH();
    const int half = m_listRect.width() / 2;
    const bool right = index >= m_scroll + perCol;
    const int row = index - (right ? m_scroll + perCol : m_scroll);
    if (row < 0 || row >= perCol)
        return QRect();
    const int x = m_listRect.left() + (right ? half + 2 : 0);
    const int w = half - 4;
    return QRect(x, m_listRect.top() + row * rowH, w, rowH);
}

int TvFileDock::rowAt(const QPoint& p) const
{
    if (!m_listRect.contains(p))
        return -1;
    if (p.y() > listBodyBottom(m_listRect))
        return -1;
    const int rowH = cellH();
    const int row = (p.y() - m_listRect.top()) / rowH;
    const int col = p.x() < m_listRect.left() + m_listRect.width() / 2 ? 0 : 1;
    const int idx = rowIndex(col, row);
    return (idx >= 0 && idx < m_files.size()) ? idx : -1;
}

void TvFileDock::ensureVisible(int index)
{
    const int perCol = visibleRows();
    if (index < 0)
        return;
    if (index < m_scroll)
        m_scroll = index;
    else if (index >= m_scroll + 2 * perCol)
        m_scroll = qMax(0, index - 2 * perCol + 1);
    m_scroll = qBound(0, m_scroll, qMax(0, m_files.size() - 2 * perCol));
}

void TvFileDock::moveSelection(int delta)
{
    if (m_files.isEmpty())
        return;
    const int next = qBound(0, m_selected + delta, m_files.size() - 1);
    if (next == m_selected)
        return;
    m_selected = next;
    ensureVisible(next);
    m_edit->setText(m_files.at(next));
    update();
}

void TvFileDock::descendInto(const QString& name)
{
    if (name == QLatin1String("..")) {
        setDirectory(QFileInfo(m_dir).absolutePath());
        m_edit->clear();
        return;
    }
    const QString path = QDir(m_dir).absoluteFilePath(name);
    if (QFileInfo(path).isDir()) {
        setDirectory(path);
        m_edit->clear();
    }
}

// --- painting ---------------------------------------------------------------

void TvFileDock::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    const int cw = cellW();
    const int ch = cellH();
    const QFontMetrics fm(font());

    // Drop shadow, then the panel over it, so only the offset L survives. The
    // gutter around the panel is deliberately left UNPAINTED: the CRT
    // background shows through there, which is what makes the black read as a
    // shadow. Filling the whole widget with panel grey (the old behaviour) left
    // a grey stripe down the right edge, a grey notch above the shadow, and a
    // missing corner.
    p.fillRect(m_frameRect.translated(kShadowOffset, kShadowOffset), kBlack);
    p.fillRect(m_frameRect, kPanelGray);

    // The OK / Cancel plates own their block shadows and their press
    // travel (TvPlate paints both), so this pass draws none of them.
    // White double-line frame with the title in the top row (shared with
    // every other TV modal - see TvFrame::drawDoubleFrame).
    if (!TvFrame::drawDoubleFrame(p, m_frameRect,
                                  m_title.isEmpty() ? QStringLiteral("File") : m_title,
                                  cw, ch))
        return;

    // Yellow labels.
    QFont inkFont = font();
    inkFont.setStyleStrategy(QFont::NoAntialias);
    p.save();
    p.setFont(inkFont);
    p.setPen(kYellow);
    p.drawText(m_pathLabelRect, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Path"));
    p.drawText(m_filesLabelRect, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Files"));
    p.drawText(m_nameLabelRect, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("File name"));
    p.drawText(m_typeLabelRect, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("File type"));
    p.restore();

    // Green dropdown cells (recents / type presets), each with its own block
    // shadow so they read like the plates. Pressed, the cell turns the bright
    // green and travels onto that shadow - the same push the plates animate.
    auto drawDropCell = [&](const QRect& cell, bool pressed) {
        if (cell.isNull())
            return;
        p.fillRect(cell.translated(kButtonShadow, kButtonShadow), kBlack);
        const QRect plate = pressed ? cell.translated(kButtonShadow, kButtonShadow) : cell;
        p.fillRect(plate, pressed ? kGreenBright : kGreen);
        const QPoint c = plate.center();
        QPolygon tri;
        tri << QPoint(c.x() - 3, c.y() - 2) << QPoint(c.x() + 3, c.y() - 2)
            << QPoint(c.x(), c.y() + 2);
        p.save();
        p.setPen(kBlack);
        p.setBrush(kBlack);
        p.drawPolygon(tri);
        p.restore();
    };
    drawDropCell(m_nameDropRect, m_pressedDrop == DropCell::Name);
    drawDropCell(m_typeDropRect, m_pressedDrop == DropCell::Type);

    // Focus mark. The shell is never an active window, so a QLineEdit never
    // draws a caret here - without this the user clicks a field and sees
    // nothing. A 2 px yellow frame around the field is the CRT-safe "you are
    // typing here".
    QRect active;
    if (m_activeField == m_pathEdit)
        active = m_pathRect;
    else if (m_activeField == m_edit)
        active = m_nameRect;
    else if (m_activeField == m_typeEdit)
        active = m_typeRect;
    if (!active.isNull()) {
        p.save();
        p.setPen(QPen(kYellow, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(active.adjusted(1, 1, -1, -1));
        p.restore();
    }

    // ---- Files box: teal field, divider, two name columns, scroll strip ----
    p.fillRect(m_listRect, kTeal);
    const int stripTop = listBodyBottom(m_listRect) + 1;
    const int listBodyH = stripTop - m_listRect.top();
    const int half = m_listRect.width() / 2;
    // Dark-blue divider between the two columns (reference: a solid vertical
    // rule splitting the teal box).
    p.fillRect(QRect(m_listRect.left() + half, m_listRect.top(), 2, listBodyH), kBlue);

    p.save();
    p.setClipRect(QRect(m_listRect.left(), m_listRect.top(), m_listRect.width(), listBodyH));
    for (int i = 0; i < m_files.size(); ++i) {
        const QRect r = rowRect(i);
        if (r.isEmpty())
            continue;
        const bool sel = (i == m_selected);
        if (sel)
            p.fillRect(r, kBlue);
        p.save();
        p.setFont(inkFont);
        // TV list language: black names on the cyan field, the selected row
        // inverted to white on blue. Long names are read by scrolling the
        // offset (Shift+wheel), so the text starts left of the row when one is
        // set and the row clip keeps the neighbour column clean.
        p.setPen(sel ? kWhite : kBlack);
        p.drawText(r.adjusted(2 - m_hOffset, 0, -2, 0),
                   Qt::AlignLeft | Qt::AlignVCenter, m_files.at(i));
        p.restore();
    }
    p.restore();

    // Scroll strip: teal track + green thumb for VERTICAL scrolling, in the
    // strip's row left of the horizontal bar (the bar is a child widget that
    // paints itself over the rest of that row).
    p.fillRect(stripRect(), kTeal);
    const QRect thumb = stripThumbRect();
    if (!thumb.isEmpty())
        p.fillRect(thumb, m_draggingStrip ? kGreenBright : kGreen);

    // ---- Bottom path status row: blue field, white path, right-aligned count --
    p.fillRect(m_statusRect, kBlue);
    p.save();
    p.setFont(inkFont);
    p.setPen(kWhite);
    QString path = QDir::toNativeSeparators(m_dir);
    if (!m_filter.isEmpty())
        path += QLatin1Char(' ') + m_filter;
    p.drawText(m_statusRect.adjusted(4, 0, -60, 0), Qt::AlignLeft | Qt::AlignVCenter, path);
    p.drawText(m_statusRect.adjusted(m_statusRect.width() - 58, 0, -6, 0),
               Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("%1").arg(qMax(0, m_files.size() - 1)));
    p.restore();
}

// --- input ------------------------------------------------------------------

void TvFileDock::mousePressEvent(QMouseEvent* ev)
{
    const QPoint pos = ev->pos();
    // A cell left lit by a press that ended outside the dock (no release ever
    // reached us) pops back up on the next click - a plate stuck down is worse
    // than a lost frame.
    if (m_pressedDrop != DropCell::None) {
        m_pressedDrop = DropCell::None;
        update();
    }
    if (m_okRect.contains(pos)) { accept(); return; }
    if (m_cancelRect.contains(pos)) { reject(); return; }
    // The green cells go DOWN on press and fire on release, so the push is
    // visible before the menu takes the pointer.
    if (m_nameDropRect.contains(pos)) {
        m_pressedDrop = DropCell::Name;
        update();
        return;
    }
    if (m_typeDropRect.contains(pos)) {
        m_pressedDrop = DropCell::Type;
        update();
        return;
    }
    // The strip under the Files box is a real scroll control now: pressing the
    // thumb grabs it, pressing the track pages toward the click. The CLICK
    // target is stripTrackRect (the vertical slice) - the rest of the strip's
    // row belongs to the horizontal bar, a child widget with its own handler.
    if (stripTrackRect().contains(pos)) {
        const QRect thumb = stripThumbRect();
        if (thumb.contains(pos)) {
            m_draggingStrip = true;
            m_stripGrabDx = pos.x() - thumb.left();
        } else {
            scrollByPage(pos.x() < thumb.left() ? -1 : +1);
        }
        setFocus(Qt::MouseFocusReason);
        update();
        return;
    }
    if (m_listRect.contains(pos)) {
        const int idx = rowAt(pos);
        if (idx >= 0) {
            m_selected = idx;
            m_edit->setText(m_files.at(idx));
            update();
        }
        setFocus(Qt::MouseFocusReason);
        return;
    }
    QWidget::mousePressEvent(ev);
}

void TvFileDock::mouseMoveEvent(QMouseEvent* ev)
{
    if (!m_draggingStrip) {
        QWidget::mouseMoveEvent(ev);
        return;
    }
    const QRect track = stripTrackRect();   // drag maps across the vertical slice
    const QRect thumb = stripThumbRect();
    const int span = qMax(1, track.width() - thumb.width());
    const int perCol = visibleRows();
    const int maxScroll = qMax(0, m_files.size() - 2 * perCol);
    const int x = qBound(0, ev->pos().x() - m_stripGrabDx - track.left(), span);
    const int next = (x * maxScroll + span / 2) / span;
    if (next != m_scroll) {
        m_scroll = next;
        update();
    }
    ev->accept();
}

void TvFileDock::mouseReleaseEvent(QMouseEvent* ev)
{
    if (m_draggingStrip) {
        m_draggingStrip = false;
        update();
    }
    const DropCell held = m_pressedDrop;
    if (held != DropCell::None) {
        // Come back up BEFORE the menu opens: the menu runs its own event
        // loop, and a cell left pressed would stay lit under it.
        m_pressedDrop = DropCell::None;
        update();

        const QPoint pos = ev->pos();
        const bool stillOnCell = (held == DropCell::Name) ? m_nameDropRect.contains(pos)
                                                          : m_typeDropRect.contains(pos);
        if (stillOnCell) {
            if (held == DropCell::Name)
                showRecentsMenu();
            else
                showTypeMenu();
            return;
        }
    }
    QWidget::mouseReleaseEvent(ev);
}

void TvFileDock::mouseDoubleClickEvent(QMouseEvent* ev)
{
    if (m_listRect.contains(ev->pos())) {
        const int idx = rowAt(ev->pos());
        if (idx >= 0)
            descendInto(m_files.at(idx));
        return;
    }
    QWidget::mouseDoubleClickEvent(ev);
}

void TvFileDock::wheelEvent(QWheelEvent* ev)
{
    // Sideways first: Shift+wheel (and a real horizontal wheel) scrolls long
    // names. The dedicated horizontal bar (fileListHBar) does the same job
    // with a visible control; this stays for muscle memory and for wheel mice
    // with no Shift habit. Both funnel through setHorizontalOffset(), so the
    // bar's thumb stays in step.
    const int dx = ev->angleDelta().x();
    const bool sideways = (ev->modifiers() & Qt::ShiftModifier) || (dx != 0);
    if (sideways) {
        const int delta = dx != 0 ? -dx : -ev->angleDelta().y();
        if (delta != 0) {
            setHorizontalOffset(m_hOffset + (delta > 0 ? 12 : -12));
            ev->accept();
            return;
        }
    }
    const int perCol = visibleRows();
    const int step = ev->angleDelta().y() > 0 ? -perCol : perCol;
    const int maxScroll = qMax(0, m_files.size() - 2 * perCol);
    const int next = qBound(0, m_scroll + step, maxScroll);
    if (next != m_scroll) {
        m_scroll = next;
        update();
    }
    ev->accept();
}

void TvFileDock::keyPressEvent(QKeyEvent* ev)
{
    switch (ev->key()) {
    case Qt::Key_Escape: reject(); return;
    case Qt::Key_Return:
    case Qt::Key_Enter: accept(); return;
    case Qt::Key_Down: moveSelection(+1); return;
    case Qt::Key_Up: moveSelection(-1); return;
    default: break;
    }
    QWidget::keyPressEvent(ev);
}

bool TvFileDock::eventFilter(QObject* watched, QEvent* ev)
{
    const bool isField = (watched == m_edit || watched == m_pathEdit
                          || watched == m_typeEdit);
    if (isField && ev->type() == QEvent::MouseButtonPress) {
        // Mark the field immediately so the click shows itself at once.
        // The other fields give up their selection: a highlight left behind in
        // a field you are no longer editing reads as "this is still active",
        // and a QLineEdit only drops it when it really loses focus - which
        // depends on window activation the shell never gets.
        for (QLineEdit* other : {m_edit, m_pathEdit, m_typeEdit})
            if (other && other != watched)
                other->deselect();
        m_activeField = static_cast<QWidget*>(watched);
        update();
        return false;               // the field still places its own caret
    }
    // 2026-11-20: the release is NOT consumed any more. It used to call
    // selectAll() so the selection could stand in for a caret, but that made a
    // second click re-select the whole string - you could never drop the caret
    // to edit part of it, and the highlight never cleared. TvLineEdit now draws
    // the caret, so plain editing behaviour is correct: click places the caret,
    // drag selects, Ctrl+A selects all.
    if (watched == m_edit && ev->type() == QEvent::KeyPress) {
        auto* kev = static_cast<QKeyEvent*>(ev);
        if (kev->key() == Qt::Key_Escape) {
            reject();
            return true;
        }
        if (kev->key() == Qt::Key_Down) {
            moveSelection(+1);
            return true;
        }
        if (kev->key() == Qt::Key_Up) {
            moveSelection(-1);
            return true;
        }
    }
    return QWidget::eventFilter(watched, ev);
}

// --- entry points -----------------------------------------------------------

TvFileDock* TvFileDock::openOn(QWidget* anchor, Mode mode, const QString& title,
                               const QString& dir, const QString& filter,
                               const QStringList& recents)
{
    auto* dock = new TvFileDock(anchor);
    dock->setMode(mode);
    dock->setTitle(title);
    if (!filter.isEmpty())
        dock->setFilter(filter);
    dock->setRecents(recents);
    dock->setDirectory(dir);
    // Size it like a TV modal: a fixed share of the host, clamped.
    const QSize host = anchor ? anchor->size() : QSize(900, 600);
    const int w = qBound(420, int(host.width() * 0.72), 760);
    const int h = qBound(300, int(host.height() * 0.78), 560);
    dock->resize(w, h);
    if (anchor)
        dock->move((host.width() - w) / 2, (host.height() - h) / 2);
    dock->show();
    dock->raise();
    dock->setFocus(Qt::OtherFocusReason);
    return dock;
}

QString TvFileDock::getOpenFileName(QWidget* anchor, const QString& title,
                                    const QString& dir, const QString& filter,
                                    const QStringList& recents)
{
    return runModal(anchor, Open, title, dir, filter, recents);
}

QString TvFileDock::getSaveFileName(QWidget* anchor, const QString& title,
                                    const QString& dir, const QString& filter,
                                    const QStringList& recents)
{
    return runModal(anchor, Save, title, dir, filter, recents);
}

// The modal round loop. QMenu::exec() deadlocks under this shell (the popup has
// no grab and its nested loop never returns), but a plain child widget is a
// different code path: this loop exits as soon as the dock reports a result,
// which the OK/Cancel buttons and the Escape/Return keys all emit.
QString TvFileDock::runModal(QWidget* anchor, Mode mode, const QString& title,
                             const QString& dir, const QString& filter,
                             const QStringList& recents)
{
    TvFileDock dock(anchor);
    dock.setMode(mode);
    dock.setTitle(title);
    if (!filter.isEmpty())
        dock.setFilter(filter);
    dock.setRecents(recents);
    dock.setDirectory(dir);
    const QSize host = anchor ? anchor->size() : QSize(900, 600);
    const int w = qBound(420, int(host.width() * 0.72), 760);
    const int h = qBound(300, int(host.height() * 0.78), 560);
    dock.resize(w, h);
    if (anchor)
        dock.move((host.width() - w) / 2, (host.height() - h) / 2);

    QString picked;
    QEventLoop loop;
    QObject::connect(&dock, &TvFileDock::dialogFinished, &loop,
                     [&picked, &loop](const QString& path) {
                         picked = path;
                         loop.quit();
                     });
    dock.show();
    dock.raise();
    dock.setFocus(Qt::OtherFocusReason);
    loop.exec();
    return picked;
}
