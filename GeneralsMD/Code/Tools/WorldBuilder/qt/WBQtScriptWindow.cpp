// WBQtScriptWindow.cpp -- see WBQtScriptWindow.h.
#include "WBQtScriptWindow.h"
#include "ui_WBQtScriptWindow.h"
#include "WBQtPanelBridge.h"
#include "WBQtTreeStyle.h"
#include "WBQtWindowPos.h"
#include "WBQtTheme.h"
#include "panels/WBQtScriptEditBridge.h"	// SetInitialFocus: the [Missing] link jump

#include <QApplication>
#include <QBrush>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QColor>
#include <QCompleter>
#include <QDropEvent>
#include <QFont>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QShortcut>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStringList>
#include <QStringListModel>
#include <QStyle>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QTreeWidgetItemIterator>
#include <QUrl>
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
// The node's packed state flags (bit0=active, bit1=warnings, bit2=subroutine, bit3=easy,
// bit4=normal, bit5=hard), so the context menu can show the current active state as a check
// mark (== MFC CheckMenuItem) and the Show: chips can filter by state without re-querying.
static const int kFlagsRole = Qt::UserRole + 2;

//----------------------------------------------------------------------------------------
// WBQtScriptTree
//----------------------------------------------------------------------------------------
WBQtScriptTree::WBQtScriptTree(WBQtScriptWindow *owner)
	: QTreeWidget(owner),
	  m_owner(owner)
{
}

// Qt reserves Ctrl+press for selection toggling and enters drag-SELECT mode on the move,
// so a Ctrl-held drag never starts a drag-and-drop -- the Script Merge gesture (hold Ctrl,
// drag a script onto another) could never even begin. Strip Ctrl before the base press:
// this tree is SingleSelection, so Ctrl-toggle has no other meaning here, and qtMDropOn
// reads the physical Ctrl state at drop time for the merge decision.
void WBQtScriptTree::mousePressEvent(QMouseEvent *event)
{
	if ((event->modifiers() & Qt::ControlModifier) && event->button() == Qt::LeftButton)
	{
		QMouseEvent plain(event->type(), event->localPos(), event->windowPos(),
			event->screenPos(), event->button(), event->buttons(),
			event->modifiers() & ~Qt::ControlModifier);
		QTreeWidget::mousePressEvent(&plain);
		event->setAccepted(plain.isAccepted());
		return;
	}
	QTreeWidget::mousePressEvent(event);
}

