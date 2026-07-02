// WBQtMiscModals.cpp -- see WBQtMiscModals.h. Layouts and behavior mirror the MFC
// IDD_SHADOW_OPTIONS / IDD_IMPASSABLEOPTIONS / IDD_MACRO_TEXTURE / IDD_MAP_SETTINGS /
// IDD_EXPORT_SCRIPTS_OPTIONS dialogs; all state round-trips through the bridge.
#include "WBQtMiscModals.h"
#include "WBQtMiscModalsBridge.h"
#include "WBQtParamBridge.h"	// subroutine-script enumeration for Building Properties

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

	QPushButton *addOkCancel(QVBoxLayout *root, QDialog *dlg, bool withCancel)
	{
		QHBoxLayout *buttons = new QHBoxLayout();
		buttons->addStretch(1);
		QPushButton *okButton = new QPushButton("OK", dlg);
		okButton->setDefault(true);
		buttons->addWidget(okButton);
		QObject::connect(okButton, SIGNAL(clicked()), dlg, SLOT(accept()));
		if (withCancel)
		{
			QPushButton *cancelButton = new QPushButton("Cancel", dlg);
			cancelButton->setAutoDefault(false);
			buttons->addWidget(cancelButton);
			QObject::connect(cancelButton, SIGNAL(clicked()), dlg, SLOT(reject()));
		}
		root->addLayout(buttons);
		return okButton;
	}
}

// ===================== WBQtShadowDialog =====================

