// WBQtTeamSheetDialog.cpp -- see WBQtTeamSheetDialog.h. Tab layouts mirror IDD_TeamIdentity /
// IDD_TeamReinforcement / IDD_TeamBehavior / IDD_TeamGeneric. Control IDs come from the WB
// resource.h (pure #defines, Qt-safe; the res dir is on the qt lib include path).
#include "WBQtTeamSheetDialog.h"
#include "ui_WBQtTeamSheetDialog.h"
#include "WBQtTeamsBridge.h"
#include "resource.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace
{
	const int kTextCap = 1024;

	QString pageText(int page, int ctrlId)
	{
		char buf[kTextCap];
		buf[0] = 0;
		WBQtTeamPage_GetText(page, ctrlId, buf, sizeof(buf));
		return QString::fromLocal8Bit(buf);
	}

	// Ensure the combo's current (possibly "[???] ..." placeholder) text is selectable.
	void seedComboCurrent(QComboBox *combo, const QString &current)
	{
		if (!current.isEmpty() && combo->findText(current) < 0)
		{
			combo->addItem(current);
		}
		combo->setCurrentIndex(combo->findText(current));
	}
}

WBQtTeamSheetDialog::WBQtTeamSheetDialog(QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtTeamSheetDialog),
	m_nameEdit(NULL)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// The static widget tree (tabs, group boxes, labels, edits, checks, combos) lives in
	// WBQtTeamSheetDialog.ui; the setup*Tab calls wire each control to its hidden MFC page
	// and populate the dynamic member/script rows.
	m_ui->setupUi(this);

	setupIdentityTab();
	setupReinforcementTab();
	setupBehaviorTab();
	setupGenericTab();

	// Edits apply live to the Teams dialog's working copy (== the MFC sheet, whose OK/Cancel
	// result was ignored); a single OK just closes.
	connect(m_ui->okButton, SIGNAL(clicked()), this, SLOT(accept()));

	// Restore the last session's size (persisted in done()); layout minimums keep a
	// nonsense stored value from collapsing the sheet.
	resize(WBQtTeamSheet_GetProfileInt("TeamSheetWidth", 760),
		WBQtTeamSheet_GetProfileInt("TeamSheetHeight", 620));
}

WBQtTeamSheetDialog::~WBQtTeamSheetDialog()
{
	delete m_ui;
}

void WBQtTeamSheetDialog::done(int r)
{
	// One write per close (not per resize tick -- profile writes hit WorldBuilder.ini).
	WBQtTeamSheet_SetProfileInt("TeamSheetWidth", width());
	WBQtTeamSheet_SetProfileInt("TeamSheetHeight", height());
	QDialog::done(r);
}

QStringList WBQtTeamSheetDialog::readComboItems(int page, int ctrlId) const
{
	QStringList items;
	char buf[kTextCap];
	int count = WBQtTeamPage_ComboCount(page, ctrlId);
	for (int i = 0; i < count; i++)
	{
		buf[0] = 0;
		WBQtTeamPage_ComboItem(page, ctrlId, i, buf, sizeof(buf));
		items.append(QString::fromLocal8Bit(buf));
	}
	return items;
}

QLineEdit *WBQtTeamSheetDialog::bindEdit(int page, int ctrlId, QWidget *parent, int notify)
{
	QLineEdit *edit = new QLineEdit(parent);
	bindEdit(page, ctrlId, edit, notify);
	return edit;
}

void WBQtTeamSheetDialog::bindEdit(int page, int ctrlId, QLineEdit *edit, int notify)
{
	edit->setText(pageText(page, ctrlId));
	connect(edit, &QLineEdit::editingFinished, edit, [edit, page, ctrlId, notify]()
	{
		QByteArray text = edit->text().toLocal8Bit();
		WBQtTeamPage_SetText(page, ctrlId, text.constData(), notify);
		// The handler may reject/normalize (the team-name rename validation); read back.
		QString stored = pageText(page, ctrlId);
		if (stored != edit->text())
		{
			edit->setText(stored);
		}
	});
}

