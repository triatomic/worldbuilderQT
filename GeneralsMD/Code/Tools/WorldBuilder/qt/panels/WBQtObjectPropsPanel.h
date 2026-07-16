// WBQtObjectPropsPanel.h -- Qt replacement for the MFC MapObjectProps dialog.
//
// The big object-properties panel: edits the Dict of the selected map object(s) via the hidden
// MFC MapObjectProps dialog (TheMapObjectProps), which stays the owner of the working Dict(s) and
// the toggle-OFF fallback. This is the RTS_HAS_QT path -- a top-level Qt::Tool window owned by the
// shared QWinWidget bridge. It has no state of its own; every control reads/writes through the
// WBQtObjectProps_* facade. MapObjectProps::updateTheUI() re-seeds it via WBQtObjectProps_PushRefresh().
//
// Phase 1: General section (object name, owning team).
// Phase 2: Logical section (starting health, hit points, aggressiveness, veterancy, the seven
// object flags, and the vision / shroud / stopping distances).
// Later phases add the Visual, Sound and build-with-upgrades sections.
#ifndef WB_QT_OBJECTPROPS_PANEL_H
#define WB_QT_OBJECTPROPS_PANEL_H

#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QEvent;
class QKeyEvent;
class QLineEdit;
class QListWidget;
class QPushButton;
class WBQtScrubSpinBox;

namespace Ui { class WBQtObjectPropsPanel; }	// generated from WBQtObjectPropsPanel.ui

class WBQtObjectPropsPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtObjectPropsPanel(QWidget *owner);
	virtual ~WBQtObjectPropsPanel();

	// Re-seed every control from the current MFC selection (WBQtObjectProps_PushRefresh).
	void pushRefresh();

	static WBQtObjectPropsPanel *instance() { return s_instance; }

protected:
	// Bare Delete/Backspace anywhere in this panel deletes the selected map object (== the MFC
	// view's OnEditDelete). The Qt inversion's focus gate only routes those keys when the
	// viewport or main window owns focus, so with this panel focused they would otherwise be
	// lost. An app-level event filter catches the key before whatever child widget has focus
	// (a combo / list / checkbox) can swallow it.
	bool eventFilter(QObject *watched, QEvent *event);

private slots:
	void onNameChanged();
	void onTeamChanged(int index);
	// Logical section.
	void onHealthChanged(int index);
	void onHealthEditChanged();
	void onMaxHPsChanged();
	void onAggressivenessChanged(int index);
	void onVeterancyChanged(int index);
	void onFlagToggled();
	void onTargetingChanged();
	void onShroudChanged();
	void onStoppingChanged();
	// Visual section.
	void onWeatherChanged(int index);
	void onTimeChanged(int index);
	void onPositionChanged();
	void onZChanged();
	void onAngleChanged();
	void onPosScrubStarted();
	void onPosScrubFinished();
	// Sound section.
	void onSoundChanged(int index);
	void onListenClicked();
	void onSoundFlagToggled();
	void onLoopCountChanged();
	void onPriorityChanged(int index);
	void onVolumeChanged();
	void onMinVolumeChanged();
	void onMinRangeChanged();
	void onMaxRangeChanged();
	// Pre-built upgrades.
	void onUpgradeSelectionChanged();

private:
	void rebuildTeams();	// repopulate the team combo from the bridge
	void applyFlag(int flagId, QCheckBox *box);
	void rebuildSoundList();	// enumerate the (large) attached-sound combo once

	Ui::WBQtObjectPropsPanel *m_ui;	// owns the static widget tree (WBQtObjectPropsPanel.ui)

	QLabel    *m_selectionLabel;	// "No Selection" / "N objects" / the object name
	QGroupBox *m_generalBox;
	QLineEdit *m_name;
	QComboBox *m_team;
	QComboBox *m_script;	// disabled, matches the MFC "Script:" row

	// Logical section.
	QGroupBox      *m_logicalBox;
	QComboBox      *m_health;		// 0% / 25% / 50% / 75% / 100% / Other
	QLineEdit      *m_healthEdit;	// the "Other" value (enabled only when Other is selected)
	QComboBox      *m_maxHPs;		// "Default For Unit" or an explicit value
	QComboBox      *m_aggressiveness;
	QComboBox      *m_veterancy;
	QCheckBox      *m_enabled;
	QCheckBox      *m_indestructible;
	QCheckBox      *m_unsellable;
	QCheckBox      *m_targetable;
	QCheckBox      *m_powered;
	QCheckBox      *m_recruitableAI;
	QCheckBox      *m_selectable;	// tri-state (default = partially checked)
	QGroupBox      *m_distanceBox;
	QLineEdit      *m_stopping;		// Stopping distance (real; edit like the MFC ES_AUTOHSCROLL)
	QLineEdit      *m_targeting;	// "Targeting" == the object's vision/visual range (int)
	QLineEdit      *m_shroud;		// Shroud clearing distance (int)

	// Visual section. Z and Angle use a scrub-spinbox: click-drag horizontally to scrub the value
	// (the drag-to-change affordance mirroring the MFC WBPopupSlider button beside those edits).
	QGroupBox       *m_visualBox;
	QLineEdit       *m_xyPos;		// "x, y"
	WBQtScrubSpinBox *m_zOffset;	// range -100..100 (matches IDC_HEIGHT_POPUP)
	QComboBox       *m_weather;
	WBQtScrubSpinBox *m_angle;		// range 0..360 (matches IDC_ANGLE_POPUP)
	QComboBox       *m_time;

	// Sound section.
	QGroupBox   *m_soundBox;
	QComboBox   *m_sound;			// attached-sound combo (mirrors the large MFC combo)
	QPushButton *m_listen;			// preview toggle (Listen / Stop)
	QCheckBox   *m_customize;
	QCheckBox   *m_soundEnabled;
	QCheckBox   *m_looping;
	QLineEdit   *m_loopCount;
	QComboBox   *m_priority;
	QLineEdit   *m_volume;
	QLineEdit   *m_minVolume;
	QLineEdit   *m_minRange;
	QLineEdit   *m_maxRange;
	bool         m_soundListBuilt;	// the sound combo is enumerated once (it holds thousands)

	// Pre-built upgrades.
	QGroupBox   *m_upgradesBox;
	QListWidget *m_upgrades;		// multi-select list of grantable upgrades

	bool m_updating;	// re-entrancy guard, mirrors MFC MapObjectProps::m_updating

	static WBQtObjectPropsPanel *s_instance;
};

#endif // WB_QT_OBJECTPROPS_PANEL_H
