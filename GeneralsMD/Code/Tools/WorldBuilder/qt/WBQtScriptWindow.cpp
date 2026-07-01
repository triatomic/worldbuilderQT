// WBQtScriptWindow.cpp -- see WBQtScriptWindow.h.
#include "WBQtScriptWindow.h"
#include "WBQtPanelBridge.h"
#include "qwinwidget.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <qt_windows.h>

WBQtScriptWindow *WBQtScriptWindow::s_instance = NULL;

// The list index the tree stores per node is the packed ListType int (opaque to Qt).
static const int kListTypeRole = Qt::UserRole + 1;

// A dedicated owner bridge for the script window (separate from the option panels' owner).
static QWinWidget *g_scriptOwner = NULL;

WBQtScriptWindow::WBQtScriptWindow(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_updating(false)
{
	setWindowTitle("Script Editor");
	resize(420, 640);

	QVBoxLayout *root = new QVBoxLayout(this);

	m_tree = new QTreeWidget(this);
	m_tree->setHeaderHidden(true);
	m_tree->setColumnCount(1);
	root->addWidget(m_tree, 1);

	// Script/folder command row.
	QHBoxLayout *cmdRow = new QHBoxLayout();
	m_newFolder = new QPushButton("New Folder", this);
	m_newScript = new QPushButton("New Script", this);
	m_editScript = new QPushButton("Edit", this);
	m_copyScript = new QPushButton("Copy", this);
	m_delete = new QPushButton("Delete", this);
	cmdRow->addWidget(m_newFolder);
	cmdRow->addWidget(m_newScript);
	cmdRow->addWidget(m_editScript);
	cmdRow->addWidget(m_copyScript);
	cmdRow->addWidget(m_delete);
	root->addLayout(cmdRow);

	// Commit row.
	QHBoxLayout *okRow = new QHBoxLayout();
	okRow->addStretch(1);
	m_ok = new QPushButton("OK", this);
	m_cancel = new QPushButton("Cancel", this);
	okRow->addWidget(m_ok);
	okRow->addWidget(m_cancel);
	root->addLayout(okRow);

	rebuildTree();
	updateButtonStates();

	connect(m_tree, SIGNAL(itemSelectionChanged()), this, SLOT(onTreeSelectionChanged()));
	connect(m_newFolder, SIGNAL(clicked()), this, SLOT(onNewFolder()));
	connect(m_newScript, SIGNAL(clicked()), this, SLOT(onNewScript()));
	connect(m_editScript, SIGNAL(clicked()), this, SLOT(onEditScript()));
	connect(m_copyScript, SIGNAL(clicked()), this, SLOT(onCopyScript()));
	connect(m_delete, SIGNAL(clicked()), this, SLOT(onDelete()));
	connect(m_ok, SIGNAL(clicked()), this, SLOT(onOk()));
	connect(m_cancel, SIGNAL(clicked()), this, SLOT(onCancel()));

	s_instance = this;
}

void WBQtScriptWindow::rebuildTree()
{
	m_updating = true;
	m_tree->clear();

	// Parent for each depth level: depth 0 -> top level, 1 -> under last depth-0, etc.
	QTreeWidgetItem *lastAtDepth[3] = { NULL, NULL, NULL };

	const int cap = 512;
	char labelBuf[cap];
	int count = WBQtScript_GetNodeCount();
	for (int i = 0; i < count; ++i)
	{
		int depth = 0, listType = 0;
		if (!WBQtScript_GetNode(i, &depth, &listType, labelBuf, cap))
		{
			continue;
		}
		if (depth < 0 || depth > 2)
		{
			continue;
		}

		QTreeWidgetItem *item = NULL;
		if (depth == 0)
		{
			item = new QTreeWidgetItem(m_tree);
		}
		else
		{
			QTreeWidgetItem *parent = lastAtDepth[depth - 1];
			if (parent == NULL)
			{
				// Malformed ordering -- fall back to top level rather than drop the node.
				item = new QTreeWidgetItem(m_tree);
			}
			else
			{
				item = new QTreeWidgetItem(parent);
			}
		}
		item->setText(0, QString::fromLatin1(labelBuf));
		item->setData(0, kListTypeRole, listType);
		lastAtDepth[depth] = item;
		// Deeper levels can't dangle off a stale parent once this node opens a new branch.
		for (int d = depth + 1; d < 3; ++d)
		{
			lastAtDepth[d] = NULL;
		}
	}

	m_tree->expandAll();
	m_updating = false;
}

void WBQtScriptWindow::pushSelectionToDialog()
{
	QList<QTreeWidgetItem*> sel = m_tree->selectedItems();
	if (sel.isEmpty())
	{
		return;
	}
	int listType = sel.first()->data(0, kListTypeRole).toInt();
	WBQtScript_SetSelection(listType);
}

void WBQtScriptWindow::updateButtonStates()
{
	bool haveSel = !m_tree->selectedItems().isEmpty();
	bool hasScript = haveSel && (WBQtScript_HasScript() != 0);
	bool hasGroup = haveSel && (WBQtScript_HasGroup() != 0);

	// Edit/Copy act on a script or a group; Delete acts on anything except a player. New
	// Folder / New Script are always available (they insert relative to the selection).
	m_editScript->setEnabled(hasScript || hasGroup);
	m_copyScript->setEnabled(hasScript || hasGroup);
	m_delete->setEnabled(hasScript || hasGroup);
}

void WBQtScriptWindow::onTreeSelectionChanged()
{
	if (m_updating)
	{
		return;
	}
	pushSelectionToDialog();
	updateButtonStates();
}

void WBQtScriptWindow::onNewFolder()
{
	pushSelectionToDialog();
	WBQtScript_NewFolder();
	rebuildTree();
	updateButtonStates();
}

void WBQtScriptWindow::onNewScript()
{
	pushSelectionToDialog();
	WBQtScript_NewScript();
	rebuildTree();
	updateButtonStates();
}

void WBQtScriptWindow::onEditScript()
{
	pushSelectionToDialog();
	WBQtScript_EditScript();
	rebuildTree();
	updateButtonStates();
}

void WBQtScriptWindow::onCopyScript()
{
	pushSelectionToDialog();
	WBQtScript_CopyScript();
	rebuildTree();
	updateButtonStates();
}

void WBQtScriptWindow::onDelete()
{
	pushSelectionToDialog();
	WBQtScript_Delete();
	rebuildTree();
	updateButtonStates();
}

void WBQtScriptWindow::onOk()
{
	WBQtScript_Commit();
	WBQtScript_Close();
}

void WBQtScriptWindow::onCancel()
{
	WBQtScript_Cancel();
	WBQtScript_Close();
}

// --- Open / close (Qt-side), the front-end lifetime called from CMainFrame::onEditScripts --
extern "C" void WBQtScript_Open(void *frameHwnd, int x, int y)
{
	if (qApp == NULL || frameHwnd == NULL)
	{
		return;
	}
	if (g_scriptOwner == NULL)
	{
		g_scriptOwner = new QWinWidget(reinterpret_cast<HWND>(frameHwnd));
	}

	WBQtScriptWindow *win = WBQtScriptWindow::instance();
	if (win == NULL)
	{
		win = new WBQtScriptWindow(g_scriptOwner);
	}
	else
	{
		win->rebuildTree();
	}

	win->setAttribute(Qt::WA_ShowWithoutActivating, true);
	win->move(x, y);
	win->show();
	win->raise();
}

extern "C" void WBQtScript_Close(void)
{
	WBQtScriptWindow *win = WBQtScriptWindow::instance();
	if (win != NULL)
	{
		win->hide();
	}
}
