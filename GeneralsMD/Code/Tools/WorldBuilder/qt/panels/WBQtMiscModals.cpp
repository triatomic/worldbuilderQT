// WBQtMiscModals.cpp -- see WBQtMiscModals.h. Layouts and behavior mirror the MFC
// IDD_SHADOW_OPTIONS / IDD_IMPASSABLEOPTIONS / IDD_MACRO_TEXTURE / IDD_MAP_SETTINGS /
// IDD_EXPORT_SCRIPTS_OPTIONS dialogs; all state round-trips through the bridge.
#include "WBQtMiscModals.h"
#include "ui_WBQtShadowDialog.h"
#include "ui_WBQtImpassableDialog.h"
#include "ui_WBQtMacrotextureDialog.h"
#include "ui_WBQtMapSettingsDialog.h"
#include "ui_WBQtFixTeamOwnerDialog.h"
#include "ui_WBQtBaseBuildPropsDialog.h"
#include "ui_WBQtNewHeightMapDialog.h"
#include "ui_WBQtExportScriptsDialog.h"
#include "WBQtMiscModalsBridge.h"
#include "WBQtParamBridge.h"	// subroutine-script enumeration for Building Properties

// Stage 1 phase 3: modal-dialog parent (active modal if nested, else main window). WBQtBridge.cpp.
QWidget *WBQt_DialogParent(void);

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include <qt_windows.h>

#include <string.h>

namespace
{
	const int kNameCap = 512;
}

// ===================== WBQtShadowDialog =====================

WBQtShadowDialog::WBQtShadowDialog(QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtShadowDialog),
	m_updating(false)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// The static widget tree lives in WBQtShadowDialog.ui; bind the members the logic uses.
	m_ui->setupUi(this);
	m_red = m_ui->red;
	m_green = m_ui->green;
	m_blue = m_ui->blue;
	m_intensity = m_ui->intensity;

	double r = 0.0, g = 0.0, b = 0.0, intensity = 0.0;
	WBQtShadow_Get(&r, &g, &b, &intensity);
	m_red->setText(QString::number(r, 'f', 2));
	m_green->setText(QString::number(g, 'f', 2));
	m_blue->setText(QString::number(b, 'f', 2));
	m_intensity->setText(QString::number(intensity, 'f', 2));

	// The dialog has only OK; it applies live as the fields change (== the MFC EN_CHANGE
	// handlers), so OK just closes.
	connect(m_ui->okBtn, SIGNAL(clicked()), this, SLOT(accept()));

	connect(m_red, SIGNAL(textChanged(QString)), this, SLOT(onFieldChanged()));
	connect(m_green, SIGNAL(textChanged(QString)), this, SLOT(onFieldChanged()));
	connect(m_blue, SIGNAL(textChanged(QString)), this, SLOT(onFieldChanged()));
	connect(m_intensity, SIGNAL(textChanged(QString)), this, SLOT(onFieldChanged()));
}

WBQtShadowDialog::~WBQtShadowDialog()
{
	delete m_ui;
}

void WBQtShadowDialog::onFieldChanged()
{
	if (m_updating)
	{
		return;
	}
	apply();
}

void WBQtShadowDialog::apply()
{
	// == each EN_CHANGE handler: parse what parses, ignore the rest (they leave the member as
	// last-good). We read all four, substituting the last-applied on a bad field is overkill;
	// a failed toDouble yields 0.0 which matches sscanf-fail leaving the value only if it
	// parsed -- so guard each field individually.
	bool ok = false;
	double r = m_red->text().toDouble(&ok);
	if (!ok) return;
	double g = m_green->text().toDouble(&ok);
	if (!ok) return;
	double b = m_blue->text().toDouble(&ok);
	if (!ok) return;
	double intensity = m_intensity->text().toDouble(&ok);
	if (!ok) return;
	WBQtShadow_Apply(r, g, b, intensity);
}

// ===================== WBQtImpassableDialog =====================

