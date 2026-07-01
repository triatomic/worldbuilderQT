// WBQtScriptEditDialog.cpp -- see WBQtScriptEditDialog.h. Layout and behavior mirror the four
// MFC property pages (IDD_ScriptProperties / IDD_ScriptConditions / IDD_ScriptActionsTrue /
// IDD_ScriptActionsFalse); every command routes through the bridge, which ports the page
// handlers verbatim and reports back the row to select after the rebuild.
#include "WBQtScriptEditDialog.h"
#include "WBQtScriptEditBridge.h"
#include "WBQtScriptWindow.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <qt_windows.h>

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
	m_script(script),
	m_mode(mode),
	m_updating(false),
	m_orButton(NULL),
	m_moveToOtherButton(NULL)
{
	QVBoxLayout *root = new QVBoxLayout(this);

	const char *caption = "Conditions for this script:";
	if (m_mode == ModeActionsTrue)
	{
		caption = "Actions to take if conditions are true:";
	}
	else if (m_mode == ModeActionsFalse)
	{
		caption = "Actions to take if conditions are false:";
	}
	root->addWidget(new QLabel(caption, this));

	QHBoxLayout *middle = new QHBoxLayout();
	m_list = new QListWidget(this);
	m_list->setSelectionMode(QAbstractItemView::SingleSelection);
	m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	middle->addWidget(m_list, 1);

	QVBoxLayout *buttons = new QVBoxLayout();
	m_smartCopyCheck = new QCheckBox("Smart Copy", this);
	buttons->addWidget(m_smartCopyCheck);
	m_newButton = new QPushButton((m_mode == ModeConditions) ? "New... [&S]" : "New...[&S]", this);
	buttons->addWidget(m_newButton);
	m_editButton = new QPushButton("&Edit...", this);
	buttons->addWidget(m_editButton);
	m_copyButton = new QPushButton("&Copy", this);
	buttons->addWidget(m_copyButton);
	m_deleteButton = new QPushButton("&Delete", this);
	buttons->addWidget(m_deleteButton);
	if (m_mode == ModeConditions)
	{
		m_orButton = new QPushButton("O&r", this);
		buttons->addWidget(m_orButton);
	}
	else
	{
		m_moveToOtherButton = new QPushButton((m_mode == ModeActionsTrue) ? "Move to False" : "Move to True", this);
		buttons->addWidget(m_moveToOtherButton);
	}
	buttons->addStretch(1);
	m_moveUpButton = new QPushButton("Move Up [&Z]", this);
	buttons->addWidget(m_moveUpButton);
	m_moveDownButton = new QPushButton("Move Down [&X]", this);
	buttons->addWidget(m_moveDownButton);
	middle->addLayout(buttons);
	root->addLayout(middle, 1);

	m_commentEdit = new QPlainTextEdit(this);
	m_commentEdit->setFixedHeight(64);
	root->addWidget(m_commentEdit);

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

	// Initial fill: the conditions page selected row 1 (the first condition) in OnInitDialog;
	// the actions pages started at the top.
	reload((m_mode == ModeConditions) ? 1 : 0);
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
	m_script(script),
	m_updating(false)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	QVBoxLayout *root = new QVBoxLayout(this);
	m_tabs = new QTabWidget(this);
	root->addWidget(m_tabs, 1);

	m_tabs->addTab(buildPropertiesTab(), "Script Properties");
	m_conditionsTab = new WBQtScriptEditListTab(m_script, WBQtScriptEditListTab::ModeConditions, this);
	m_tabs->addTab(m_conditionsTab, "Script Conditions");
	m_trueTab = new WBQtScriptEditListTab(m_script, WBQtScriptEditListTab::ModeActionsTrue, this);
	m_tabs->addTab(m_trueTab, "Actions if true.");
	m_falseTab = new WBQtScriptEditListTab(m_script, WBQtScriptEditListTab::ModeActionsFalse, this);
	m_tabs->addTab(m_falseTab, "Actions if false.");
	connect(m_tabs, SIGNAL(currentChanged(int)), this, SLOT(onTabChanged(int)));

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
	connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
	root->addWidget(buttons);

	seedProperties();
	resize(780, 560);
}

