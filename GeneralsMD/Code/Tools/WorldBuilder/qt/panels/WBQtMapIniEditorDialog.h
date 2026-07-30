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

#include <QDialog>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QSyntaxHighlighter>
#include <QWidget>

class QCompleter;
class QLineEdit;
class QListWidget;
class QMenu;
class QListWidgetItem;
class QPlainTextEdit;
class QSplitter;
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
		KindSide,			///< SideInfo <name>                         -> ThePlayerTemplateStore
		// Autocomplete-only catalogs, harvested from the game's INI tree rather than an engine
		// store. Used when no checked kind applies, so completion works on every line instead of
		// only the six that can be validated. Never used for underlining -- a name harvested from
		// the INI tree says nothing about whether THIS key accepts it.
		KindAnyIniName,		///< any block name declared anywhere in Data\INI
		KindAnyIniKey		///< any "Key =" seen anywhere in Data\INI
	};
	enum { kNameKindCount = 9 };

	// The boundary between kinds that VALIDATE (an engine store can answer "does this exist?")
	// and the suggest-only INI-tree kinds above, which must never underline anything. Insert new
	// validating kinds before KindAnyIniName and this keeps holding.
	static bool isValidatingKind(int kind)
	{
		return kind > KindNone && kind < KindAnyIniName;
	}

	// Off = plain INI colouring with no name checking (the "Check names" toggle).
	void setCheckNames(bool on);
	// Off = no structural checking (the "Check syntax" toggle).
	void setCheckSyntax(bool on);
	// The checkable name at `posInBlock` within `blockText`, when that name is flagged as
	// unknown; empty otherwise. Drives the context menu. `kindOut` says which catalog to
	// suggest from.
	QString unknownNameAt(const QString &blockText, int posInBlock, int context,
		int *startOut, int *lengthOut, NameKind *kindOut);
	// True when this line carries a checkable name, filling the name, its span and its kind.
	static bool checkableNameOnLine(const QString &line, QString *nameOut, int *startOut,
		int *lengthOut, NameKind *kindOut);
	// Cached lookup against the catalog for `kind`, then this file's own declarations.
	bool isKnownName(const QString &name, NameKind kind) const;
	// The catalog half alone, without the per-file fallback. Static because the catalogs and
	// their cache are shared by every open file -- only the local-declaration set is per-file.
	static bool isInCatalog(const QString &name, NameKind kind);
	static void clearNameCache();

	// Names the FILE ITSELF declares (its own Object / Upgrade / CommandSet / Science blocks).
	// A map.ini routinely defines a thing and then refers to it, and those names are not in the
	// loaded game data until it is loaded -- without this they would all read as unknown.
	// Rebuilt from the whole document on load and on edit.
	// PER-HIGHLIGHTER, not static: this set belongs to the document being highlighted, unlike the
	// catalogs (which come from the loaded game data and are shared).
	void setLocalNames(const QSet<QString> &names);
	const QSet<QString> &localNames() const;
	bool isLocallyDeclared(const QString &name) const;
	// Scan for the block declarations the file defines. The document overload walks blocks
	// directly -- toPlainText() on a 10,000-line file copies the whole buffer.
	static QSet<QString> scanLocalNames(const QString &text);
	static QSet<QString> scanLocalNames(const QTextDocument *doc);

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

	// ---- basic syntax checking ----
	//
	// The highlighter's per-block state packs the block-nesting DEPTH alongside the context, so
	// each line knows how deep it sits without re-reading the file. That is what lets an "End"
	// with nothing open, and a block left unclosed at end of file, both be flagged.
	enum SyntaxProblem
	{
		SyntaxOk = 0,
		SyntaxStrayEnd,			///< "End" with no block open
		SyntaxMissingValue		///< "Key =" with nothing after the '='
		// NOTE unclosed blocks are NOT here: a line cannot tell "not closed yet" from "never
		// closed", so they are found by the dialog's whole-file pass, which knows where the end is.
	};

	// What (if anything) is wrong with `line` on its own, plus what it does to the nesting depth.
	// `depthBefore` is the depth going in; `depthAfterOut` receives the depth going out.
	static SyntaxProblem checkLineSyntax(const QString &line, int depthBefore, int *depthAfterOut);
	// The code half of a line: everything before the first ';', trimmed. opensBlock/isEndLine
	// take that form, so a caller testing both strips once and passes the result to each rather
	// than having every test re-strip the same line.
	static QString codePart(const QString &line);
	// True when `line` opens a block (a bare `Keyword Name`, or a bare keyword like SkillSet1).
	// Takes the code part (codePart above); passing a raw line still works, since stripping is
	// idempotent -- it just costs an extra copy.
	static bool opensBlock(const QString &line);
	// True when `line` is an End. Takes the code part, as opensBlock does.
	static bool isEndLine(const QString &line);

	// Pack/unpack the per-block state: low bits context, high bits depth.
	static int packState(int context, int depth) { return (depth << 4) | (context & 0xF); }
	static int stateContext(int state) { return (state < 0) ? 0 : (state & 0xF); }
	static int stateDepth(int state) { return (state < 0) ? 0 : (state >> 4); }

protected:
	virtual void highlightBlock(const QString &text);

private:
	bool m_checkNames;
	bool m_checkSyntax;
	QSet<QString> m_localNames;		///< the block names THIS file declares
};

