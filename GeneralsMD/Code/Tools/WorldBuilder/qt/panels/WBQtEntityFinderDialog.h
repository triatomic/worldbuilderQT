// WBQtEntityFinderDialog.h -- the native Qt "Help / Entity Finder / Shortcut Finder"
// window (Tier 5a), rebuilding the modeless CAboutDlg (WorldBuilder.cpp) over the
// WBQtEntityFinderBridge facade. A cached singleton: opened/raised by
// WBQtEntityFinder_Open, hidden on close.
#ifndef WB_QT_ENTITYFINDER_DIALOG_H
#define WB_QT_ENTITYFINDER_DIALOG_H

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

namespace Ui { class WBQtEntityFinderDialog; }	// generated from WBQtEntityFinderDialog.ui

class WBQtEntityFinderDialog : public QWidget
{
	Q_OBJECT
public:
	explicit WBQtEntityFinderDialog(void *frameHwnd);
	virtual ~WBQtEntityFinderDialog();

	static WBQtEntityFinderDialog *instance() { return s_instance; }

	void refreshFinders();
	bool ownsWin32Focus() const;

protected:
	void moveEvent(QMoveEvent *event);
	void closeEvent(QCloseEvent *event);

private slots:
	void onFindHotkey();
	void onFindObject();
	void onRefreshObjects();
	void onFindWaypoint();
	void onRefreshWaypoints();
	void onOpenDiscord();
	void onLaunchOnStartupToggled(bool on);
	void onFontChanged(int index);
	void onResolutionChanged(int index);
	void onMaxUndosChanged(int value);
	void onNewSearchToggled(bool on);
	void onTutorialPromptsToggled(bool on);
	void onRenderParticlesToggled(bool on);
	void onToggleHotkeyPanel();

private:
	void populateFonts();
	void populateResolutions();
	void applyHotkeyPanelState(bool visible);

	static WBQtEntityFinderDialog *s_instance;

	Ui::WBQtEntityFinderDialog *m_ui;	// owns the static widget tree (WBQtEntityFinderDialog.ui)

	QComboBox *m_objectCombo;
	QComboBox *m_waypointCombo;
	QComboBox *m_fontCombo;
	QComboBox *m_resolutionCombo;
	QSpinBox  *m_undoSpin;
	QCheckBox *m_launchCheck;
	QCheckBox *m_newSearchCheck;
	QCheckBox *m_tutorialPromptsCheck;
	QCheckBox *m_renderParticlesCheck;
	QPushButton *m_toggleButton;
	QWidget *m_hotkeyPanel;
	QLineEdit *m_searchEdit;
	QPlainTextEdit *m_hotkeyText;
};

#endif // WB_QT_ENTITYFINDER_DIALOG_H
