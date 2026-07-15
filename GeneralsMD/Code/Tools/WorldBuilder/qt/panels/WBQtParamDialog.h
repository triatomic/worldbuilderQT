// WBQtParamDialog.h -- the native Qt parameter editors (Tier 2c): the generic single-value
// dialog (== EditParameter), the coordinate editor (== EditCoordParameter), the object-type
// picker (== EditObjectParameter) and the group editor (== EditGroup). The color parameter is
// handled with QColorDialog inside WBQtParam_Edit. All engine access goes through the C facade
// in WBQtParamBridge.h.
#ifndef WB_QT_PARAM_DIALOG_H
#define WB_QT_PARAM_DIALOG_H

#include <QDialog>

class QCheckBox;
class QGroupBox;
class QLineEdit;
class QListWidget;
class QTreeWidget;
class QTreeWidgetItem;

// The generic single-value editor: a caption box holding an edit, an editable combo (edit +
// option list), or a selection-only list, plus the audio preview row for sound parameters.
class WBQtParamDialog : public QDialog
{
	Q_OBJECT
public:
	WBQtParamDialog(void *parameter, const char *unitName, QWidget *parent = 0);

	void accept();	// == OnOK: validate + write back; beep and stay open on bad input

protected:
	// Up/Down/PageUp/PageDown in the filter edit walk the visible list rows (== a combo
	// box), copying the highlighted row's text into the edit without leaving the field.
	virtual bool eventFilter(QObject *watched, QEvent *event);

private slots:
	void onRowChanged(int row);
	void onTextEdited(const QString &text);
	void onEditTextChanged(const QString &text);
	void onPreviewSound();

private:
	void *m_parameter;
	int m_kind;
	bool m_updating;

	QLineEdit *m_edit;			// EDIT + COMBO kinds
	QListWidget *m_list;		// COMBO + LIST kinds
	QCheckBox *m_autoPreviewCheck;
};

// The coordinate editor: X/Y/Z fields (== EditCoordParameter).
class WBQtCoordDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtCoordDialog(void *parameter, QWidget *parent = 0);

	void accept();

private:
	void *m_parameter;
	QLineEdit *m_editX;
	QLineEdit *m_editY;
	QLineEdit *m_editZ;
};

// The object-type picker: the template catalog tree ([TEST/]side/editor-sorting/name +
// "Object Lists") over a free-text override edit (== EditObjectParameter).
class WBQtObjectPickDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtObjectPickDialog(void *parameter, QWidget *parent = 0);

	void accept();

private slots:
	void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

private:
	void *m_parameter;
	QTreeWidget *m_tree;
	QLineEdit *m_edit;
};

// The group editor: name / active / subroutine (== EditGroup).
class WBQtGroupDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtGroupDialog(void *scriptGroup, QWidget *parent = 0);

	void accept();

private:
	void *m_scriptGroup;
	QLineEdit *m_nameEdit;
	QCheckBox *m_activeCheck;
	QCheckBox *m_subroutineCheck;
};

#endif // WB_QT_PARAM_DIALOG_H