QWidget *WBQtScriptEditDialog::buildPropertiesTab()
{
	QWidget *page = new QWidget(this);
	QVBoxLayout *root = new QVBoxLayout(page);

	QHBoxLayout *nameRow = new QHBoxLayout();
	nameRow->addWidget(new QLabel("Script Name:", page));
	m_nameEdit = new QLineEdit(page);
	nameRow->addWidget(m_nameEdit, 1);
	root->addLayout(nameRow);

	QHBoxLayout *middle = new QHBoxLayout();

	QGroupBox *flagsBox = new QGroupBox("Script Flags:", page);
	QVBoxLayout *flagsLay = new QVBoxLayout(flagsBox);
	m_subroutineCheck = new QCheckBox("Script is Subroutine (Make this script usable by teams, read the manual for more info)", page);
	flagsLay->addWidget(m_subroutineCheck);
	m_activeCheck = new QCheckBox("Script is Active", page);
	flagsLay->addWidget(m_activeCheck);
	m_oneShotCheck = new QCheckBox("Deactivate upon success (automatically disables itself after completing successfully)", page);
	flagsLay->addWidget(m_oneShotCheck);
	flagsLay->addStretch(1);
	middle->addWidget(flagsBox, 1);

	QGroupBox *activeInBox = new QGroupBox("Active in:", page);
	QVBoxLayout *activeInLay = new QVBoxLayout(activeInBox);
	m_easyCheck = new QCheckBox("Easy", page);
	activeInLay->addWidget(m_easyCheck);
	m_normalCheck = new QCheckBox("Normal", page);
	activeInLay->addWidget(m_normalCheck);
	m_hardCheck = new QCheckBox("Hard", page);
	activeInLay->addWidget(m_hardCheck);
	activeInLay->addStretch(1);
	middle->addWidget(activeInBox);

	QGroupBox *evalBox = new QGroupBox("Evaluate script:", page);
	QVBoxLayout *evalLay = new QVBoxLayout(evalBox);
	m_everyFrameRadio = new QRadioButton("Every Frame", page);
	evalLay->addWidget(m_everyFrameRadio);
	QHBoxLayout *secondsRow = new QHBoxLayout();
	m_everySecondRadio = new QRadioButton("Every", page);
	secondsRow->addWidget(m_everySecondRadio);
	m_secondsSpin = new QSpinBox(page);
	m_secondsSpin->setRange(1, 9999);
	secondsRow->addWidget(m_secondsSpin);
	secondsRow->addWidget(new QLabel("seconds.", page));
	secondsRow->addStretch(1);
	evalLay->addLayout(secondsRow);
	evalLay->addStretch(1);
	middle->addWidget(evalBox);

	root->addLayout(middle);

	QGroupBox *commentBox = new QGroupBox("Script Comment:", page);
	QVBoxLayout *commentLay = new QVBoxLayout(commentBox);
	m_commentEdit = new QPlainTextEdit(page);
	commentLay->addWidget(m_commentEdit);
	root->addWidget(commentBox, 1);

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

	return page;
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

extern "C" int WBQtScriptEdit_Run(void *script, void *frameHwnd)
{
	if (script == NULL)
	{
		return 0;
	}
	// Parent to the Qt Script window: the dialog becomes transient to it (always stacks above
	// the script window and the frame, centers over it), like the MFC sheet owned by the
	// ScriptDialog.
	QWidget *owner = WBQtScriptWindow::instance();
	if (owner != NULL && !owner->isVisible())
	{
		owner = NULL;
	}
	WBQtScriptEditDialog dlg(script, owner);
	// Application-modal + a disabled MFC frame == the old CPropertySheet::DoModal discipline
	// (Qt modality only fences Qt windows; the frame is a native window and needs the explicit
	// EnableWindow). The Qt Script window is fenced by the modality.
	dlg.setWindowModality(Qt::ApplicationModal);
	HWND frame = reinterpret_cast<HWND>(frameHwnd);
	if (frame != NULL)
	{
		::EnableWindow(frame, FALSE);
	}
	// Register our HWND so the MFC EditCondition/EditAction modals the bridge pops are owned
	// by (and disable) this dialog.
	WBQtScriptEdit_SetModalOwner(reinterpret_cast<void *>(dlg.winId()));
	int rc = dlg.exec();
	WBQtScriptEdit_SetModalOwner(NULL);
	if (frame != NULL)
	{
		::EnableWindow(frame, TRUE);
	}
	return (rc == QDialog::Accepted) ? 1 : 0;
}
