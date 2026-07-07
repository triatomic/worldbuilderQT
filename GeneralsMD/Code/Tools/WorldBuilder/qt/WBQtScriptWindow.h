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

#include <QIcon>
#include <QTreeWidget>
#include <QWidget>

class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTextBrowser;
class QUrl;
class WBQtScriptWindow;

// A QTreeWidget that reports internal drops back to the window instead of moving items itself
// (the model rebuild does the actual move). Drag is enabled per-item in the window.
class WBQtScriptTree : public QTreeWidget
{
	Q_OBJECT

public:
	explicit WBQtScriptTree(WBQtScriptWindow *owner);

protected:
	virtual void dropEvent(QDropEvent *event);

private:
	WBQtScriptWindow *m_owner;
};

class WBQtScriptWindow : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtScriptWindow(QWidget *owner);

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

	WBQtScriptTree *m_tree;
	QPlainTextEdit *m_description;
	QTextBrowser *m_comment;	// browser, not edit: the "[Referenced in]" names are links
	QLineEdit   *m_search;
	QPushButton *m_findBtn;

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

	static WBQtScriptWindow *s_instance;
};

#endif // WB_QT_SCRIPT_WINDOW_H