void WBQtTeamSheetDialog::bindCheck(int page, int ctrlId, QCheckBox *check)
{
	check->setChecked(WBQtTeamPage_GetCheck(page, ctrlId) != 0);
	check->setEnabled(WBQtTeamPage_IsEnabled(page, ctrlId) != 0);
	connect(check, &QCheckBox::toggled, check, [page, ctrlId](bool on)
	{
		WBQtTeamPage_SetCheck(page, ctrlId, on ? 1 : 0);
	});
}

QComboBox *WBQtTeamSheetDialog::bindCombo(int page, int ctrlId, const QStringList &items, QWidget *parent, int notify)
{
	QComboBox *combo = new QComboBox(parent);
	bindCombo(page, ctrlId, items, combo, notify);
	return combo;
}

void WBQtTeamSheetDialog::bindCombo(int page, int ctrlId, const QStringList &items, QComboBox *combo, int notify)
{
	// A QComboBox defaults to sizing itself to its WIDEST item, measuring every entry on
	// first show -- the seven template-catalog combos plus 22 script combos here made
	// opening the sheet crawl compared to the MFC original (which never measures). Cap
	// the width like the Object Properties combos.
	combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	combo->setMinimumContentsLength(24);
	combo->addItems(items);
	seedComboCurrent(combo, pageText(page, ctrlId));
	connect(combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), combo,
		[combo, page, ctrlId, notify](int index)
	{
		QByteArray text = combo->itemText(index).toLocal8Bit();
		WBQtTeamPage_ComboSelectText(page, ctrlId, text.constData(), notify);
	});
}

