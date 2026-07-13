// WBQtMapFileDialogs.h -- the native Qt Open Map / Save Map pickers (Tier 3d). Open Map fronts
// the hidden MFC OpenMap dialog (system/user/packed-.big logic reused verbatim); Save Map is
// native over the small data bridge. Both run via their WBQt..._Run entry points in
// WBQtMapFileBridge.h.
#ifndef WB_QT_MAPFILE_DIALOGS_H
#define WB_QT_MAPFILE_DIALOGS_H

#include <QDialog>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

class WBQtOpenMapDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtOpenMapDialog(QWidget *parent = 0);

	void accept();	// == OK: pick the current row; stays open when the pick only drills

private slots:
	void onModeClicked();
	void onFind();
	void onReset();
	void onBrowse();
	void onDoubleClicked();
	void onSelectionChanged();

private:
	void reload();
	void updatePreview();

	bool m_updating;
	QPushButton *m_packedButton;
	QPushButton *m_userButton;
	QPushButton *m_systemButton;
	QLineEdit *m_searchEdit;
	QListWidget *m_list;
	QLabel *m_preview;
	QPushButton *m_okButton;
};

class WBQtSaveMapDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtSaveMapDialog(const QString &initialFilename, QWidget *parent = 0);

	void accept();	// == OK: overwrite confirmation may keep the dialog open

	bool browseRequested() const { return m_browse; }
	bool usingSystemDir() const { return m_systemDir; }
	QString mapName() const;

private slots:
	void onModeClicked();
	void onListSelectionChanged();
	void onBrowse();

private:
	void reload();

	bool m_updating;
	bool m_browse;
	bool m_systemDir;
	QPushButton *m_userButton;
	QPushButton *m_systemButton;
	QListWidget *m_list;
	QLineEdit *m_nameEdit;
};

#endif // WB_QT_MAPFILE_DIALOGS_H
