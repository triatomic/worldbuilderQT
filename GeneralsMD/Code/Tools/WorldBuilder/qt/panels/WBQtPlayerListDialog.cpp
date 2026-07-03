// WBQtPlayerListDialog.cpp -- see WBQtPlayerListDialog.h. Layout mirrors IDD_PLAYERLIST; the
// dialog re-reads its whole state from the hidden MFC dialog after every action (all bridge
// calls are synchronous, so no push mechanism is needed).
#include "WBQtPlayerListDialog.h"
#include "WBQtPlayerListBridge.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <qt_windows.h>

// Stage 1 phase 3: modal-dialog parent (active modal if nested, else main window). WBQtBridge.cpp.
QWidget *WBQt_DialogParent(void);

namespace
{
	const int kTextCap = 1024;

	QString bridgeStr(void (*getter)(char *, int))
	{
		char buf[kTextCap];
		buf[0] = 0;
		getter(buf, sizeof(buf));
		return QString::fromLocal8Bit(buf);
	}

	QString bridgeStrIdx(void (*getter)(int, char *, int), int i)
	{
		char buf[kTextCap];
		buf[0] = 0;
		getter(i, buf, sizeof(buf));
		return QString::fromLocal8Bit(buf);
	}
}

WBQtPlayerListDialog::WBQtPlayerListDialog(QWidget *parent)
	: QDialog(parent),
	m_updating(false)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle("Player List");

	QVBoxLayout *root = new QVBoxLayout(this);

	m_players = new QListWidget(this);
	m_players->setMinimumHeight(120);
	root->addWidget(m_players);

	// Player Name group (edit + Set Name; renames run team fixups, hence the explicit button)
	// and the identity row.
	QHBoxLayout *identityRow = new QHBoxLayout();
	QGroupBox *nameBox = new QGroupBox("Player Name", this);
	QHBoxLayout *nameLay = new QHBoxLayout(nameBox);
	m_nameEdit = new QLineEdit(nameBox);
	nameLay->addWidget(m_nameEdit, 1);
	QPushButton *setNameButton = new QPushButton("Set Name", nameBox);
	setNameButton->setAutoDefault(false);
	nameLay->addWidget(setNameButton);
	identityRow->addWidget(nameBox, 1);

	m_isComputerCheck = new QCheckBox("Is Player computer-controlled?", this);
	identityRow->addWidget(m_isComputerCheck);
	root->addLayout(identityRow);

	QHBoxLayout *displayRow = new QHBoxLayout();
	displayRow->addWidget(new QLabel("Player Display Name:", this));
	m_displayNameEdit = new QLineEdit(this);
	displayRow->addWidget(m_displayNameEdit, 1);
	QGroupBox *factionBox = new QGroupBox("Faction", this);
	QHBoxLayout *factionLay = new QHBoxLayout(factionBox);
	m_factionCombo = new QComboBox(factionBox);
	factionLay->addWidget(m_factionCombo);
	displayRow->addWidget(factionBox);
	root->addLayout(displayRow);

	QHBoxLayout *colorRow = new QHBoxLayout();
	m_colorButton = new QPushButton(this);
	m_colorButton->setFixedSize(28, 20);
	m_colorButton->setAutoDefault(false);
	colorRow->addWidget(m_colorButton);
	colorRow->addWidget(new QLabel("Color:", this));
	m_colorCombo = new QComboBox(this);
	colorRow->addWidget(m_colorCombo, 1);
	root->addLayout(colorRow);

	QGridLayout *relations = new QGridLayout();
	relations->addWidget(new QLabel("Allies:", this), 0, 0);
	relations->addWidget(new QLabel("Enemies:", this), 0, 1);
	m_allies = new QListWidget(this);
	m_allies->setSelectionMode(QAbstractItemView::MultiSelection);
	relations->addWidget(m_allies, 1, 0);
	m_enemies = new QListWidget(this);
	m_enemies->setSelectionMode(QAbstractItemView::MultiSelection);
	relations->addWidget(m_enemies, 1, 1);
	relations->addWidget(new QLabel("How Player Regard Others:", this), 2, 0);
	relations->addWidget(new QLabel("How Others Regard Player:", this), 2, 1);
	m_regardOut = new QListWidget(this);
	m_regardOut->setSelectionMode(QAbstractItemView::NoSelection);
	relations->addWidget(m_regardOut, 3, 0);
	m_regardIn = new QListWidget(this);
	m_regardIn->setSelectionMode(QAbstractItemView::NoSelection);
	relations->addWidget(m_regardIn, 3, 1);
	root->addLayout(relations, 1);

	QHBoxLayout *buttons = new QHBoxLayout();
	m_newButton = new QPushButton("New Player", this);
	m_newButton->setAutoDefault(false);
	buttons->addWidget(m_newButton);
	m_removeButton = new QPushButton("Remove Player", this);
	m_removeButton->setAutoDefault(false);
	buttons->addWidget(m_removeButton);
	QPushButton *skirmishButton = new QPushButton("Add Skirmish Players", this);
	skirmishButton->setAutoDefault(false);
	buttons->addWidget(skirmishButton);
	buttons->addStretch(1);
	QPushButton *okButton = new QPushButton("OK", this);
	okButton->setDefault(true);
	buttons->addWidget(okButton);
	QPushButton *cancelButton = new QPushButton("Cancel", this);
	cancelButton->setAutoDefault(false);
	buttons->addWidget(cancelButton);
	root->addLayout(buttons);

	connect(m_players, SIGNAL(currentRowChanged(int)), this, SLOT(onPlayerRowChanged(int)));
	connect(setNameButton, SIGNAL(clicked()), this, SLOT(onSetName()));
	connect(m_displayNameEdit, SIGNAL(textEdited(QString)), this, SLOT(onDisplayNameEdited(QString)));
	connect(m_isComputerCheck, SIGNAL(toggled(bool)), this, SLOT(onIsComputerToggled(bool)));
	connect(m_factionCombo, SIGNAL(activated(int)), this, SLOT(onFactionChanged(int)));
	connect(m_colorCombo, SIGNAL(activated(int)), this, SLOT(onColorComboChanged(int)));
	connect(m_colorButton, SIGNAL(clicked()), this, SLOT(onColorButton()));
	connect(m_allies, SIGNAL(itemSelectionChanged()), this, SLOT(onAlliesChanged()));
	connect(m_enemies, SIGNAL(itemSelectionChanged()), this, SLOT(onEnemiesChanged()));
	connect(m_newButton, SIGNAL(clicked()), this, SLOT(onNewPlayer()));
	connect(m_removeButton, SIGNAL(clicked()), this, SLOT(onRemovePlayer()));
	connect(skirmishButton, SIGNAL(clicked()), this, SLOT(onAddSkirmishPlayers()));
	connect(okButton, SIGNAL(clicked()), this, SLOT(accept()));
	connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

	refreshAll();
	resize(460, 620);
}

