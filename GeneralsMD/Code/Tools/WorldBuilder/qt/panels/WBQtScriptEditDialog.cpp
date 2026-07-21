// WBQtScriptEditDialog.cpp -- see WBQtScriptEditDialog.h. Layout and behavior mirror the four
// MFC property pages (IDD_ScriptProperties / IDD_ScriptConditions / IDD_ScriptActionsTrue /
// IDD_ScriptActionsFalse); every command routes through the bridge, which ports the page
// handlers verbatim and reports back the row to select after the rebuild.
#include "WBQtScriptEditDialog.h"
#include "ui_WBQtScriptEditListTab.h"
#include "ui_WBQtScriptEditDialog.h"
#include "WBQtScriptEditBridge.h"
#include "WBQtScriptWindow.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QShortcut>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>

#include <qt_windows.h>

// Stage 1 phase 3: modal-dialog parent (active modal if nested, else main window). WBQtBridge.cpp.
QWidget *WBQt_DialogParent(void);

namespace
{
	const int kLabelCap = 1024;
	const int kCommentCap = 8192;

	// Item data role holding the row kind on the conditions list (1 condition, 0 OR header).
	const int kRowKindRole = Qt::UserRole;

	QString bridgeText(void *script, int field)
	{
		char buf[kCommentCap];
		buf[0] = 0;
		WBQtScriptEditData_GetText(script, field, buf, sizeof(buf));
		return QString::fromLocal8Bit(buf);
	}
}

// ===================== WBQtScriptEditListTab =====================

WBQtScriptEditListTab::WBQtScriptEditListTab(void *script, Mode mode, QWidget *parent)
	: QWidget(parent),
	m_ui(new Ui::WBQtScriptEditListTab),
	m_script(script),
	m_mode(mode),
	m_updating(false),
	m_orButton(NULL),
	m_moveToOtherButton(NULL)
{
	// The static widget tree lives in WBQtScriptEditListTab.ui; the mode-dependent
	// pieces (caption, New button text, the Or / Move-to-Other button) are set here.
	m_ui->setupUi(this);

	m_list = m_ui->list;
	m_smartCopyCheck = m_ui->smartCopyCheck;
	m_newButton = m_ui->newButton;
	m_editButton = m_ui->editButton;
	m_copyButton = m_ui->copyButton;
	m_deleteButton = m_ui->deleteButton;
	m_moveUpButton = m_ui->moveUpButton;
	m_moveDownButton = m_ui->moveDownButton;
	m_commentEdit = m_ui->commentEdit;

	const char *caption = "Conditions for this script:";
	if (m_mode == ModeActionsTrue)
	{
		caption = "Actions to take if conditions are true:";
	}
	else if (m_mode == ModeActionsFalse)
	{
		caption = "Actions to take if conditions are false:";
	}
	m_ui->captionLabel->setText(caption);

	m_newButton->setText((m_mode == ModeConditions) ? "New... [&S]" : "New...[&S]");
	if (m_mode == ModeConditions)
	{
		m_orButton = new QPushButton("O&r", this);
		m_ui->buttonsLayout->insertWidget(5, m_orButton);	// after Delete, before the stretch
	}
	else
	{
		m_moveToOtherButton = new QPushButton((m_mode == ModeActionsTrue) ? "Move to False" : "Move to True", this);
		m_ui->buttonsLayout->insertWidget(5, m_moveToOtherButton);	// after Delete, before the stretch
	}

	connect(m_list, SIGNAL(currentRowChanged(int)), this, SLOT(onSelectionChanged()));
	connect(m_list, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(onEdit()));
	connect(m_newButton, SIGNAL(clicked()), this, SLOT(onNew()));
	connect(m_editButton, SIGNAL(clicked()), this, SLOT(onEdit()));
	connect(m_copyButton, SIGNAL(clicked()), this, SLOT(onCopy()));
	connect(m_deleteButton, SIGNAL(clicked()), this, SLOT(onDelete()));
	if (m_orButton != NULL)
	{
		connect(m_orButton, SIGNAL(clicked()), this, SLOT(onOr()));
	}
	if (m_moveToOtherButton != NULL)
	{
		connect(m_moveToOtherButton, SIGNAL(clicked()), this, SLOT(onMoveToOther()));
	}
	connect(m_moveUpButton, SIGNAL(clicked()), this, SLOT(onMoveUp()));
	connect(m_moveDownButton, SIGNAL(clicked()), this, SLOT(onMoveDown()));
	connect(m_smartCopyCheck, SIGNAL(toggled(bool)), this, SLOT(onSmartCopyToggled(bool)));
	connect(m_commentEdit, SIGNAL(textChanged()), this, SLOT(onCommentChanged()));

	// Ctrl+C / Ctrl+V: copy the selected condition/action to the cross-script clipboard and paste
	// it (into any script's matching list). WidgetShortcut = active only while THIS list has focus,
	// so Ctrl+C/V keep their text meaning in the comment box. Same-type is enforced by the bridge:
	// a condition list only has a condition clipboard, an action list only an action clipboard.
	QShortcut *copySc = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_C), m_list);
	copySc->setContext(Qt::WidgetShortcut);
	connect(copySc, SIGNAL(activated()), this, SLOT(onCopyClipboard()));
	QShortcut *pasteSc = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_V), m_list);
	pasteSc->setContext(Qt::WidgetShortcut);
	connect(pasteSc, SIGNAL(activated()), this, SLOT(onPasteClipboard()));

	// Initial fill: the conditions page selected row 1 (the first condition) in OnInitDialog;
	// the actions pages started at the top.
	reload((m_mode == ModeConditions) ? 1 : 0);
}

