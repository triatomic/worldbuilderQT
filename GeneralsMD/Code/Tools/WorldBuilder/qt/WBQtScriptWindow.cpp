// WBQtScriptWindow.cpp -- see WBQtScriptWindow.h.
#include "WBQtScriptWindow.h"
#include "WBQtPanelBridge.h"
#include "WBQtTreeStyle.h"
#include "WBQtWindowPos.h"

#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QDropEvent>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStyle>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <QWindow>

#include <qt_windows.h>

// Defined in WBQtBridge.cpp: the Qt main window's HWND when inverted, else the passed
// MFC frame HWND -- the native owner for standalone Qt top-levels.
void *WBQt_EffectiveOwnerHwnd(void *frameHwnd);
// Defined in WBQtBridge.cpp: the Qt main window as a QWidget* (NULL when not inverted).
QWidget *WBQt_MainWindowWidget(void);

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
	resize(900, 640);
	WBQtWindowPos_Track(this, "ScriptEditor");

	QVBoxLayout *root = new QVBoxLayout(this);

	// --- Option checkboxes (top strip), mirroring the MFC dialog's row ---
	QGroupBox *optBox = new QGroupBox("Options", this);
	QGridLayout *optGrid = new QGridLayout(optBox);
	m_ckCompress = new QCheckBox("Compress Script", optBox);
	m_ckNewIcons = new QCheckBox("New Icons", optBox);
	// The Qt tree always draws the native Qt icon set -- there is no "old icons" BMP set
	// on the Qt side -- so the choice is fixed: show the box checked and grayed out.
	m_ckNewIcons->setChecked(true);
	m_ckNewIcons->setEnabled(false);
	m_ckCleanName = new QCheckBox("Clean Script Name", optBox);
	m_ckAutoVerify = new QCheckBox("Auto Verify", optBox);
	m_ckSmartCopy = new QCheckBox("Smart Copy", optBox);
	m_ckFastLoad = new QCheckBox("Fast Load", optBox);
	m_ckScriptMerge = new QCheckBox("Script Merge", optBox);
	m_ckRefByParam = new QCheckBox("Detect References via picked Parameters", optBox);
	m_ckDisableRef = new QCheckBox("Disable references (reduces input lag)", optBox);
	optGrid->addWidget(m_ckCompress, 0, 0);
	optGrid->addWidget(m_ckNewIcons, 0, 1);
	optGrid->addWidget(m_ckCleanName, 0, 2);
	optGrid->addWidget(m_ckAutoVerify, 0, 3);
	optGrid->addWidget(m_ckSmartCopy, 1, 0);
	optGrid->addWidget(m_ckFastLoad, 1, 1);
	optGrid->addWidget(m_ckScriptMerge, 1, 2);
	optGrid->addWidget(m_ckRefByParam, 2, 0, 1, 2);
	optGrid->addWidget(m_ckDisableRef, 2, 2, 1, 2);
	root->addWidget(optBox);

	// --- Search row ---
	QHBoxLayout *searchRow = new QHBoxLayout();
	searchRow->addWidget(new QLabel("Search:", this));
	m_search = new QLineEdit(this);
	m_search->setPlaceholderText("name / comment / parameter");
	m_findBtn = new QPushButton("Find Next", this);
	searchRow->addWidget(m_search, 1);
	searchRow->addWidget(m_findBtn);
	root->addLayout(searchRow);

	// --- Middle: tree | (description over comment) ---
	QSplitter *split = new QSplitter(Qt::Horizontal, this);

	m_tree = new WBQtScriptTree(this);
	m_tree->setHeaderHidden(true);
	m_tree->setColumnCount(1);
	m_tree->setDragEnabled(true);
	m_tree->setAcceptDrops(true);
	m_tree->setDragDropMode(QAbstractItemView::InternalMove);
	m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
	m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
	WBQtTreeStyle::applyTreeLines(m_tree);
	split->addWidget(m_tree);

	QWidget *detailPane = new QWidget(split);
	QVBoxLayout *detailLay = new QVBoxLayout(detailPane);
	detailLay->setContentsMargins(0, 0, 0, 0);
	m_description = new QPlainTextEdit(detailPane);
	m_description->setReadOnly(true);
	m_comment = new QPlainTextEdit(detailPane);
	m_comment->setReadOnly(true);
	m_comment->setMaximumHeight(140);
	detailLay->addWidget(m_description, 1);
	detailLay->addWidget(m_comment);
	split->addWidget(detailPane);
	split->setStretchFactor(0, 1);
	split->setStretchFactor(1, 2);
	root->addWidget(split, 1);

	// --- Command button rows ---
	QHBoxLayout *cmdRow = new QHBoxLayout();
	m_newFolder = new QPushButton("New Folder", this);
	m_newScript = new QPushButton("New Script", this);
	m_editScript = new QPushButton("Edit", this);
	m_copyScript = new QPushButton("Copy", this);
	m_delete = new QPushButton("Delete", this);
	m_verify = new QPushButton("Re-Verify All", this);
	cmdRow->addWidget(m_newFolder);
	cmdRow->addWidget(m_newScript);
	cmdRow->addWidget(m_editScript);
	cmdRow->addWidget(m_copyScript);
	cmdRow->addWidget(m_delete);
	cmdRow->addWidget(m_verify);
	root->addLayout(cmdRow);

	QHBoxLayout *cmdRow2 = new QHBoxLayout();
	m_addDebug = new QPushButton("Add Debug", this);
	m_removeDebug = new QPushButton("Delete Debug", this);
	m_patchGC = new QPushButton("Patch \"GC_\"", this);
	m_export = new QPushButton("Export Scripts", this);
	m_import = new QPushButton("Import Scripts", this);
	m_saveNow = new QPushButton("Save Now (Ctrl+S)", this);
	cmdRow2->addWidget(m_addDebug);
	cmdRow2->addWidget(m_removeDebug);
	cmdRow2->addWidget(m_patchGC);
	cmdRow2->addWidget(m_export);
	cmdRow2->addWidget(m_import);
	cmdRow2->addWidget(m_saveNow);
	root->addLayout(cmdRow2);

	// --- Commit row ---
	QHBoxLayout *okRow = new QHBoxLayout();
	okRow->addStretch(1);
	m_ok = new QPushButton("Save + Close", this);
	m_cancel = new QPushButton("Cancel", this);
	okRow->addWidget(m_ok);
	okRow->addWidget(m_cancel);
	root->addLayout(okRow);

	buildIcons();
	seedCheckboxes();
	rebuildTree();
	updateButtonStates();
	updateDetail();

	connect(m_tree, SIGNAL(itemSelectionChanged()), this, SLOT(onTreeSelectionChanged()));
	connect(m_findBtn, SIGNAL(clicked()), this, SLOT(onFind()));
	connect(m_search, SIGNAL(returnPressed()), this, SLOT(onFind()));
	connect(m_newFolder, SIGNAL(clicked()), this, SLOT(onNewFolder()));
	connect(m_newScript, SIGNAL(clicked()), this, SLOT(onNewScript()));
	connect(m_editScript, SIGNAL(clicked()), this, SLOT(onEditScript()));
	connect(m_copyScript, SIGNAL(clicked()), this, SLOT(onCopyScript()));
	connect(m_delete, SIGNAL(clicked()), this, SLOT(onDelete()));
	connect(m_verify, SIGNAL(clicked()), this, SLOT(onVerify()));
	connect(m_addDebug, SIGNAL(clicked()), this, SLOT(onAddDebug()));
	connect(m_removeDebug, SIGNAL(clicked()), this, SLOT(onRemoveDebug()));
	connect(m_patchGC, SIGNAL(clicked()), this, SLOT(onPatchGC()));
	connect(m_export, SIGNAL(clicked()), this, SLOT(onExport()));
	connect(m_import, SIGNAL(clicked()), this, SLOT(onImport()));
	connect(m_saveNow, SIGNAL(clicked()), this, SLOT(onSaveNow()));
	connect(m_tree, SIGNAL(customContextMenuRequested(const QPoint &)),
		this, SLOT(onTreeContextMenu(const QPoint &)));
	connect(m_tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),
		this, SLOT(onTreeDoubleClicked(QTreeWidgetItem *, int)));
	connect(m_ok, SIGNAL(clicked()), this, SLOT(onOk()));
	connect(m_cancel, SIGNAL(clicked()), this, SLOT(onCancel()));

	// Ctrl+S = Save Now, matching the MFC dialog's accelerator.
	QShortcut *saveSc = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_S), this);
	connect(saveSc, SIGNAL(activated()), this, SLOT(onSaveNow()));

	// Tree-scoped editing keys. WidgetShortcut = active only while the tree itself has
	// focus, so Delete / Ctrl+Z keep their text-editing meaning inside the search and
	// rename line edits.
	QShortcut *delSc = new QShortcut(QKeySequence(Qt::Key_Delete), m_tree);
	delSc->setContext(Qt::WidgetShortcut);
	connect(delSc, SIGNAL(activated()), this, SLOT(onDeleteShortcut()));
	QShortcut *undoSc = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Z), m_tree);
	undoSc->setContext(Qt::WidgetShortcut);
	connect(undoSc, SIGNAL(activated()), this, SLOT(onUndo()));
	QShortcut *redoSc = new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Z), m_tree);
	redoSc->setContext(Qt::WidgetShortcut);
	connect(redoSc, SIGNAL(activated()), this, SLOT(onRedo()));
	QShortcut *redoSc2 = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Y), m_tree);
	redoSc2->setContext(Qt::WidgetShortcut);
	connect(redoSc2, SIGNAL(activated()), this, SLOT(onRedo()));

	connect(m_ckCompress, SIGNAL(clicked()), this, SLOT(onCheckboxToggled()));
	connect(m_ckNewIcons, SIGNAL(clicked()), this, SLOT(onCheckboxToggled()));
	connect(m_ckCleanName, SIGNAL(clicked()), this, SLOT(onCheckboxToggled()));
	connect(m_ckAutoVerify, SIGNAL(clicked()), this, SLOT(onCheckboxToggled()));
	connect(m_ckSmartCopy, SIGNAL(clicked()), this, SLOT(onCheckboxToggled()));
	connect(m_ckFastLoad, SIGNAL(clicked()), this, SLOT(onCheckboxToggled()));
	connect(m_ckScriptMerge, SIGNAL(clicked()), this, SLOT(onCheckboxToggled()));
	connect(m_ckRefByParam, SIGNAL(clicked()), this, SLOT(onCheckboxToggled()));
	connect(m_ckDisableRef, SIGNAL(clicked()), this, SLOT(onCheckboxToggled()));

	s_instance = this;
}