void WBQtPlayerListDialog::refreshAll()
{
	m_updating = true;

	// player list
	int cur = WBQtPlayerListData_GetCurPlayer();
	m_players->clear();
	int count = WBQtPlayerListData_GetPlayerCount();
	for (int i = 0; i < count; i++)
	{
		new QListWidgetItem(bridgeStrIdx(WBQtPlayerListData_GetPlayerLabel, i), m_players);
	}
	if (cur >= 0 && cur < count)
	{
		m_players->setCurrentRow(cur);
	}

	// identity
	bool nameEditable = (WBQtPlayerListData_IsNameEditable() != 0);
	QString name = bridgeStr(WBQtPlayerListData_GetName);
	if (m_nameEdit->text() != name)
	{
		m_nameEdit->setText(name);
	}
	m_nameEdit->setEnabled(nameEditable);
	QString displayName = bridgeStr(WBQtPlayerListData_GetDisplayName);
	if (m_displayNameEdit->text() != displayName)
	{
		m_displayNameEdit->setText(displayName);
	}
	m_displayNameEdit->setEnabled(nameEditable);
	m_isComputerCheck->setChecked(WBQtPlayerListData_IsComputer() != 0);
	m_isComputerCheck->setEnabled(nameEditable);

	// faction
	m_factionCombo->clear();
	int factionCount = WBQtPlayerListData_GetFactionCount();
	for (int i = 0; i < factionCount; i++)
	{
		m_factionCombo->addItem(bridgeStrIdx(WBQtPlayerListData_GetFactionName, i));
	}
	m_factionCombo->setCurrentIndex(WBQtPlayerListData_GetFactionIndex());

	// color
	int rgb = WBQtPlayerListData_GetColorRGB();
	m_colorButton->setStyleSheet(QString("background-color: #%1;").arg(rgb & 0xffffff, 6, 16, QChar('0')));
	m_colorCombo->clear();
	int colorCount = WBQtPlayerListData_GetColorCount();
	for (int i = 0; i < colorCount; i++)
	{
		m_colorCombo->addItem(bridgeStrIdx(WBQtPlayerListData_GetColorName, i));
	}
	m_colorCombo->setCurrentIndex(WBQtPlayerListData_GetColorIndex());

	// allies / enemies (same row space as the hidden sorted listboxes)
	bool relationsEnabled = (WBQtPlayerListData_RelationsEnabled() != 0);
	m_allies->clear();
	m_enemies->clear();
	int otherCount = WBQtPlayerListData_GetOtherCount();
	for (int i = 0; i < otherCount; i++)
	{
		QString other = bridgeStrIdx(WBQtPlayerListData_GetOtherName, i);
		QListWidgetItem *ally = new QListWidgetItem(other, m_allies);
		ally->setSelected(WBQtPlayerListData_GetAllySel(i) != 0);
		QListWidgetItem *enemy = new QListWidgetItem(other, m_enemies);
		enemy->setSelected(WBQtPlayerListData_GetEnemySel(i) != 0);
	}
	m_allies->setEnabled(relationsEnabled);
	m_enemies->setEnabled(relationsEnabled);

	// regard summaries
	m_regardOut->clear();
	m_regardIn->clear();
	int outCount = WBQtPlayerListData_GetRegardCount(0);
	for (int i = 0; i < outCount; i++)
	{
		char buf[kTextCap];
		buf[0] = 0;
		WBQtPlayerListData_GetRegardLine(0, i, buf, sizeof(buf));
		new QListWidgetItem(QString::fromLocal8Bit(buf), m_regardOut);
	}
	int inCount = WBQtPlayerListData_GetRegardCount(1);
	for (int i = 0; i < inCount; i++)
	{
		char buf[kTextCap];
		buf[0] = 0;
		WBQtPlayerListData_GetRegardLine(1, i, buf, sizeof(buf));
		new QListWidgetItem(QString::fromLocal8Bit(buf), m_regardIn);
	}

	m_newButton->setEnabled(WBQtPlayerListData_CanAddPlayer() != 0);
	m_removeButton->setEnabled(WBQtPlayerListData_GetRemoveEnabled() != 0);

	m_updating = false;
}