WBQtImpassableDialog::WBQtImpassableDialog(QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtImpassableDialog)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// The static widget tree lives in WBQtImpassableDialog.ui; bind the members the logic uses.
	m_ui->setupUi(this);
	m_angle = m_ui->angle;

	WBQtImpassable_Begin();	// force the overlay on for the dialog's lifetime (== the ctor)

	double slope = WBQtImpassable_GetSlope();
	m_angle->setText(QString::number(slope, 'f', 2));

	connect(m_ui->previewBtn, SIGNAL(clicked()), this, SLOT(onPreview()));
	connect(m_ui->okBtn, SIGNAL(clicked()), this, SLOT(accept()));
	connect(m_ui->cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
	connect(m_angle, SIGNAL(textChanged(QString)), this, SLOT(onAngleChanged()));
}

WBQtImpassableDialog::~WBQtImpassableDialog()
{
	delete m_ui;
}

void WBQtImpassableDialog::onAngleChanged()
{
	// == OnAngleChange: apply live, clamping; reflect the clamped value back into the field.
	bool ok = false;
	double slope = m_angle->text().toDouble(&ok);
	if (!ok)
	{
		return;
	}
	double clamped = WBQtImpassable_SetSlope(slope);
	if (clamped != slope)
	{
		bool block = m_angle->blockSignals(true);
		m_angle->setText(QString::number(clamped, 'f', 2));
		m_angle->blockSignals(block);
	}
}

void WBQtImpassableDialog::onPreview()
{
	WBQtImpassable_Preview();
}

// ===================== WBQtMacrotextureDialog =====================

WBQtMacrotextureDialog::WBQtMacrotextureDialog(QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtMacrotextureDialog)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// The static widget tree lives in WBQtMacrotextureDialog.ui; bind the members the logic uses.
	m_ui->setupUi(this);
	m_list = m_ui->list;

	char buf[kNameCap];
	int count = WBQtMacrotexture_GetCount();
	for (int i = 0; i < count; i++)
	{
		buf[0] = 0;
		WBQtMacrotexture_GetName(i, buf, sizeof(buf));
		new QListWidgetItem(QString::fromLocal8Bit(buf), m_list);
	}
	m_list->sortItems(Qt::AscendingOrder);

	connect(m_ui->okBtn, SIGNAL(clicked()), this, SLOT(accept()));
	connect(m_list, SIGNAL(currentRowChanged(int)), this, SLOT(onRowChanged(int)));
}

WBQtMacrotextureDialog::~WBQtMacrotextureDialog()
{
	delete m_ui;
}

void WBQtMacrotextureDialog::onRowChanged(int row)
{
	// == OnNotify TVN_SELCHANGED: applying the texture live. The list is sorted for display,
	// so map the row's text back to the bridge index.
	if (row < 0)
	{
		return;
	}
	QString name = m_list->item(row)->text();
	char buf[kNameCap];
	int count = WBQtMacrotexture_GetCount();
	for (int i = 0; i < count; i++)
	{
		buf[0] = 0;
		WBQtMacrotexture_GetName(i, buf, sizeof(buf));
		if (QString::fromLocal8Bit(buf) == name)
		{
			WBQtMacrotexture_Apply(i);
			return;
		}
	}
}

// ===================== WBQtMapSettingsDialog =====================

