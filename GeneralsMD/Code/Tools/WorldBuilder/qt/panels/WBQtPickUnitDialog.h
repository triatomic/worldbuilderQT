// WBQtPickUnitDialog.h -- the native Qt "Pick A Unit" / "Replace Missing Unit" dialogs
// (Tier 3e), rebuilding PickUnitDialog/ReplaceUnitDialog over the WBQtPickUnitBridge
// catalog. Run via the WBQtPickUnit_Run/WBQtReplaceUnit_Run entry points.
#ifndef WB_QT_PICKUNIT_DIALOG_H
#define WB_QT_PICKUNIT_DIALOG_H

#include <QDialog>

class QLabel;
class QLineEdit;
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

	void accept();	// == OK: closes even with a folder/nothing selected (empty pick)

protected:
	void moveEvent(QMoveEvent *event);

private slots:
	void onSearch();
	void onReset();
	void onIgnore();
	void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

private:
	int populate(const QString &filter);	// returns the number of leaves added
	void refreshPreview(const QString &name);

	bool m_replaceMode;
	QString m_pickedName;
	QLineEdit *m_searchEdit;
	QTreeWidget *m_tree;
	QLabel *m_preview;
};

#endif // WB_QT_PICKUNIT_DIALOG_H