namespace
{
	// Produce a state variant of a base pixmap: warnings -> red-tinted, inactive -> dimmed.
	QPixmap variantPixmap(const QPixmap &base, int state)
	{
		if (state == 0 || base.isNull())
		{
			return base;
		}
		QPixmap out = base;
		QPainter p(&out);
		if (state == 1)
		{
			// warnings: multiply a red wash over the opaque pixels.
			p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
			p.fillRect(out.rect(), QColor(200, 40, 40, 130));
		}
		else
		{
			// inactive: fade the icon out.
			p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
			p.fillRect(out.rect(), QColor(0, 0, 0, 110));
		}
		p.end();
		return out;
	}
}

void WBQtScriptWindow::buildIcons()
{
	QStyle *st = style();
	// Base per node type: player -> a "group/drive" glyph, folder -> directory, script -> file.
	QPixmap basePix[3];
	basePix[0] = st->standardIcon(QStyle::SP_DriveHDIcon).pixmap(16, 16);
	basePix[1] = st->standardIcon(QStyle::SP_DirIcon).pixmap(16, 16);
	basePix[2] = st->standardIcon(QStyle::SP_FileIcon).pixmap(16, 16);
	for (int type = 0; type < 3; ++type)
	{
		for (int state = 0; state < 3; ++state)
		{
			m_icons[type][state] = QIcon(variantPixmap(basePix[type], state));
		}
	}
}