WBQtScriptEditListTab::~WBQtScriptEditListTab()
{
	delete m_ui;
}

int WBQtScriptEditListTab::currentRow() const
{
	return m_list->currentRow();
}

void WBQtScriptEditListTab::reload(int selectRow)
{
	m_updating = true;
	if (selectRow < 0)
	{
		selectRow = m_list->currentRow();
	}
	m_list->clear();
	char buf[kLabelCap];
	if (m_mode == ModeConditions)
	{
		int count = WBQtScriptEditData_GetConditionRowCount(m_script);
		for (int i = 0; i < count; i++)
		{
			int kind = WBQtScriptEditData_GetConditionRow(m_script, i, buf, sizeof(buf));
			QListWidgetItem *item = new QListWidgetItem(QString::fromLocal8Bit(buf), m_list);
			item->setData(kRowKindRole, kind);
		}
	}
	else
	{
		int isFalse = (m_mode == ModeActionsFalse) ? 1 : 0;
		int count = WBQtScriptEditData_GetActionCount(m_script, isFalse);
		for (int i = 0; i < count; i++)
		{
			WBQtScriptEditData_GetActionLabel(m_script, isFalse, i, buf, sizeof(buf));
			new QListWidgetItem(QString::fromLocal8Bit(buf), m_list);
		}
	}
	if (m_list->count() > 0)
	{
		if (selectRow >= m_list->count())
		{
			selectRow = m_list->count() - 1;
		}
		if (selectRow < 0)
		{
			selectRow = 0;
		}
		m_list->setCurrentRow(selectRow);
	}

	m_smartCopyCheck->setChecked(WBQtScriptEdit_GetSmartCopy() != 0);
	int commentField = (m_mode == ModeConditions) ? WB_QT_SCRIPTEDIT_TEXT_CONDITION_COMMENT
												  : WB_QT_SCRIPTEDIT_TEXT_ACTION_COMMENT;
	QString comment = bridgeText(m_script, commentField);
	if (m_commentEdit->toPlainText() != comment)
	{
		m_commentEdit->setPlainText(comment);
	}
	m_updating = false;
	updateButtonStates();
}

