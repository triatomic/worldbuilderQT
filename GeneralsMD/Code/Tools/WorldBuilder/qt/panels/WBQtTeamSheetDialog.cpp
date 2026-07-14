// WBQtTeamSheetDialog.cpp -- see WBQtTeamSheetDialog.h. Tab layouts mirror IDD_TeamIdentity /
// IDD_TeamReinforcement / IDD_TeamBehavior / IDD_TeamGeneric. Control IDs come from the WB
// resource.h (pure #defines, Qt-safe; the res dir is on the qt lib include path).
#include "WBQtTeamSheetDialog.h"
#include "WBQtTeamsBridge.h"
#include "resource.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

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
	m_nameEdit(NULL)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle(QString("Edit Team Template."));

	QVBoxLayout *root = new QVBoxLayout(this);
	QTabWidget *tabs = new QTabWidget(this);
	tabs->addTab(buildIdentityTab(), "Identity");
	tabs->addTab(buildReinforcementTab(), "Reinforcement");
	tabs->addTab(buildBehaviorTab(), "Behavior");
	tabs->addTab(buildGenericTab(), "Generic");
	root->addWidget(tabs, 1);

	QHBoxLayout *buttons = new QHBoxLayout();
	buttons->addStretch(1);
	// Edits apply live to the Teams dialog's working copy (== the MFC sheet, whose OK/Cancel
	// result was ignored); a single OK just closes.
	QPushButton *okButton = new QPushButton("OK", this);
	okButton->setDefault(true);
	buttons->addWidget(okButton);
	root->addLayout(buttons);
	connect(okButton, SIGNAL(clicked()), this, SLOT(accept()));

	// Restore the last session's size (persisted in done()); layout minimums keep a
	// nonsense stored value from collapsing the sheet.
	resize(WBQtTeamSheet_GetProfileInt("TeamSheetWidth", 760),
		WBQtTeamSheet_GetProfileInt("TeamSheetHeight", 620));
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
	QLineEdit *edit = new QLineEdit(pageText(page, ctrlId), parent);
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
	return edit;
}

QCheckBox *WBQtTeamSheetDialog::bindCheck(int page, int ctrlId, const QString &label, QWidget *parent)
{
	QCheckBox *check = new QCheckBox(label, parent);
	check->setChecked(WBQtTeamPage_GetCheck(page, ctrlId) != 0);
	check->setEnabled(WBQtTeamPage_IsEnabled(page, ctrlId) != 0);
	connect(check, &QCheckBox::toggled, check, [page, ctrlId](bool on)
	{
		WBQtTeamPage_SetCheck(page, ctrlId, on ? 1 : 0);
	});
	return check;
}

QComboBox *WBQtTeamSheetDialog::bindCombo(int page, int ctrlId, const QStringList &items, QWidget *parent, int notify)
{
	QComboBox *combo = new QComboBox(parent);
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
	return combo;
}

