// WBQtMapGenDialog.h -- the Map Generator settings modal.
//
// Engine access goes through the C facade in WBQtMapGenBridge.h; the dialog is
// run via WBQtMapGen_Run, which returns the chosen settings through out-params.
#ifndef WB_QT_MAP_GEN_DIALOG_H
#define WB_QT_MAP_GEN_DIALOG_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;

// generated from the .ui file
namespace Ui
{
	class WBQtMapGenDialog;
}

/*************************************************************************/
/**                        WBQtMapGenDialog
	Settings for a map generation run.
***************************************************************************/
class WBQtMapGenDialog : public QDialog
{
	Q_OBJECT
public:
	WBQtMapGenDialog(int seed, int numPlayers, int baseHeight,
									 int doCliffs, int cliffDensity, int doTextures,
									 int doTrees, int treeDensity, int doRocks, QWidget *parent = 0);
	virtual ~WBQtMapGenDialog();

	int seed(void) const;
	int numPlayers(void) const;
	int baseHeight(void) const;
	int doCliffs(void) const;
	int cliffDensity(void) const;
	int doTextures(void) const;
	int doTrees(void) const;
	int treeDensity(void) const;
	int doRocks(void) const;

private slots:
	void onRandomize();
	void onCliffsToggled(bool checked);
	void onPlayersChanged(int index);
	void onTreesToggled(bool checked);

private:
	Ui::WBQtMapGenDialog *m_ui;
	QSpinBox *m_seed;
	QComboBox *m_players;
	QSpinBox *m_baseHeight;
	QCheckBox *m_doCliffs;
	QComboBox *m_cliffDensity;
	QCheckBox *m_doTextures;
	QCheckBox *m_doTrees;
	QComboBox *m_treeDensity;
	QCheckBox *m_doRocks;
	QLabel *m_sizeNote;

	/// Updates the note about the map being enlarged for the player count.
	void refreshSizeNote();
};

#endif // WB_QT_MAP_GEN_DIALOG_H