// Ctrl+Space's searchable value picker. A filter box over the completion candidates, for when
// the inline popup's prefix matching is not enough -- you often know a fragment from the middle
// of a name ("Ambush", "Quad") rather than how it starts.
//
// Built in C++ rather than a .ui: it is a filter box plus a list plus OK/Cancel, used only from
// this editor, and a Designer file for it would be more indirection than it saves.
class WBQtMapIniValuePicker : public QDialog
{
	Q_OBJECT
public:
	// `caption` names what is being picked; `current` preselects a row.
	WBQtMapIniValuePicker(const QStringList &candidates, const QString &current, QWidget *parent);

	// The picked value, or empty when cancelled.
	QString picked() const { return m_picked; }

protected:
	virtual bool eventFilter(QObject *watched, QEvent *event);

private slots:
	void onFilterChanged(const QString &text);
	void onRowActivated();

private:
	QStringList m_candidates;
	QLineEdit *m_filter;
	QListWidget *m_list;
	QString m_picked;
};

class WBQtMapIniEditorDialog : public QWidget
{
	Q_OBJECT
public:
	explicit WBQtMapIniEditorDialog(void *frameHwnd);
	virtual ~WBQtMapIniEditorDialog();

	static WBQtMapIniEditorDialog *instance() { return s_instance; }

	// Load `path` into the editor. The entry point, every File-menu open and Reload route here.
	void loadFile(const QString &path);

protected:
	virtual void closeEvent(QCloseEvent *event);
	virtual bool eventFilter(QObject *watched, QEvent *event);

private slots:
	void onSave();
	void onSaveAs();
	void onOpen();				///< File > Open: any .ini, not just the map's
	void onOpenMapIni();		///< File > back to the current map's map.ini
	void onOpenRecent();		///< a File > Recent entry
	void onReload();
	void onFindNext();
	void onFindPrevious();
	void onCheckNamesToggled(bool on);
	void onCheckSyntaxToggled(bool on);
	void onWhitespaceToggled(bool on);
	void onModificationChanged(bool modified);
	void onEditorContextMenu(const QPoint &pos);
	void onCursorMoved();
	// Re-scan the file's own declarations and re-highlight. Coalesced through a timer so it
	// runs once after a burst of typing, not once per keystroke.
	void onTextChanged();
	void rescanLocalNames();
	// "Show errors": swap the editor for a read-only list of just the flagged lines.
	void onShowErrorsToggled(bool on);
	// "Autocomplete": suggest names from the loaded data for the value being typed.
	void onAutoCompleteToggled(bool on);
	void onCompletionChosen(const QString &completion);
	// Selecting a listed line moves the cursor to it in the editor above; the pane stays open.
	void onErrorRowChanged(QListWidgetItem *item, QListWidgetItem *previous);
	void onUndo();
	void onRedo();

private:
	void find(bool forward);
	// Offer completions for the value being typed on the cursor's line, from the catalog that
	// line's key expects. Does nothing when the line takes no checkable name.
	void maybeComplete(bool force);
	// The partial word immediately left of the cursor, and where it starts in the block.
	QString wordUnderCursor(int *startInBlockOut) const;
	// Which catalog the cursor's position expects (a NameKind). Shared by the inline popup and
	// the Ctrl+Space picker so the two never offer different candidates.
	int catalogKindAtCursor(int wordStart) const;
	// The candidates for the cursor's position, best-scoped first: the values actually seen with
	// this line's key, else the kind's catalog, else the whole tree. `labelOut` names what is
	// being offered, for the picker's title.
	// `key` and `kind` come from keyOnCurrentLine/catalogKindAtCursor; the caller passes them in
	// because both are needed for its own cache key and each costs a full line parse.
	QStringList candidatesAtCursor(int wordStart, const QString &key, int kind,
		QString *labelOut) const;
	// The key on the cursor's line ("Surfaces" from "Surfaces = GROUND"), or empty.
	QString keyOnCurrentLine(int wordStart) const;
	// Ctrl+Space: the searchable picker over those candidates.
	void openValuePicker();
	// Refill the flagged-line list from the current text (only while the filter is showing).
	void rebuildErrorList();
	void updateTitle();
	void updateStatus(const QString &message);
	// Replace the token at [start,length) of `blockNumber` with `replacement`, as one undo step.
	void replaceToken(int blockNumber, int start, int length, const QString &replacement);
	bool maybeSave();
	void savePosition();
	// The File menu, built in C++ (a QMenuBar in a .ui adds nothing over this).
	void buildMenuBar();
	// Remember/rebuild the recent-file list, persisted in the editor's profile section.
	void noteRecentFile(const QString &path);
	void rebuildRecentMenu();

	// Build the editor, its highlighter, the error pane and the rescan timer into the .ui's
	// editorHost. Called once, from the constructor: one file open at a time.
	void createEditor();

	Ui::WBQtMapIniEditorDialog *m_ui;	// owns the static widget tree
	// The editor and its companions, created by createEditor and owned by the widget tree.
	WBQtMapIniHighlighter *m_highlighter;
	QPlainTextEdit *m_editor;
	QListWidget *m_errorList;
	QSplitter *m_split;
	QString m_path;
	QTimer *m_rescanTimer;
	QCompleter *m_completer;	// autocomplete over the catalogs; model swapped per line
	QString m_completerFor;		// "key/kind" the completer's current model was built for
	QMenu *m_recentMenu;		// File > Recent

	static WBQtMapIniEditorDialog *s_instance;
};

#endif // WB_QT_MAPINI_EDITOR_DIALOG_H
