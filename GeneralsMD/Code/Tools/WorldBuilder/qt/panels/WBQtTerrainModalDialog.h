// WBQtTerrainModalDialog.h -- the native Qt "missing terrain texture" picker (Tier 5b),
// rebuilding TerrainModal over the WBQtTerrainModalBridge catalog; the swatch preview
// reuses the WBQtTerrainMaterial pixel bridge. Run via WBQtTerrainModal_Run.
#ifndef WB_QT_TERRAINMODAL_DIALOG_H
#define WB_QT_TERRAINMODAL_DIALOG_H

#include <QDialog>

class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace Ui { class WBQtTerrainModalDialog; }	// generated from WBQtTerrainModalDialog.ui

class WBQtTerrainModalDialog : public QDialog
{
	Q_OBJECT
public:
	WBQtTerrainModalDialog(const QString &missingPath, QWidget *parent = 0);
	virtual ~WBQtTerrainModalDialog();

	int pickedIndex() const { return m_picked; }

private slots:
	void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void onSearch();
	void onSearchLive(const QString &text);	// NewSearch: live filter, no beep / no message box
	void onReset();

private:
	int populate(const QString &filter);	// (re)build the tree; returns the number of leaves added
	void selectTexClass(int texClass);
	void refreshPreview(int texClass);

	Ui::WBQtTerrainModalDialog *m_ui;	// owns the static widget tree (WBQtTerrainModalDialog.ui)

	int m_picked;
	QTreeWidget *m_tree;
	QLineEdit *m_searchEdit;
	QLabel *m_nameLabel;
	QLabel *m_preview;
};

#endif // WB_QT_TERRAINMODAL_DIALOG_H