QIcon WBQtScriptWindow::nodeIcon(int listType, int flags) const
{
	// Decode the ListType objType (top nibble): 1 PLAYER, 2 GROUP(folder), 3/4 SCRIPT.
	int objType = (listType >> 28) & 0xF;
	int type = 2;	// script
	if (objType == 1)
	{
		type = 0;	// player
	}
	else if (objType == 2)
	{
		type = 1;	// folder
	}
	int state = 0;
	if (flags & 2)
	{
		state = 1;	// warnings
	}
	else if (!(flags & 1))
	{
		state = 2;	// inactive
	}
	return m_icons[type][state];
}

void WBQtScriptWindow::seedCheckboxes()
{
	m_updating = true;
	m_ckCompress->setChecked(WBQtScript_GetCheckbox(WBQT_SCK_COMPRESS) != 0);
	// New Icons is fixed ON in the Qt window (native Qt icons; no old set) -- not re-seeded.
	m_ckCleanName->setChecked(WBQtScript_GetCheckbox(WBQT_SCK_CLEANNAME) != 0);
	m_ckAutoVerify->setChecked(WBQtScript_GetCheckbox(WBQT_SCK_AUTOVERIFY) != 0);
	m_ckSmartCopy->setChecked(WBQtScript_GetCheckbox(WBQT_SCK_SMARTCOPY) != 0);
	m_ckFastLoad->setChecked(WBQtScript_GetCheckbox(WBQT_SCK_FASTLOAD) != 0);
	m_ckScriptMerge->setChecked(WBQtScript_GetCheckbox(WBQT_SCK_SCRIPTMERGE) != 0);
	m_ckRefByParam->setChecked(WBQtScript_GetCheckbox(WBQT_SCK_REFBYPARAM) != 0);
	m_ckDisableRef->setChecked(WBQtScript_GetCheckbox(WBQT_SCK_DISABLEREF) != 0);
	m_updating = false;
}