WBQtMapSettingsDialog::WBQtMapSettingsDialog(QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtMapSettingsDialog)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// The static widget tree lives in WBQtMapSettingsDialog.ui; bind the members the logic uses.
	m_ui->setupUi(this);
	m_mapName = m_ui->mapName;
	m_compression = m_ui->compression;
	m_timeOfDay = m_ui->timeOfDay;
	m_weather = m_ui->weather;

	char buf[kNameCap];
	buf[0] = 0;
	WBQtMapSettings_GetMapName(buf, sizeof(buf));
	m_mapName->setText(QString::fromLocal8Bit(buf));

	int compCount = WBQtMapSettings_GetCompressionCount();
	for (int i = 0; i < compCount; i++)
	{
		buf[0] = 0;
		WBQtMapSettings_GetCompressionName(i, buf, sizeof(buf));
		m_compression->addItem(QString::fromLocal8Bit(buf));
	}
	m_compression->setCurrentIndex(WBQtMapSettings_GetCompressionIndex());

	int todCount = WBQtMapSettings_GetTimeOfDayCount();
	for (int i = 0; i < todCount; i++)
	{
		buf[0] = 0;
		WBQtMapSettings_GetTimeOfDayName(i, buf, sizeof(buf));
		m_timeOfDay->addItem(QString::fromLocal8Bit(buf));
	}
	m_timeOfDay->setCurrentIndex(WBQtMapSettings_GetTimeOfDayIndex());

	int weatherCount = WBQtMapSettings_GetWeatherCount();
	for (int i = 0; i < weatherCount; i++)
	{
		buf[0] = 0;
		WBQtMapSettings_GetWeatherName(i, buf, sizeof(buf));
		m_weather->addItem(QString::fromLocal8Bit(buf));
	}
	m_weather->setCurrentIndex(WBQtMapSettings_GetWeatherIndex());

	connect(m_ui->okBtn, SIGNAL(clicked()), this, SLOT(accept()));
	connect(m_ui->cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
}

WBQtMapSettingsDialog::~WBQtMapSettingsDialog()
{
	delete m_ui;
}

void WBQtMapSettingsDialog::accept()
{
	QByteArray name = m_mapName->text().toLocal8Bit();
	WBQtMapSettings_Store(m_timeOfDay->currentIndex(), m_weather->currentIndex(),
		m_compression->currentIndex(), name.constData());
	QDialog::accept();
}

// ===================== WBQtFixTeamOwnerDialog =====================

WBQtFixTeamOwnerDialog::WBQtFixTeamOwnerDialog(void *teamsInfo, void *sidesList, QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtFixTeamOwnerDialog),
	m_sidesList(sidesList),
	m_pickedSide(-1)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// The static widget tree lives in WBQtFixTeamOwnerDialog.ui; bind the members the logic uses.
	m_ui->setupUi(this);
	m_list = m_ui->list;

	char buf[kNameCap];
	buf[0] = 0;
	WBQtFixOwnerData_GetPrompt(teamsInfo, buf, sizeof(buf));
	m_ui->promptLabel->setText(QString::fromLocal8Bit(buf));

	int count = WBQtFixOwnerData_GetCount(sidesList);
	for (int i = 0; i < count; i++)
	{
		buf[0] = 0;
		WBQtFixOwnerData_GetDisplay(sidesList, i, buf, sizeof(buf));
		QListWidgetItem *item = new QListWidgetItem(QString::fromLocal8Bit(buf), m_list);
		item->setData(Qt::UserRole, i);	// side index survives the sort
	}
	m_list->sortItems(Qt::AscendingOrder);	// == the LBS_SORT display

	connect(m_ui->okBtn, SIGNAL(clicked()), this, SLOT(accept()));
	connect(m_ui->cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
}

WBQtFixTeamOwnerDialog::~WBQtFixTeamOwnerDialog()
{
	delete m_ui;
}

void WBQtFixTeamOwnerDialog::accept()
{
	// == OnOK, but the row maps back through the stored side index (the MFC dialog indexed
	// the SORTED row into the unsorted sides list).
	QListWidgetItem *item = m_list->currentItem();
	if (item != NULL)
	{
		m_pickedSide = item->data(Qt::UserRole).toInt();
	}
	QDialog::accept();
}

// ===================== WBQtBaseBuildPropsDialog =====================

