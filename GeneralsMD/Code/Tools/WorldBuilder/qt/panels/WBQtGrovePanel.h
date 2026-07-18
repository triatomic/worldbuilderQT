// WBQtGrovePanel.h -- Qt replacement for the MFC GroveOptions dialog (the grove config panel).
//
// The grove tool scatters a mix of tree/prop templates. This panel reproduces every MFC control:
// a "set name" combo (20 named presets), 11 tree-type rows (each a template combo + a weight
// spinbox), a running weight-total display, a number-of-trees spinbox, three placement checkboxes
// (allow water / allow cliff / use props only), Save + Settings buttons, and a rendered object
// preview of the last-touched tree combo. It edits none of its own state -- every control drives
// the hidden MFC GroveOptions controls (via the bridge), because GroveTool reads its choices back
// out of those controls through TheGroveOptions. The MFC GroveOptions stays as the toggle-OFF
// fallback; this is the RTS_HAS_QT path. A top-level Qt::Tool window owned by the shared QWinWidget
// bridge.
#ifndef WB_QT_GROVE_PANEL_H
#define WB_QT_GROVE_PANEL_H

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;

namespace Ui { class WBQtGrovePanel; }	// generated from WBQtGrovePanel.ui

class WBQtGrovePanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtGrovePanel(QWidget *owner);
	virtual ~WBQtGrovePanel();

	// Re-seed weights/total/combos/preview from the current MFC state (WBQtGrove_PushRefresh),
	// e.g. after a set load pulls in a whole new tree makeup.
	void pushRefresh();

	static WBQtGrovePanel *instance() { return s_instance; }

protected:
	// Re-read Grovesets.ini into the set-name combo each time the panel is shown, so a set
	// renamed (via the Settings button / Notepad) while the panel was hidden shows up.
	virtual void showEvent(QShowEvent *event);

private slots:
	void onSetNameChanged(int index);
	void onTreeTypeChanged(int index);	// which row is inferred from the sender
	void onWeightChanged(int value);	// which row is inferred from the sender
	void onNumTreesChanged(int value);
	void onAllowWaterToggled();
	void onAllowCliffToggled();
	void onUsePropsOnlyToggled();
	void onSaveSet();
	void onOpenSettings();

private:
	enum { TREES_PER_SET = 11 };

	void fillTreeCombo(int type);		// (re)populate one tree-type combo from the bridge
	void fillSetCombo();				// (re)populate the set-name combo from the bridge
	void seedFromMfc();					// pull every control's state from the hidden MFC panel
	void refreshTotal();				// re-display the running weight total
	void refreshPreview();
	int  rowOfSender(QObject *sender);	// map a signalling child widget back to its 1..11 slot

	Ui::WBQtGrovePanel *m_ui;	// owns the static widget tree (WBQtGrovePanel.ui)

	QComboBox *m_setName;
	QComboBox *m_treeType[TREES_PER_SET];
	QSpinBox  *m_weight[TREES_PER_SET];
	QLabel    *m_total;
	QSpinBox  *m_numTrees;
	QCheckBox *m_allowWater;
	QCheckBox *m_allowCliff;
	QCheckBox *m_usePropsOnly;
	QLabel    *m_preview;

	bool m_updating;	// re-entrancy guard while seeding controls

	static WBQtGrovePanel *s_instance;
};

#endif // WB_QT_GROVE_PANEL_H