void WBQtScriptWindow::rebuildTree()
{
	m_updating = true;
	m_tree->clear();

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
				item = new QTreeWidgetItem(m_tree);
			}
			else
			{
				item = new QTreeWidgetItem(parent);
			}
		}
		item->setText(0, QString::fromLatin1(labelBuf));
		item->setData(0, kListTypeRole, listType);
		item->setFlags(item->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
		item->setIcon(0, nodeIcon(listType, flags));

		// Visual state (script/group nodes only). Warnings -> red, inactive -> dimmed,
		// subroutine -> italic. Mirrors the MFC state-icon meaning.
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
	m_lastFoundListType = 0;
	if (m_search != NULL)
	{
		m_search->clear();
	}
	seedCheckboxes();	// pick up any persisted-setting changes
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
	m_addDebug->setEnabled(hasScript);
	m_removeDebug->setEnabled(hasScript);
}

void WBQtScriptWindow::updateDetail()
{
	int lt = selectedListType();
	if (lt == -1)
	{
		m_description->clear();
		m_comment->clear();
		return;
	}
	const int cap = 8192;
	static char descBuf[cap];
	static char commentBuf[cap];
	descBuf[0] = 0;
	commentBuf[0] = 0;
	WBQtScript_GetDetail(lt, descBuf, cap, commentBuf, cap);
	m_description->setPlainText(QString::fromLatin1(descBuf));
	m_comment->setPlainText(QString::fromLatin1(commentBuf));
}

void WBQtScriptWindow::onTreeSelectionChanged()
{
	if (m_updating)
	{
		return;
	}
	pushSelectionToDialog();
	updateButtonStates();
	updateDetail();
}

void WBQtScriptWindow::handleDrop(int dragListType, int targetListType)
{
	WBQtScript_DropOn(dragListType, targetListType);
	rebuildTree();
	updateButtonStates();
	updateDetail();
}