WBQtShadowDialog::WBQtShadowDialog(QWidget *parent)
	: QDialog(parent),
	m_updating(false)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle("Shadow Options");

	double r = 0.0, g = 0.0, b = 0.0, intensity = 0.0;
	WBQtShadow_Get(&r, &g, &b, &intensity);

	QVBoxLayout *root = new QVBoxLayout(this);
	QGroupBox *colorBox = new QGroupBox("Shadow Color:", this);
	QVBoxLayout *colorLay = new QVBoxLayout(colorBox);
	const char *labels[3] = { "RED:", "GREEN:", "BLUE:" };
	double values[3];
	values[0] = r;
	values[1] = g;
	values[2] = b;
	QLineEdit *edits[3];
	for (int i = 0; i < 3; i++)
	{
		QHBoxLayout *row = new QHBoxLayout();
		row->addWidget(new QLabel(labels[i], colorBox));
		edits[i] = new QLineEdit(QString::number(values[i], 'f', 2), colorBox);
		row->addWidget(edits[i]);
		colorLay->addLayout(row);
	}
	m_red = edits[0];
	m_green = edits[1];
	m_blue = edits[2];
	root->addWidget(colorBox);

	QGroupBox *intensityBox = new QGroupBox("Shadow Intensity:", this);
	QHBoxLayout *intensityLay = new QHBoxLayout(intensityBox);
	intensityLay->addWidget(new QLabel("Intensity:", intensityBox));
	m_intensity = new QLineEdit(QString::number(intensity, 'f', 2), intensityBox);
	intensityLay->addWidget(m_intensity);
	root->addWidget(intensityBox);

	// The dialog has only OK; it applies live as the fields change (== the MFC EN_CHANGE
	// handlers), so OK just closes.
	addOkCancel(root, this, false);

	connect(m_red, SIGNAL(textChanged(QString)), this, SLOT(onFieldChanged()));
	connect(m_green, SIGNAL(textChanged(QString)), this, SLOT(onFieldChanged()));
	connect(m_blue, SIGNAL(textChanged(QString)), this, SLOT(onFieldChanged()));
	connect(m_intensity, SIGNAL(textChanged(QString)), this, SLOT(onFieldChanged()));

	resize(240, 220);
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
	: QDialog(parent)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle("Impassable Options");

	WBQtImpassable_Begin();	// force the overlay on for the dialog's lifetime (== the ctor)

	double slope = WBQtImpassable_GetSlope();

	QVBoxLayout *root = new QVBoxLayout(this);
	QHBoxLayout *angleRow = new QHBoxLayout();
	angleRow->addWidget(new QLabel("Angle (deg): ", this));
	m_angle = new QLineEdit(QString::number(slope, 'f', 2), this);
	angleRow->addWidget(m_angle);
	root->addLayout(angleRow);

	QHBoxLayout *buttons = new QHBoxLayout();
	QPushButton *previewButton = new QPushButton("Preview", this);
	previewButton->setAutoDefault(false);
	buttons->addWidget(previewButton);
	buttons->addStretch(1);
	QPushButton *okButton = new QPushButton("OK", this);
	okButton->setDefault(true);
	buttons->addWidget(okButton);
	QPushButton *cancelButton = new QPushButton("Cancel", this);
	cancelButton->setAutoDefault(false);
	buttons->addWidget(cancelButton);
	root->addLayout(buttons);
	connect(previewButton, SIGNAL(clicked()), this, SLOT(onPreview()));
	connect(okButton, SIGNAL(clicked()), this, SLOT(accept()));
	connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));
	connect(m_angle, SIGNAL(textChanged(QString)), this, SLOT(onAngleChanged()));

	resize(280, 110);
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
	: QDialog(parent)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle("Select Mactotexture");

	QVBoxLayout *root = new QVBoxLayout(this);
	m_list = new QListWidget(this);
	root->addWidget(m_list, 1);

	char buf[kNameCap];
	int count = WBQtMacrotexture_GetCount();
	for (int i = 0; i < count; i++)
	{
		buf[0] = 0;
		WBQtMacrotexture_GetName(i, buf, sizeof(buf));
		new QListWidgetItem(QString::fromLocal8Bit(buf), m_list);
	}
	m_list->sortItems(Qt::AscendingOrder);

	addOkCancel(root, this, false);
	connect(m_list, SIGNAL(currentRowChanged(int)), this, SLOT(onRowChanged(int)));

	resize(220, 300);
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
	: QDialog(parent)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle("Map Settings");

	QVBoxLayout *root = new QVBoxLayout(this);

	QHBoxLayout *nameRow = new QHBoxLayout();
	nameRow->addWidget(new QLabel("Map Name", this));
	m_mapName = new QLineEdit(this);
	nameRow->addWidget(m_mapName, 1);
	root->addLayout(nameRow);

	char buf[kNameCap];
	buf[0] = 0;
	WBQtMapSettings_GetMapName(buf, sizeof(buf));
	m_mapName->setText(QString::fromLocal8Bit(buf));

	QHBoxLayout *compRow = new QHBoxLayout();
	compRow->addWidget(new QLabel("Compression", this));
	m_compression = new QComboBox(this);
	int compCount = WBQtMapSettings_GetCompressionCount();
	for (int i = 0; i < compCount; i++)
	{
		buf[0] = 0;
		WBQtMapSettings_GetCompressionName(i, buf, sizeof(buf));
		m_compression->addItem(QString::fromLocal8Bit(buf));
	}
	m_compression->setCurrentIndex(WBQtMapSettings_GetCompressionIndex());
	compRow->addWidget(m_compression, 1);
	root->addLayout(compRow);

	QHBoxLayout *todRow = new QHBoxLayout();
	todRow->addWidget(new QLabel("Time", this));
	m_timeOfDay = new QComboBox(this);
	int todCount = WBQtMapSettings_GetTimeOfDayCount();
	for (int i = 0; i < todCount; i++)
	{
		buf[0] = 0;
		WBQtMapSettings_GetTimeOfDayName(i, buf, sizeof(buf));
		m_timeOfDay->addItem(QString::fromLocal8Bit(buf));
	}
	m_timeOfDay->setCurrentIndex(WBQtMapSettings_GetTimeOfDayIndex());
	todRow->addWidget(m_timeOfDay, 1);
	root->addLayout(todRow);

	QHBoxLayout *weatherRow = new QHBoxLayout();
	weatherRow->addWidget(new QLabel("Weather", this));
	m_weather = new QComboBox(this);
	int weatherCount = WBQtMapSettings_GetWeatherCount();
	for (int i = 0; i < weatherCount; i++)
	{
		buf[0] = 0;
		WBQtMapSettings_GetWeatherName(i, buf, sizeof(buf));
		m_weather->addItem(QString::fromLocal8Bit(buf));
	}
	m_weather->setCurrentIndex(WBQtMapSettings_GetWeatherIndex());
	weatherRow->addWidget(m_weather, 1);
	root->addLayout(weatherRow);

	addOkCancel(root, this, true);
	resize(360, 190);
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
	m_sidesList(sidesList),
	m_pickedSide(-1)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle("Select a valid Player");

	QVBoxLayout *root = new QVBoxLayout(this);
	char buf[kNameCap];
	buf[0] = 0;
	WBQtFixOwnerData_GetPrompt(teamsInfo, buf, sizeof(buf));
	QLabel *prompt = new QLabel(QString::fromLocal8Bit(buf), this);
	prompt->setWordWrap(true);
	root->addWidget(prompt);

	m_list = new QListWidget(this);
	int count = WBQtFixOwnerData_GetCount(sidesList);
	for (int i = 0; i < count; i++)
	{
		buf[0] = 0;
		WBQtFixOwnerData_GetDisplay(sidesList, i, buf, sizeof(buf));
		QListWidgetItem *item = new QListWidgetItem(QString::fromLocal8Bit(buf), m_list);
		item->setData(Qt::UserRole, i);	// side index survives the sort
	}
	m_list->sortItems(Qt::AscendingOrder);	// == the LBS_SORT display
	root->addWidget(m_list, 1);

	addOkCancel(root, this, true);
	resize(300, 240);
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
	: QDialog(parent)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle("Building Properties");

	QVBoxLayout *root = new QVBoxLayout(this);

	QGridLayout *grid = new QGridLayout();
	grid->addWidget(new QLabel("Name:", this), 0, 0);
	m_nameEdit = new QLineEdit(name, this);
	grid->addWidget(m_nameEdit, 0, 1);
	grid->addWidget(new QLabel("Script:", this), 1, 0);
	m_scriptCombo = new QComboBox(this);
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
	grid->addWidget(m_scriptCombo, 1, 1);
	grid->addWidget(new QLabel("Starting Health", this), 2, 0);
	m_healthEdit = new QLineEdit(QString::number(health), this);
	m_healthEdit->setFixedWidth(60);
	grid->addWidget(m_healthEdit, 2, 1, Qt::AlignLeft);
	root->addLayout(grid);

	m_unsellableCheck = new QCheckBox("Unsellable", this);
	m_unsellableCheck->setChecked(unsellable != 0);
	root->addWidget(m_unsellableCheck);

	addOkCancel(root, this, true);
	resize(340, 190);
}

