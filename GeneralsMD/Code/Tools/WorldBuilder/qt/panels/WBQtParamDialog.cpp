// WBQtParamDialog.cpp -- see WBQtParamDialog.h. Layouts and behavior mirror the MFC
// IDD_EDIT_PARAMETER / IDD_EDIT_COORD_PARAMETER / EditObjectParameter / IDD_EDIT_GROUP
// dialogs; all values round-trip through the bridge, which ports the MFC handlers verbatim.
#include "WBQtParamDialog.h"
#include "WBQtParamBridge.h"
#include "WBQtScriptWindow.h"
#include "WBQtTreeStyle.h"

// Stage 1 phase 3: modal-dialog parent (active modal if nested, else main window). WBQtBridge.cpp.
QWidget *WBQt_DialogParent(void);

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <qt_windows.h>

namespace
{
	const int kTextCap = 1024;

	// Item data role marking template-catalog leaves in the object picker.
	const int kLeafRole = Qt::UserRole;
}

// ===================== WBQtParamDialog =====================

WBQtParamDialog::WBQtParamDialog(void *parameter, const char *unitName, QWidget *parent)
	: QDialog(parent),
	m_parameter(parameter),
	m_kind(WB_QT_PARAM_KIND_EDIT),
	m_updating(false),
	m_edit(NULL),
	m_list(NULL),
	m_autoPreviewCheck(NULL)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle("Edit Parameter:");

	char caption[256];
	caption[0] = 0;
	char initialText[kTextCap];
	initialText[0] = 0;
	int showAudio = 0;
	int initialSel = -1;
	int count = WBQtParamData_Describe(m_parameter, unitName, caption, sizeof(caption),
		&m_kind, &showAudio, &initialSel, initialText, sizeof(initialText));

	QVBoxLayout *root = new QVBoxLayout(this);
	QGroupBox *box = new QGroupBox(QString::fromLocal8Bit(caption), this);
	QVBoxLayout *boxLay = new QVBoxLayout(box);

	if (m_kind == WB_QT_PARAM_KIND_EDIT || m_kind == WB_QT_PARAM_KIND_COMBO)
	{
		m_edit = new QLineEdit(box);
		connect(m_edit, SIGNAL(textChanged(QString)), this, SLOT(onEditTextChanged(QString)));
		boxLay->addWidget(m_edit);
	}
	if (m_kind == WB_QT_PARAM_KIND_COMBO || m_kind == WB_QT_PARAM_KIND_LIST)
	{
		m_list = new QListWidget(box);
		char buf[kTextCap];
		for (int i = 0; i < count; i++)
		{
			buf[0] = 0;
			WBQtParamData_GetOption(i, buf, sizeof(buf));
			new QListWidgetItem(QString::fromLocal8Bit(buf), m_list);
		}
		boxLay->addWidget(m_list, 1);
	}
	root->addWidget(box, 1);

	QHBoxLayout *buttons = new QHBoxLayout();
	if (showAudio)
	{
		QPushButton *previewButton = new QPushButton("Preview Sound", this);
		previewButton->setAutoDefault(false);
		buttons->addWidget(previewButton);
		m_autoPreviewCheck = new QCheckBox("Auto Preview", this);
		m_autoPreviewCheck->setChecked(true);	// == the MFC dialog checking it on show
		buttons->addWidget(m_autoPreviewCheck);
		connect(previewButton, SIGNAL(clicked()), this, SLOT(onPreviewSound()));
	}
	buttons->addStretch(1);
	QPushButton *okButton = new QPushButton("OK", this);
	okButton->setDefault(true);
	buttons->addWidget(okButton);
	QPushButton *cancelButton = new QPushButton("Cancel", this);
	cancelButton->setAutoDefault(false);
	buttons->addWidget(cancelButton);
	root->addLayout(buttons);
	connect(okButton, SIGNAL(clicked()), this, SLOT(accept()));
	connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

	// Seed the initial value/selection (== OnInitDialog's SetWindowText/SetCurSel block).
	m_updating = true;
	if (m_edit != NULL)
	{
		m_edit->setText(QString::fromLocal8Bit(initialText));
		m_edit->selectAll();
	}
	if (m_list != NULL && initialSel >= 0 && initialSel < m_list->count())
	{
		m_list->setCurrentRow(initialSel);
		m_list->scrollToItem(m_list->item(initialSel), QAbstractItemView::PositionAtTop);
	}
	m_updating = false;

	if (m_list != NULL)
	{
		connect(m_list, SIGNAL(currentRowChanged(int)), this, SLOT(onRowChanged(int)));
	}
	if (m_edit != NULL && m_kind == WB_QT_PARAM_KIND_COMBO)
	{
		connect(m_edit, SIGNAL(textEdited(QString)), this, SLOT(onTextEdited(QString)));
	}

	if (m_edit != NULL)
	{
		m_edit->setFocus();
	}
	else if (m_list != NULL)
	{
		m_list->setFocus();
	}

	resize(440, (m_kind == WB_QT_PARAM_KIND_EDIT) ? 140 : 520);
}