void WBQtScriptWindow::onNewFolder()
{
	pushSelectionToDialog();
	WBQtScript_NewFolder();
	rebuildTree();
	updateButtonStates();
	updateDetail();
}

void WBQtScriptWindow::onNewScript()
{
	pushSelectionToDialog();
	WBQtScript_NewScript();
	rebuildTree();
	updateButtonStates();
	updateDetail();
}

void WBQtScriptWindow::onEditScript()
{
	pushSelectionToDialog();
	WBQtScript_EditScript();
	rebuildTree();
	updateButtonStates();
	updateDetail();
}

void WBQtScriptWindow::onCopyScript()
{
	pushSelectionToDialog();
	WBQtScript_CopyScript();
	rebuildTree();
	updateButtonStates();
	updateDetail();
}

void WBQtScriptWindow::onDelete()
{
	pushSelectionToDialog();
	WBQtScript_Delete();
	rebuildTree();
	updateButtonStates();
	updateDetail();
}

void WBQtScriptWindow::onDeleteShortcut()
{
	// Mirrors the Delete button's enable state (a script or folder is selected).
	if (m_delete->isEnabled())
	{
		onDelete();
	}
}

void WBQtScriptWindow::onUndo()
{
	if (WBQtScript_Undo())
	{
		rebuildTree();
		updateButtonStates();
		updateDetail();
	}
}

void WBQtScriptWindow::onRedo()
{
	if (WBQtScript_Redo())
	{
		rebuildTree();
		updateButtonStates();
		updateDetail();
	}
}

void WBQtScriptWindow::onVerify()
{
	WBQtScript_Verify();
	rebuildTree();
	updateButtonStates();
	updateDetail();
}

void WBQtScriptWindow::onToggleActive()
{
	pushSelectionToDialog();
	WBQtScript_ToggleActive();
	rebuildTree();
	updateButtonStates();
	updateDetail();
}

void WBQtScriptWindow::onAddDebug()
{
	pushSelectionToDialog();
	WBQtScript_AddDebug();
	rebuildTree();
	updateDetail();
}

void WBQtScriptWindow::onRemoveDebug()
{
	pushSelectionToDialog();
	WBQtScript_RemoveDebug();
	rebuildTree();
	updateDetail();
}

void WBQtScriptWindow::onPatchGC()
{
	WBQtScript_PatchGC();
	rebuildTree();
	updateDetail();
}

void WBQtScriptWindow::onExport()
{
	pushSelectionToDialog();
	WBQtScript_ExportScripts();
}

void WBQtScriptWindow::onImport()
{
	WBQtScript_ImportScripts();
	rebuildTree();
	updateButtonStates();
	updateDetail();
}

void WBQtScriptWindow::onSaveNow()
{
	WBQtScript_SaveNow();
}

void WBQtScriptWindow::onCheckboxToggled()
{
	if (m_updating)
	{
		return;
	}
	QCheckBox *box = qobject_cast<QCheckBox*>(sender());
	if (box == NULL)
	{
		return;
	}
	int which = -1;
	if (box == m_ckCompress) { which = WBQT_SCK_COMPRESS; }
	else if (box == m_ckNewIcons) { which = WBQT_SCK_NEWICONS; }
	else if (box == m_ckCleanName) { which = WBQT_SCK_CLEANNAME; }
	else if (box == m_ckAutoVerify) { which = WBQT_SCK_AUTOVERIFY; }
	else if (box == m_ckSmartCopy) { which = WBQT_SCK_SMARTCOPY; }
	else if (box == m_ckFastLoad) { which = WBQT_SCK_FASTLOAD; }
	else if (box == m_ckScriptMerge) { which = WBQT_SCK_SCRIPTMERGE; }
	else if (box == m_ckRefByParam) { which = WBQT_SCK_REFBYPARAM; }
	else if (box == m_ckDisableRef) { which = WBQT_SCK_DISABLEREF; }
	if (which < 0)
	{
		return;
	}
	WBQtScript_SetCheckbox(which, box->isChecked() ? 1 : 0);
	// Clean Script Name changes the labels; New Icons/Compress affect display -- rebuild so the
	// Qt tree reflects any label change, and refresh the detail (Disable references etc.).
	rebuildTree();
	updateDetail();
}