WBQtBaseBuildPropsDialog::WBQtBaseBuildPropsDialog(const QString &name, const QString &script, int health, int unsellable, QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtBaseBuildPropsDialog)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// The static widget tree lives in WBQtBaseBuildPropsDialog.ui; bind the members the logic uses.
	m_ui->setupUi(this);
	m_nameEdit = m_ui->nameEdit;
	m_scriptCombo = m_ui->scriptCombo;
	m_healthEdit = m_ui->healthEdit;
	m_unsellableCheck = m_ui->unsellableCheck;

	m_nameEdit->setText(name);
	char buf[kNameCap];
	int count = WBQtParamData_LoadSubroutineScripts();
	QStringList scripts;
	for (int i = 0; i < count; i++)
	{
		buf[0] = 0;
		WBQtParamData_GetOption(i, buf, sizeof(buf));
		scripts.append(QString::fromLocal8Bit(buf));
	}
	scripts.append("<none>");
	m_scriptCombo->addItems(scripts);
	// == OnInitDialog: empty script selects <none>; unknown scripts fall back gracefully.
	QString current = script.isEmpty() ? QString("<none>") : script;
	int index = m_scriptCombo->findText(current);
	if (index < 0)
	{
		m_scriptCombo->addItem(current);
		index = m_scriptCombo->findText(current);
	}
	m_scriptCombo->setCurrentIndex(index);
	m_healthEdit->setText(QString::number(health));
	m_unsellableCheck->setChecked(unsellable != 0);

	connect(m_ui->okBtn, SIGNAL(clicked()), this, SLOT(accept()));
	connect(m_ui->cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
}

WBQtBaseBuildPropsDialog::~WBQtBaseBuildPropsDialog()
{
	delete m_ui;
}

// ===================== WBQtNewHeightMapDialog =====================

WBQtNewHeightMapDialog::WBQtNewHeightMapDialog(const QString &label, bool forResize,
	int initialHeight, int xExtent, int yExtent, int borderWidth, QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtNewHeightMapDialog)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// The static widget tree lives in WBQtNewHeightMapDialog.ui; bind the members the logic uses.
	// The resize-only anchor grid stays dynamic and goes into the .ui's empty anchorsHost layout.
	m_ui->setupUi(this);
	setWindowTitle(label.isEmpty() ? QString("New Height Map") : label);
	m_xEdit = m_ui->xEdit;
	m_yEdit = m_ui->yEdit;
	m_borderEdit = m_ui->borderEdit;
	m_heightEdit = m_ui->heightEdit;

	m_xEdit->setText(QString::number(xExtent));
	m_yEdit->setText(QString::number(yExtent));
	m_borderEdit->setText(QString::number(borderWidth));
	m_heightEdit->setText(QString::number(initialHeight));

	for (int i = 0; i < 9; i++)
	{
		m_anchors[i] = NULL;
	}
	if (forResize)
	{
		// == the 3x3 BS_PUSHLIKE anchor grid: exclusive, center = grow evenly (no anchors).
		m_ui->anchorsHost->addWidget(new QLabel("Anchor:", this));
		QGridLayout *anchorGrid = new QGridLayout();
		anchorGrid->setSpacing(0);
		for (int i = 0; i < 9; i++)
		{
			QPushButton *cell = new QPushButton(this);
			cell->setCheckable(true);
			cell->setAutoDefault(false);
			cell->setFixedSize(34, 30);
			m_anchors[i] = cell;
			anchorGrid->addWidget(cell, i / 3, i % 3);
			connect(cell, &QPushButton::clicked, cell, [this, i]()
			{
				for (int j = 0; j < 9; j++)
				{
					m_anchors[j]->setChecked(j == i);
				}
			});
		}
		QHBoxLayout *anchorRow = new QHBoxLayout();
		anchorRow->addLayout(anchorGrid);
		anchorRow->addStretch(1);
		m_ui->anchorsHost->addLayout(anchorRow);
		m_anchors[4]->setChecked(true);	// center, == OnInitDialog's default
	}

	connect(m_ui->okBtn, SIGNAL(clicked()), this, SLOT(accept()));
	connect(m_ui->cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
	resize(260, forResize ? 300 : 180);
}

