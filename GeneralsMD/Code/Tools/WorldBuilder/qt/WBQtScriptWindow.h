// WBQtScriptWindow.h -- Qt front-end for the WorldBuilder Script editor (Phases 9a-9d).
//
// This is NOT an option panel; it's a standalone top-level window opened from
// CMainFrame::onEditScripts. In Qt mode the MFC ScriptDialog is still Create()d but kept
// hidden -- it owns the working model (m_sides), the condition/action sub-editors, the
// option persistence, and the OK/Cancel commit. This window replaces the whole dialog UI:
// the tree, the description + comment detail panels, the option checkboxes, and every command
// button -- driving the hidden dialog through the WBQtScript_* seam. Only the native Qt
// condition/action sub-editors are still deferred (they use the MFC property sheet).
#ifndef WB_QT_SCRIPT_WINDOW_H
#define WB_QT_SCRIPT_WINDOW_H

#include <QByteArray>
#include <QFont>
#include <QIcon>
#include <QTreeWidget>
#include <QWidget>

class QCheckBox;
class QCompleter;
class QLabel;
class QLineEdit;
class QStandardItemModel;
class QStringListModel;
class QPlainTextEdit;
class QPushButton;
class QTextBrowser;
class QTimer;
class QToolButton;
class QUrl;
class WBQtScriptWindow;

namespace Ui { class WBQtScriptWindow; }	// generated from WBQtScriptWindow.ui

// A QTreeWidget that reports internal drops back to the window instead of moving items itself
// (the model rebuild does the actual move). Drag is enabled per-item in the window.
class WBQtScriptTree : public QTreeWidget
{
	Q_OBJECT

public:
	explicit WBQtScriptTree(WBQtScriptWindow *owner);

protected:
	virtual void dropEvent(QDropEvent *event);
	virtual void mousePressEvent(QMouseEvent *event);

private:
	WBQtScriptWindow *m_owner;
};

class WBQtScriptWindow : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtScriptWindow(QWidget *owner);
	virtual ~WBQtScriptWindow();

	void rebuildTree();
	// Clear per-session UI state (search text + find cursor) before a fresh dialog session.
	void resetForNewSession();
	// Called by WBQtScriptTree on an internal drop: move/reorder the dragged node onto target.
	void handleDrop(int dragListType, int targetListType);

	static WBQtScriptWindow *instance() { return s_instance; }

private slots:
	void onTreeSelectionChanged();
	void onNewFolder();
	void onNewScript();
	void onEditScript();
	void onCopyScript();
	void onDelete();
	void onDeleteShortcut();
	void onRename();			// rename the selected script/folder (a prefilled input dialog)
	void onDuplicateShortcut();	// Ctrl+D: duplicate the selected script/folder (== Copy)
	void onUndo();
	void onRedo();
	void onVerify();
	void onToggleActive();
	void onAddDebug();
	void onRemoveDebug();
	void onPatchGC();
	void onExport();
	void onImport();
	void onSaveNow();
	void onCheckboxToggled();
	void onFind();
	void onSearchTextChanged(const QString &text);	// live filter when NewSearch is on (debounced)
	void onSearchDebounce();						// the debounce timer fired -> apply the filter
	void onFilterChanged();							// a Show: chip toggled
	// Inline find/replace bar (Ctrl+H): rename a parameter value across every script.
	void onToggleReplaceBar();		// Ctrl+H / Replace... button: show/hide the bar (all scripts)
	void onOpenReplaceScoped();		// "In script" button: open the bar scoped to the selection
	void onReplaceScopeToggled(bool on);	// "This script only" checkbox
	void onReplaceCriteriaChanged();	// find text or a toggle changed -> debounce a recount + suggestions
	void onReplaceDebounce();		// the debounce fired -> recount + refresh suggestions
	void onReplaceNext();
	void onReplacePrev();
	void onReplaceAll();
	void onReplaceClose();
	void onReplaceSuggestionPicked(const QString &text);	// completer choice -> fill Find with the value
	void onTreeContextMenu(const QPoint &pos);
	void onTreeDoubleClicked(QTreeWidgetItem *item, int column);
	void onReferenceClicked(const QUrl &url);
	void onOk();
	void onCancel();

