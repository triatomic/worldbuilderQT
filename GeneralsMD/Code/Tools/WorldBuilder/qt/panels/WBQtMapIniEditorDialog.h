// WBQtMapIniEditorDialog.h -- the native Qt map.ini editor (File > Map.ini > Open map.ini
// (internal)). A modeless text editor over the current map's map.ini: edit and save, find /
// find next, and undo/redo (QPlainTextEdit's own document undo stack, so it covers typing as
// well as the applied fixes).
//
// The "suggest fixes" part: WBQtMapIniHighlighter underlines the object name on any line that
// declares or references one when the game data defines no such template, and the context menu
// on an underlined name offers the closest matches (the same WBQtNameMatch ranking the Replace
// Missing passes use elsewhere). Picking one replaces just that token, as a single undo step.
#ifndef WB_QT_MAPINI_EDITOR_DIALOG_H
#define WB_QT_MAPINI_EDITOR_DIALOG_H

#include <QSet>
#include <QString>
#include <QStringList>
#include <QSyntaxHighlighter>
#include <QWidget>

class QPlainTextEdit;

namespace Ui { class WBQtMapIniEditorDialog; }	// generated from WBQtMapIniEditorDialog.ui

// Colours the INI structure (comments, block keywords, key names) and underlines object names
// the loaded game data does not define. The unknown-name test is cached per name, since the
// highlighter re-runs on every edited block.
class WBQtMapIniHighlighter : public QSyntaxHighlighter
{
	Q_OBJECT
public:
	explicit WBQtMapIniHighlighter(QTextDocument *doc);

	// Off = plain INI colouring with no name checking (the "Check object names" toggle).
	void setCheckNames(bool on);
	// The object name at `posInBlock` within `blockText`, when that name is flagged as unknown;
	// empty otherwise. Drives the context menu.
	static QString unknownNameAt(const QString &blockText, int posInBlock, int *startOut,
		int *lengthOut);
	// True when this line declares/references an object name, filling the name and its span.
	static bool objectNameOnLine(const QString &line, QString *nameOut, int *startOut,
		int *lengthOut);
	// Cached WBQtMapIniEditorData_IsTemplate.
	static bool isKnownTemplate(const QString &name);
	static void clearNameCache();

protected:
	virtual void highlightBlock(const QString &text);

private:
	bool m_checkNames;
};

class WBQtMapIniEditorDialog : public QWidget
{
	Q_OBJECT
public:
	explicit WBQtMapIniEditorDialog(void *frameHwnd);
	virtual ~WBQtMapIniEditorDialog();

	static WBQtMapIniEditorDialog *instance() { return s_instance; }

	// Load `path` into the editor (used on open and when the map changes underneath it).
	void loadFile(const QString &path);

protected:
	virtual void closeEvent(QCloseEvent *event);
	virtual bool eventFilter(QObject *watched, QEvent *event);

private slots:
	void onSave();
	void onReload();
	void onFindNext();
	void onFindPrevious();
	void onCheckNamesToggled(bool on);
	void onModificationChanged(bool modified);
	void onEditorContextMenu(const QPoint &pos);
	void onCursorMoved();

private:
	void find(bool forward);
	void updateTitle();
	void updateStatus(const QString &message);
	// Replace the token at [start,length) of `blockNumber` with `replacement`, as one undo step.
	void replaceToken(int blockNumber, int start, int length, const QString &replacement);
	bool maybeSave();
	void savePosition();

	Ui::WBQtMapIniEditorDialog *m_ui;	// owns the static widget tree
	WBQtMapIniHighlighter *m_highlighter;
	QPlainTextEdit *m_editor;
	QString m_path;

	static WBQtMapIniEditorDialog *s_instance;
};

#endif // WB_QT_MAPINI_EDITOR_DIALOG_H