// ===================== WBQtExportScriptsDialog =====================

WBQtExportScriptsDialog::WBQtExportScriptsDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle("Export Script Options");

	int waypoints = 0, triggers = 0, units = 0, teams = 0, sides = 0, allScripts = 0;
	WBQtExportScripts_Get(&waypoints, &triggers, &units, &teams, &sides, &allScripts);

	QVBoxLayout *root = new QVBoxLayout(this);

	QGroupBox *includeBox = new QGroupBox("Include items referenced in the scripts:", this);
	QVBoxLayout *includeLay = new QVBoxLayout(includeBox);
	m_waypoints = new QCheckBox("Include waypoints and waypoint paths.", includeBox);
	m_waypoints->setChecked(waypoints != 0);
	includeLay->addWidget(m_waypoints);
	m_triggers = new QCheckBox("Include trigger areas.", includeBox);
	m_triggers->setChecked(triggers != 0);
	includeLay->addWidget(m_triggers);
	m_units = new QCheckBox("Include units and buildings.", includeBox);
	m_units->setChecked(units != 0);
	includeLay->addWidget(m_units);
	m_teams = new QCheckBox("Include Teams", includeBox);
	m_teams->setChecked(teams != 0);
	includeLay->addWidget(m_teams);
	m_sides = new QCheckBox("Include Players.", includeBox);
	m_sides->setChecked(sides != 0);
	includeLay->addWidget(m_sides);
	root->addWidget(includeBox);

	QGroupBox *modeBox = new QGroupBox("Export Mode:", this);
	QVBoxLayout *modeLay = new QVBoxLayout(modeBox);
	m_allScripts = new QRadioButton("Export all scripts.", modeBox);
	modeLay->addWidget(m_allScripts);
	m_selectedScripts = new QRadioButton("Export selected scripts/folder.", modeBox);
	modeLay->addWidget(m_selectedScripts);
	m_allScripts->setChecked(allScripts != 0);
	m_selectedScripts->setChecked(allScripts == 0);
	root->addWidget(modeBox);

	addOkCancel(root, this, true);
	resize(320, 250);
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
	// Run a modal QDialog over the MFC frame (disabled for its lifetime == DoModal), app-modal
	// so any Qt windows are fenced too.
	int runModal(QDialog &dlg, void *frameHwnd)
	{
		dlg.setWindowModality(Qt::ApplicationModal);
		HWND frame = reinterpret_cast<HWND>(frameHwnd);
		if (frame != NULL)
		{
			::EnableWindow(frame, FALSE);
		}
		int rc = dlg.exec();
		if (frame != NULL)
		{
			::EnableWindow(frame, TRUE);
		}
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