void WBQtScriptTree::dropEvent(QDropEvent *event)
{
	// DragDrop mode (needed so Ctrl+drag merge drops arrive) also admits foreign drags;
	// only this tree's own items are meaningful here.
	if (event->source() != this)
	{
		event->ignore();
		return;
	}
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
	  m_ui(new Ui::WBQtScriptWindow),
	  m_lastFoundListType(0),
	  m_updating(false),
	  m_treeDefaultFontValid(false)
{
	// Deliberately a STANDALONE top-level window (parent = NULL), NOT a child of the QWinWidget
	// owner like the option panels. A QWinWidget-child reflects keyboard activation back to its
	// native MFC parent (the frame), so keystrokes fell through to the main view's tool hotkeys
	// instead of the search / rename fields. A real top-level owns its own focus. It still gets
	// the dark title bar (WBQtTheme targets all top-level windows). owner is now unused.
	(void)owner;
	// The static widget tree lives in WBQtScriptWindow.ui; bind the members the logic
	// below uses, then wire what Designer can't express.
	m_ui->setupUi(this);
	resize(900, 640);
	WBQtWindowPos_Track(this, "ScriptEditor");

	// --- Option checkboxes (top strip), mirroring the MFC dialog's row ---
	m_ckCompress = m_ui->ckCompress;
	m_ckNewIcons = m_ui->ckNewIcons;
	// The Qt tree always draws the native Qt icon set -- there is no "old icons" BMP set
	// on the Qt side -- so the choice is fixed: show the box checked and grayed out.
	m_ckNewIcons->setChecked(true);
	m_ckNewIcons->setEnabled(false);
	m_ckCleanName = m_ui->ckCleanName;
	m_ckAutoVerify = m_ui->ckAutoVerify;
	m_ckSmartCopy = m_ui->ckSmartCopy;
	m_ckFastLoad = m_ui->ckFastLoad;
	m_ckScriptMerge = m_ui->ckScriptMerge;
	m_ckRefByParam = m_ui->ckRefByParam;
	m_ckDisableRef = m_ui->ckDisableRef;

	// --- Search row ---
	m_search = m_ui->search;
	m_filterWarnings = m_ui->filterWarnings;
	m_filterActive = m_ui->filterActive;
	m_filterInactive = m_ui->filterInactive;
	m_filterEasy = m_ui->filterEasy;
	m_filterNormal = m_ui->filterNormal;
	m_filterHard = m_ui->filterHard;
	m_replaceBar = m_ui->replaceBar;
	m_replaceFind = m_ui->replaceFind;
	m_replaceWith = m_ui->replaceWith;
	m_replaceMatchCase = m_ui->replaceMatchCase;
	m_replaceWholeValue = m_ui->replaceWholeValue;
	m_replaceScopeCheck = m_ui->replaceScopeCheck;
	m_replaceCount = m_ui->replaceCount;
	m_replaceScopeListType = -1;

	// Autocomplete the Find box with the distinct parameter values in the map. A 2-column model
	// (value | "(count)") drives it: the popup shows both columns, but the completer inserts only
	// column 0 -- so clicking a suggestion fills Find with the bare value, never the "(count)".
	m_replaceSuggestModel = new QStandardItemModel(this);
	m_replaceCompleter = new QCompleter(m_replaceSuggestModel, this);
	m_replaceCompleter->setCaseSensitivity(Qt::CaseInsensitive);
	m_replaceCompleter->setCompletionMode(QCompleter::UnfilteredPopupCompletion);	// we filter model-side
	{
		// A tree-view popup so both columns (value | count) show; column 0 is what's inserted.
		QTreeView *popup = new QTreeView;
		popup->setRootIsDecorated(false);
		popup->setHeaderHidden(true);
		popup->setSelectionBehavior(QAbstractItemView::SelectRows);
		m_replaceCompleter->setPopup(popup);
		popup->setColumnWidth(0, 240);
	}
	m_replaceCompleter->setCompletionColumn(0);	// insert the value, not the count column
	m_replaceFind->setCompleter(m_replaceCompleter);
	connect(m_replaceCompleter, SIGNAL(activated(const QString &)),
		this, SLOT(onReplaceSuggestionPicked(const QString &)));

	// The Replace box completes over ALL existing parameter values (no counts) -- so you can rename
	// TO a value that already exists in the map, picked from the list. Qt filters this one by prefix.
	m_replaceWithModel = new QStringListModel(this);
	m_replaceWithCompleter = new QCompleter(m_replaceWithModel, this);
	m_replaceWithCompleter->setCaseSensitivity(Qt::CaseInsensitive);
	m_replaceWithCompleter->setFilterMode(Qt::MatchContains);
	m_replaceWith->setCompleter(m_replaceWithCompleter);

	// Debounce the Find box: each keystroke walks every script's params (count + suggestions), so
	// coalesce a typing burst into one walk per pause, mirroring the tree-search debounce.
	m_replaceDebounce = new QTimer(this);
	m_replaceDebounce->setSingleShot(true);
	m_replaceDebounce->setInterval(180);
	connect(m_replaceDebounce, SIGNAL(timeout()), this, SLOT(onReplaceDebounce()));

	m_findBtn = m_ui->findBtn;

	// --- Middle: tree | (description over comment) ---
	// The tree's ctor needs this window (no setter), so it stays created here and is
	// inserted into the .ui splitter in front of the detail pane.
	m_tree = new WBQtScriptTree(this);
	m_tree->setHeaderHidden(true);
	m_tree->setColumnCount(1);
	m_tree->setDragEnabled(true);
	m_tree->setAcceptDrops(true);
	// NOT InternalMove: that mode rejects any non-move drop, and Ctrl+drag proposes a COPY
	// action, so the Script Merge Ctrl+drop never reached dropEvent (forbidden cursor).
	// DragDrop accepts both; plain drag still defaults to move, and dropEvent guards that
	// the drag came from this tree. qtMDropOn reads the live Ctrl state itself for merge.
	m_tree->setDragDropMode(QAbstractItemView::DragDrop);
	m_tree->setDefaultDropAction(Qt::MoveAction);
	m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
	m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
	WBQtTreeStyle::applyTreeLines(m_tree);
	m_ui->split->insertWidget(0, m_tree);
	m_ui->split->setStretchFactor(0, 1);
	m_ui->split->setStretchFactor(1, 2);

	m_description = m_ui->description;
	// Browser (not edit) so the "[Referenced in]" script names render as clickable links
	// that jump the tree to the referencing script. Read-only by default.
	m_comment = m_ui->comment;

	// --- Command button rows ---
	m_newFolder = m_ui->newFolder;
	m_newScript = m_ui->newScript;
	m_editScript = m_ui->editScript;
	m_copyScript = m_ui->copyScript;
	m_delete = m_ui->deleteBtn;
	m_verify = m_ui->verify;
	m_addDebug = m_ui->addDebug;
	m_removeDebug = m_ui->removeDebug;
	m_patchGC = m_ui->patchGC;
	m_export = m_ui->exportBtn;
	m_import = m_ui->importBtn;
	m_saveNow = m_ui->saveNow;

	// --- Commit row ---
	m_ok = m_ui->ok;
	m_cancel = m_ui->cancel;

	buildIcons();
	seedCheckboxes();
	rebuildTree();
	updateButtonStates();
	updateDetail();

	connect(m_tree, SIGNAL(itemSelectionChanged()), this, SLOT(onTreeSelectionChanged()));
	connect(m_findBtn, SIGNAL(clicked()), this, SLOT(onFind()));
	connect(m_search, SIGNAL(returnPressed()), this, SLOT(onFind()));
	connect(m_search, SIGNAL(textChanged(const QString &)), this, SLOT(onSearchTextChanged(const QString &)));
	// Debounce the live filter: each keystroke restarts a short single-shot timer, so a burst of
	// typing runs the (deep-scanning) filter once when typing pauses instead of on every character.
	m_searchDebounce = new QTimer(this);
	m_searchDebounce->setSingleShot(true);
	m_searchDebounce->setInterval(180);
	connect(m_searchDebounce, SIGNAL(timeout()), this, SLOT(onSearchDebounce()));
	connect(m_filterWarnings, SIGNAL(toggled(bool)), this, SLOT(onFilterChanged()));
	connect(m_filterActive, SIGNAL(toggled(bool)), this, SLOT(onFilterChanged()));
	connect(m_filterInactive, SIGNAL(toggled(bool)), this, SLOT(onFilterChanged()));
	connect(m_filterEasy, SIGNAL(toggled(bool)), this, SLOT(onFilterChanged()));
	connect(m_filterNormal, SIGNAL(toggled(bool)), this, SLOT(onFilterChanged()));
	connect(m_filterHard, SIGNAL(toggled(bool)), this, SLOT(onFilterChanged()));

	// Inline find/replace bar. Ctrl+H toggles it; the criteria recount live; Enter in the find box
	// steps to the next match; Esc closes it.
	QShortcut *replaceSc = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_H), this);
	connect(replaceSc, SIGNAL(activated()), this, SLOT(onToggleReplaceBar()));
	connect(m_ui->replaceBtn, SIGNAL(clicked()), this, SLOT(onToggleReplaceBar()));
	connect(m_ui->replaceScopedBtn, SIGNAL(clicked()), this, SLOT(onOpenReplaceScoped()));
	connect(m_replaceScopeCheck, SIGNAL(toggled(bool)), this, SLOT(onReplaceScopeToggled(bool)));
	connect(m_replaceFind, SIGNAL(textChanged(const QString &)), this, SLOT(onReplaceCriteriaChanged()));
	connect(m_replaceFind, SIGNAL(returnPressed()), this, SLOT(onReplaceNext()));
	connect(m_replaceMatchCase, SIGNAL(toggled(bool)), this, SLOT(onReplaceCriteriaChanged()));
	connect(m_replaceWholeValue, SIGNAL(toggled(bool)), this, SLOT(onReplaceCriteriaChanged()));
	connect(m_ui->replaceNext, SIGNAL(clicked()), this, SLOT(onReplaceNext()));
	connect(m_ui->replacePrev, SIGNAL(clicked()), this, SLOT(onReplacePrev()));
	connect(m_ui->replaceAll, SIGNAL(clicked()), this, SLOT(onReplaceAll()));
	connect(m_ui->replaceClose, SIGNAL(clicked()), this, SLOT(onReplaceClose()));
	QShortcut *replaceEsc = new QShortcut(QKeySequence(Qt::Key_Escape), m_replaceBar);
	replaceEsc->setContext(Qt::WidgetWithChildrenShortcut);
	connect(replaceEsc, SIGNAL(activated()), this, SLOT(onReplaceClose()));
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
	connect(m_comment, SIGNAL(anchorClicked(QUrl)), this, SLOT(onReferenceClicked(QUrl)));
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
	QShortcut *renameSc = new QShortcut(QKeySequence(Qt::Key_F2), m_tree);
	renameSc->setContext(Qt::WidgetShortcut);
	connect(renameSc, SIGNAL(activated()), this, SLOT(onRename()));
	QShortcut *dupSc = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_D), m_tree);
	dupSc->setContext(Qt::WidgetShortcut);
	connect(dupSc, SIGNAL(activated()), this, SLOT(onDuplicateShortcut()));

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