WBQtNewHeightMapDialog::~WBQtNewHeightMapDialog()
{
	delete m_ui;
}

void WBQtNewHeightMapDialog::anchorsOut(int *top, int *bottom, int *left, int *right) const
{
	*top = 0;
	*bottom = 0;
	*left = 0;
	*right = 0;
	if (m_anchors[0] == NULL)
	{
		return;
	}
	int checked = 4;
	for (int i = 0; i < 9; i++)
	{
		if (m_anchors[i]->isChecked())
		{
			checked = i;
			break;
		}
	}
	int row = checked / 3;
	int col = checked % 3;
	*top = (row == 0) ? 1 : 0;
	*bottom = (row == 2) ? 1 : 0;
	*left = (col == 0) ? 1 : 0;
	*right = (col == 2) ? 1 : 0;
}

// ===================== WBQtExportScriptsDialog =====================

WBQtExportScriptsDialog::WBQtExportScriptsDialog(QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtExportScriptsDialog)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	int waypoints = 0, triggers = 0, units = 0, teams = 0, sides = 0, allScripts = 0;
	WBQtExportScripts_Get(&waypoints, &triggers, &units, &teams, &sides, &allScripts);

	// The static widget tree lives in WBQtExportScriptsDialog.ui; bind the members the logic uses.
	m_ui->setupUi(this);
	m_waypoints = m_ui->waypoints;
	m_triggers = m_ui->triggers;
	m_units = m_ui->units;
	m_teams = m_ui->teams;
	m_sides = m_ui->sides;
	m_allScripts = m_ui->allScripts;
	m_selectedScripts = m_ui->selectedScripts;

	m_waypoints->setChecked(waypoints != 0);
	m_triggers->setChecked(triggers != 0);
	m_units->setChecked(units != 0);
	m_teams->setChecked(teams != 0);
	m_sides->setChecked(sides != 0);
	m_allScripts->setChecked(allScripts != 0);
	m_selectedScripts->setChecked(allScripts == 0);

	connect(m_ui->okBtn, SIGNAL(clicked()), this, SLOT(accept()));
	connect(m_ui->cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
}

WBQtExportScriptsDialog::~WBQtExportScriptsDialog()
{
	delete m_ui;
}

void WBQtExportScriptsDialog::accept()
{
	WBQtExportScripts_Store(
		m_waypoints->isChecked() ? 1 : 0,
		m_triggers->isChecked() ? 1 : 0,
		m_units->isChecked() ? 1 : 0,
		m_teams->isChecked() ? 1 : 0,
		m_sides->isChecked() ? 1 : 0,
		m_allScripts->isChecked() ? 1 : 0);
	QDialog::accept();
}

// ===================== the modal entry points =====================

namespace
{
	// Stage 1 phase 3: parent to the main window (or active modal, if nested) + Qt
	// ApplicationModal, which fences every Qt window incl. the hosted viewport (QWinHost
	// WindowBlocked). The old EnableWindow(frame) discipline is gone.
	int runModal(QDialog &dlg, void * /*frameHwnd*/)
	{
		dlg.setParent(WBQt_DialogParent(), dlg.windowFlags());
		dlg.setWindowModality(Qt::ApplicationModal);
		int rc = dlg.exec();
		return (rc == QDialog::Accepted) ? 1 : 0;
	}
}

extern "C" int WBQtShadowOptions_Run(void *frameHwnd)
{
	WBQtShadowDialog dlg;
	runModal(dlg, frameHwnd);
	return 1;	// == ShadowOptions: no cancel path (only OK)
}

extern "C" int WBQtImpassableOptions_Run(void *frameHwnd)
{
	WBQtImpassableDialog dlg;
	int rc = runModal(dlg, frameHwnd);
	// Restore the overlay state the dialog forced on, AFTER exec returns (== the dtor); on
	// cancel this also reverts the live-applied slope to the snapshot (== the call site).
	WBQtImpassable_End(rc);
	return rc;
}