void WBQtParamDialog::onRowChanged(int row)
{
	if (m_updating || row < 0)
	{
		return;
	}
	QString text = m_list->item(row)->text();
	if (m_edit != NULL)
	{
		// == CBS_SIMPLE: picking a row puts its text in the edit field.
		m_updating = true;
		m_edit->setText(text);
		m_updating = false;
	}
	if (m_autoPreviewCheck != NULL && m_autoPreviewCheck->isChecked())
	{
		// == OnComboSelChange's auto preview.
		QByteArray name = text.toLocal8Bit();
		WBQtParam_PreviewAudio(m_parameter, name.constData());
	}
}

void WBQtParamDialog::onTextEdited(const QString &text)
{
	if (m_updating || m_list == NULL)
	{
		return;
	}
	// Filter the list to case-insensitive substring matches: non-matching rows are
	// hidden (empty text shows everything), and the first surviving row is selected +
	// scrolled into view without overwriting what the user typed. Any character is fine
	// -- there is no format restriction on the edit, so the whole catalog is searchable.
	m_updating = true;
	QListWidgetItem *firstShown = NULL;
	for (int i = 0; i < m_list->count(); i++)
	{
		QListWidgetItem *item = m_list->item(i);
		bool match = text.isEmpty() || item->text().contains(text, Qt::CaseInsensitive);
		item->setHidden(!match);
		if (match && firstShown == NULL)
		{
			firstShown = item;
		}
	}
	if (firstShown != NULL)
	{
		m_list->setCurrentItem(firstShown);
		m_list->scrollToItem(firstShown, QAbstractItemView::PositionAtTop);
	}
	m_updating = false;
}

void WBQtParamDialog::onEditTextChanged(const QString &text)
{
	// The MFC dialog's single-line edit dropped everything from the first CR/LF when text
	// was pasted; QLineEdit keeps the newline characters, so a string key pasted with a
	// trailing newline ended up stored in the script. Match the Win32 edit control.
	int cut = -1;
	for (int i = 0; i < text.length(); i++)
	{
		if (text[i] == QLatin1Char('\r') || text[i] == QLatin1Char('\n'))
		{
			cut = i;
			break;
		}
	}
	if (cut >= 0)
	{
		m_edit->setText(text.left(cut));
	}
}

void WBQtParamDialog::onPreviewSound()
{
	if (m_edit != NULL)
	{
		QByteArray name = m_edit->text().toLocal8Bit();
		WBQtParam_PreviewAudio(m_parameter, name.constData());
	}
}

void WBQtParamDialog::accept()
{
	QString text;
	int sel = -1;
	if (m_kind == WB_QT_PARAM_KIND_LIST)
	{
		sel = m_list->currentRow();
		if (sel >= 0)
		{
			text = m_list->item(sel)->text();
		}
	}
	else
	{
		text = m_edit->text();
		if (m_list != NULL)
		{
			// The selection index only counts when it still matches the text (the user may
			// have typed free text after picking a row) -- mirrors the combo's GetCurSel.
			int row = m_list->currentRow();
			if (row >= 0 && m_list->item(row)->text() == text)
			{
				sel = row;
			}
		}
	}
	QByteArray textBytes = text.toLocal8Bit();
	if (WBQtParam_Store(m_parameter, textBytes.constData(), sel) == 0)
	{
		// == OnOK's SetFocus + MessageBeep on unparsable input.
		QApplication::beep();
		if (m_edit != NULL)
		{
			m_edit->setFocus();
			m_edit->selectAll();
		}
		return;
	}
	QDialog::accept();
}

