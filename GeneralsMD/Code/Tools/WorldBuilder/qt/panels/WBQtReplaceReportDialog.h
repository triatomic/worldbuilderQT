// WBQtReplaceReportDialog.h -- the "Replaced Missing Units" report.
//
// Shown after a "Replace All by name match" pass: one row per missing template name, what it was
// replaced with, and how many objects carried it. Selecting a row selects those objects on the map
// and centres the view on the first, so a wrong guess can be seen in place. "Change Replacement"
// reopens the normal pick dialog for that row and re-applies live, and clearing a replacement puts
// the original missing name back.
//
// Run via WBQtReplaceReport_Run; the rows themselves live on the MFC side
// (src/WBQtPickUnitBridge.cpp), which owns the map objects being re-pointed.
#ifndef WB_QT_REPLACE_REPORT_DIALOG_H
#define WB_QT_REPLACE_REPORT_DIALOG_H

#include <QDialog>

namespace Ui { class WBQtReplaceReportDialog; }	// generated from WBQtReplaceReportDialog.ui

class QTreeWidgetItem;

class WBQtReplaceReportDialog : public QDialog
{
	Q_OBJECT

public:
	explicit WBQtReplaceReportDialog(QWidget *parent = 0);
	virtual ~WBQtReplaceReportDialog();

private slots:
	void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void onChangeReplacement();

private:
	void reload();
	void refreshSummary();
	int currentRow() const;

	Ui::WBQtReplaceReportDialog *m_ui;
};

#endif // WB_QT_REPLACE_REPORT_DIALOG_H
