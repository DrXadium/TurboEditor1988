// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QString>
#include <QStringList>

class QLineEdit;
class QPushButton;
class QMouseEvent;
class QKeyEvent;

// Turbo Vision file dialog, drawn ON the app's own surface.
//
// The native QFileDialog opens its own top-level window with the host's theme,
// which lands outside the CRT pass and looks nothing like the chrome. This dock
// is an ordinary CHILD of the shell window instead, so ContentBridge's existing
// QWidget::render pass composites it into the CRT texture for free, and the
// bridge's synthesized mouse/key routing drives it like every other control.
//
// Layout follows the reference "Save File As" screenshot: gray panel, white
// double-line frame with the title in the top row, black offset drop shadow,
// yellow labels, a blue entry field with a green stub, a teal two-column Files
// box with a divider and a scroll strip, green OK / Cancel / Help buttons, and
// a blue path status row along the bottom.
class TvFileDock : public QWidget
{
    Q_OBJECT
public:
    enum Mode { Open, Save };
    Q_ENUM(Mode)

    explicit TvFileDock(QWidget* parent = nullptr);

    void setMode(Mode m);
    Mode mode() const { return m_mode; }

    void setTitle(const QString& title);
    QString title() const { return m_title; }

    // The directory the Files box lists. An empty path means the current
    // working directory.
    void setDirectory(const QString& dir);
    QString directory() const { return m_dir; }

    // Name filter, either bare ("*.txt") or a dialog-style entry
    // ("Text files (*.txt)"). The first extension seen also becomes the default
    // extension used to complete a Save name.
    void setFilter(const QString& filter);
    QString filter() const { return m_filter; }
    QString defaultExtension() const { return m_defaultExt; }

    // Names shown in the Files box (".." first, then directories, then files).
    QStringList files() const { return m_files; }
    void selectFile(const QString& name);
    QString selectedFile() const;

    void setEditText(const QString& text);
    QString editText() const;

    // The path bar along the top: where the Files box is looking.
    void setPathText(const QString& text);
    QString pathText() const;

    // The file-type box along the bottom. Free text ("*.md", "Text files
    // (*.txt)", "All files (*)"); typePresets() feeds its dropdown.
    void setTypeText(const QString& text);
    QString typeText() const;
    static QStringList typePresets();

    // Act on what the path bar says: go to a directory, or go to a file's
    // directory and drop its name into the name box. Bound to the bar's Return
    // key; public so the behaviour is testable without a pointer.
    void navigateToTypedPath();

    // The last-10 recents shown by the name box's green dropdown cell.
    void setRecents(const QStringList& paths);
    QStringList recents() const { return m_recents; }
    void applyRecent(const QString& path);
    void showRecentsMenu();
    void showTypeMenu();

    // Finish: accept() emits the resolved absolute path, reject() emits an
    // empty one. Both hide the dock.
    void accept();
    void reject();

    // Geometry, exposed for tests and hit-testing.
    QRect listRect() const { return m_listRect; }
    QRect nameRect() const { return m_nameRect; }
    QRect pathRect() const { return m_pathRect; }
    QRect typeRect() const { return m_typeRect; }
    QRect nameDropRect() const { return m_nameDropRect; }
    QRect typeDropRect() const { return m_typeDropRect; }
    QRect frameRect() const { return m_frameRect; }
    // The field the user last clicked (null until then). Exposed because the
    // focus mark is the only feedback a never-activated window can give.
    QWidget* activeField() const { return m_activeField; }
    QRect okRect() const { return m_okRect; }
    QRect cancelRect() const { return m_cancelRect; }
    QRect helpRect() const { return m_helpRect; }

    // Show a dock over `anchor` and return it (not modal: the caller decides).
    static TvFileDock* openOn(QWidget* anchor, Mode mode, const QString& title,
                              const QString& dir = QString(),
                              const QString& filter = QString(),
                              const QStringList& recents = QStringList());
    // Modal convenience wrappers used by the File menu. They run a local event
    // loop (a plain widget has no popup grab, so unlike QMenu::exec this one
    // exits reliably on dialogFinished).
    static QString getOpenFileName(QWidget* anchor, const QString& title,
                                   const QString& dir = QString(),
                                   const QString& filter = QString(),
                                   const QStringList& recents = QStringList());
    static QString getSaveFileName(QWidget* anchor, const QString& title,
                                   const QString& dir = QString(),
                                   const QString& filter = QString(),
                                   const QStringList& recents = QStringList());

signals:
    void dialogFinished(const QString& path);
    // The green cells were clicked (the pointer-free test hook, same shape as
    // TabStrip::tabListRequested).
    void recentsRequested();
    void typePresetsRequested();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseDoubleClickEvent(QMouseEvent* ev) override;
    void wheelEvent(QWheelEvent* ev) override;
    void keyPressEvent(QKeyEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;
    void changeEvent(QEvent* ev) override;
    bool eventFilter(QObject* watched, QEvent* ev) override;

private:
    // Shared modal round loop for the two convenience wrappers.
    static QString runModal(QWidget* anchor, Mode mode, const QString& title,
                            const QString& dir, const QString& filter,
                            const QStringList& recents);

    void refreshFiles();
    QStringList filterGlobs() const;           // "Text files (*.txt)" -> {"*.txt"}
    bool matchesFilter(const QString& name) const;
    void layoutChildren();
    void syncSelectionFromEdit();
    void moveSelection(int delta);
    void descendInto(const QString& name);
    QString resolvedPath() const;

    int cellW() const;
    int cellH() const;
    int visibleRows() const;
    int rowAt(const QPoint& p) const;      // -1 when the point is not on a row
    int columnAt(const QPoint& p) const;   // 0 = left column, 1 = right
    int rowIndex(int column, int row) const;   // list index for a cell
    QRect rowRect(int index) const;            // painted rect of list[index]
    void ensureVisible(int index);

    Mode m_mode = Open;
    QString m_title = QStringLiteral("Open File");
    QString m_dir;
    QString m_filter;
    QString m_defaultExt = QStringLiteral("txt");
    QStringList m_files;
    QStringList m_recents;
    int m_selected = -1;
    int m_scroll = 0;

    QLineEdit* m_edit = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QLineEdit* m_typeEdit = nullptr;
    QWidget* m_activeField = nullptr;
    QPushButton* m_ok = nullptr;
    QPushButton* m_cancel = nullptr;
    QPushButton* m_help = nullptr;

    QRect m_frameRect;
    QRect m_listRect;
    QRect m_pathRect;
    QRect m_nameRect;
    QRect m_typeRect;
    QRect m_nameDropRect;     // green completion cell right of the name box
    QRect m_typeDropRect;     // green preset cell right of the type box
    QRect m_okRect;
    QRect m_cancelRect;
    QRect m_helpRect;
    QRect m_statusRect;
    QRect m_pathLabelRect;    // "Path"
    QRect m_filesLabelRect;   // "Files"
    QRect m_nameLabelRect;    // "File name"
    QRect m_typeLabelRect;    // "File type"
};
