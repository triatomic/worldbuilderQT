// WBQtMapIniReportPrivate.h -- the QDialog for the map.ini report viewer. Separate from the
// public WBQtMapIniReport.h (a pure C facade) so this Qt header is only ever seen by the Qt
// TU (WBQtMapIniReport.cpp), and AUTOMOC generates the moc the standard header-class way.
#ifndef WB_QT_MAPINI_REPORT_PRIVATE_H
#define WB_QT_MAPINI_REPORT_PRIVATE_H

#include <QDialog>
#include <QString>

class QLineEdit;
class QTreeWidget;

namespace Ui { class WBQtMapIniReportDialog; }	// generated from WBQtMapIniReportDialog.ui

class WBQtMapIniReportDialog : public QDialog
{
	Q_OBJECT
public:
	// applyMode true -> OK/Cancel (caller applies on OK); false -> a single Close button.
	WBQtMapIniReportDialog(const QString &title, const QString &text, bool applyMode,
		QWidget *parent = 0);
	virtual ~WBQtMapIniReportDialog();

private slots:
	void onFilterChanged(const QString &text);
	void onExpandAll();
	void onCollapseAll();
	void onCopy();

private:
	// Parse the report into (header, body-lines) sections keyed off its ';'-comment headers.
	void buildTree(const QString &text);

	Ui::WBQtMapIniReportDialog *m_ui;	// owns the static widget tree (WBQtMapIniReportDialog.ui)

	QLineEdit *m_filter;
	QTreeWidget *m_tree;
	QString m_rawText;	// the full report, for Copy
};

#endif // WB_QT_MAPINI_REPORT_PRIVATE_H