private:
	void pushSelectionToDialog();
	void updateButtonStates();
	void updateDetail();				// fill the description + comment panels for the selection
	void seedCheckboxes();				// initial checkbox states from the seam
	void buildIcons();					// build the base + state-variant node icons (once)
	QIcon nodeIcon(int listType, int flags) const;	// pick the icon for a node's type + state
	int  selectedListType() const;		// -1 if nothing selected
	void selectByListType(int listType);
	void applyCompressFont();			// == OnCompress: 14px "Segoe UI" on the tree when compressed
	// Hide tree rows that don't match the current search text AND the active Show: chips (a folder
	// stays visible if any descendant matches). No-op producing a fully-shown tree when nothing is
	// filtering. filterActive() is true iff any filter is in effect; textFilterActive() iff the
	// search box (NewSearch on, non-empty) is contributing.
	void applyFilters();
	bool filterActive() const;
	bool textFilterActive() const;

	// The active filter criteria, resolved once per applyFilters and passed down the recursion so
	// each node doesn't re-query the widgets. flags bits: bit0 active, bit1 warnings, bit3 easy,
	// bit4 normal, bit5 hard.
	struct FilterState
	{
		bool useText;
		QByteArray needle;
		bool wantWarn, wantActive, wantInactive, wantEasy, wantNormal, wantHard;
	};
	bool nodeSelfMatches(QTreeWidgetItem *item, const FilterState &fs) const;
	bool filterItemRec(QTreeWidgetItem *item, const FilterState &fs);

	// Find/replace bar helpers.
	void refreshReplaceCount();			// recompute the match count label + button enable states
	void refreshReplaceSuggestions();	// repopulate the Find autocomplete from the current text
	void refreshReplaceValueList();		// repopulate the Replace autocomplete (all values; built rarely)
	int  replaceScope() const;			// the scope listType for find/replace: -1 all, else the selected script
	void stepReplaceMatch(int dir);		// select the next (+1) / previous (-1) script containing a match
	void applyReplaceSelection(int listType);	// select + sync the found match (shared by Next/Prev)
	QByteArray replaceFindText() const;	// trimmed find text as latin1 (empty if none)

	Ui::WBQtScriptWindow *m_ui;	// owns the static widget tree (WBQtScriptWindow.ui)

	WBQtScriptTree *m_tree;
	QPlainTextEdit *m_description;
	QTextBrowser *m_comment;	// browser, not edit: the "[Referenced in]" names are links
	QLineEdit   *m_search;
	QPushButton *m_findBtn;
	QTimer      *m_searchDebounce;	// coalesces keystrokes so the live filter scans once per pause

	// Inline find/replace bar widgets (from the .ui).
	QWidget     *m_replaceBar;
	QLineEdit   *m_replaceFind;
	QLineEdit   *m_replaceWith;
	QToolButton *m_replaceMatchCase;
	QToolButton *m_replaceWholeValue;
	QCheckBox   *m_replaceScopeCheck;	// "This script only"
	QLabel      *m_replaceCount;
	// When scoped, the listType of the script the bar is pinned to (captured when scope turns on,
	// so it stays fixed as you click matches). -1 when unscoped.
	int          m_replaceScopeListType;
	QCompleter       *m_replaceCompleter;	// Find autocomplete: parameter values matching the text
	QStandardItemModel *m_replaceSuggestModel;	// 2 cols: value | "(count)" -- only col 0 is inserted
	QCompleter       *m_replaceWithCompleter;	// Replace autocomplete: all existing parameter values
	QStringListModel *m_replaceWithModel;
	QTimer           *m_replaceDebounce;	// coalesces Find keystrokes so the param walk runs once per pause

	// "Show:" filter chips (narrow the tree by state). Warnings/Active/Inactive/difficulty.
	QCheckBox   *m_filterWarnings;
	QCheckBox   *m_filterActive;
	QCheckBox   *m_filterInactive;
	QCheckBox   *m_filterEasy;
	QCheckBox   *m_filterNormal;
	QCheckBox   *m_filterHard;

	// Option checkboxes (indexed by WBQT_SCK_*).
	QCheckBox   *m_ckCompress;
	QCheckBox   *m_ckNewIcons;
	QCheckBox   *m_ckCleanName;
	QCheckBox   *m_ckAutoVerify;
	QCheckBox   *m_ckSmartCopy;
	QCheckBox   *m_ckFastLoad;
	QCheckBox   *m_ckScriptMerge;
	QCheckBox   *m_ckRefByParam;
	QCheckBox   *m_ckDisableRef;

	QPushButton *m_newFolder;
	QPushButton *m_newScript;
	QPushButton *m_editScript;
	QPushButton *m_copyScript;
	QPushButton *m_delete;
	QPushButton *m_verify;
	QPushButton *m_addDebug;
	QPushButton *m_removeDebug;
	QPushButton *m_patchGC;
	QPushButton *m_export;
	QPushButton *m_import;
	QPushButton *m_saveNow;
	QPushButton *m_ok;
	QPushButton *m_cancel;

	// Node icons, built once. [type 0=player,1=folder,2=script][state 0=normal,1=warnings,2=inactive]
	QIcon m_icons[3][3];

	int  m_lastFoundListType;	// find-next cursor (0 = start from top)
	bool m_updating;			// re-entrancy guard while (re)building the tree / seeding

	QFont m_treeDefaultFont;		// the tree's font before any Compress swap (for revert)
	bool  m_treeDefaultFontValid;	// captured m_treeDefaultFont yet?

	static WBQtScriptWindow *s_instance;
};

#endif // WB_QT_SCRIPT_WINDOW_H
