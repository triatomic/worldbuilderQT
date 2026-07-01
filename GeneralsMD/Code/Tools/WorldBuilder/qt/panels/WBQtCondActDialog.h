// WBQtCondActDialog.h -- the native Qt condition/action editor (Tier 2b), replacing the MFC
// EditCondition / EditAction modals. One class serves both (mode flag), like the facade. Left:
// the template category tree (built from '/'-separated template name paths, searchable).
// Right: the parameter "sentence" -- the item's UI text with each parameter as a clickable
// link that pops the parameter editor -- plus the warnings box and the template's developer
// notes. Run via WBQtCondAct_Run() (exec()'d over the script-edit dialog).
#ifndef WB_QT_CONDACT_DIALOG_H
#define WB_QT_CONDACT_DIALOG_H

#include <QDialog>

class QCheckBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextBrowser;
class QTreeWidget;
class QTreeWidgetItem;
class QUrl;

class WBQtCondActDialog : public QDialog
{
	Q_OBJECT
public:
	WBQtCondActDialog(void *item, bool isAction, QWidget *parent = 0);

protected:
	bool eventFilter(QObject *watched, QEvent *event);

private slots:
	void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void onLinkClicked(const QUrl &url);
	void onSearch();
	void onReset();
	void onCompressToggled(bool checked);

private:
	void populateTree();
	void selectCurrentType(QTreeWidgetItem *leaf);
	void renderSentence();
	void updateWarnings();
	void showHelpForType(int type);
	void applyTreeFont();

	void *m_item;
	int m_isAction;
	bool m_updating;

	QLineEdit *m_searchEdit;
	QCheckBox *m_compressCheck;
	QTreeWidget *m_tree;
	QTextBrowser *m_sentence;
	QGroupBox *m_warningsBox;
	QLabel *m_warningsLabel;
	QLabel *m_helpLabel;
};

#endif // WB_QT_CONDACT_DIALOG_H