QString WBQtPlayerListDialog::relationMask(QListWidget *list) const
{
	QString mask;
	for (int i = 0; i < list->count(); i++)
	{
		mask += list->item(i)->isSelected() ? '1' : '0';
	}
	return mask;
}

void WBQtPlayerListDialog::onPlayerRowChanged(int row)
{
	if (m_updating || row < 0)
	{
		return;
	}
	WBQtPlayerList_SelectPlayer(row);
	refreshAll();
}

void WBQtPlayerListDialog::onSetName()
{
	if (m_updating)
	{
		return;
	}
	QByteArray name = m_nameEdit->text().toLocal8Bit();
	WBQtPlayerList_SetName(name.constData());	// may pop the name-in-use warning
	refreshAll();
}

void WBQtPlayerListDialog::onDisplayNameEdited(const QString &text)
{
	if (m_updating)
	{
		return;
	}
	QByteArray name = text.toLocal8Bit();
	WBQtPlayerList_SetDisplayName(name.constData());
	// No full refresh while typing (it would rebuild the player list on every keystroke);
	// just keep the player-list label in sync.
	m_updating = true;
	int cur = WBQtPlayerListData_GetCurPlayer();
	if (cur >= 0 && cur < m_players->count())
	{
		m_players->item(cur)->setText(bridgeStrIdx(WBQtPlayerListData_GetPlayerLabel, cur));
	}
	m_updating = false;
}

void WBQtPlayerListDialog::onIsComputerToggled(bool checked)
{
	if (m_updating)
	{
		return;
	}
	WBQtPlayerList_SetIsComputer(checked ? 1 : 0);
	refreshAll();
}

void WBQtPlayerListDialog::onFactionChanged(int index)
{
	if (m_updating || index < 0)
	{
		return;
	}
	QByteArray name = m_factionCombo->itemText(index).toLocal8Bit();
	WBQtPlayerList_SetFaction(name.constData());
	refreshAll();
}

void WBQtPlayerListDialog::onColorComboChanged(int index)
{
	if (m_updating || index < 0)
	{
		return;
	}
	WBQtPlayerList_SetColorIndex(index);
	refreshAll();
}