void WBQtTeamSheetDialog::setupIdentityTab()
{
	const int page = WB_QT_TEAMPAGE_IDENTITY;

	// name + max quantity row
	m_nameEdit = m_ui->nameEdit;
	bindEdit(page, IDC_TEAM_NAME, m_ui->nameEdit, WB_QT_TEAMNOTIFY_KILLFOCUS);
	bindEdit(page, IDC_MAX, m_ui->maxEdit, WB_QT_TEAMNOTIFY_CHANGE);

	// owner / home position
	bindCombo(page, IDC_TEAMOWNER, readComboItems(page, IDC_TEAMOWNER), m_ui->ownerCombo, WB_QT_TEAMNOTIFY_SELENDOK);
	bindCombo(page, IDC_HOME_WAYPOINT, readComboItems(page, IDC_HOME_WAYPOINT), m_ui->homeWaypointCombo, WB_QT_TEAMNOTIFY_SELCHANGE);

	// production box
	bindCombo(page, IDC_PRODUCTION_CONDITION, readComboItems(page, IDC_PRODUCTION_CONDITION), m_ui->productionConditionCombo, WB_QT_TEAMNOTIFY_SELCHANGE);
	bindCheck(page, IDC_PRODUCTION_EXECUTEACTIONS, m_ui->executeActionsCheck);
	bindEdit(page, IDC_PRODUCTION_PRIORITY, m_ui->priorityEdit, WB_QT_TEAMNOTIFY_CHANGE);
	bindEdit(page, IDC_PRIORITY_INCREASE, m_ui->increaseEdit, WB_QT_TEAMNOTIFY_CHANGE);
	bindEdit(page, IDC_TEAM_BUILD_FRAMES, m_ui->framesEdit, WB_QT_TEAMNOTIFY_CHANGE);
	bindEdit(page, IDC_PRIORITY_DECREASE, m_ui->decreaseEdit, WB_QT_TEAMNOTIFY_CHANGE);

	// members: 7 unit rows. The seven hidden combos share the template catalog, so enumerate
	// slot 1 ONCE (the per-slot "[???]" placeholder is folded in by seedComboCurrent).
	static const int minIds[7] = { IDC_MIN_UNIT1, IDC_MIN_UNIT2, IDC_MIN_UNIT3, IDC_MIN_UNIT4, IDC_MIN_UNIT5, IDC_MIN_UNIT6, IDC_MIN_UNIT7 };
	static const int maxIds[7] = { IDC_MAX_UNIT1, IDC_MAX_UNIT2, IDC_MAX_UNIT3, IDC_MAX_UNIT4, IDC_MAX_UNIT5, IDC_MAX_UNIT6, IDC_MAX_UNIT7 };
	static const int typeIds[7] = { IDC_UNIT_TYPE1, IDC_UNIT_TYPE2, IDC_UNIT_TYPE3, IDC_UNIT_TYPE4, IDC_UNIT_TYPE5, IDC_UNIT_TYPE6, IDC_UNIT_TYPE7 };
	static const int pickIds[7] = { IDC_UNIT_TYPE1_BUTTON, IDC_UNIT_TYPE2_BUTTON, IDC_UNIT_TYPE3_BUTTON, IDC_UNIT_TYPE4_BUTTON, IDC_UNIT_TYPE5_BUTTON, IDC_UNIT_TYPE6_BUTTON, IDC_UNIT_TYPE7_BUTTON };

	QStringList unitItems = readComboItems(page, IDC_UNIT_TYPE1);
	for (int i = 0; i < 7; i++)
	{
		QLineEdit *minEdit = bindEdit(page, minIds[i], m_ui->membersBox, WB_QT_TEAMNOTIFY_CHANGE);
		minEdit->setFixedWidth(40);
		m_ui->membersGrid->addWidget(minEdit, i + 1, 0);
		QLineEdit *maxEdit2 = bindEdit(page, maxIds[i], m_ui->membersBox, WB_QT_TEAMNOTIFY_CHANGE);
		maxEdit2->setFixedWidth(40);
		m_ui->membersGrid->addWidget(maxEdit2, i + 1, 1);
		m_unitCombos[i] = bindCombo(page, typeIds[i], unitItems, m_ui->membersBox, WB_QT_TEAMNOTIFY_SELCHANGE);
		m_ui->membersGrid->addWidget(m_unitCombos[i], i + 1, 2);
		QPushButton *pickButton = new QPushButton("...", m_ui->membersBox);
		pickButton->setFixedWidth(28);
		pickButton->setAutoDefault(false);
		m_ui->membersGrid->addWidget(pickButton, i + 1, 3);
		QComboBox *combo = m_unitCombos[i];
		int typeId = typeIds[i];
		int pickId = pickIds[i];
		connect(pickButton, &QPushButton::clicked, pickButton, [combo, typeId, pickId, page]()
		{
			// Pops the (still MFC) PickUnitDialog; the page handler writes the dict and the
			// hidden combo -- mirror the result back.
			WBQtTeamPage_ClickButton(page, pickId);
			seedComboCurrent(combo, pageText(page, typeId));
		});
	}

	// recruitment + singleton + description
	bindCheck(page, IDC_AUTO_REINFORCE, m_ui->autoReinforceCheck);
	bindCheck(page, IDC_AI_RECRUITABLE, m_ui->aiRecruitableCheck);
	bindCheck(page, IDC_TEAM_SINGLETON, m_ui->singletonCheck);
	bindEdit(page, IDC_DESCRIPTION, m_ui->descriptionEdit, WB_QT_TEAMNOTIFY_CHANGE);
}

void WBQtTeamSheetDialog::setupReinforcementTab()
{
	const int page = WB_QT_TEAMPAGE_REINFORCEMENT;

	bindCheck(page, IDC_DEPLOY_BY, m_ui->deployByCheck);
	bindCombo(page, IDC_TRANSPORT_COMBO, readComboItems(page, IDC_TRANSPORT_COMBO), m_ui->transportCombo, WB_QT_TEAMNOTIFY_SELCHANGE);
	bindCheck(page, IDC_TRANSPORTS_EXIT, m_ui->transportsExitCheck);
	bindCombo(page, IDC_WAYPOINT_COMBO, readComboItems(page, IDC_WAYPOINT_COMBO), m_ui->waypointCombo, WB_QT_TEAMNOTIFY_SELCHANGE);
	bindCheck(page, IDC_TEAM_STARTS_FULL, m_ui->teamStartsFullCheck);
	bindCombo(page, IDC_VETERANCY, readComboItems(page, IDC_VETERANCY), m_ui->veterancyCombo, WB_QT_TEAMNOTIFY_SELCHANGE);
}