void WBQtScriptEditListTab::updateButtonStates()
{
	int row = currentRow();
	if (m_mode == ModeConditions)
	{
		// == ScriptConditionsDlg::enableUI: Edit/Copy need a condition row, Delete needs any row.
		bool isCondition = false;
		if (row >= 0)
		{
			QListWidgetItem *item = m_list->item(row);
			isCondition = (item != NULL && item->data(kRowKindRole).toInt() == 1);
		}
		m_editButton->setEnabled(isCondition);
		m_copyButton->setEnabled(isCondition);
		m_deleteButton->setEnabled(row >= 0);
	}
	else
	{
		// == ScriptActionsTrue/False::enableUI.
		bool hasAction = (row >= 0);
		m_editButton->setEnabled(hasAction);
		m_copyButton->setEnabled(hasAction);
		m_deleteButton->setEnabled(hasAction);
		m_moveToOtherButton->setEnabled(hasAction);
		m_moveDownButton->setEnabled(hasAction && row < m_list->count() - 1);
		m_moveUpButton->setEnabled(hasAction && row > 0);
	}
}

void WBQtScriptEditListTab::onSelectionChanged()
{
	if (m_updating)
	{
		return;
	}
	updateButtonStates();
}

void WBQtScriptEditListTab::onNew()
{
	int newRow;
	if (m_mode == ModeConditions)
	{
		newRow = WBQtScriptEdit_ConditionNew(m_script, currentRow());
	}
	else
	{
		newRow = WBQtScriptEdit_ActionNew(m_script, (m_mode == ModeActionsFalse) ? 1 : 0, currentRow());
	}
	if (newRow >= 0)
	{
		reload(newRow);
	}
}

void WBQtScriptEditListTab::onEdit()
{
	int newRow;
	if (m_mode == ModeConditions)
	{
		newRow = WBQtScriptEdit_ConditionEdit(m_script, currentRow());
	}
	else
	{
		newRow = WBQtScriptEdit_ActionEdit(m_script, (m_mode == ModeActionsFalse) ? 1 : 0, currentRow());
	}
	if (newRow >= 0)
	{
		reload(newRow);
	}
}

void WBQtScriptEditListTab::onCopy()
{
	int newRow;
	if (m_mode == ModeConditions)
	{
		newRow = WBQtScriptEdit_ConditionCopy(m_script, currentRow());
	}
	else
	{
		newRow = WBQtScriptEdit_ActionCopy(m_script, (m_mode == ModeActionsFalse) ? 1 : 0, currentRow());
	}
	if (newRow >= 0)
	{
		reload(newRow);
	}
}

void WBQtScriptEditListTab::onCopyClipboard()
{
	// Stash the selected item to the app-lifetime clipboard (no list change, so no reload).
	if (m_mode == ModeConditions)
	{
		WBQtScriptEdit_ConditionCopyToClipboard(m_script, currentRow());
	}
	else
	{
		WBQtScriptEdit_ActionCopyToClipboard(m_script, (m_mode == ModeActionsFalse) ? 1 : 0, currentRow());
	}
}

void WBQtScriptEditListTab::onPasteClipboard()
{
	int newRow;
	if (m_mode == ModeConditions)
	{
		newRow = WBQtScriptEdit_ConditionPasteFromClipboard(m_script, currentRow());
	}
	else
	{
		newRow = WBQtScriptEdit_ActionPasteFromClipboard(m_script, (m_mode == ModeActionsFalse) ? 1 : 0, currentRow());
	}
	if (newRow >= 0)
	{
		reload(newRow);
	}
}

void WBQtScriptEditListTab::onDelete()
{
	int newRow;
	if (m_mode == ModeConditions)
	{
		newRow = WBQtScriptEdit_ConditionDelete(m_script, currentRow());
	}
	else
	{
		newRow = WBQtScriptEdit_ActionDelete(m_script, (m_mode == ModeActionsFalse) ? 1 : 0, currentRow());
	}
	if (newRow >= 0)
	{
		reload(newRow);
	}
	else if (currentRow() >= 0)
	{
		// Deleting the last remaining row leaves nothing to select; still rebuild.
		reload(0);
	}
}

void WBQtScriptEditListTab::onOr()
{
	int newRow = WBQtScriptEdit_ConditionOr(m_script, currentRow());
	if (newRow >= 0)
	{
		reload(newRow);
	}
}