void WBQtScriptWindow::onTreeContextMenu(const QPoint &pos)
{
	QTreeWidgetItem *item = m_tree->itemAt(pos);
	if (item == NULL)
	{
		return;
	}
	m_tree->setCurrentItem(item);
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
	menu.addSeparator();
	QAction *actAddDebug = menu.addAction("Add Debug");
	actAddDebug->setEnabled(hasScript);
	QAction *actRemoveDebug = menu.addAction("Delete Debug");
	actRemoveDebug->setEnabled(hasScript);

	QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
	if (chosen == actActivate) { onToggleActive(); }
	else if (chosen == actEdit) { onEditScript(); }
	else if (chosen == actCopy) { onCopyScript(); }
	else if (chosen == actDelete) { onDelete(); }
	else if (chosen == actAddDebug) { onAddDebug(); }
	else if (chosen == actRemoveDebug) { onRemoveDebug(); }
}

void WBQtScriptWindow::onTreeDoubleClicked(QTreeWidgetItem *item, int)
{
	if (item == NULL)
	{
		return;
	}
	// Double-click edits a script OR a folder (folder -> the EditGroup rename/settings dialog),
	// matching the MFC OnDblclkScriptTree -> OnEditScript. onEditScript pushes the selection and
	// only acts if it resolves to a script/group, so double-clicking a player is a no-op.
	m_tree->setCurrentItem(item);
	onEditScript();
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
		WBQtScript_SetSelection(out);
		updateButtonStates();
		updateDetail();
	}
	else
	{
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

	WBQtScriptWindow *win = WBQtScriptWindow::instance();
	if (win == NULL)
	{
		win = new WBQtScriptWindow(NULL);
	}
	else
	{
		win->rebuildTree();
	}
	win->resetForNewSession();

	// Own the window by the visible top-level (the Qt main window when inverted, else the
	// WB frame), like the MFC modeless ScriptDialog: an owned window always stacks above
	// its owner (the script window can never fall behind the main window) and minimizes
	// with it. The ownership MUST go through the QWindow transient parent: the Windows QPA
	// maintains GWLP_HWNDPARENT from that property and resets a raw SetWindowLongPtr write
	// back to 0 on its next internal update (verified live -- the owner read back NULL and
	// the window fell behind the main window). Still NOT a Qt widget parent, so the 9b
	// standalone-top-level focus behavior is unchanged.
	QWidget *mainWidget = WBQt_MainWindowWidget();
	if (mainWidget != NULL)
	{
		win->winId();			// force the QWindow to exist before setting its owner
		mainWidget->winId();
		win->windowHandle()->setTransientParent(mainWidget->windowHandle());
	}
	else
	{
		// Not inverted: the owner is the native MFC frame, which has no QWindow; the raw
		// write is best effort there (the frame path predates the inversion).
		::SetWindowLongPtr(reinterpret_cast<HWND>(win->winId()), GWLP_HWNDPARENT,
			reinterpret_cast<LONG_PTR>(WBQt_EffectiveOwnerHwnd(frameHwnd)));
	}

	win->move(x, y);
	win->show();
	win->raise();
	win->activateWindow();
	HWND h = reinterpret_cast<HWND>(win->winId());
	::SetForegroundWindow(h);
	::SetFocus(h);
	win->setFocus(Qt::ActiveWindowFocusReason);
}

extern "C" int WBQtScript_IsOpen(void)
{
	WBQtScriptWindow *win = WBQtScriptWindow::instance();
	return (win != NULL && win->isVisible()) ? 1 : 0;
}

// Bring the already-open editor to the front and give it keyboard focus (F4 / menu while
// it is open re-focuses instead of recreating the session).
extern "C" void WBQtScript_Focus(void)
{
	WBQtScriptWindow *win = WBQtScriptWindow::instance();
	if (win == NULL || !win->isVisible())
	{
		return;
	}
	win->raise();
	win->activateWindow();
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
