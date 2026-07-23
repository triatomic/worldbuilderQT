// WBQtPickUnitDialog.h -- the native Qt "Pick A Unit" / "Replace Missing Unit" dialogs
// (Tier 3e), rebuilding PickUnitDialog/ReplaceUnitDialog over the WBQtPickUnitBridge
// catalog. Run via the WBQtPickUnit_Run/WBQtReplaceUnit_Run entry points.
#ifndef WB_QT_PICKUNIT_DIALOG_H
#define WB_QT_PICKUNIT_DIALOG_H

#include <QDialog>
#include <QVector>

#include "WBQtNameMatch.h"	// MatchCursor: the Find Next / "^" match stepper

class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace Ui { class WBQtPickUnitDialog; }	// generated from WBQtPickUnitDialog.ui

class WBQtPickUnitDialog : public QDialog
{
	Q_OBJECT
public:
	// Replace mode == IDD_REPLACEUNIT: missing-name label on top, a "Continue without
	// replacing..." button that finishes with code 2 (== IDIGNORE), and no search row
	// (the MFC replace dialog hooks no search buttons). The model preview shows in both
	// modes, like the MFC ObjectPreview.
	WBQtPickUnitDialog(bool replaceMode, const QString &missingName, QWidget *parent = 0);
	virtual ~WBQtPickUnitDialog();

	QString pickedName() const { return m_pickedName; }

	// BuildListTool's modeless panel mode (== PickUnitDialog::SetupAsPanel): a floating
	// Qt::Tool window with no Cancel button, shown without activating; the pick is the LIVE
	// tree selection (currentLeafName), and populate() rebuilds the shared bridge catalog
	// with these filters first (a modal pick/replace dialog may have re-filtered it since).
	void setupAsPanel(const int *allowable, int allowCount, int factionOnly);
	QString currentLeafName() const;

	void accept();	// == OK: closes even with a folder/nothing selected (empty pick)

protected:
	void moveEvent(QMoveEvent *event);

private slots:
	void onSearch();
	void onSearchLive(const QString &text);	// NewSearch: live filter, no beep / no message box
	void onReset();
	void onIgnore();
	void onFindNextMatch();	// replace mode: step to the next close name match
	void onFindPrevMatch();	// replace mode: step to the previous close name match
	void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

private:
	int populate(const QString &filter);	// returns the number of leaves added
	void refreshPreview(const QString &name);

	Ui::WBQtPickUnitDialog *m_ui;	// owns the static widget tree (WBQtPickUnitDialog.ui)

	bool m_replaceMode;
	bool m_panelMode;
	QVector<int> m_panelAllowable;
	int m_panelFactionOnly;
	QString m_pickedName;
	QString m_missingName;	// replace mode: the name being replaced (drives the match list)
	// Replace mode: the close name matches (best-first) for "Find Next" / "^" to cycle.
	WBQtNameMatch::MatchCursor m_matches;
	QLineEdit *m_searchEdit;
	QTreeWidget *m_tree;
	QLabel *m_preview;
	QPushButton *m_cancelButton;
	QPushButton *m_findNextButton;
	QPushButton *m_findPrevButton;
};

#endif // WB_QT_PICKUNIT_DIALOG_H