void WBQtScriptEditListTab::onMoveToOther()
{
	int newRow = WBQtScriptEdit_ActionMoveToOther(m_script, (m_mode == ModeActionsFalse) ? 1 : 0, currentRow());
	if (newRow >= 0)
	{
		// The other tab is rebuilt when it is activated (== the MFC pages' OnSetActive reload).
		reload(newRow);
	}
}

void WBQtScriptEditListTab::onMoveUp()
{
	int newRow;
	if (m_mode == ModeConditions)
	{
		newRow = WBQtScriptEdit_ConditionMoveUp(m_script, currentRow());
	}
	else
	{
		newRow = WBQtScriptEdit_ActionMoveUp(m_script, (m_mode == ModeActionsFalse) ? 1 : 0, currentRow());
	}
	if (newRow >= 0)
	{
		reload(newRow);
	}
}

void WBQtScriptEditListTab::onMoveDown()
{
	int newRow;
	if (m_mode == ModeConditions)
	{
		newRow = WBQtScriptEdit_ConditionMoveDown(m_script, currentRow());
	}
	else
	{
		newRow = WBQtScriptEdit_ActionMoveDown(m_script, (m_mode == ModeActionsFalse) ? 1 : 0, currentRow());
	}
	if (newRow >= 0)
	{
		reload(newRow);
	}
}

void WBQtScriptEditListTab::onSmartCopyToggled(bool checked)
{
	if (m_updating)
	{
		return;
	}
	WBQtScriptEdit_SetSmartCopy(checked ? 1 : 0);
}

void WBQtScriptEditListTab::onCommentChanged()
{
	if (m_updating)
	{
		return;
	}
	int commentField = (m_mode == ModeConditions) ? WB_QT_SCRIPTEDIT_TEXT_CONDITION_COMMENT
												  : WB_QT_SCRIPTEDIT_TEXT_ACTION_COMMENT;
	QByteArray text = m_commentEdit->toPlainText().toLocal8Bit();
	WBQtScriptEditData_SetText(m_script, commentField, text.constData());
}

// ===================== WBQtScriptEditDialog =====================

