// WBQtPickUnitDialog.h -- the native Qt "Pick A Unit" / "Replace Missing Unit" dialogs
// (Tier 3e), rebuilding PickUnitDialog/ReplaceUnitDialog over the WBQtPickUnitBridge
// catalog. Run via the WBQtPickUnit_Run/WBQtReplaceUnit_Run entry points.
#ifndef WB_QT_PICKUNIT_DIALOG_H
#define WB_QT_PICKUNIT_DIALOG_H

#include <QDialog>
#include <QVector>

class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class WBQtPickUnitDialog : public QDialog
{
	Q_OBJECT
public:
	// Replace mode == IDD_REPLACEUNIT: missing-name label on top, a "Continue without
	// replacing..." button that finishes with code 2 (== IDIGNORE), and no search row
	// (the MFC replace dialog hooks no search buttons). The model preview shows in both
	// modes, like the MFC ObjectPreview.
	WBQtPickUnitDialog(bool replaceMode, const QString &missingName, QWidget *parent = 0);

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
	void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

private:
	int populate(const QString &filter);	// returns the number of leaves added
	void refreshPreview(const QString &name);

	bool m_replaceMode;
	bool m_panelMode;
	QVector<int> m_panelAllowable;
	int m_panelFactionOnly;
	QString m_pickedName;
	QLineEdit *m_searchEdit;
	QTreeWidget *m_tree;
	QLabel *m_preview;
	QPushButton *m_cancelButton;
};

#endif // WB_QT_PICKUNIT_DIALOG_H
