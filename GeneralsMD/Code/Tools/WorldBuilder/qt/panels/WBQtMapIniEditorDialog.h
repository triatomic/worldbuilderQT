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
class QTimer;

namespace Ui { class WBQtMapIniEditorDialog; }	// generated from WBQtMapIniEditorDialog.ui

// Colours the INI structure (comments, block keywords, key names) and underlines object names
// the loaded game data does not define. The unknown-name test is cached per name, since the
// highlighter re-runs on every edited block.
class WBQtMapIniHighlighter : public QSyntaxHighlighter
{
	Q_OBJECT
public:
	explicit WBQtMapIniHighlighter(QTextDocument *doc);

	// What kind of name a line carries, which decides both the catalog it is checked against
	// and the catalog the suggestions come from.
	enum NameKind
	{
		KindNone = 0,
		KindObject,			///< Object / ChildObject / ObjectReskin      -> TheThingFactory
		KindUpgrade,		///< TriggeredBy / ConflictsWith / Upgrade =  -> TheUpgradeCenter
		KindCommandButton,	///< the numbered entries of a CommandSet     -> TheControlBar
		KindScience,		///< Science = inside a SkillSet              -> TheScienceStore
		KindCommandSet,		///< CommandSet = / CommandSetAlt =           -> TheControlBar
		KindSide			///< SideInfo <name>                         -> ThePlayerTemplateStore
	};
	enum { kNameKindCount = 7 };

	// Off = plain INI colouring with no name checking (the "Check names" toggle).
	void setCheckNames(bool on);
	// The checkable name at `posInBlock` within `blockText`, when that name is flagged as
	// unknown; empty otherwise. Drives the context menu. `kindOut` says which catalog to
	// suggest from.
	static QString unknownNameAt(const QString &blockText, int posInBlock, int context,
		int *startOut, int *lengthOut, NameKind *kindOut);
	// True when this line carries a checkable name, filling the name, its span and its kind.
	static bool checkableNameOnLine(const QString &line, QString *nameOut, int *startOut,
		int *lengthOut, NameKind *kindOut);
	// Cached lookup against the catalog for `kind`.
	static bool isKnownName(const QString &name, NameKind kind);
	static void clearNameCache();

	// Names the FILE ITSELF declares (its own Object / Upgrade / CommandSet / Science blocks).
	// A map.ini routinely defines a thing and then refers to it, and those names are not in the
	// loaded game data until it is loaded -- without this they would all read as unknown.
	// Rebuilt from the whole document on load and on edit.
	static void setLocalNames(const QSet<QString> &names);
	static bool isLocallyDeclared(const QString &name);
	// Scan `text` for the block declarations it defines.
	static QSet<QString> scanLocalNames(const QString &text);

	// Which enclosing block a line sits in, carried across lines via the highlighter's per-block
	// user state. "Command = X" means a command button only inside a CommandSet; elsewhere it is
	// an unrelated key that must not be checked.
	enum BlockContext
	{
		ContextOther = 0,
		ContextCommandSet
	};

	// The enclosing-block context a line establishes, given the context of the line before it.
	static int contextAfterLine(const QString &line, int previousContext);

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
	// Re-scan the file's own declarations and re-highlight. Coalesced through a timer so it
	// runs once after a burst of typing, not once per keystroke.
	void onTextChanged();
	void rescanLocalNames();

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
	QTimer *m_rescanTimer;		// coalesces the local-declaration rescan while typing

	static WBQtMapIniEditorDialog *s_instance;
};

#endif // WB_QT_MAPINI_EDITOR_DIALOG_H