// ===================== WBQtCoordDialog =====================

WBQtCoordDialog::WBQtCoordDialog(void *parameter, QWidget *parent)
	: QDialog(parent),
	m_parameter(parameter)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle("Edit Coordinate Parameter:");

	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
	WBQtParamData_GetCoord(m_parameter, &x, &y, &z);

	QVBoxLayout *root = new QVBoxLayout(this);
	QGroupBox *box = new QGroupBox("Location:", this);
	QVBoxLayout *boxLay = new QVBoxLayout(box);
	const char *labels[3] = { "X:", "Y:", "Z:" };
	double values[3];
	values[0] = x;
	values[1] = y;
	values[2] = z;
	QLineEdit *edits[3];
	for (int i = 0; i < 3; i++)
	{
		QHBoxLayout *row = new QHBoxLayout();
		row->addWidget(new QLabel(labels[i], box));
		edits[i] = new QLineEdit(QString::number(values[i], 'f', 2), box);
		row->addWidget(edits[i]);
		boxLay->addLayout(row);
	}
	m_editX = edits[0];
	m_editY = edits[1];
	m_editZ = edits[2];
	root->addWidget(box);

	QHBoxLayout *buttons = new QHBoxLayout();
	buttons->addStretch(1);
	QPushButton *okButton = new QPushButton("OK", this);
	okButton->setDefault(true);
	buttons->addWidget(okButton);
	QPushButton *cancelButton = new QPushButton("Cancel", this);
	cancelButton->setAutoDefault(false);
	buttons->addWidget(cancelButton);
	root->addLayout(buttons);
	connect(okButton, SIGNAL(clicked()), this, SLOT(accept()));
	connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

	resize(260, 180);
}

void WBQtCoordDialog::accept()
{
	// == EditCoordParameter::OnOK: each field must scan as a float, else beep + focus it.
	double values[3];
	QLineEdit *edits[3];
	edits[0] = m_editX;
	edits[1] = m_editY;
	edits[2] = m_editZ;
	for (int i = 0; i < 3; i++)
	{
		bool ok = false;
		values[i] = edits[i]->text().toDouble(&ok);
		if (!ok)
		{
			QApplication::beep();
			edits[i]->setFocus();
			edits[i]->selectAll();
			return;
		}
	}
	WBQtParamData_SetCoord(m_parameter, values[0], values[1], values[2]);
	QDialog::accept();
}

// ===================== WBQtObjectPickDialog =====================

