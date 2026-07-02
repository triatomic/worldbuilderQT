// WBQtMiscModals.h -- the native Qt Tier 3a workflow modals (ShadowOptions, ImpassableOptions,
// SelectMacrotexture, MapSettings, ExportScriptsOptions). All engine access goes through the C
// facade in WBQtMiscModalsBridge.h; each is run via its WBQt..._Run entry point.
#ifndef WB_QT_MISC_MODALS_H
#define WB_QT_MISC_MODALS_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QRadioButton;

// Shadow Options: R/G/B/intensity fields (0..1), applied live to the shadow manager.
class WBQtShadowDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtShadowDialog(QWidget *parent = 0);

private slots:
	void onFieldChanged();

private:
	void apply();
	bool m_updating;
	QLineEdit *m_red;
	QLineEdit *m_green;
	QLineEdit *m_blue;
	QLineEdit *m_intensity;
};

// Impassable Options: show-slope angle + Preview; the overlay is forced on for the lifetime.
class WBQtImpassableDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtImpassableDialog(QWidget *parent = 0);

private slots:
	void onAngleChanged();
	void onPreview();

private:
	QLineEdit *m_angle;
};

// Select Macrotexture: a list of .tga names + "***Default"; applied live on selection.
class WBQtMacrotextureDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtMacrotextureDialog(QWidget *parent = 0);

private slots:
	void onRowChanged(int row);

private:
	QListWidget *m_list;
};

// Map Settings: time-of-day / weather / compression combos + map name.
class WBQtMapSettingsDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtMapSettingsDialog(QWidget *parent = 0);

	void accept();

private:
	QComboBox *m_timeOfDay;
	QComboBox *m_weather;
	QComboBox *m_compression;
	QLineEdit *m_mapName;
};

// Fix Team Owner: pick a valid player for an orphaned team (== CFixTeamOwnerDialog). Rows
// carry the side index as item data, so the sorted display maps to the right player (fixes
// the MFC sorted-listbox/unsorted-sides index bug).
class WBQtFixTeamOwnerDialog : public QDialog
{
	Q_OBJECT
public:
	WBQtFixTeamOwnerDialog(void *teamsInfo, void *sidesList, QWidget *parent = 0);

	void accept();
	int pickedSide() const { return m_pickedSide; }	// -1 = none

private:
	void *m_sidesList;
	QListWidget *m_list;
	int m_pickedSide;
};

// Building Properties (== BaseBuildProps): name / subroutine-script / health / unsellable.
class WBQtBaseBuildPropsDialog : public QDialog
{
	Q_OBJECT
public:
	WBQtBaseBuildPropsDialog(const QString &name, const QString &script, int health, int unsellable, QWidget *parent = 0);

	QLineEdit *m_nameEdit;
	QComboBox *m_scriptCombo;
	QLineEdit *m_healthEdit;
	QCheckBox *m_unsellableCheck;
};

// Export Scripts Options: the six export flags + the all/selected radio.
class WBQtExportScriptsDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtExportScriptsDialog(QWidget *parent = 0);

	void accept();

private:
	QCheckBox *m_waypoints;
	QCheckBox *m_triggers;
	QCheckBox *m_units;
	QCheckBox *m_teams;
	QCheckBox *m_sides;
	QRadioButton *m_allScripts;
	QRadioButton *m_selectedScripts;
};

#endif // WB_QT_MISC_MODALS_H