WBQtScriptEditDialog::WBQtScriptEditDialog(void *script, QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtScriptEditDialog),
	m_script(script),
	m_updating(false)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// The static widget tree (tab widget + Script Properties page + button box) lives in
	// WBQtScriptEditDialog.ui; the three list pages are runtime-built WBQtScriptEditListTabs.
	m_ui->setupUi(this);

	m_tabs = m_ui->tabs;

	wirePropertiesTab();
	m_conditionsTab = new WBQtScriptEditListTab(m_script, WBQtScriptEditListTab::ModeConditions, this);
	m_tabs->addTab(m_conditionsTab, "Script Conditions");
	m_trueTab = new WBQtScriptEditListTab(m_script, WBQtScriptEditListTab::ModeActionsTrue, this);
	m_tabs->addTab(m_trueTab, "Actions if true.");
	m_falseTab = new WBQtScriptEditListTab(m_script, WBQtScriptEditListTab::ModeActionsFalse, this);
	m_tabs->addTab(m_falseTab, "Actions if false.");
	connect(m_tabs, SIGNAL(currentChanged(int)), this, SLOT(onTabChanged(int)));

	connect(m_ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(m_ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	seedProperties();
	resize(780, 560);
}

WBQtScriptEditDialog::~WBQtScriptEditDialog()
{
	delete m_ui;
}

void WBQtScriptEditDialog::wirePropertiesTab()
{
	m_nameEdit = m_ui->nameEdit;
	m_subroutineCheck = m_ui->subroutineCheck;
	m_activeCheck = m_ui->activeCheck;
	m_oneShotCheck = m_ui->oneShotCheck;
	m_easyCheck = m_ui->easyCheck;
	m_normalCheck = m_ui->normalCheck;
	m_hardCheck = m_ui->hardCheck;
	m_everyFrameRadio = m_ui->everyFrameRadio;
	m_everySecondRadio = m_ui->everySecondRadio;
	m_secondsSpin = m_ui->secondsSpin;
	m_commentEdit = m_ui->commentEdit;

	connect(m_nameEdit, SIGNAL(textChanged(QString)), this, SLOT(onNameChanged(QString)));
	connect(m_commentEdit, SIGNAL(textChanged()), this, SLOT(onCommentChanged()));
	connect(m_everyFrameRadio, SIGNAL(clicked()), this, SLOT(onEveryFrame()));
	connect(m_everySecondRadio, SIGNAL(clicked()), this, SLOT(onEverySecond()));
	connect(m_secondsSpin, SIGNAL(valueChanged(int)), this, SLOT(onSecondsChanged(int)));

	// The six flag checkboxes write straight through (== the page's ON_BN_CLICKED handlers).
	connect(m_subroutineCheck, &QCheckBox::toggled, this, [this](bool on)
	{
		if (!m_updating)
		{
			WBQtScriptEditData_SetFlag(m_script, WB_QT_SCRIPTEDIT_FLAG_SUBROUTINE, on ? 1 : 0);
		}
	});
	connect(m_activeCheck, &QCheckBox::toggled, this, [this](bool on)
	{
		if (!m_updating)
		{
			WBQtScriptEditData_SetFlag(m_script, WB_QT_SCRIPTEDIT_FLAG_ACTIVE, on ? 1 : 0);
		}
	});
	connect(m_oneShotCheck, &QCheckBox::toggled, this, [this](bool on)
	{
		if (!m_updating)
		{
			WBQtScriptEditData_SetFlag(m_script, WB_QT_SCRIPTEDIT_FLAG_ONE_SHOT, on ? 1 : 0);
		}
	});
	connect(m_easyCheck, &QCheckBox::toggled, this, [this](bool on)
	{
		if (!m_updating)
		{
			WBQtScriptEditData_SetFlag(m_script, WB_QT_SCRIPTEDIT_FLAG_EASY, on ? 1 : 0);
		}
	});
	connect(m_normalCheck, &QCheckBox::toggled, this, [this](bool on)
	{
		if (!m_updating)
		{
			WBQtScriptEditData_SetFlag(m_script, WB_QT_SCRIPTEDIT_FLAG_NORMAL, on ? 1 : 0);
		}
	});
	connect(m_hardCheck, &QCheckBox::toggled, this, [this](bool on)
	{
		if (!m_updating)
		{
			WBQtScriptEditData_SetFlag(m_script, WB_QT_SCRIPTEDIT_FLAG_HARD, on ? 1 : 0);
		}
	});
}

void WBQtScriptEditDialog::seedProperties()
{
	m_updating = true;
	QString name = bridgeText(m_script, WB_QT_SCRIPTEDIT_TEXT_NAME);
	m_nameEdit->setText(name);
	setWindowTitle(name.isEmpty() ? QString("Script") : name);
	m_commentEdit->setPlainText(bridgeText(m_script, WB_QT_SCRIPTEDIT_TEXT_COMMENT));
	m_subroutineCheck->setChecked(WBQtScriptEditData_GetFlag(m_script, WB_QT_SCRIPTEDIT_FLAG_SUBROUTINE) != 0);
	m_activeCheck->setChecked(WBQtScriptEditData_GetFlag(m_script, WB_QT_SCRIPTEDIT_FLAG_ACTIVE) != 0);
	m_oneShotCheck->setChecked(WBQtScriptEditData_GetFlag(m_script, WB_QT_SCRIPTEDIT_FLAG_ONE_SHOT) != 0);
	m_easyCheck->setChecked(WBQtScriptEditData_GetFlag(m_script, WB_QT_SCRIPTEDIT_FLAG_EASY) != 0);
	m_normalCheck->setChecked(WBQtScriptEditData_GetFlag(m_script, WB_QT_SCRIPTEDIT_FLAG_NORMAL) != 0);
	m_hardCheck->setChecked(WBQtScriptEditData_GetFlag(m_script, WB_QT_SCRIPTEDIT_FLAG_HARD) != 0);

	int delay = WBQtScriptEditData_GetDelaySeconds(m_script);
	m_everySecondRadio->setChecked(delay > 0);
	m_everyFrameRadio->setChecked(delay == 0);
	if (delay > 0)
	{
		m_secondsSpin->setValue(delay);
	}
	m_secondsSpin->setEnabled(delay > 0);
	m_updating = false;
}

void WBQtScriptEditDialog::onTabChanged(int index)
{
	// == the MFC pages' OnSetActive: the list pages rebuild on activation (this is what lets
	// "Move to False/True" show up on the other page) and re-read the Smart Copy setting.
	QWidget *page = m_tabs->widget(index);
	if (page == m_conditionsTab)
	{
		m_conditionsTab->reload(-1);
	}
	else if (page == m_trueTab)
	{
		m_trueTab->reload(-1);
	}
	else if (page == m_falseTab)
	{
		m_falseTab->reload(-1);
	}
}

void WBQtScriptEditDialog::onNameChanged(const QString &text)
{
	if (m_updating)
	{
		return;
	}
	QByteArray name = text.toLocal8Bit();
	WBQtScriptEditData_SetText(m_script, WB_QT_SCRIPTEDIT_TEXT_NAME, name.constData());
	// == ScriptProperties::OnChangeScriptName updating the sheet caption live.
	setWindowTitle(text);
}

void WBQtScriptEditDialog::onCommentChanged()
{
	if (m_updating)
	{
		return;
	}
	QByteArray text = m_commentEdit->toPlainText().toLocal8Bit();
	WBQtScriptEditData_SetText(m_script, WB_QT_SCRIPTEDIT_TEXT_COMMENT, text.constData());
}

void WBQtScriptEditDialog::onEveryFrame()
{
	if (m_updating)
	{
		return;
	}
	// == ScriptProperties::OnEveryFrame.
	WBQtScriptEditData_SetDelaySeconds(m_script, 0);
	m_secondsSpin->setEnabled(false);
}

void WBQtScriptEditDialog::onEverySecond()
{
	if (m_updating)
	{
		return;
	}
	// == ScriptProperties::OnEverySecond (seeds 1 second; the spin then edits it).
	m_secondsSpin->setEnabled(true);
	WBQtScriptEditData_SetDelaySeconds(m_script, m_secondsSpin->value());
}

void WBQtScriptEditDialog::onSecondsChanged(int value)
{
	if (m_updating)
	{
		return;
	}
	// == ScriptProperties::OnChangeSecondsEdit (typing switches to per-seconds evaluation).
	m_everySecondRadio->setChecked(true);
	m_secondsSpin->setEnabled(true);
	WBQtScriptEditData_SetDelaySeconds(m_script, value);
}

// ===================== the modal entry point =====================

extern "C" int WBQtScriptEdit_Run(void *script, void * /*frameHwnd*/)
{
	if (script == NULL)
	{
		return 0;
	}
	// Parent to the Qt Script window: the dialog becomes transient to it (always stacks above
	// the script window, centers over it), like the MFC sheet owned by the ScriptDialog. When
	// the script window isn't visible, parent to the main window instead.
	QWidget *owner = WBQtScriptWindow::instance();
	if (owner == NULL || !owner->isVisible())
	{
		owner = WBQt_DialogParent();
	}
	WBQtScriptEditDialog dlg(script, owner);
	// Stage 1 phase 3: Qt ApplicationModal fences every Qt window incl. the hosted viewport
	// (QWinHost WindowBlocked), so the old EnableWindow(frame) discipline is gone.
	dlg.setWindowModality(Qt::ApplicationModal);
	// Register our HWND so the MFC EditCondition/EditAction modals the bridge pops are owned
	// by (and disable) this dialog.
	WBQtScriptEdit_SetModalOwner(reinterpret_cast<void *>(dlg.winId()));
	int rc = dlg.exec();
	WBQtScriptEdit_SetModalOwner(NULL);
	return (rc == QDialog::Accepted) ? 1 : 0;
}