WBQtObjectPickDialog::WBQtObjectPickDialog(void *parameter, QWidget *parent)
	: QDialog(parent),
	m_parameter(parameter)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle("Edit Object Parameter:");

	QVBoxLayout *root = new QVBoxLayout(this);

	m_tree = new QTreeWidget(this);
	m_tree->setHeaderHidden(true);
	WBQtTreeStyle::applyTreeLines(m_tree);
	root->addWidget(m_tree, 1);

	// Free-text override (map.ini objects WorldBuilder cannot parse yet); seeded with the
	// current value -- the MFC dialog left it empty, but seeding round-trips harmlessly and
	// shows what is being replaced.
	char current[kTextCap];
	current[0] = 0;
	WBQtParamData_GetString(m_parameter, current, sizeof(current));
	m_edit = new QLineEdit(QString::fromLocal8Bit(current), this);
	root->addWidget(m_edit);

	QHBoxLayout *buttons = new QHBoxLayout();
	buttons->addStretch(1);
	QPushButton *okButton = new QPushButton("OK", this);
	okButton->setDefault(true);
	buttons->addWidget(okButton);
	QPushButton *cancelButton = new QPushButton("Cancel", this);
	cancelButton->setAutoDefault(false);
	buttons->addWidget(cancelButton);
	root->addLayout(buttons);
	connect(okButton, SIGNAL(clicked()), this, SLOT(accept()));
	connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

	// == EditObjectParameter::OnInitDialog/addObject: [TEST/]side/editor-sorting/name.
	QHash<QString, QTreeWidgetItem *> folders;
	char name[kTextCap];
	char side[256];
	char sorting[256];
	int count = WBQtParamData_GetTemplateCount();
	for (int i = 0; i < count; i++)
	{
		name[0] = 0;
		side[0] = 0;
		sorting[0] = 0;
		int isTest = 0;
		if (WBQtParamData_GetTemplateInfo(i, name, sizeof(name), side, sizeof(side),
				sorting, sizeof(sorting), &isTest) == 0)
		{
			continue;
		}
		QStringList parts;
		if (isTest)
		{
			parts << "TEST";
		}
		parts << QString::fromLocal8Bit(side) << QString::fromLocal8Bit(sorting);
		QTreeWidgetItem *parentItem = NULL;
		QString key;
		for (int p = 0; p < parts.size(); p++)
		{
			key += parts[p];
			key += '/';
			QTreeWidgetItem *folder = folders.value(key, NULL);
			if (folder == NULL)
			{
				if (parentItem == NULL)
				{
					folder = new QTreeWidgetItem(m_tree, QStringList(parts[p]));
				}
				else
				{
					folder = new QTreeWidgetItem(parentItem, QStringList(parts[p]));
				}
				folder->setData(0, kLeafRole, 0);
				folders.insert(key, folder);
			}
			parentItem = folder;
		}
		QTreeWidgetItem *leaf = new QTreeWidgetItem(parentItem, QStringList(QString::fromLocal8Bit(name)));
		leaf->setData(0, kLeafRole, 1);
	}

	// == addObjectLists: the script-defined object lists under their own folder.
	QTreeWidgetItem *listsFolder = new QTreeWidgetItem(m_tree, QStringList("Object Lists"));
	listsFolder->setData(0, kLeafRole, 0);
	int listCount = WBQtParamData_GetObjectListCount();
	for (int i = 0; i < listCount; i++)
	{
		name[0] = 0;
		WBQtParamData_GetObjectList(i, name, sizeof(name));
		QTreeWidgetItem *leaf = new QTreeWidgetItem(listsFolder, QStringList(QString::fromLocal8Bit(name)));
		leaf->setData(0, kLeafRole, 1);
	}

	m_tree->sortItems(0, Qt::AscendingOrder);
	connect(m_tree, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
			this, SLOT(onCurrentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));

	resize(420, 560);
}

void WBQtObjectPickDialog::onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
	Q_UNUSED(previous);
	// == the TVN_SELCHANGED handler: only leaves feed the edit field.
	if (current != NULL && current->data(0, kLeafRole).toInt() == 1)
	{
		m_edit->setText(current->text(0));
	}
}

void WBQtObjectPickDialog::accept()
{
	// == EditObjectParameter::OnOK: the edit text wins; otherwise the selected leaf; a folder
	// or nothing selected beeps.
	QString text = m_edit->text();
	if (text.isEmpty())
	{
		QTreeWidgetItem *item = m_tree->currentItem();
		if (item == NULL || item->data(0, kLeafRole).toInt() != 1)
		{
			QApplication::beep();
			return;
		}
		text = item->text(0);
	}
	QByteArray textBytes = text.toLocal8Bit();
	WBQtParam_Store(m_parameter, textBytes.constData(), -1);
	QDialog::accept();
}

// ===================== WBQtGroupDialog =====================

