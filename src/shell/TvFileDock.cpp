// SPDX-License-Identifier: GPL-3.0-or-later
#include "TvFileDock.h"
#include "TvFrame.h"
#include "TabListMenu.h"
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
const QColor kBlue(0x00, 0x00, 0xAA);
const int kScrollStrip = 7;      // height of the teal scroll strip under the list
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

    m_edit = new QLineEdit(this);
    m_edit->setObjectName(QStringLiteral("tvFileDockEdit"));
    m_edit->setAutoFillBackground(true);
    m_edit->installEventFilter(this);

    // Path bar (top): where the Files box is looking.
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setObjectName(QStringLiteral("tvFileDockPath"));
    m_pathEdit->setAutoFillBackground(true);
    m_pathEdit->installEventFilter(this);

    // File-type box (bottom): free text, seeded with the era default.
    m_typeEdit = new QLineEdit(this);
    m_typeEdit->setObjectName(QStringLiteral("tvFileDockType"));
    m_typeEdit->setAutoFillBackground(true);
    m_typeEdit->installEventFilter(this);
    m_typeEdit->setText(typePresets().first());
    connect(m_typeEdit, &QLineEdit::textChanged, this,
            [this](const QString& text) { setFilter(text); });

    auto makeButton = [this](const QString& text, const char* name) {
        auto* b = new QPushButton(text, this);
        b->setObjectName(QLatin1String(name));
        b->setFocusPolicy(Qt::NoFocus);
        // TV green plate with BLACK ink (the app-wide QSS rule paints white).
        b->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #00AA00; color: #000000; "
            "border: none; padding: 0; } "
            "QPushButton:hover { background-color: #1FBE1F; } "
            "QPushButton:pressed { background-color: #148A14; }"));
        return b;
    };
    m_ok = makeButton(QStringLiteral("OK"), "tvFileDockOk");
    m_cancel = makeButton(QStringLiteral("Cancel"), "tvFileDockCancel");
    m_help = makeButton(QStringLiteral("Help"), "tvFileDockHelp");
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
    m_cancelRect = QRect(btnX, m_listRect.top() + 3 * (bh + 4), btnW, bh);
    m_helpRect = QRect(btnX, m_listRect.bottom() - bh + 1, btnW, bh);
    if (m_cancelRect.bottom() > m_helpRect.top())
        m_cancelRect.moveTop(qMax(m_okRect.bottom() + 4, m_helpRect.top() - bh - 4));

    if (m_pathEdit)
        m_pathEdit->setGeometry(m_pathRect.adjusted(2, 2, -2, -2));
    if (m_edit)
        m_edit->setGeometry(m_nameRect.adjusted(2, 2, -2, -2));
    if (m_typeEdit)
        m_typeEdit->setGeometry(m_typeRect.adjusted(2, 2, -2, -2));
    if (m_ok)
        m_ok->setGeometry(m_okRect);
    if (m_cancel)
        m_cancel->setGeometry(m_cancelRect);
    if (m_help)
        m_help->setGeometry(m_helpRect);
}

int TvFileDock::visibleRows() const
{
    const int rowH = cellH();
    if (rowH <= 0)
        return 1;
    return qMax(1, (m_listRect.height() - kScrollStrip) / rowH);
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
    if (p.y() >= m_listRect.bottom() - kScrollStrip)
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

    // Hard black block shadow behind each green plate, as on the reference
    // modal. The buttons are child widgets and paint AFTER this pass, so their
    // plate covers the overlap and only the offset L survives.
    for (const QRect& plate : {m_okRect, m_cancelRect, m_helpRect})
        if (!plate.isNull())
            TvFrame::drawPlateShadow(p, plate, kButtonShadow);

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
    // shadow so they read like the plates.
    auto drawDropCell = [&](const QRect& cell) {
        if (cell.isNull())
            return;
        p.fillRect(cell.translated(kButtonShadow, kButtonShadow), kBlack);
        p.fillRect(cell, kGreen);
        const QPoint c = cell.center();
        QPolygon tri;
        tri << QPoint(c.x() - 3, c.y() - 2) << QPoint(c.x() + 3, c.y() - 2)
            << QPoint(c.x(), c.y() + 2);
        p.save();
        p.setPen(kBlack);
        p.setBrush(kBlack);
        p.drawPolygon(tri);
        p.restore();
    };
    drawDropCell(m_nameDropRect);
    drawDropCell(m_typeDropRect);

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
    const int stripTop = m_listRect.bottom() - kScrollStrip + 1;
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
        // inverted to white on blue.
        p.setPen(sel ? kWhite : kBlack);
        p.drawText(r.adjusted(2, 0, -2, 0), Qt::AlignLeft | Qt::AlignVCenter, m_files.at(i));
        p.restore();
    }
    p.restore();

    // Scroll strip: teal track with a green thumb sized to the visible share.
    p.fillRect(QRect(m_listRect.left(), stripTop, m_listRect.width(), kScrollStrip), kTeal);
    if (!m_files.isEmpty()) {
        const int perCol = visibleRows();
        const int total = qMax(1, m_files.size());
        const int thumbW = qMax(8, m_listRect.width() * qMin(total, 2 * perCol) / total);
        const int maxScroll = qMax(1, total - 2 * perCol);
        const int x = m_listRect.left()
                      + (m_listRect.width() - thumbW) * m_scroll / maxScroll;
        p.fillRect(QRect(x, stripTop + 1, thumbW, kScrollStrip - 2), kGreen);
    }

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
    if (m_okRect.contains(pos)) { accept(); return; }
    if (m_cancelRect.contains(pos)) { reject(); return; }
    if (m_helpRect.contains(pos)) { return; }   // no help topic in this build
    if (m_nameDropRect.contains(pos)) { showRecentsMenu(); return; }
    if (m_typeDropRect.contains(pos)) { showTypeMenu(); return; }
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
    if (isField && ev->type() == QEvent::MouseButtonRelease) {
        // Highlight the content: with no active window the caret never blinks,
        // so the selection is the visible proof that the click landed. Consume
        // the release so the field cannot immediately clear it again.
        auto* edit = static_cast<QLineEdit*>(watched);
        m_activeField = edit;
        edit->selectAll();
        update();
        return true;
    }
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
