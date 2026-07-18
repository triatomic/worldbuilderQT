// WBQtPlayerListDialog.h -- the native Qt Player List dialog (Tier 3b-1) and its Add-Player
// sub-dialog. The hidden MFC PlayerListDlg stays the model owner (working-copy SidesList,
// rename/team fixups, ally-enemy dedup, one-undoable commit); this dialog reads state back
// from it and pushes edits through the C facade in WBQtPlayerListBridge.h after every change.
#ifndef WB_QT_PLAYERLIST_DIALOG_H
#define WB_QT_PLAYERLIST_DIALOG_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace Ui { class WBQtPlayerListDialog; }	// generated from WBQtPlayerListDialog.ui
namespace Ui { class WBQtAddPlayerDialog; }	// generated from WBQtAddPlayerDialog.ui

class WBQtPlayerListDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtPlayerListDialog(QWidget *parent = 0);
	virtual ~WBQtPlayerListDialog();

private slots:
	void onPlayerRowChanged(int row);
	void onSetName();
	void onDisplayNameEdited(const QString &text);
	void onIsComputerToggled(bool checked);
	void onFactionChanged(int index);
	void onFactionTextCommitted();
	void onColorComboChanged(int index);
	void onColorButton();
	void onAlliesChanged();
	void onEnemiesChanged();
	void onNewPlayer();
	void onRemovePlayer();
	void onAddSkirmishPlayers();

private:
	void refreshAll();
	QString relationMask(QListWidget *list) const;

	Ui::WBQtPlayerListDialog *m_ui;	// owns the static widget tree (WBQtPlayerListDialog.ui)

	bool m_updating;
	QListWidget *m_players;
	QLineEdit *m_nameEdit;
	QLineEdit *m_displayNameEdit;
	QCheckBox *m_isComputerCheck;
	QComboBox *m_factionCombo;
	QPushButton *m_colorButton;
	QComboBox *m_colorCombo;
	QListWidget *m_allies;
	QListWidget *m_enemies;
	QListWidget *m_regardOut;
	QListWidget *m_regardIn;
	QPushButton *m_newButton;
	QPushButton *m_removeButton;
};

// The Add-Player picker (== AddPlayerDialog): a sorted template combo; OK commits the pick and
// hands the template name back via addedTemplate(). onlySide (Tier 5b) filters the list to one
// side's templates, like AddPlayerDialog's m_side in the object-placement auto-add flow.
class WBQtAddPlayerDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtAddPlayerDialog(QWidget *parent = 0, const QString &onlySide = QString());
	virtual ~WBQtAddPlayerDialog();

	void accept();
	QString addedTemplate() const { return m_addedTemplate; }

private:
	Ui::WBQtAddPlayerDialog *m_ui;	// owns the static widget tree (WBQtAddPlayerDialog.ui)

	QComboBox *m_templates;
	QString m_addedTemplate;
};

#endif // WB_QT_PLAYERLIST_DIALOG_H