WBQtScriptWindow::~WBQtScriptWindow()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
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

	// Preserve the user's expand/collapse state across model rebuilds (every command
	// triggers one; expandAll() here used to blow the state away and reopen every folder).
	// On the first build of a window there is no state yet: everything starts collapsed.
	const bool firstBuild = (m_tree->topLevelItemCount() == 0);
	QSet<int> expanded;
	// Also remember the current selection: rebuild clears the tree, so opening/editing a
	// script (which rebuilds) used to drop the selection -- capture it and restore it below.
	const int selectedType = firstBuild ? -1 : selectedListType();
	if (!firstBuild)
	{
		for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
		{
			if ((*it)->isExpanded())
			{
				expanded.insert((*it)->data(0, kListTypeRole).toInt());
			}
		}
	}

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
		item->setData(0, kFlagsRole, flags);
		// == OnBegindragScriptTree's guard (a drag never starts on a PLAYER_TYPE row): players
		// (depth 0) stay drop targets but aren't draggable, or dragging one pushed a do-nothing
		// undo snapshot (qtMDropOn resolves neither script nor group and no-ops).
		Qt::ItemFlags flagBits = item->flags() | Qt::ItemIsDropEnabled;
		if (depth > 0)
		{
			flagBits |= Qt::ItemIsDragEnabled;
		}
		else
		{
			flagBits &= ~Qt::ItemIsDragEnabled;
		}
		item->setFlags(flagBits);
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

		if (!firstBuild)
		{
			item->setExpanded(expanded.contains(listType));
		}
	}

	m_updating = false;

	// == OnCompress: when the Compress flag is set, the MFC tree switches to a 14px "Segoe UI"
	// font; otherwise it reverts to its default. Apply the same to the Qt tree.
	applyCompressFont();

	// Re-select the node that was selected before the rebuild (it keeps the same listType),
	// so closing the Edit dialog leaves the just-edited script highlighted.
	if (selectedType != -1)
	{
		selectByListType(selectedType);
	}

	// A command (new/copy/delete/edit) rebuilds the tree; re-apply the filters so hidden rows stay
	// hidden across the rebuild instead of all reappearing. No-op when nothing is filtering.
	applyFilters();
}

