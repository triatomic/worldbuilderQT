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
class QLineEdit;

class WBQtTeamSheetDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtTeamSheetDialog(QWidget *parent = 0);

	// Persist the window size on ANY close path (OK, Esc, the X button all funnel here).
	virtual void done(int r);

private:
	QWidget *buildIdentityTab();
	QWidget *buildReinforcementTab();
	QWidget *buildBehaviorTab();
	QWidget *buildGenericTab();

	// binding helpers (each wires the widget to the hidden page control)
	QLineEdit *bindEdit(int page, int ctrlId, QWidget *parent, int notify);
	QCheckBox *bindCheck(int page, int ctrlId, const QString &label, QWidget *parent);
	QComboBox *bindCombo(int page, int ctrlId, const QStringList &items, QWidget *parent, int notify);
	QStringList readComboItems(int page, int ctrlId) const;

	QLineEdit *m_nameEdit;
	QComboBox *m_unitCombos[7];
};

#endif // WB_QT_TEAMSHEET_DIALOG_H
