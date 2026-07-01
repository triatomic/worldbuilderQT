// WBQtScriptWindow.cpp -- see WBQtScriptWindow.h.
#include "WBQtScriptWindow.h"
#include "WBQtPanelBridge.h"

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QDropEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <qt_windows.h>

WBQtScriptWindow *WBQtScriptWindow::s_instance = NULL;

// The list index the tree stores per node is the packed ListType int (opaque to Qt).
static const int kListTypeRole = Qt::UserRole + 1;

//----------------------------------------------------------------------------------------
// WBQtScriptTree
//----------------------------------------------------------------------------------------
WBQtScriptTree::WBQtScriptTree(WBQtScriptWindow *owner)
	: QTreeWidget(owner),
	  m_owner(owner)
{
}

void WBQtScriptTree::dropEvent(QDropEvent *event)
{
	QTreeWidgetItem *dragItem = currentItem();
	QTreeWidgetItem *targetItem = itemAt(event->pos());
	if (dragItem == NULL || targetItem == NULL || dragItem == targetItem)
	{
		event->ignore();
		return;
	}
	int dragLT = dragItem->data(0, kListTypeRole).toInt();
	int targetLT = targetItem->data(0, kListTypeRole).toInt();

	// Don't let Qt actually move the items -- the model rebuild reflects the real move.
	event->setDropAction(Qt::IgnoreAction);
	event->accept();
	m_owner->handleDrop(dragLT, targetLT);
}

//----------------------------------------------------------------------------------------
// WBQtScriptWindow
//----------------------------------------------------------------------------------------
WBQtScriptWindow::WBQtScriptWindow(QWidget *owner)
	: QWidget(NULL, Qt::Window),
	  m_lastFoundListType(0),
	  m_updating(false)
{
	// Deliberately a STANDALONE top-level window (parent = NULL), NOT a child of the QWinWidget
	// owner like the option panels. A QWinWidget-child reflects keyboard activation back to its
	// native MFC parent (the frame), so keystrokes fell through to the main view's tool hotkeys
	// instead of the search / rename fields. A real top-level owns its own focus. It still gets
	// the dark title bar (WBQtTheme targets all top-level windows). owner is now unused.
	(void)owner;
	setWindowTitle("Script Editor");
	resize(420, 660);

	QVBoxLayout *root = new QVBoxLayout(this);

	// Search row.
	QHBoxLayout *searchRow = new QHBoxLayout();
	m_search = new QLineEdit(this);
	m_search->setPlaceholderText("Find (name / comment / parameter)...");
	m_findBtn = new QPushButton("Find Next", this);
	searchRow->addWidget(m_search, 1);
	searchRow->addWidget(m_findBtn);
	root->addLayout(searchRow);

	m_tree = new WBQtScriptTree(this);
	m_tree->setHeaderHidden(true);
	m_tree->setColumnCount(1);
	// Internal drag-drop; the actual move is done by the model rebuild in handleDrop.
	m_tree->setDragEnabled(true);
	m_tree->setAcceptDrops(true);
	m_tree->setDragDropMode(QAbstractItemView::InternalMove);
	m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
	m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
	root->addWidget(m_tree, 1);

	// Script/folder command row.
	QHBoxLayout *cmdRow = new QHBoxLayout();
	m_newFolder = new QPushButton("New Folder", this);
	m_newScript = new QPushButton("New Script", this);
	m_editScript = new QPushButton("Edit", this);
	m_copyScript = new QPushButton("Copy", this);
	m_delete = new QPushButton("Delete", this);
	m_verify = new QPushButton("Verify", this);
	cmdRow->addWidget(m_newFolder);
	cmdRow->addWidget(m_newScript);
	cmdRow->addWidget(m_editScript);
	cmdRow->addWidget(m_copyScript);
	cmdRow->addWidget(m_delete);
	cmdRow->addWidget(m_verify);
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
	connect(m_findBtn, SIGNAL(clicked()), this, SLOT(onFind()));
	connect(m_search, SIGNAL(returnPressed()), this, SLOT(onFind()));
	connect(m_newFolder, SIGNAL(clicked()), this, SLOT(onNewFolder()));
	connect(m_newScript, SIGNAL(clicked()), this, SLOT(onNewScript()));
	connect(m_editScript, SIGNAL(clicked()), this, SLOT(onEditScript()));
	connect(m_copyScript, SIGNAL(clicked()), this, SLOT(onCopyScript()));
	connect(m_delete, SIGNAL(clicked()), this, SLOT(onDelete()));
	connect(m_verify, SIGNAL(clicked()), this, SLOT(onVerify()));
	connect(m_tree, SIGNAL(customContextMenuRequested(const QPoint &)),
		this, SLOT(onTreeContextMenu(const QPoint &)));
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
		int depth = 0, listType = 0, flags = 0;
		if (!WBQtScript_GetNode(i, &depth, &listType, &flags, labelBuf, cap))
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
		// Players and folders can receive drops; scripts are drag sources but reordering onto
		// a script inserts next to it, so they accept drops too. Everything is draggable.
		item->setFlags(item->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);

		// Visual state (script/group nodes only; player nodes report flags 0). Warnings ->
		// red, inactive -> dimmed, subroutine -> italic. Mirrors the MFC state-icon meaning.
		if (depth > 0)
		{
			const bool active = (flags & 1) != 0;
			const bool warnings = (flags & 2) != 0;
			const bool subroutine = (flags & 4) != 0;
			if (warnings)
			{
				item->setForeground(0, QBrush(QColor(200, 60, 60)));
			}
			else if (!active)
			{
				item->setForeground(0, QBrush(QColor(140, 140, 140)));
			}
			if (subroutine)
			{
				QFont f = item->font(0);
				f.setItalic(true);
				item->setFont(0, f);
			}
		}

		lastAtDepth[depth] = item;
		for (int d = depth + 1; d < 3; ++d)
		{
			lastAtDepth[d] = NULL;
		}
	}

	m_tree->expandAll();
	m_updating = false;
}

