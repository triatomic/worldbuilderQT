// WBQtTeamSheetDialog.h -- the native Qt team template sheet (Tier 3b-3), replacing the MFC
// CPropertySheet of TeamIdentity / TeamReinforcement / TeamBehavior / TeamGeneric. Four HIDDEN
// MFC pages stay the logic owners (their handlers write the team dict live); this dialog
// mirrors their controls and drives them through the generic page facade in WBQtTeamsBridge.h
// (set control -> send the real WM_COMMAND notification). Edits are live against the Teams
// dialog's working copy -- like the MFC sheet, closing never reverts (the outer Teams dialog's
// Cancel does).
#ifndef WB_QT_TEAMSHEET_DIALOG_H
#define WB_QT_TEAMSHEET_DIALOG_H

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;

namespace Ui { class WBQtTeamSheetDialog; }	// generated from WBQtTeamSheetDialog.ui

class WBQtTeamSheetDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtTeamSheetDialog(QWidget *parent = 0);
	virtual ~WBQtTeamSheetDialog();

	// Persist the window size on ANY close path (OK, Esc, the X button all funnel here).
	virtual void done(int r);

private:
	// wire the .ui widgets of each tab to their hidden page controls
	void setupIdentityTab();
	void setupReinforcementTab();
	void setupBehaviorTab();
	void setupGenericTab();
	// Re-seed the 16 generic-script combos from the compacted hook chain and hide the rows
	// past the first empty slot (== TeamGeneric::_dictToScripts).
	void refreshGenericScripts();

	// binding helpers (each wires the widget to the hidden page control); the
	// widget-creating overloads serve the dynamic member/script row loops
	QLineEdit *bindEdit(int page, int ctrlId, QWidget *parent, int notify);
	void bindEdit(int page, int ctrlId, QLineEdit *edit, int notify);
	void bindCheck(int page, int ctrlId, QCheckBox *check);
	QComboBox *bindCombo(int page, int ctrlId, const QStringList &items, QWidget *parent, int notify);
	void bindCombo(int page, int ctrlId, const QStringList &items, QComboBox *combo, int notify);
	QStringList readComboItems(int page, int ctrlId) const;

	Ui::WBQtTeamSheetDialog *m_ui;	// owns the static widget tree (WBQtTeamSheetDialog.ui)

	QLineEdit *m_nameEdit;
	QComboBox *m_unitCombos[7];
	QComboBox *m_genericCombos[16];		// the 16 generic-script combos (for live compaction)
	QLabel    *m_genericLabels[16];		// their row labels (hidden together with the combos)
};

#endif // WB_QT_TEAMSHEET_DIALOG_H
