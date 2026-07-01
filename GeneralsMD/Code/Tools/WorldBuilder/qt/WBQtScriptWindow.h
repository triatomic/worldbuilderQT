// WBQtScriptWindow.h -- Qt front-end for the WorldBuilder Script editor (Phases 9a/9b).
//
// This is NOT an option panel; it's a standalone top-level window opened from
// CMainFrame::onEditScripts. In Qt mode the MFC ScriptDialog is still Create()d but kept
// hidden -- it owns the working model (m_sides), the condition/action sub-editors, and the
// OK/Cancel commit. This window replaces only the script tree + command buttons, driving the
// hidden dialog through the WBQtScript_* seam. 9a: tree + 5 core commands. 9b: internal
// drag-drop reorder/move (reuses doDropOn incl. Ctrl auto-merge) + search/find-next. Verify,
// the icon/warning system, and native sub-editors are still deferred.
#ifndef WB_QT_SCRIPT_WINDOW_H
#define WB_QT_SCRIPT_WINDOW_H

#include <QTreeWidget>
#include <QWidget>

class QLineEdit;
class QPushButton;
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
	void onFind();
	void onOk();
	void onCancel();

private:
	void pushSelectionToDialog();
	void updateButtonStates();
	int  selectedListType() const;	// -1 if nothing selected
	void selectByListType(int listType);

	WBQtScriptTree *m_tree;
	QLineEdit   *m_search;
	QPushButton *m_findBtn;
	QPushButton *m_newFolder;
	QPushButton *m_newScript;
	QPushButton *m_editScript;
	QPushButton *m_copyScript;
	QPushButton *m_delete;
	QPushButton *m_ok;
	QPushButton *m_cancel;

	int  m_lastFoundListType;	// find-next cursor (0 = start from top)

	bool m_updating;	// re-entrancy guard while (re)building the tree

	static WBQtScriptWindow *s_instance;
};

#endif // WB_QT_SCRIPT_WINDOW_H