void WBQtPlayerListDialog::onColorButton()
{
	int rgb = WBQtPlayerListData_GetColorRGB();
	QColor initial((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
	QColor picked = QColorDialog::getColor(initial, this, "Color");
	if (!picked.isValid())
	{
		return;
	}
	WBQtPlayerList_SetColorRGB((picked.red() << 16) | (picked.green() << 8) | picked.blue());
	refreshAll();
}

void WBQtPlayerListDialog::onAlliesChanged()
{
	if (m_updating)
	{
		return;
	}
	QByteArray mask = relationMask(m_allies).toLatin1();
	WBQtPlayerList_SetRelations(0, mask.constData());
	refreshAll();	// the handler dedups enemies against allies; re-read both
}

void WBQtPlayerListDialog::onEnemiesChanged()
{
	if (m_updating)
	{
		return;
	}
	QByteArray mask = relationMask(m_enemies).toLatin1();
	WBQtPlayerList_SetRelations(1, mask.constData());
	refreshAll();
}

void WBQtPlayerListDialog::onNewPlayer()
{
	if (WBQtPlayerListData_CanAddPlayer() == 0)
	{
		return;
	}
	WBQtAddPlayerDialog dlg(this);
	if (dlg.exec() != QDialog::Accepted || dlg.addedTemplate().isEmpty())
	{
		return;
	}
	QByteArray name = dlg.addedTemplate().toLocal8Bit();
	WBQtPlayerList_NewPlayer(name.constData());
	refreshAll();
}

void WBQtPlayerListDialog::onRemovePlayer()
{
	WBQtPlayerList_RemovePlayer();	// may pop the in-use confirmation
	refreshAll();
}

void WBQtPlayerListDialog::onAddSkirmishPlayers()
{
	WBQtPlayerList_AddSkirmishPlayers();
	refreshAll();
}

// ===================== WBQtAddPlayerDialog =====================

WBQtAddPlayerDialog::WBQtAddPlayerDialog(QWidget *parent, const QString &onlySide)
	: QDialog(parent)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle("Add A Player");

	QVBoxLayout *root = new QVBoxLayout(this);
	QLabel *label = new QLabel("Which of the following PlayerTemplates shall I use to add the Player you need?", this);
	label->setWordWrap(true);
	root->addWidget(label);

	m_templates = new QComboBox(this);
	QStringList names;
	int count = WBQtAddPlayerData_GetTemplateCount();
	for (int i = 0; i < count; i++)
	{
		char buf[512];
		buf[0] = 0;
		WBQtAddPlayerData_GetTemplateName(i, buf, sizeof(buf));
		if (buf[0] == 0)
		{
			continue;
		}
		if (!onlySide.isEmpty())
		{
			// == AddPlayerDialog's m_side filter (exact match, like AsciiString ==).
			char side[512];
			side[0] = 0;
			WBQtAddPlayerData_GetTemplateSide(i, side, sizeof(side));
			if (onlySide != QString::fromLocal8Bit(side))
			{
				continue;
			}
		}
		names.append(QString::fromLocal8Bit(buf));
	}
	names.sort(Qt::CaseInsensitive);	// == the CBS_SORT combo
	m_templates->addItems(names);
	m_templates->setCurrentIndex(0);
	root->addWidget(m_templates);

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

	resize(300, 140);
}

void WBQtAddPlayerDialog::accept()
{
	QString name = m_templates->currentText();
	QByteArray nameBytes = name.toLocal8Bit();
	// == AddPlayerDialog::OnOK (its immediate global add is a preserved quirk; the Player
	// List's own commit supersedes it on OK).
	if (WBQtAddPlayer_Commit(nameBytes.constData()) != 0)
	{
		m_addedTemplate = name;
	}
	QDialog::accept();
}

// ===================== the modal entry point =====================

extern "C" int WBQtPlayerList_Run(void * /*frameHwnd*/)
{
	WBQtPlayerListData_Open();
	// Stage 1 phase 3: parent to the main window + Qt ApplicationModal (fences the viewport).
	WBQtPlayerListDialog dlg(WBQt_DialogParent());
	dlg.setWindowModality(Qt::ApplicationModal);
	int rc = (dlg.exec() == QDialog::Accepted) ? 1 : 0;
	WBQtPlayerListData_Close(rc);
	return rc;
}

// Tier 5b: the standalone Add-Player run for the object-placement auto-add flow
// (ObjectOptions.cpp). Nesting-safe -- placement can happen while other windows are up, so
// the parent is the active modal if one exists, else the main window.
extern "C" int WBQtAddPlayer_Run(void * /*frameHwnd*/, const char *onlySide, char *addedOut, int cap)
{
	if (addedOut != NULL && cap > 0)
	{
		addedOut[0] = 0;
	}
	if (qApp == NULL)
	{
		return -1;	// pre-Qt startup -- the caller falls back to the MFC dialog
	}
	WBQtAddPlayerDialog dlg(WBQt_DialogParent(),
		QString::fromLocal8Bit((onlySide != NULL) ? onlySide : ""));
	dlg.setWindowModality(Qt::ApplicationModal);
	int rc = dlg.exec();
	if (rc != QDialog::Accepted || dlg.addedTemplate().isEmpty())
	{
		return 0;
	}
	QByteArray added = dlg.addedTemplate().toLocal8Bit();
	if (addedOut != NULL && cap > 0)
	{
		int n = added.size();
		if (n > cap - 1)
		{
			n = cap - 1;
		}
		memcpy(addedOut, added.constData(), n);
		addedOut[n] = 0;
	}
	return 1;
}