QWidget *WBQtTeamSheetDialog::buildIdentityTab()
{
	const int page = WB_QT_TEAMPAGE_IDENTITY;
	QWidget *tab = new QWidget(this);
	QVBoxLayout *root = new QVBoxLayout(tab);

	// name + max quantity row
	QHBoxLayout *nameRow = new QHBoxLayout();
	m_nameEdit = bindEdit(page, IDC_TEAM_NAME, tab, WB_QT_TEAMNOTIFY_KILLFOCUS);
	nameRow->addWidget(m_nameEdit, 1);
	nameRow->addWidget(new QLabel("Maximum Quantity:", tab));
	QLineEdit *maxEdit = bindEdit(page, IDC_MAX, tab, WB_QT_TEAMNOTIFY_CHANGE);
	maxEdit->setFixedWidth(50);
	nameRow->addWidget(maxEdit);
	root->addLayout(nameRow);

	// owner / home position
	QGridLayout *identityGrid = new QGridLayout();
	identityGrid->addWidget(new QLabel("Owner:", tab), 0, 0);
	identityGrid->addWidget(bindCombo(page, IDC_TEAMOWNER, readComboItems(page, IDC_TEAMOWNER), tab, WB_QT_TEAMNOTIFY_SELENDOK), 0, 1);
	identityGrid->addWidget(new QLabel("Home Position:", tab), 1, 0);
	identityGrid->addWidget(bindCombo(page, IDC_HOME_WAYPOINT, readComboItems(page, IDC_HOME_WAYPOINT), tab, WB_QT_TEAMNOTIFY_SELCHANGE), 1, 1);
	root->addLayout(identityGrid);

	// production box
	QGroupBox *prodBox = new QGroupBox("Production:", tab);
	QGridLayout *prodGrid = new QGridLayout(prodBox);
	prodGrid->addWidget(new QLabel("Condition:", prodBox), 0, 0);
	prodGrid->addWidget(bindCombo(page, IDC_PRODUCTION_CONDITION, readComboItems(page, IDC_PRODUCTION_CONDITION), prodBox, WB_QT_TEAMNOTIFY_SELCHANGE), 0, 1, 1, 3);
	prodGrid->addWidget(bindCheck(page, IDC_PRODUCTION_EXECUTEACTIONS, "Execute associated actions", prodBox), 1, 1, 1, 3);
	prodGrid->addWidget(new QLabel("Priority:", prodBox), 2, 0);
	QLineEdit *priorityEdit = bindEdit(page, IDC_PRODUCTION_PRIORITY, prodBox, WB_QT_TEAMNOTIFY_CHANGE);
	priorityEdit->setFixedWidth(50);
	prodGrid->addWidget(priorityEdit, 2, 1);
	prodGrid->addWidget(new QLabel("Success Priority Increase:", prodBox), 2, 2);
	QLineEdit *increaseEdit = bindEdit(page, IDC_PRIORITY_INCREASE, prodBox, WB_QT_TEAMNOTIFY_CHANGE);
	increaseEdit->setFixedWidth(40);
	prodGrid->addWidget(increaseEdit, 2, 3);
	QHBoxLayout *framesRow = new QHBoxLayout();
	framesRow->addWidget(new QLabel("Build for", prodBox));
	QLineEdit *framesEdit = bindEdit(page, IDC_TEAM_BUILD_FRAMES, prodBox, WB_QT_TEAMNOTIFY_CHANGE);
	framesEdit->setFixedWidth(50);
	framesRow->addWidget(framesEdit);
	framesRow->addWidget(new QLabel("frames.", prodBox));
	framesRow->addStretch(1);
	prodGrid->addLayout(framesRow, 3, 0, 1, 2);
	prodGrid->addWidget(new QLabel("Failure Priority Decrease:", prodBox), 3, 2);
	QLineEdit *decreaseEdit = bindEdit(page, IDC_PRIORITY_DECREASE, prodBox, WB_QT_TEAMNOTIFY_CHANGE);
	decreaseEdit->setFixedWidth(40);
	prodGrid->addWidget(decreaseEdit, 3, 3);
	root->addWidget(prodBox);

	// members: 7 unit rows. The seven hidden combos share the template catalog, so enumerate
	// slot 1 ONCE (the per-slot "[???]" placeholder is folded in by seedComboCurrent).
	static const int minIds[7] = { IDC_MIN_UNIT1, IDC_MIN_UNIT2, IDC_MIN_UNIT3, IDC_MIN_UNIT4, IDC_MIN_UNIT5, IDC_MIN_UNIT6, IDC_MIN_UNIT7 };
	static const int maxIds[7] = { IDC_MAX_UNIT1, IDC_MAX_UNIT2, IDC_MAX_UNIT3, IDC_MAX_UNIT4, IDC_MAX_UNIT5, IDC_MAX_UNIT6, IDC_MAX_UNIT7 };
	static const int typeIds[7] = { IDC_UNIT_TYPE1, IDC_UNIT_TYPE2, IDC_UNIT_TYPE3, IDC_UNIT_TYPE4, IDC_UNIT_TYPE5, IDC_UNIT_TYPE6, IDC_UNIT_TYPE7 };
	static const int pickIds[7] = { IDC_UNIT_TYPE1_BUTTON, IDC_UNIT_TYPE2_BUTTON, IDC_UNIT_TYPE3_BUTTON, IDC_UNIT_TYPE4_BUTTON, IDC_UNIT_TYPE5_BUTTON, IDC_UNIT_TYPE6_BUTTON, IDC_UNIT_TYPE7_BUTTON };

	QGroupBox *membersBox = new QGroupBox("Members:", tab);
	QGridLayout *membersGrid = new QGridLayout(membersBox);
	membersGrid->addWidget(new QLabel("Min:", membersBox), 0, 0);
	membersGrid->addWidget(new QLabel("Max:", membersBox), 0, 1);
	membersGrid->addWidget(new QLabel("Unit Type:", membersBox), 0, 2);
	QStringList unitItems = readComboItems(page, IDC_UNIT_TYPE1);
	for (int i = 0; i < 7; i++)
	{
		QLineEdit *minEdit = bindEdit(page, minIds[i], membersBox, WB_QT_TEAMNOTIFY_CHANGE);
		minEdit->setFixedWidth(40);
		membersGrid->addWidget(minEdit, i + 1, 0);
		QLineEdit *maxEdit2 = bindEdit(page, maxIds[i], membersBox, WB_QT_TEAMNOTIFY_CHANGE);
		maxEdit2->setFixedWidth(40);
		membersGrid->addWidget(maxEdit2, i + 1, 1);
		m_unitCombos[i] = bindCombo(page, typeIds[i], unitItems, membersBox, WB_QT_TEAMNOTIFY_SELCHANGE);
		membersGrid->addWidget(m_unitCombos[i], i + 1, 2);
		QPushButton *pickButton = new QPushButton("...", membersBox);
		pickButton->setFixedWidth(28);
		pickButton->setAutoDefault(false);
		membersGrid->addWidget(pickButton, i + 1, 3);
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
	membersGrid->setColumnStretch(2, 1);
	root->addWidget(membersBox);

	// recruitment + singleton + description
	QGroupBox *recruitBox = new QGroupBox("Recruitment Options:", tab);
	QVBoxLayout *recruitLay = new QVBoxLayout(recruitBox);
	recruitLay->addWidget(bindCheck(page, IDC_AUTO_REINFORCE, "Automatically reinforce whenever possible", recruitBox));
	recruitLay->addWidget(bindCheck(page, IDC_AI_RECRUITABLE, "Team members are AI Recruitable", recruitBox));
	root->addWidget(recruitBox);

	root->addWidget(bindCheck(page, IDC_TEAM_SINGLETON, "Team created once and only once.", tab));

	QGroupBox *descBox = new QGroupBox("Team Description:", tab);
	QVBoxLayout *descLay = new QVBoxLayout(descBox);
	descLay->addWidget(bindEdit(page, IDC_DESCRIPTION, descBox, WB_QT_TEAMNOTIFY_CHANGE));
	root->addWidget(descBox);

	root->addStretch(1);
	return tab;
}

QWidget *WBQtTeamSheetDialog::buildReinforcementTab()
{
	const int page = WB_QT_TEAMPAGE_REINFORCEMENT;
	QWidget *tab = new QWidget(this);
	QVBoxLayout *root = new QVBoxLayout(tab);

	QGroupBox *reinfBox = new QGroupBox("If reinforcing, then:", tab);
	QVBoxLayout *reinfLay = new QVBoxLayout(reinfBox);
	QHBoxLayout *deployRow = new QHBoxLayout();
	deployRow->addWidget(bindCheck(page, IDC_DEPLOY_BY, "Deploy by", reinfBox));
	deployRow->addWidget(bindCombo(page, IDC_TRANSPORT_COMBO, readComboItems(page, IDC_TRANSPORT_COMBO), reinfBox, WB_QT_TEAMNOTIFY_SELCHANGE), 1);
	reinfLay->addLayout(deployRow);
	reinfLay->addWidget(bindCheck(page, IDC_TRANSPORTS_EXIT, "Transports exit map after unloading.", reinfBox));
	QGroupBox *waypointBox = new QGroupBox("Start team or transports at waypoint:", reinfBox);
	QVBoxLayout *waypointLay = new QVBoxLayout(waypointBox);
	waypointLay->addWidget(bindCombo(page, IDC_WAYPOINT_COMBO, readComboItems(page, IDC_WAYPOINT_COMBO), waypointBox, WB_QT_TEAMNOTIFY_SELCHANGE));
	reinfLay->addWidget(waypointBox);
	reinfLay->addWidget(bindCheck(page, IDC_TEAM_STARTS_FULL, "Load members into transports (if applicable).", reinfBox));
	root->addWidget(reinfBox);

	QGroupBox *vetBox = new QGroupBox("Veterancy Level", tab);
	QVBoxLayout *vetLay = new QVBoxLayout(vetBox);
	vetLay->addWidget(bindCombo(page, IDC_VETERANCY, readComboItems(page, IDC_VETERANCY), vetBox, WB_QT_TEAMNOTIFY_SELCHANGE));
	root->addWidget(vetBox);

	root->addStretch(1);
	return tab;
}

QWidget *WBQtTeamSheetDialog::buildBehaviorTab()
{
	const int page = WB_QT_TEAMPAGE_BEHAVIOR;
	QWidget *tab = new QWidget(this);
	QVBoxLayout *root = new QVBoxLayout(tab);

	// One representative script combo enumerates the (shared) subroutine-script list.
	QStringList scriptItems = readComboItems(page, IDC_ON_CREATE_SCRIPT);

	QGroupBox *triggersBox = new QGroupBox("Behavior Script Triggers:", tab);
	QGridLayout *grid = new QGridLayout(triggersBox);
	grid->addWidget(new QLabel("On Create:", triggersBox), 0, 0);
	grid->addWidget(bindCombo(page, IDC_ON_CREATE_SCRIPT, scriptItems, triggersBox, WB_QT_TEAMNOTIFY_SELCHANGE), 0, 1, 1, 2);
	grid->addWidget(new QLabel("On Enemy Sighted:", triggersBox), 1, 0);
	grid->addWidget(bindCombo(page, IDC_ON_ENEMY_SIGHTED, scriptItems, triggersBox, WB_QT_TEAMNOTIFY_SELCHANGE), 1, 1, 1, 2);
	grid->addWidget(new QLabel("On All Clear:", triggersBox), 2, 0);
	grid->addWidget(bindCombo(page, IDC_ON_ALL_CLEAR, scriptItems, triggersBox, WB_QT_TEAMNOTIFY_SELCHANGE), 2, 1, 1, 2);
	QHBoxLayout *destroyedRow = new QHBoxLayout();
	destroyedRow->addWidget(new QLabel("On Destroyed %", triggersBox));
	QLineEdit *percentEdit = bindEdit(page, IDC_PERCENT_DESTROYED, triggersBox, WB_QT_TEAMNOTIFY_CHANGE);
	percentEdit->setFixedWidth(40);
	destroyedRow->addWidget(percentEdit);
	grid->addLayout(destroyedRow, 3, 0);
	grid->addWidget(bindCombo(page, IDC_ON_DESTROYED, scriptItems, triggersBox, WB_QT_TEAMNOTIFY_SELCHANGE), 3, 1, 1, 2);
	grid->addWidget(new QLabel("On Idle:", triggersBox), 4, 0);
	grid->addWidget(bindCombo(page, IDC_ON_IDLE_SCRIPT, scriptItems, triggersBox, WB_QT_TEAMNOTIFY_SELCHANGE), 4, 1, 1, 2);
	grid->addWidget(new QLabel("On Unit Destroyed:", triggersBox), 5, 0);
	grid->addWidget(bindCombo(page, IDC_ON_UNIT_DESTROYED_SCRIPT, scriptItems, triggersBox, WB_QT_TEAMNOTIFY_SELCHANGE), 5, 1, 1, 2);
	root->addWidget(triggersBox);

	QGroupBox *optionsBox = new QGroupBox("Behavior Options", tab);
	QVBoxLayout *optionsLay = new QVBoxLayout(optionsBox);
	optionsLay->addWidget(bindCheck(page, IDC_TRANSPORTS_RETURN, "Transports return to base after unloading.", optionsBox));
	optionsLay->addWidget(bindCheck(page, IDC_AVOID_THREATS, "Team avoids threats.", optionsBox));
	QGroupBox *initialBox = new QGroupBox("Initial Team Behavior", optionsBox);
	QVBoxLayout *initialLay = new QVBoxLayout(initialBox);
	initialLay->addWidget(bindCombo(page, IDC_ENEMY_INTERACTIONS, readComboItems(page, IDC_ENEMY_INTERACTIONS), initialBox, WB_QT_TEAMNOTIFY_SELCHANGE));
	optionsLay->addWidget(initialBox);
	QGroupBox *attackBox = new QGroupBox("Attack:", optionsBox);
	QVBoxLayout *attackLay = new QVBoxLayout(attackBox);
	attackLay->addWidget(bindCheck(page, IDC_ATTACK_COMMON_TARGET, "Does this team focus on a single target at a time\nin Hard and Brutal (never in Normal)?", attackBox));
	optionsLay->addWidget(attackBox);
	root->addWidget(optionsBox);

	root->addStretch(1);
	return tab;
}

QWidget *WBQtTeamSheetDialog::buildGenericTab()
{
	const int page = WB_QT_TEAMPAGE_GENERIC;
	QWidget *tab = new QWidget(this);
	QGridLayout *grid = new QGridLayout(tab);

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
		grid->addWidget(new QLabel(QString("Script %1:").arg(i + 1), tab), i, 0);
		grid->addWidget(bindCombo(page, scriptIds[i], scriptItems, tab, WB_QT_TEAMNOTIFY_SELCHANGE), i, 1);
	}
	grid->setColumnStretch(1, 1);
	grid->setRowStretch(16, 1);
	return tab;
}
