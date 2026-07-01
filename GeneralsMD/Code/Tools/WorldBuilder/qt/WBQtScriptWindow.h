// WBQtScriptWindow.h -- Qt front-end for the WorldBuilder Script editor (Phase 9a).
//
// This is NOT an option panel; it's a standalone top-level window opened from
// CMainFrame::onEditScripts. In Qt mode the MFC ScriptDialog is still Create()d but kept
// hidden -- it owns the working model (m_sides), the condition/action sub-editors, and the
// OK/Cancel commit. This window replaces only the script tree + command buttons, driving the
// hidden dialog through the WBQtScript_* seam. Drag-drop, search, verify, the icon/warning
// system, and native sub-editors are deferred to later phases.
#ifndef WB_QT_SCRIPT_WINDOW_H
#define WB_QT_SCRIPT_WINDOW_H

#include <QWidget>

class QPushButton;
class QTreeWidget;

class WBQtScriptWindow : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtScriptWindow(QWidget *owner);

	void rebuildTree();

	static WBQtScriptWindow *instance() { return s_instance; }

private slots:
	void onTreeSelectionChanged();
	void onNewFolder();
	void onNewScript();
	void onEditScript();
	void onCopyScript();
	void onDelete();
	void onOk();
	void onCancel();

private:
	void pushSelectionToDialog();
	void updateButtonStates();

	QTreeWidget *m_tree;
	QPushButton *m_newFolder;
	QPushButton *m_newScript;
	QPushButton *m_editScript;
	QPushButton *m_copyScript;
	QPushButton *m_delete;
	QPushButton *m_ok;
	QPushButton *m_cancel;

	bool m_updating;	// re-entrancy guard while (re)building the tree

	static WBQtScriptWindow *s_instance;
};

#endif // WB_QT_SCRIPT_WINDOW_H
