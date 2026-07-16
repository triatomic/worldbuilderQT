// WBQtScriptEditDialog.h -- the native Qt script editor (Tier 2a): a tabbed modal replacing the
// MFC CPropertySheet of ScriptProperties / ScriptConditionsDlg / ScriptActionsTrue/False. It
// edits a caller-owned Script* through the C facade in WBQtScriptEditBridge.h (all engine access
// stays MFC-side); the EditCondition / EditAction sub-editors are still the MFC modals, popped
// by the bridge ops and owned by this dialog's HWND. Run via WBQtScriptEdit_Run() (exec()'d
// application-modal with the MFC frame disabled, matching the old DoModal discipline).
#ifndef WB_QT_SCRIPT_EDIT_DIALOG_H
#define WB_QT_SCRIPT_EDIT_DIALOG_H

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTabWidget;

namespace Ui { class WBQtScriptEditListTab; }	// generated from WBQtScriptEditListTab.ui
namespace Ui { class WBQtScriptEditDialog; }	// generated from WBQtScriptEditDialog.ui

// One list page (Script Conditions / Actions if true / Actions if false) -- the three MFC pages
// are structural twins (list + button column + Smart Copy + comment), so one widget serves all
// three, parameterized by mode.
class WBQtScriptEditListTab : public QWidget
{
	Q_OBJECT
public:
	enum Mode
	{
		ModeConditions,
		ModeActionsTrue,
		ModeActionsFalse
	};

	WBQtScriptEditListTab(void *script, Mode mode, QWidget *parent = 0);
	virtual ~WBQtScriptEditListTab();

	// Rebuild the list from the script (== the MFC page's loadList()); selects selectRow
	// (clamped; -1 keeps the current row) and re-seeds the Smart Copy checkbox + comment.
	void reload(int selectRow);

private slots:
	void onSelectionChanged();
	void onNew();
	void onEdit();
	void onCopy();
	void onDelete();
	void onOr();
	void onMoveToOther();
	void onMoveUp();
	void onMoveDown();
	void onSmartCopyToggled(bool checked);
	void onCommentChanged();

private:
	int  currentRow() const;
	void updateButtonStates();

	Ui::WBQtScriptEditListTab *m_ui;	// owns the static widget tree (WBQtScriptEditListTab.ui)

	void *m_script;
	Mode m_mode;
	bool m_updating;

	QListWidget *m_list;
	QCheckBox *m_smartCopyCheck;
	QPushButton *m_newButton;
	QPushButton *m_editButton;
	QPushButton *m_copyButton;
	QPushButton *m_deleteButton;
	QPushButton *m_orButton;			// conditions only
	QPushButton *m_moveToOtherButton;	// actions only
	QPushButton *m_moveUpButton;
	QPushButton *m_moveDownButton;
	QPlainTextEdit *m_commentEdit;
};

class WBQtScriptEditDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtScriptEditDialog(void *script, QWidget *parent = 0);
	virtual ~WBQtScriptEditDialog();

private slots:
	void onTabChanged(int index);
	void onNameChanged(const QString &text);
	void onCommentChanged();
	void onEveryFrame();
	void onEverySecond();
	void onSecondsChanged(int value);

private:
	void wirePropertiesTab();	// binds + connects the Script Properties page (tree lives in the .ui)
	void seedProperties();

	Ui::WBQtScriptEditDialog *m_ui;	// owns the static widget tree (WBQtScriptEditDialog.ui)

	void *m_script;
	bool m_updating;

	QTabWidget *m_tabs;

	// Script Properties page
	QLineEdit *m_nameEdit;
	QCheckBox *m_subroutineCheck;
	QCheckBox *m_activeCheck;
	QCheckBox *m_oneShotCheck;
	QCheckBox *m_easyCheck;
	QCheckBox *m_normalCheck;
	QCheckBox *m_hardCheck;
	QRadioButton *m_everyFrameRadio;
	QRadioButton *m_everySecondRadio;
	QSpinBox *m_secondsSpin;
	QPlainTextEdit *m_commentEdit;

	WBQtScriptEditListTab *m_conditionsTab;
	WBQtScriptEditListTab *m_trueTab;
	WBQtScriptEditListTab *m_falseTab;
};

#endif // WB_QT_SCRIPT_EDIT_DIALOG_H
