// SPDX-License-Identifier: GPL-3.0-or-later
#include "TvFileDock.h"
#include "TvFrame.h"
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
const int kShadowRight = 10;     // black drop shadow gutter (right/bottom)
const int kShadowBottom = 8;
const int kButtonShadow = 4;     // black block shadow behind each green plate
const int kPad = 6;
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

    m_dir = QDir::currentPath();
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
        else
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
    m_frameRect = rect().adjusted(2, 2, -kShadowRight, -kShadowBottom);

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

    int y = inner.top() + 2;
    m_labelRect = QRect(x0, y, listW, ch);
    y += ch + 2;

    const int entryH = ch + 6;
    m_entryRect = QRect(x0, y, qMax(cw * 8, listW - cw - 6), entryH);
    m_stubRect = QRect(m_entryRect.right() + 5, y, cw, entryH);
    y += entryH + 6;

    m_filesLabelRect = QRect(x0, y, listW, ch);
    y += ch + 2;

    const int listBottom = m_statusRect.top() - 6;
    m_listRect = QRect(x0, y, listW, qMax(ch * 3, listBottom - y));

    const int bh = ch + 6;
    m_okRect = QRect(btnX, m_listRect.top(), btnW, bh);
    m_cancelRect = QRect(btnX, m_listRect.top() + 3 * (bh + 4), btnW, bh);
    m_helpRect = QRect(btnX, m_listRect.bottom() - bh + 1, btnW, bh);
    if (m_cancelRect.bottom() > m_helpRect.top())
        m_cancelRect.moveTop(qMax(m_okRect.bottom() + 4, m_helpRect.top() - bh - 4));

    if (m_edit)
        m_edit->setGeometry(m_entryRect.adjusted(2, 2, -2, -2));
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

    // Panel + hard black drop shadow (offset right/bottom), as on every TV modal.
    p.fillRect(rect(), kPanelGray);

    // Hard black block shadow behind each green plate, as on the reference
    // modal. The buttons are child widgets and paint AFTER this pass, so their
    // plate covers the overlap and only the offset L survives.
    for (const QRect& plate : {m_okRect, m_cancelRect, m_helpRect})
        if (!plate.isNull())
            p.fillRect(plate.translated(kButtonShadow, kButtonShadow), kBlack);

    p.fillRect(QRect(width() - kShadowRight, m_frameRect.top() + 4, kShadowRight - 2,
                     height() - kShadowBottom - 4), kBlack);
    p.fillRect(QRect(m_frameRect.left() + 4, height() - kShadowBottom, width() - kShadowRight - 4,
                     kShadowBottom), kBlack);

    // White DOUBLE-line frame, drawn with the CP437 glyphs so it tiles.
    const QRect f = m_frameRect;
    if (f.width() < 4 * cw || f.height() < 3 * ch)
        return;
    // Top row: \u2554 \u2550 title \u2550 \u2557
    TvFrame::glyph(p, QRect(f.left(), f.top(), cw, ch), QChar(0x2554), kWhite);
    TvFrame::glyph(p, QRect(f.right() - cw + 1, f.top(), cw, ch), QChar(0x2557), kWhite);
    const QString title = m_title.isEmpty() ? QStringLiteral("File") : m_title;
    int titleCells = (fm.horizontalAdvance(title) + cw - 1) / cw;
    titleCells = qMin(titleCells, qMax(1, f.width() / cw - 8));
    const int titleW = (titleCells + 2) * cw;
    int titleX = f.left() + ((f.width() - titleW) / 2 / cw) * cw;
    if (titleX < f.left() + cw)
        titleX = f.left() + cw;
    if (titleX > f.left() + cw)
        TvFrame::tileH(p, QRect(f.left() + cw, f.top(), titleX - f.left() - cw, ch),
                       cw, QChar(0x2550), kWhite);
    {
        QFont tf = font();
        tf.setStyleStrategy(QFont::NoAntialias);
        p.save();
        p.setFont(tf);
        p.setPen(kWhite);
        p.drawText(QRect(titleX, f.top(), titleW, ch), Qt::AlignCenter, title);
        p.restore();
    }
    const int afterTitle = titleX + titleW;
    if (afterTitle < f.right() - cw)
        TvFrame::tileH(p, QRect(afterTitle, f.top(), f.right() - cw - afterTitle + 1, ch),
                       cw, QChar(0x2550), kWhite);

    // Bottom row: \u255a \u2550 \u255d
    TvFrame::glyph(p, QRect(f.left(), f.bottom() - ch + 1, cw, ch), QChar(0x255A), kWhite);
    TvFrame::glyph(p, QRect(f.right() - cw + 1, f.bottom() - ch + 1, cw, ch), QChar(0x255D), kWhite);
    TvFrame::tileH(p, QRect(f.left() + cw, f.bottom() - ch + 1,
                            f.width() - 2 * cw, ch), cw, QChar(0x2550), kWhite);
    // Side columns: \u2551
    TvFrame::tileV(p, QRect(f.left(), f.top() + ch, cw, f.height() - 2 * ch), ch,
                   QChar(0x2551), kWhite);
    TvFrame::tileV(p, QRect(f.right() - cw + 1, f.top() + ch, cw, f.height() - 2 * ch), ch,
                   QChar(0x2551), kWhite);

    // Yellow labels.
    QFont inkFont = font();
    inkFont.setStyleStrategy(QFont::NoAntialias);
    p.save();
    p.setFont(inkFont);
    p.setPen(kYellow);
    const QString label = (m_mode == Save) ? QStringLiteral("Save file as")
                                           : QStringLiteral("Open file as");
    p.drawText(m_labelRect, Qt::AlignLeft | Qt::AlignVCenter, label);
    p.drawText(m_filesLabelRect, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Files"));
    p.restore();

    // Green completion stub at the right end of the entry field.
    p.fillRect(m_stubRect, kGreen);

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
                               const QString& dir, const QString& filter)
{
    auto* dock = new TvFileDock(anchor);
    dock->setMode(mode);
    dock->setTitle(title);
    if (!filter.isEmpty())
        dock->setFilter(filter);
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
                                    const QString& dir, const QString& filter)
{
    return runModal(anchor, Open, title, dir, filter);
}

QString TvFileDock::getSaveFileName(QWidget* anchor, const QString& title,
                                    const QString& dir, const QString& filter)
{
    return runModal(anchor, Save, title, dir, filter);
}

// The modal round loop. QMenu::exec() deadlocks under this shell (the popup has
// no grab and its nested loop never returns), but a plain child widget is a
// different code path: this loop exits as soon as the dock reports a result,
// which the OK/Cancel buttons and the Escape/Return keys all emit.
QString TvFileDock::runModal(QWidget* anchor, Mode mode, const QString& title,
                             const QString& dir, const QString& filter)
{
    TvFileDock dock(anchor);
    dock.setMode(mode);
    dock.setTitle(title);
    if (!filter.isEmpty())
        dock.setFilter(filter);
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