// == OnCompress's font swap: 14px "Segoe UI" on the tree while compressed, the tree's
// original font otherwise. We capture the default font once (m_treeDefaultFont) so the
// revert restores exactly what the tree started with.
void WBQtScriptWindow::applyCompressFont()
{
	if (!m_treeDefaultFontValid)
	{
		m_treeDefaultFont = m_tree->font();
		m_treeDefaultFontValid = true;
	}

	const bool compressed = (WBQtScript_GetCheckbox(WBQT_SCK_COMPRESS) != 0);
	if (compressed)
	{
		QFont f("Segoe UI");
		f.setPixelSize(14);
		m_tree->setFont(f);
	}
	else
	{
		m_tree->setFont(m_treeDefaultFont);
	}
}

void WBQtScriptWindow::resetForNewSession()
{
	m_lastFoundListType = 0;
	if (m_search != NULL)
	{
		m_search->clear();
	}
	// Clear the Show: filter chips so a new session starts unfiltered. Block signals so this
	// doesn't fire applyFilters mid-reset (the tree isn't rebuilt yet).
	QCheckBox *chips[6] = { m_filterWarnings, m_filterActive, m_filterInactive,
		m_filterEasy, m_filterNormal, m_filterHard };
	for (int i = 0; i < 6; ++i)
	{
		if (chips[i] != NULL)
		{
			chips[i]->blockSignals(true);
			chips[i]->setChecked(false);
			chips[i]->blockSignals(false);
		}
	}
	// Hide + clear the find/replace bar so a new session starts without it.
	if (m_replaceBar != NULL)
	{
		m_replaceDebounce->stop();
		m_replaceBar->setVisible(false);
		m_replaceFind->clear();
		m_replaceWith->clear();
		m_replaceScopeCheck->blockSignals(true);
		m_replaceScopeCheck->setChecked(false);
		m_replaceScopeCheck->blockSignals(false);
		m_replaceScopeListType = -1;
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

// Escape the comment text for rich text and turn each script name on a reference line
// ("[Referenced in] : <names>" -- who calls this script -- or "[Uses] : <names>" -- who this
// script calls) into a wbref: link, so the detail pane can jump to that script. Done per line so
// both tags (now separated by a blank line) linkify independently.
static QString wbLinkifyReferences(const QString &comment)
{
	// Each reference-tag marker maps its comma-separated names to a link URL scheme:
	//   wbref:<script>        -> jump to a script in the tree (onReferenceClicked)
	//   wbent:unit:<name>     -> select+center the named placed unit in the 3D view
	//   wbent:wp:<name>       -> select+center the named waypoint
	//   wbmiss:<name>         -> open the Edit dialog on the condition/action naming it
	struct Marker { const char *text; const char *scheme; };
	static const Marker markers[] = {
		{ "[Referenced in] : ", "wbref:" },
		{ "[Uses] : ",          "wbref:" },
		{ "[Units] : ",         "wbent:unit:" },
		{ "[Waypoints] : ",     "wbent:wp:" },
		{ "[Missing] : ",       "wbmiss:" }
	};
	const int nMarkers = (int)(sizeof(markers) / sizeof(markers[0]));

	QStringList lines = comment.split("\n");
	for (int li = 0; li < lines.size(); ++li)
	{
		const QString &line = lines.at(li);
		int mi = -1;
		for (int m = 0; m < nMarkers; ++m)
		{
			if (line.startsWith(markers[m].text))
			{
				mi = m;
				break;
			}
		}
		if (mi == -1)
		{
			lines[li] = line.toHtmlEscaped();
			continue;
		}
		const QString marker = markers[mi].text;
		const QString scheme = markers[mi].scheme;
		QString html = marker.toHtmlEscaped();
		const QStringList names = line.mid(marker.size()).split(", ");
		for (int i = 0; i < names.size(); ++i)
		{
			if (i > 0)
			{
				html += ", ";
			}
			html += "<a href=\"" + scheme
				+ QString::fromLatin1(QUrl::toPercentEncoding(names.at(i)))
				+ "\">" + names.at(i).toHtmlEscaped() + "</a>";
		}
		lines[li] = html;
	}
	return lines.join("<br>");
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
	m_comment->setHtml(wbLinkifyReferences(QString::fromLatin1(commentBuf)));
}

void WBQtScriptWindow::onReferenceClicked(const QUrl &url)
{
	// wbent:unit:<name> / wbent:wp:<name> -> select + center the entity in the 3D view. Parse from
	// the full URL string (not url.path(): the "unit:"/"wp:" kind looks like a nested scheme to QUrl).
	const QString full = url.toString(QUrl::FullyDecoded);

	// wbmiss:<name> -> open the Edit dialog on the tab + row of the first condition/action that
	// names the missing object, so the fix is one click away. Parse from the full string ("???"
	// would read as a query to QUrl's path()).
	if (full.startsWith("wbmiss:"))
	{
		pushSelectionToDialog();	// the [Missing] tag belongs to the selected script
		const QByteArray name = full.mid(7).toLatin1();
		int tab = -1;
		int row = -1;
		if (WBQtScript_FindObjectParamLocation(name.constData(), &tab, &row) == 0)
		{
			QApplication::beep();	// already fixed / stale detail pane
			return;
		}
		WBQtScriptEdit_SetInitialFocus(tab, row);
		onEditScript();
		return;
	}

	if (full.startsWith("wbent:"))
	{
		const QString rest = full.mid(6);	// after "wbent:"
		const int colon = rest.indexOf(':');
		if (colon <= 0)
		{
			return;
		}
		const QString kind = rest.left(colon);
		QByteArray name = rest.mid(colon + 1).toLatin1();
		if (WBQtScript_SelectEntity(name.constData(), kind == QLatin1String("wp") ? 1 : 0) == 0)
		{
			QApplication::beep();	// not on the map (renamed/deleted since the script was written)
		}
		return;
	}

	if (url.scheme() != QLatin1String("wbref"))
	{
		return;
	}
	// path() decodes the percent-encoding wbLinkifyReferences applied.
	QByteArray name = url.path().toLatin1();
	int lt = WBQtScript_FindScriptByName(name.constData());
	if (lt == -1)
	{
		QApplication::beep();
		return;
	}
	// Same jump sequence as onFind: select, push to the dialog, refresh the panes.
	selectByListType(lt);
	WBQtScript_SetSelection(lt);
	updateButtonStates();
	updateDetail();
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
	// Confirm (once) before deleting -- deletes aren't obvious to undo. A "Don't ask again"
	// checkbox persists the choice via WBQtScript_SetConfirmDelete (default on).
	if (WBQtScript_GetConfirmDelete() != 0)
	{
		QMessageBox box(QMessageBox::Question, "Delete",
			"Delete the selected script/folder?", QMessageBox::Yes | QMessageBox::No, this);
		QCheckBox *dontAsk = new QCheckBox("Don't ask again", &box);
		box.setCheckBox(dontAsk);
		if (box.exec() != QMessageBox::Yes)
		{
			return;
		}
		if (dontAsk->isChecked())
		{
			WBQtScript_SetConfirmDelete(0);
		}
	}
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

void WBQtScriptWindow::onRename()
{
	pushSelectionToDialog();
	if (WBQtScript_HasScript() == 0 && WBQtScript_HasGroup() == 0)
	{
		return;	// a player row -- nothing to rename
	}
	char nameBuf[512];
	nameBuf[0] = 0;
	WBQtScript_GetSelectionName(nameBuf, sizeof(nameBuf));

	bool ok = false;
	const QString title = (WBQtScript_HasGroup() != 0) ? "Rename Folder" : "Rename Script";
	QString newName = QInputDialog::getText(this, title, "Name:", QLineEdit::Normal,
		QString::fromLatin1(nameBuf), &ok);
	if (!ok)
	{
		return;
	}
	newName = newName.trimmed();
	if (newName.isEmpty())
	{
		return;
	}
	if (WBQtScript_RenameSelection(newName.toLatin1().constData()) != 0)
	{
		rebuildTree();
		updateButtonStates();
		updateDetail();
	}
}

void WBQtScriptWindow::onDuplicateShortcut()
{
	// Ctrl+D duplicates in place, exactly like the Copy button/menu.
	if (m_copyScript->isEnabled())
	{
		onCopyScript();
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
	// Creation (always available; insert relative to the clicked node).
	QAction *actNewScript = menu.addAction("New Script");
	QAction *actNewFolder = menu.addAction("New Folder");
	menu.addSeparator();
	QAction *actActivate = menu.addAction("Activate / Deactivate");
	actActivate->setEnabled(hasScript || hasGroup);
	// == ScriptDialog CSDTreeCtrl::OnRButtonDown: display a check mark for the current
	// active state (only meaningful when the row is a script or group, matching MFC's
	// friend_getCurScript()/friend_getCurGroup() guard). Bit 0 of the node flags is active.
	if (hasScript || hasGroup)
	{
		const bool active = (item->data(0, kFlagsRole).toInt() & 1) != 0;
		actActivate->setCheckable(true);
		actActivate->setChecked(active);
	}
	menu.addSeparator();
	QAction *actEdit = menu.addAction("Edit");
	actEdit->setEnabled(hasScript || hasGroup);
	QAction *actRename = menu.addAction("Rename\tF2");
	actRename->setEnabled(hasScript || hasGroup);
	QAction *actCopy = menu.addAction("Duplicate\tCtrl+D");
	actCopy->setEnabled(hasScript || hasGroup);
	QAction *actDelete = menu.addAction("Delete");
	actDelete->setEnabled(hasScript || hasGroup);
	menu.addSeparator();
	QAction *actAddDebug = menu.addAction("Add Debug");
	actAddDebug->setEnabled(hasScript);
	QAction *actRemoveDebug = menu.addAction("Delete Debug");
	actRemoveDebug->setEnabled(hasScript);

	QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
	if (chosen == actNewScript) { onNewScript(); }
	else if (chosen == actNewFolder) { onNewFolder(); }
	else if (chosen == actActivate) { onToggleActive(); }
	else if (chosen == actEdit) { onEditScript(); }
	else if (chosen == actRename) { onRename(); }
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

void WBQtScriptWindow::onSearchTextChanged(const QString &text)
{
	// Live filter, only when NewSearch is on (same toggle as the tree pickers). Off = the classic
	// find-next-on-Enter behaviour, so text does nothing here (the Show: chips still work).
	if (m_updating || WBQtConfig_GetNewSearch() == 0)
	{
		return;
	}
	// Clearing the box should un-filter instantly; a partial edit debounces (the filter deep-scans
	// every script, so we don't want to run it on each keystroke of a fast typist).
	if (text.trimmed().isEmpty())
	{
		m_searchDebounce->stop();
		applyFilters();
	}
	else
	{
		m_searchDebounce->start();	// restart the single-shot; fires after the typing pause
	}
}

void WBQtScriptWindow::onSearchDebounce()
{
	applyFilters();
}

// ---- inline find/replace bar (Ctrl+H) ----

QByteArray WBQtScriptWindow::replaceFindText() const
{
	return m_replaceFind->text().trimmed().toLatin1();
}

// Show the find/replace bar (creating focus + seeding) if not already visible; refresh its state.
void WBQtScriptWindow::onToggleReplaceBar()
{
	const bool show = !m_replaceBar->isVisible();
	m_replaceBar->setVisible(show);
	if (show)
	{
		// Seed from the current search text if the find box is empty, then focus + select it.
		if (m_replaceFind->text().isEmpty())
		{
			m_replaceFind->setText(m_search->text().trimmed());
		}
		m_replaceFind->setFocus();
		m_replaceFind->selectAll();
		refreshReplaceCount();
		refreshReplaceSuggestions();
		refreshReplaceValueList();	// build the rename-to list once here, not per keystroke
	}
}

void WBQtScriptWindow::onOpenReplaceScoped()
{
	// "In script": pin the bar to the selected script and open it. Do nothing if no script is
	// selected.
	pushSelectionToDialog();
	if (WBQtScript_HasScript() == 0)
	{
		QApplication::beep();
		return;
	}
	m_replaceScopeListType = selectedListType();		// pin (or re-pin) to the current selection
	m_replaceScopeCheck->setChecked(true);				// idempotent; onReplaceScopeToggled is a no-op re-pin
	if (m_replaceBar->isVisible())
	{
		onReplaceCriteriaChanged();
		m_replaceFind->setFocus();
		m_replaceFind->selectAll();
	}
	else
	{
		onToggleReplaceBar();	// opens + focuses + selects + refreshes
	}
}

void WBQtScriptWindow::onReplaceScopeToggled(bool on)
{
	// Pin to the selected script when turning scope on; clear the pin when off.
	m_replaceScopeListType = on ? selectedListType() : -1;
	if (!m_updating)
	{
		onReplaceCriteriaChanged();
	}
}

int WBQtScriptWindow::replaceScope() const
{
	// m_replaceScopeListType is the single source of truth: -1 when unscoped, else the pinned
	// script. onReplaceScopeToggled keeps it in step with the checkbox (clears it to -1 on uncheck).
	return m_replaceScopeListType;
}

void WBQtScriptWindow::onReplaceClose()
{
	m_replaceBar->setVisible(false);
	m_tree->setFocus();
}

// Collect the distinct parameter values containing `substr` from the model (limited to scopeListType,
// -1 = all), parsing the bridge's "value\tcount\n" wire format into parallel value/count lists.
static void parseParamValueLines(const char *substr, int scopeListType,
	QStringList &values, QStringList &counts)
{
	const int cap = 16384;
	static char buf[cap];
	buf[0] = 0;
	WBQtScript_CollectParamValues(substr, buf, cap, scopeListType);
	const QStringList lines = QString::fromLatin1(buf).split('\n', QString::SkipEmptyParts);
	for (int i = 0; i < lines.size(); ++i)
	{
		const QStringList cols = lines.at(i).split('\t');
		if (cols.isEmpty())
		{
			continue;
		}
		values << cols.at(0);
		counts << (cols.size() >= 2 ? cols.at(1) : QString());
	}
}

void WBQtScriptWindow::refreshReplaceCount()
{
	QByteArray find = replaceFindText();
	int count = 0;
	if (find.isEmpty())
	{
		m_replaceCount->setText("No results");
	}
	else
	{
		count = WBQtScript_ReplaceInParams(find.constData(), NULL,
			m_replaceMatchCase->isChecked() ? 1 : 0,
			m_replaceWholeValue->isChecked() ? 1 : 0, 0, replaceScope());
		m_replaceCount->setText(QString("%1 parameter value%2").arg(count).arg(count == 1 ? "" : "s"));
	}
	// Prev/Next/Replace All only make sense with matches.
	m_ui->replaceNext->setEnabled(count > 0);
	m_ui->replacePrev->setEnabled(count > 0);
	m_ui->replaceAll->setEnabled(count > 0);
}

void WBQtScriptWindow::onReplaceCriteriaChanged()
{
	if (m_updating)
	{
		return;
	}
	// Each keystroke walks every script's params; coalesce a typing burst into one walk per pause.
	m_replaceDebounce->start();
}

void WBQtScriptWindow::onReplaceDebounce()
{
	refreshReplaceCount();
	refreshReplaceSuggestions();
}

void WBQtScriptWindow::refreshReplaceSuggestions()
{
	// Find completer: distinct parameter values CONTAINING the current text, as a 2-column model
	// (value | "(count)"); the completer inserts only column 0, so the count is display-only.
	QStringList values, counts;
	parseParamValueLines(replaceFindText().constData(), replaceScope(), values, counts);
	m_replaceSuggestModel->clear();
	for (int i = 0; i < values.size(); ++i)
	{
		QList<QStandardItem *> row;
		row << new QStandardItem(values.at(i));
		row << new QStandardItem(counts.at(i).isEmpty() ? QString() : QString("(%1)").arg(counts.at(i)));
		m_replaceSuggestModel->appendRow(row);
	}
}

void WBQtScriptWindow::refreshReplaceValueList()
{
	// Replace completer: the FULL set of existing values across ALL scripts (rename-to targets -- you
	// may want to rename to a value used elsewhere, even in scoped mode). Rebuilt only when the bar
	// opens and after a replace -- NOT on Find keystrokes -- so the full scan stays off the typing path.
	QStringList values, counts;
	parseParamValueLines("", -1, values, counts);
	m_replaceWithModel->setStringList(values);
}

void WBQtScriptWindow::onReplaceSuggestionPicked(const QString &)
{
	// The completer already inserted column 0 (the bare value) into the Find box; just recount.
	refreshReplaceCount();
}

void WBQtScriptWindow::applyReplaceSelection(int listType)
{
	m_lastFoundListType = listType;
	selectByListType(listType);
	WBQtScript_SetSelection(listType);
	updateButtonStates();
	updateDetail();
}

void WBQtScriptWindow::stepReplaceMatch(int dir)
{
	// Navigate to the next/prev script that has a matching PARAMETER value -- the exact scope the
	// count + Replace act on, so nav and count agree. dir is +1 (next) or -1 (prev); the model walk
	// is forward-only, so prev restarts from the top (a full step-back would need a reverse walk).
	QByteArray find = replaceFindText();
	if (find.isEmpty())
	{
		return;
	}
	// Scoped to one script: there's a single match location -- just (re)select the pinned script.
	const int scope = replaceScope();
	if (scope >= 0)
	{
		applyReplaceSelection(scope);
		return;
	}
	const int matchCase = m_replaceMatchCase->isChecked() ? 1 : 0;
	const int whole = m_replaceWholeValue->isChecked() ? 1 : 0;
	const int from = (dir < 0) ? 0 : m_lastFoundListType;	// prev: restart from top (forward-only)
	int out = 0;
	bool ok = WBQtScript_FindNextParamMatch(from, find.constData(), matchCase, whole, &out) != 0;
	if (!ok && from != 0)
	{
		// Hit the end -> wrap to the top and try once more.
		m_lastFoundListType = 0;
		ok = WBQtScript_FindNextParamMatch(0, find.constData(), matchCase, whole, &out) != 0;
	}
	if (ok)
	{
		applyReplaceSelection(out);
	}
	else
	{
		QApplication::beep();
	}
}

void WBQtScriptWindow::onReplaceNext()
{
	stepReplaceMatch(1);
}

void WBQtScriptWindow::onReplacePrev()
{
	stepReplaceMatch(-1);
}

void WBQtScriptWindow::onReplaceAll()
{
	QByteArray find = replaceFindText();
	if (find.isEmpty())
	{
		return;
	}
	QByteArray repl = m_replaceWith->text().toLatin1();
	int n = WBQtScript_ReplaceInParams(find.constData(), repl.constData(),
		m_replaceMatchCase->isChecked() ? 1 : 0,
		m_replaceWholeValue->isChecked() ? 1 : 0, 1, replaceScope());
	if (n > 0)
	{
		rebuildTree();
		updateButtonStates();
		updateDetail();
		refreshReplaceValueList();	// the value set changed -> refresh rename-to targets
	}
	refreshReplaceCount();
}

void WBQtScriptWindow::onFilterChanged()
{
	if (m_updating)
	{
		return;
	}
	applyFilters();
}

bool WBQtScriptWindow::textFilterActive() const
{
	// The text box only filters when NewSearch is on (else it's the classic find-next behaviour).
	return (WBQtConfig_GetNewSearch() != 0)
		&& m_search != NULL && !m_search->text().trimmed().isEmpty();
}

bool WBQtScriptWindow::filterActive() const
{
	return textFilterActive()
		|| m_filterWarnings->isChecked() || m_filterActive->isChecked()
		|| m_filterInactive->isChecked() || m_filterEasy->isChecked()
		|| m_filterNormal->isChecked() || m_filterHard->isChecked();
}

// Does this one node pass the search text AND the Show: chips? Folders/players carry no difficulty
// of their own, so a difficulty chip never self-matches them -- they survive only via a matching
// descendant. Flags: bit0 active, bit1 warnings, bit3 easy, bit4 normal, bit5 hard.
bool WBQtScriptWindow::nodeSelfMatches(QTreeWidgetItem *item, const FilterState &fs) const
{
	const int flags = item->data(0, kFlagsRole).toInt();
	if (fs.useText)
	{
		const int listType = item->data(0, kListTypeRole).toInt();
		QByteArray label = item->text(0).toLatin1();
		if (WBQtScript_NodeMatches(listType, fs.needle.constData(), label.constData()) == 0)
		{
			return false;
		}
	}
	if (fs.wantWarn && (flags & 2) == 0)
	{
		return false;
	}
	if (fs.wantActive && (flags & 1) == 0)
	{
		return false;
	}
	if (fs.wantInactive && (flags & 1) != 0)
	{
		return false;
	}
	if (fs.wantEasy || fs.wantNormal || fs.wantHard)
	{
		const bool diffOk = (fs.wantEasy && (flags & 8))
			|| (fs.wantNormal && (flags & 16))
			|| (fs.wantHard && (flags & 32));
		if (!diffOk)
		{
			return false;
		}
	}
	return true;
}

// Post-order: hide item unless it self-matches or has a visible descendant; returns its visibility
// so the parent can decide its own. Expands a node kept alive only by a matching descendant so the
// match shows. One pass, no side tables.
bool WBQtScriptWindow::filterItemRec(QTreeWidgetItem *item, const FilterState &fs)
{
	bool anyChild = false;
	for (int c = 0; c < item->childCount(); ++c)
	{
		if (filterItemRec(item->child(c), fs))
		{
			anyChild = true;
		}
	}
	const bool self = nodeSelfMatches(item, fs);
	const bool visible = self || anyChild;
	item->setHidden(!visible);
	if (visible && anyChild)
	{
		item->setExpanded(true);	// open folders/players so the matching scripts show
	}
	return visible;
}

void WBQtScriptWindow::applyFilters()
{
	// Nothing filtering: show the whole tree.
	if (!filterActive())
	{
		for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
		{
			(*it)->setHidden(false);
		}
		return;
	}

	FilterState fs;
	fs.useText = textFilterActive();
	fs.needle = fs.useText ? m_search->text().trimmed().toLatin1() : QByteArray();
	fs.wantWarn = m_filterWarnings->isChecked();
	fs.wantActive = m_filterActive->isChecked();
	fs.wantInactive = m_filterInactive->isChecked();
	fs.wantEasy = m_filterEasy->isChecked();
	fs.wantNormal = m_filterNormal->isChecked();
	fs.wantHard = m_filterHard->isChecked();

	for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
	{
		filterItemRec(m_tree->topLevelItem(i), fs);
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
	// Re-apply the title-bar theme after show: the global show-time filter themes each window
	// once, but setting the transient parent above can recreate the native HWND and drop the DWM
	// dark attribute, leaving the caption light. Re-set it unconditionally here.
	WBQtTheme::applyTitleBarTheme(h);
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
