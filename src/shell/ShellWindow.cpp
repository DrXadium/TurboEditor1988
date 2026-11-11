// SPDX-License-Identifier: GPL-3.0-or-later
#include "ShellWindow.h"
#include "BorlandStyle.h"
#include "ScrollBarInstaller.h"
#include "MenuBar.h"
#include "TabStrip.h"
#include "Minimap.h"
#include "FKeyStatusBar.h"
#include "BottomBand.h"
#include <QApplication>
#include <QFile>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>
#include <QLayout>
#include <QPainter>
#include <QPaintEvent>

namespace {
const QColor kFrameGray(0xAA, 0xAA, 0xAA);
const QColor kFrameWhite(0xFF, 0xFF, 0xFF);   // TV-revision spec 3: white double line
const QColor kBgBlue(0x00, 0x00, 0xAA);

// Central editor host: fills the TV window-blue desktop, then draws the
// left/right/bottom double-line frame edges as part of its OWN paint (so a
// child editor painting inside won't cover the frame — this widget's paint is
// the frame layer). The TabStrip title line supplies the top edge + boxes.
class EditorHost : public QWidget
{
public:
    explicit EditorHost(QWidget* parent = nullptr) : QWidget(parent)
    {
        setObjectName("editorHost");
        setAutoFillBackground(false);   // we paint the blue bg ourselves
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        const QRect r = rect();
        p.fillRect(r, kBgBlue);
        p.setPen(kFrameWhite);
        // Double-line frame flush to the widget edges (r.bottom()/r.right() are
        // the LAST valid rows/cols). Verticals run the full height; the bottom
        // pair sits on the last two rows so no blue gap shows at the edge and
        // all four corners join solidly.
        const int bot = r.bottom();     // last row
        const int right = r.right();    // last col
        // Left vertical double-line.
        p.drawLine(1, 0, 1, bot);
        p.drawLine(2, 0, 2, bot);
        // Right vertical double-line.
        p.drawLine(right - 2, 0, right - 2, bot);
        p.drawLine(right - 3, 0, right - 3, bot);
        // Bottom horizontal double-line (last two rows).
        p.drawLine(0, bot - 1, right, bot - 1);
        p.drawLine(0, bot, right, bot);
        // Corner connectors (join to the TabStrip title-line top edge).
        p.drawLine(1, 0, 2, 0);
        p.drawLine(right - 3, 0, right - 2, 0);
        // Solid bottom corner joints (verticals meet the bottom pair).
        p.drawLine(1, bot - 1, 2, bot);
        p.drawLine(right - 3, bot - 1, right - 2, bot);
    }
};
} // namespace

ShellWindow::ShellWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("CRT-Kate"));
    setObjectName("shellWindow");
    resize(900, 600);

    // Borland style applied app-wide (spec: qApp->setStyle(new BorlandStyle)).
    m_borland = new BorlandStyle;
    qApp->setStyle(m_borland);
    // Convert plain QScrollBars to TV TextScrollBars app-wide.
    auto* sbInstall = new ScrollBarInstaller(qApp);
    qApp->installEventFilter(sbInstall);

    // Force the QSS resource into the link (static lib resources are dropped
    // by the linker unless referenced).
    Q_INIT_RESOURCE(shell_qss);

    // Load the navy QSS (qt_add_resources BASE=src/shell => :/borland.qss).
    QFile qss(QStringLiteral(":/borland.qss"));
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text))
        qApp->setStyleSheet(QString::fromUtf8(qss.readAll()));

    // Menu bar (top).
    auto* menu = new MenuBar(this);
    setMenuBar(menu);

    // Central editor host (sunken panel) + TabStrip above it. EditorHost fills
    // the TV window-blue desktop and draws the frame edges as its own paint.
    // The layout has a 3px margin so child content (editor/placeholder) never
    // covers the 3px frame border the host paints at its edges.
    auto* central = new EditorHost(this);
    auto* lay = new QVBoxLayout(central);
    lay->setContentsMargins(3, 0, 3, 3);   // expose the frame border on L/R/bottom
    lay->setSpacing(0);
    auto* tabs = new TabStrip(central);
    lay->addWidget(tabs);
    auto* placeholder = new QLabel(QStringLiteral("Editor"), central);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setObjectName("editorPlaceholder");
    lay->addWidget(placeholder, 1);
    m_editorLayout = lay;   // slot below TabStrip (setEditorWidget swaps here)
    setCentralWidget(central);

    // Bottom band (cursor position + horizontal scrollbar) sits between the
    // editor and the status line (spec 9 row order). The EditorHost's bottom
    // frame edge is suppressed: the band now supplies the bottom border row.
    m_bottomBand = new BottomBand(this);
    auto* outer = new QVBoxLayout;
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    auto* centralWrap = new QWidget(this);
    auto* cw = new QVBoxLayout(centralWrap);
    cw->setContentsMargins(0, 0, 0, 0);
    cw->setSpacing(0);
    cw->addWidget(central, 1);
    cw->addWidget(m_bottomBand, 0);
    outer->addWidget(centralWrap);
    setCentralWidget(centralWrap);

    // F-key status bar (bottom).
    auto* fkeys = new FKeyStatusBar(this);
    setStatusBar(fkeys);
}

void ShellWindow::setEditorWidget(QWidget* w)
{
    if (!m_editorLayout || !w)
        return;
    // Remove the current placeholder/editor row (everything below the TabStrip)
    // and replace it with an editor+minimap row. Index 0 is always the TabStrip.
    while (m_editorLayout->count() > 1) {
        QLayoutItem* old = m_editorLayout->takeAt(1);
        if (old) {
            if (old->widget() && old->widget() != w && !qobject_cast<Minimap*>(old->widget()))
                old->widget()->deleteLater();
            delete old;
        }
    }

    // Editor (stretch) + Minimap (fixed strip) side by side.
    auto* row = new QWidget;
    row->setObjectName(QStringLiteral("editorRow"));
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    w->setParent(row);
    h->addWidget(w, 1);
    auto* mini = new Minimap(row);
    mini->setObjectName(QStringLiteral("minimap"));
    h->addWidget(mini, 0);
    m_minimap = mini;
    m_editorLayout->addWidget(row);
}

void ShellWindow::setCentralWidget(QWidget* w)
{
    QMainWindow::setCentralWidget(w);
}

TabStrip* ShellWindow::tabStrip() const
{
    return findChild<TabStrip*>();
}

FKeyStatusBar* ShellWindow::fkeyStatusBar() const
{
    return findChild<FKeyStatusBar*>();
}

BottomBand* ShellWindow::bottomBand() const
{
    return m_bottomBand;
}

void ShellWindow::paintEvent(QPaintEvent* ev)
{
    QMainWindow::paintEvent(ev);
}