extern "C" int WBQtSelectMacrotexture_Run(void *frameHwnd)
{
	WBQtMacrotextureDialog dlg;
	runModal(dlg, frameHwnd);
	return 1;	// == SelectMacrotexture: applies live, no explicit OK write-back
}

extern "C" int WBQtMapSettings_Run(void *frameHwnd)
{
	WBQtMapSettingsDialog dlg;
	return runModal(dlg, frameHwnd);
}

extern "C" int WBQtExportScriptsOptions_Run(void *frameHwnd)
{
	WBQtExportScriptsDialog dlg;
	return runModal(dlg, frameHwnd);
}

extern "C" int WBQtNewHeightMap_Run(void *frameHwnd, const char *label, int forResize,
	int *initialHeight, int *xExtent, int *yExtent, int *borderWidth,
	int *anchorTop, int *anchorBottom, int *anchorLeft, int *anchorRight)
{
	WBQtNewHeightMapDialog dlg(QString::fromLocal8Bit(label ? label : ""), forResize != 0,
		*initialHeight, *xExtent, *yExtent, *borderWidth);
	if (runModal(dlg, frameHwnd) == 0)
	{
		return 0;
	}
	// == CNewHeightMap::OnOK (atoi semantics: garbage parses to 0, matching MFC).
	*initialHeight = dlg.m_heightEdit->text().toInt();
	*xExtent = dlg.m_xEdit->text().toInt();
	*yExtent = dlg.m_yEdit->text().toInt();
	*borderWidth = dlg.m_borderEdit->text().toInt();
	dlg.anchorsOut(anchorTop, anchorBottom, anchorLeft, anchorRight);
	return 1;
}

extern "C" int WBQtFixTeamOwner_Run(void *teamsInfo, void *sidesList, void *frameHwnd, char *ownerOut, int cap)
{
	if (ownerOut != NULL && cap > 0)
	{
		ownerOut[0] = 0;
	}
	WBQtFixTeamOwnerDialog dlg(teamsInfo, sidesList);
	if (runModal(dlg, frameHwnd) != 0 && dlg.pickedSide() >= 0)
	{
		WBQtFixOwnerData_GetInternal(sidesList, dlg.pickedSide(), ownerOut, cap);
		return 1;
	}
	return 0;
}

extern "C" int WBQtBaseBuildProps_Run(void *frameHwnd, const char *name, const char *script, int health, int unsellable,
	char *nameOut, int nameCap, char *scriptOut, int scriptCap, int *healthOut, int *unsellableOut)
{
	WBQtBaseBuildPropsDialog dlg(QString::fromLocal8Bit(name ? name : ""),
		QString::fromLocal8Bit(script ? script : ""), health, unsellable);
	if (runModal(dlg, frameHwnd) == 0)
	{
		return 0;
	}
	// == BaseBuildProps::OnOK: texts verbatim (including a literal "<none>", matching MFC);
	// empty health -> 100, negative -> 0.
	QByteArray nameBytes = dlg.m_nameEdit->text().toLocal8Bit();
	strncpy(nameOut, nameBytes.constData(), nameCap - 1);
	nameOut[nameCap - 1] = 0;
	QByteArray scriptBytes = dlg.m_scriptCombo->currentText().toLocal8Bit();
	strncpy(scriptOut, scriptBytes.constData(), scriptCap - 1);
	scriptOut[scriptCap - 1] = 0;
	QString healthText = dlg.m_healthEdit->text();
	int outHealth;
	if (healthText.isEmpty())
	{
		outHealth = 100;
	}
	else
	{
		outHealth = healthText.toInt();
		if (outHealth < 0)
		{
			outHealth = 0;
		}
	}
	*healthOut = outHealth;
	*unsellableOut = dlg.m_unsellableCheck->isChecked() ? 1 : 0;
	return 1;
}