void WBQtTeamSheetDialog::setupBehaviorTab()
{
	const int page = WB_QT_TEAMPAGE_BEHAVIOR;

	// One representative script combo enumerates the (shared) subroutine-script list.
	QStringList scriptItems = readComboItems(page, IDC_ON_CREATE_SCRIPT);

	bindCombo(page, IDC_ON_CREATE_SCRIPT, scriptItems, m_ui->onCreateCombo, WB_QT_TEAMNOTIFY_SELCHANGE);
	bindCombo(page, IDC_ON_ENEMY_SIGHTED, scriptItems, m_ui->onEnemySightedCombo, WB_QT_TEAMNOTIFY_SELCHANGE);
	bindCombo(page, IDC_ON_ALL_CLEAR, scriptItems, m_ui->onAllClearCombo, WB_QT_TEAMNOTIFY_SELCHANGE);
	bindEdit(page, IDC_PERCENT_DESTROYED, m_ui->percentEdit, WB_QT_TEAMNOTIFY_CHANGE);
	bindCombo(page, IDC_ON_DESTROYED, scriptItems, m_ui->onDestroyedCombo, WB_QT_TEAMNOTIFY_SELCHANGE);
	bindCombo(page, IDC_ON_IDLE_SCRIPT, scriptItems, m_ui->onIdleCombo, WB_QT_TEAMNOTIFY_SELCHANGE);
	bindCombo(page, IDC_ON_UNIT_DESTROYED_SCRIPT, scriptItems, m_ui->onUnitDestroyedCombo, WB_QT_TEAMNOTIFY_SELCHANGE);

	bindCheck(page, IDC_TRANSPORTS_RETURN, m_ui->transportsReturnCheck);
	bindCheck(page, IDC_AVOID_THREATS, m_ui->avoidThreatsCheck);
	bindCombo(page, IDC_ENEMY_INTERACTIONS, readComboItems(page, IDC_ENEMY_INTERACTIONS), m_ui->enemyInteractionsCombo, WB_QT_TEAMNOTIFY_SELCHANGE);
	bindCheck(page, IDC_ATTACK_COMMON_TARGET, m_ui->attackCommonTargetCheck);
}

void WBQtTeamSheetDialog::setupGenericTab()
{
	const int page = WB_QT_TEAMPAGE_GENERIC;

	static const int scriptIds[16] = {
		IDC_TeamGeneric_Script1, IDC_TeamGeneric_Script2, IDC_TeamGeneric_Script3,
		IDC_TeamGeneric_Script4, IDC_TeamGeneric_Script5, IDC_TeamGeneric_Script6,
		IDC_TeamGeneric_Script7, IDC_TeamGeneric_Script8, IDC_TeamGeneric_Script9,
		IDC_TeamGeneric_Script10, IDC_TeamGeneric_Script11, IDC_TeamGeneric_Script12,
		IDC_TeamGeneric_Script13, IDC_TeamGeneric_Script14, IDC_TeamGeneric_Script15,
		IDC_TeamGeneric_Script16
	};

	// The 16 combos share the subroutine-script list; enumerate slot 1 once.
	QStringList scriptItems = readComboItems(page, IDC_TeamGeneric_Script1);
	for (int i = 0; i < 16; i++)
	{
		m_ui->genericGrid->addWidget(new QLabel(QString("Script %1:").arg(i + 1), m_ui->genericTab), i, 0);
		m_ui->genericGrid->addWidget(bindCombo(page, scriptIds[i], scriptItems, m_ui->genericTab, WB_QT_TEAMNOTIFY_SELCHANGE), i, 1);
	}
	m_ui->genericGrid->setColumnStretch(1, 1);
	m_ui->genericGrid->setRowStretch(16, 1);
}
