// WBQtPlayerListDialog.cpp -- see WBQtPlayerListDialog.h. Layout mirrors IDD_PLAYERLIST; the
// dialog re-reads its whole state from the hidden MFC dialog after every action (all bridge
// calls are synchronous, so no push mechanism is needed).
#include "WBQtPlayerListDialog.h"
#include "ui_WBQtPlayerListDialog.h"
#include "ui_WBQtAddPlayerDialog.h"
#include "WBQtComboStyle.h"
#include "WBQtPlayerListBridge.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

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
	m_ui(new Ui::WBQtPlayerListDialog),
	m_updating(false)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// The static widget tree lives in WBQtPlayerListDialog.ui; bind the members the
	// logic below uses.
	m_ui->setupUi(this);

	m_players = m_ui->players;
	m_nameEdit = m_ui->nameEdit;
	m_displayNameEdit = m_ui->displayNameEdit;
	m_isComputerCheck = m_ui->isComputerCheck;
	m_factionCombo = m_ui->factionCombo;
	m_colorButton = m_ui->colorButton;
	m_colorCombo = m_ui->colorCombo;
	m_allies = m_ui->allies;
	m_enemies = m_ui->enemies;
	m_regardOut = m_ui->regardOut;
	m_regardIn = m_ui->regardIn;
	m_newButton = m_ui->newButton;
	m_removeButton = m_ui->removeButton;

	// == the MFC IDC_PLAYERFACTION (CBS_DROPDOWN): the faction combo is typable, so free text
	// that matches no list entry can still be committed. applyTypeToFilter() makes it editable
	// (and narrows the popup as you type); the rest of the dialog's combos stay pick-only.
	WBQtComboStyle::applyTypeToFilter(m_factionCombo);

	// MFC's combos are WS_VSCROLL: give every drop-down here a scrolling popup.
	WBQtComboStyle::applyPopupScrollRecursive(this);

	connect(m_players, SIGNAL(currentRowChanged(int)), this, SLOT(onPlayerRowChanged(int)));
	// Set Name is an explicit button because renames run team fixups.
	connect(m_ui->setNameButton, SIGNAL(clicked()), this, SLOT(onSetName()));
	connect(m_displayNameEdit, SIGNAL(textEdited(QString)), this, SLOT(onDisplayNameEdited(QString)));
	connect(m_isComputerCheck, SIGNAL(toggled(bool)), this, SLOT(onIsComputerToggled(bool)));
	connect(m_factionCombo, SIGNAL(activated(int)), this, SLOT(onFactionChanged(int)));
	// == MFC's ON_CBN_EDITCHANGE(IDC_PLAYERFACTION): hand-typed text matches no list entry, so
	// activated(int) never fires; commit it (the OnEditchangePlayerfaction sel==-1 path) when the
	// edit finishes instead.
	if (m_factionCombo->lineEdit() != NULL)
	{
		connect(m_factionCombo->lineEdit(), SIGNAL(editingFinished()), this, SLOT(onFactionTextCommitted()));
	}
	connect(m_colorCombo, SIGNAL(activated(int)), this, SLOT(onColorComboChanged(int)));
	connect(m_colorButton, SIGNAL(clicked()), this, SLOT(onColorButton()));
	connect(m_allies, SIGNAL(itemSelectionChanged()), this, SLOT(onAlliesChanged()));
	connect(m_enemies, SIGNAL(itemSelectionChanged()), this, SLOT(onEnemiesChanged()));
	connect(m_newButton, SIGNAL(clicked()), this, SLOT(onNewPlayer()));
	connect(m_removeButton, SIGNAL(clicked()), this, SLOT(onRemovePlayer()));
	connect(m_ui->skirmishButton, SIGNAL(clicked()), this, SLOT(onAddSkirmishPlayers()));
	connect(m_ui->okButton, SIGNAL(clicked()), this, SLOT(accept()));
	connect(m_ui->cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

	refreshAll();
}

WBQtPlayerListDialog::~WBQtPlayerListDialog()
{
	delete m_ui;
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
	int factionIndex = WBQtPlayerListData_GetFactionIndex();
	m_factionCombo->setCurrentIndex(factionIndex);
	if (factionIndex < 0 && m_factionCombo->lineEdit() != NULL)
	{
		// Free-typed faction (matches no template): setCurrentIndex(-1) blanked the editable
		// combo's line edit, so restore the stored text -- == MFC's SetCurSel(-1) leaving the
		// combo edit-control text intact.
		m_factionCombo->setEditText(bridgeStr(WBQtPlayerListData_GetFactionText));
	}

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

void WBQtPlayerListDialog::onFactionTextCommitted()
{
	if (m_updating)
	{
		return;
	}
	// == OnEditchangePlayerfaction's sel==-1 branch: store the free-typed text verbatim. If the
	// text happens to match a list entry activated(int) already handled it, but committing the
	// same name again is harmless (the bridge just re-stores it).
	QByteArray name = m_factionCombo->currentText().toLocal8Bit();
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
	: QDialog(parent),
	m_ui(new Ui::WBQtAddPlayerDialog)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// The static widget tree lives in WBQtAddPlayerDialog.ui.
	m_ui->setupUi(this);

	m_templates = m_ui->templates;

	// MFC's combos are WS_VSCROLL: give every drop-down here a scrolling popup.
	WBQtComboStyle::applyPopupScrollRecursive(this);

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

	connect(m_ui->okButton, SIGNAL(clicked()), this, SLOT(accept()));
	connect(m_ui->cancelButton, SIGNAL(clicked()), this, SLOT(reject()));
}

WBQtAddPlayerDialog::~WBQtAddPlayerDialog()
{
	delete m_ui;
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
	if (rc != QDialog::Accepted)
	{
		return 0;	// cancelled
	}
	if (dlg.addedTemplate().isEmpty())
	{
		return 1;	// OK, but nothing could be added (empty addedOut; caller decides)
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