void WBQtScriptWindow::resetForNewSession()
{
	// A new ScriptDialog session means a fresh model with fresh ListType indices; a find cursor
	// captured against the previous session's model is meaningless (and dangerous) now.
	m_lastFoundListType = 0;
	if (m_search != NULL)
	{
		m_search->clear();
	}
}

int WBQtScriptWindow::selectedListType() const
{
	QList<QTreeWidgetItem*> sel = m_tree->selectedItems();
	if (sel.isEmpty())
	{
		return -1;
	}
	return sel.first()->data(0, kListTypeRole).toInt();
}

void WBQtScriptWindow::selectByListType(int listType)
{
	m_updating = true;
	for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
	{
		if ((*it)->data(0, kListTypeRole).toInt() == listType)
		{
			m_tree->setCurrentItem(*it);
			m_tree->scrollToItem(*it);
			break;
		}
	}
	m_updating = false;
}

void WBQtScriptWindow::pushSelectionToDialog()
{
	int lt = selectedListType();
	if (lt != -1)
	{
		WBQtScript_SetSelection(lt);
	}
}

void WBQtScriptWindow::updateButtonStates()
{
	bool haveSel = !m_tree->selectedItems().isEmpty();
	bool hasScript = haveSel && (WBQtScript_HasScript() != 0);
	bool hasGroup = haveSel && (WBQtScript_HasGroup() != 0);

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

void WBQtScriptWindow::handleDrop(int dragListType, int targetListType)
{
	WBQtScript_DropOn(dragListType, targetListType);
	rebuildTree();
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

void WBQtScriptWindow::onVerify()
{
	WBQtScript_Verify();
	rebuildTree();	// pick up the recomputed warning flags
	updateButtonStates();
}

void WBQtScriptWindow::onToggleActive()
{
	pushSelectionToDialog();
	WBQtScript_ToggleActive();
	rebuildTree();	// active flag changed -> restyle + relabel
	updateButtonStates();
}

void WBQtScriptWindow::onTreeContextMenu(const QPoint &pos)
{
	QTreeWidgetItem *item = m_tree->itemAt(pos);
	if (item == NULL)
	{
		return;
	}
	m_tree->setCurrentItem(item);	// so the seam acts on the right-clicked node
	pushSelectionToDialog();

	bool hasScript = (WBQtScript_HasScript() != 0);
	bool hasGroup = (WBQtScript_HasGroup() != 0);

	QMenu menu(this);
	QAction *actActivate = menu.addAction("Activate / Deactivate");
	actActivate->setEnabled(hasScript || hasGroup);
	menu.addSeparator();
	QAction *actEdit = menu.addAction("Edit");
	actEdit->setEnabled(hasScript || hasGroup);
	QAction *actCopy = menu.addAction("Copy");
	actCopy->setEnabled(hasScript || hasGroup);
	QAction *actDelete = menu.addAction("Delete");
	actDelete->setEnabled(hasScript || hasGroup);

	QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
	if (chosen == actActivate)
	{
		onToggleActive();
	}
	else if (chosen == actEdit)
	{
		onEditScript();
	}
	else if (chosen == actCopy)
	{
		onCopyScript();
	}
	else if (chosen == actDelete)
	{
		onDelete();
	}
}

void WBQtScriptWindow::onFind()
{
	QByteArray text = m_search->text().trimmed().toLatin1();
	if (text.isEmpty())
	{
		return;
	}
	int out = 0;
	if (WBQtScript_FindNext(text.constData(), m_lastFoundListType, &out))
	{
		m_lastFoundListType = out;
		selectByListType(out);
		// selectByListType suppressed the selection signal; sync the dialog + buttons.
		WBQtScript_SetSelection(out);
		updateButtonStates();
	}
	else
	{
		// Wrap around: next Find starts from the top again.
		m_lastFoundListType = 0;
		QApplication::beep();
	}
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

	// The Script window is a standalone top-level (see the ctor) -- it doesn't need a
	// QWinWidget owner. frameHwnd is only used to pull foreground focus onto the Qt window.
	WBQtScriptWindow *win = WBQtScriptWindow::instance();
	if (win == NULL)
	{
		win = new WBQtScriptWindow(NULL);
	}
	else
	{
		win->rebuildTree();
	}
	// Fresh session: clear any leftover search text + find cursor from a previous ScriptDialog
	// (the model was rebuilt, so the old find cursor's ListType no longer maps to it).
	win->resetForNewSession();

	// Unlike the transient option panels (which use WA_ShowWithoutActivating so they never
	// steal viewport focus mid-paint), the Script editor is a real interactive window: it
	// must ACTIVATE and take keyboard focus, otherwise keystrokes fall through to the MFC
	// main view and fire tool hotkeys instead of typing into the search / rename fields.
	win->move(x, y);
	win->show();
	win->raise();
	win->activateWindow();
	// Pull Win32 foreground/focus to the Qt window's HWND so its widgets receive keys through
	// the QMfcApp message hook rather than MFC's frame accelerators.
	HWND h = reinterpret_cast<HWND>(win->winId());
	::SetForegroundWindow(h);
	::SetFocus(h);
	win->setFocus(Qt::ActiveWindowFocusReason);
}

extern "C" void WBQtScript_Close(void)
{
	WBQtScriptWindow *win = WBQtScriptWindow::instance();
	if (win != NULL)
	{
		win->hide();
	}
}

extern "C" int WBQtScript_OwnsFocus(void)
{
	WBQtScriptWindow *win = WBQtScriptWindow::instance();
	if (win == NULL || !win->isVisible())
	{
		return 0;
	}
	// True if the currently focused Win32 window is the script window's top-level HWND or a
	// descendant of it (its child controls -- the tree, the line-edits -- are child HWNDs).
	HWND focus = ::GetFocus();
	HWND winHwnd = reinterpret_cast<HWND>(win->winId());
	if (focus == NULL || winHwnd == NULL)
	{
		return 0;
	}
	if (focus == winHwnd || ::IsChild(winHwnd, focus))
	{
		return 1;
	}
	return 0;
}