WBQtGroupDialog::WBQtGroupDialog(void *scriptGroup, QWidget *parent)
	: QDialog(parent),
	m_scriptGroup(scriptGroup)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle("Edit Group");

	char name[kTextCap];
	name[0] = 0;
	int active = 0;
	int subroutine = 0;
	WBQtGroupData_Get(m_scriptGroup, name, sizeof(name), &active, &subroutine);

	QVBoxLayout *root = new QVBoxLayout(this);
	m_subroutineCheck = new QCheckBox("Group is Subroutine.", this);
	m_subroutineCheck->setChecked(subroutine != 0);
	root->addWidget(m_subroutineCheck);
	m_activeCheck = new QCheckBox("Group is Active.", this);
	m_activeCheck->setChecked(active != 0);
	root->addWidget(m_activeCheck);

	QGroupBox *nameBox = new QGroupBox("Group Name:", this);
	QVBoxLayout *nameLay = new QVBoxLayout(nameBox);
	m_nameEdit = new QLineEdit(QString::fromLocal8Bit(name), nameBox);
	nameLay->addWidget(m_nameEdit);
	root->addWidget(nameBox);

	QHBoxLayout *buttons = new QHBoxLayout();
	buttons->addStretch(1);
	QPushButton *okButton = new QPushButton("OK", this);
	okButton->setDefault(true);
	buttons->addWidget(okButton);
	QPushButton *cancelButton = new QPushButton("Cancel", this);
	cancelButton->setAutoDefault(false);
	buttons->addWidget(cancelButton);
	root->addLayout(buttons);
	connect(okButton, SIGNAL(clicked()), this, SLOT(accept()));
	connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

	m_nameEdit->setFocus();
	m_nameEdit->selectAll();
	resize(300, 170);
}

void WBQtGroupDialog::accept()
{
	QByteArray name = m_nameEdit->text().toLocal8Bit();
	WBQtGroupData_Set(m_scriptGroup, name.constData(),
		m_activeCheck->isChecked() ? 1 : 0, m_subroutineCheck->isChecked() ? 1 : 0);
	QDialog::accept();
}

// ===================== the modal entry points =====================

extern "C" int WBQtParam_Edit(void *parameter, const char *unitName)
{
	if (parameter == NULL)
	{
		return 0;
	}
	// Nested picker: parent to the active Qt modal (the condition/action editor), else the
	// main window. Qt ApplicationModal on the parent chain fences the viewport.
	QWidget *owner = WBQt_DialogParent();
	int editorKind = WBQtParamData_GetEditorKind(parameter);
	if (editorKind == WB_QT_PARAM_EDITOR_COORD)
	{
		WBQtCoordDialog dlg(parameter, owner);
		return (dlg.exec() == QDialog::Accepted) ? 1 : 0;
	}
	if (editorKind == WB_QT_PARAM_EDITOR_OBJECT)
	{
		WBQtObjectPickDialog dlg(parameter, owner);
		return (dlg.exec() == QDialog::Accepted) ? 1 : 0;
	}
	if (editorKind == WB_QT_PARAM_EDITOR_COLOR)
	{
		// == EditParameter::edit's CColorDialog branch (aarrggbb <-> display color; OK
		// forces full alpha).
		int argb = WBQtParamData_GetColor(parameter);
		QColor initial((argb >> 16) & 0xff, (argb >> 8) & 0xff, argb & 0xff);
		QColor picked = QColorDialog::getColor(initial, owner, "Color");
		if (!picked.isValid())
		{
			return 0;
		}
		WBQtParamData_SetColor(parameter,
			(0xff << 24) | (picked.red() << 16) | (picked.green() << 8) | picked.blue());
		return 1;
	}
	WBQtParamDialog dlg(parameter, unitName, owner);
	return (dlg.exec() == QDialog::Accepted) ? 1 : 0;
}

extern "C" int WBQtEditGroup_Run(void *scriptGroup, void * /*frameHwnd*/)
{
	if (scriptGroup == NULL)
	{
		return 0;
	}
	// Popped from the (modeless) Qt Script window: parent to it (else the main window).
	// Qt ApplicationModal fences the viewport; no EnableWindow(frame) needed (phase 3).
	QWidget *owner = WBQtScriptWindow::instance();
	if (owner == NULL || !owner->isVisible())
	{
		owner = WBQt_DialogParent();
	}
	WBQtGroupDialog dlg(scriptGroup, owner);
	dlg.setWindowModality(Qt::ApplicationModal);
	int rc = dlg.exec();
	return (rc == QDialog::Accepted) ? 1 : 0;
}
