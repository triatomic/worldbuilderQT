// WBQtReplaceReportDialog.h -- the "Replaced Missing Units" report.
//
// Shown after a "Replace All by name match" pass: one row per missing template name, what it was
// replaced with, and how many objects carried it. Selecting a row selects those objects on the map
// and centres the view on the first, so a wrong guess can be seen in place. "Change Replacement"
// reopens the normal pick dialog for that row and re-applies live, and clearing a replacement puts
// the original missing name back. "Find Next" / "^" (F3 / Shift+F3) walk the rows in order with
// wrap-around -- the same stepper convention as the Replace Missing Unit dialog -- so a long
// report can be reviewed from the keyboard: F3, judge the selection on the map, F3 again.
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
	void onFindNextRow();	// F3 / "Find Next": step to the next report row
	void onFindPrevRow();	// Shift+F3 / "^": step to the previous report row

private:
	void reload();
	void refreshSummary();
	int currentRow() const;
	void stepRow(int dir);	// move the selection by dir (+1/-1) with wrap-around

	Ui::WBQtReplaceReportDialog *m_ui;
};

#endif // WB_QT_REPLACE_REPORT_DIALOG_H
