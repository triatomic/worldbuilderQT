// WBQtMapIniEditorDialog.cpp -- see WBQtMapIniEditorDialog.h.
#include "WBQtMapIniEditorDialog.h"
#include "ui_WBQtMapIniEditorDialog.h"
#include "WBQtMapIniEditorBridge.h"
#include "../WBQtNameMatch.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHash>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRegExp>
#include <QSet>
#include <QTextBlock>
#include <QTimer>
#include <QTextStream>
#include <QWindow>

#include <algorithm>
#include <qt_windows.h>

// From WBQtBridge.cpp: the Qt main window, and the owner HWND to parent a top-level to.
void *WBQt_EffectiveOwnerHwnd(void *frameHwnd);
QWidget *WBQt_MainWindowWidget(void);

namespace
{
	const int kTextCap = 1024;
	const int kMaxSuggestions = 6;

	// The INI block keywords whose argument is an object/template name. These are the lines the
	// name checking looks at -- deliberately NOT every token in the file, because most values are
	// numbers, enums or module names rather than template references.
	// `Keyword Name` block headers whose NAME is checkable, with the catalog it belongs to.
	struct BlockKeyword
	{
		const char *m_keyword;
		int m_kind;
	};
	const BlockKeyword kBlockKeywords[] = {
		{ "Object",       WBQtMapIniHighlighter::KindObject },
		{ "ChildObject",  WBQtMapIniHighlighter::KindObject },
		{ "ObjectReskin", WBQtMapIniHighlighter::KindObject },
		// A SideInfo name is matched by exact string against the sides the player templates
		// define, so a typo here silently creates a side entry the game never reads.
		{ "SideInfo",     WBQtMapIniHighlighter::KindSide },
		{ NULL, 0 }
	};

	// `Key = Value` lines whose value is a checkable name. Keyed off the KEY rather than the
	// enclosing module, so they are caught wherever they sit -- these appear nested several
	// blocks deep (ReplaceModule > Behavior > TriggeredBy = ...), and tracking every module
	// type would be both fragile and endless.
	struct KeyKind
	{
		const char *m_key;
		int m_kind;
	};
	const KeyKind kValueKeys[] = {
		// upgrades
		{ "Upgrade",              WBQtMapIniHighlighter::KindUpgrade },
		{ "UpgradeToGrant",       WBQtMapIniHighlighter::KindUpgrade },
		{ "RequiresAllUpgrades",  WBQtMapIniHighlighter::KindUpgrade },
		{ "RequiresAnyUpgrades",  WBQtMapIniHighlighter::KindUpgrade },
		{ "ConflictsWith",        WBQtMapIniHighlighter::KindUpgrade },
		{ "TriggeredBy",          WBQtMapIniHighlighter::KindUpgrade },
		{ "TriggerAlt",           WBQtMapIniHighlighter::KindUpgrade },
		{ "RemovesUpgrades",      WBQtMapIniHighlighter::KindUpgrade },
		// sciences. "Science =" is a science name at every parse site the engine has for it
		// (a SkillSet entry, a CommandButton's science, CrateCollide's pickup), so the key
		// alone is enough -- no enclosing-block test needed.
		{ "Science",              WBQtMapIniHighlighter::KindScience },
		{ "IntrinsicSciences",    WBQtMapIniHighlighter::KindScience },
		{ "PrerequisiteSciences", WBQtMapIniHighlighter::KindScience },
		{ "RequiredScience",      WBQtMapIniHighlighter::KindScience },
		{ "PickupScience",        WBQtMapIniHighlighter::KindScience },
		// command SET references (the set's name, not a button in it)
		{ "CommandSet",           WBQtMapIniHighlighter::KindCommandSet },
		{ "CommandSetAlt",        WBQtMapIniHighlighter::KindCommandSet },
		// objects referenced by name from a key
		{ "BaseDefenseStructure1", WBQtMapIniHighlighter::KindObject },
		{ NULL, 0 }
	};

	QColor commentColour()
	{
		return QColor(110, 150, 110);
	}

	QColor keywordColour()
	{
		return QColor(86, 156, 214);
	}

	QColor keyColour()
	{
		return QColor(156, 220, 154);
	}

	// (name, kind) -> known, so the per-block highlight does not hit the bridge for every repeat.
	// Only catalog answers are cached; the locally-declared set is consulted BEFORE this and is
	// rebuilt on every edit, so deleting a declaration re-flags its references immediately.
	QHash<QString, bool> s_knownCache;
	// Names this file declares for itself (see scanLocalNames).
	QSet<QString> s_localNames;
	// One catalog per checked kind, built on first use.
	QStringList s_catalogs[WBQtMapIniHighlighter::kNameKindCount];

	// The catalog for `kind`, built on demand. Empty when the underlying subsystem is absent.
	const QStringList &catalogFor(int kind)
	{
		static const QStringList empty;
		if (kind <= 0 || kind >= WBQtMapIniHighlighter::kNameKindCount)
		{
			return empty;
		}
		QStringList &catalog = s_catalogs[kind];
		if (!catalog.isEmpty())
		{
			return catalog;
		}
		char buf[kTextCap];
		int count = 0;
		switch (kind)
		{
			case WBQtMapIniHighlighter::KindObject:
				count = WBQtMapIniEditorData_BuildTemplates();
				break;
			case WBQtMapIniHighlighter::KindUpgrade:
				count = WBQtMapIniEditorData_BuildUpgrades();
				break;
			case WBQtMapIniHighlighter::KindCommandButton:
				count = WBQtMapIniEditorData_BuildCommandButtons();
				break;
			case WBQtMapIniHighlighter::KindScience:
				count = WBQtMapIniEditorData_BuildSciences();
				break;
			case WBQtMapIniHighlighter::KindCommandSet:
				// Validation only -- the engine's command set list is protected, so there is
				// nothing to enumerate for suggestions.
				count = WBQtMapIniEditorData_BuildCommandSets();
				break;
			case WBQtMapIniHighlighter::KindSide:
				count = WBQtMapIniEditorData_BuildSides();
				break;
			default:
				return catalog;
		}
		for (int i = 0; i < count; ++i)
		{
			buf[0] = 0;
			switch (kind)
			{
				case WBQtMapIniHighlighter::KindObject:
					WBQtMapIniEditorData_GetTemplate(i, buf, sizeof(buf));
					break;
				case WBQtMapIniHighlighter::KindUpgrade:
					WBQtMapIniEditorData_GetUpgrade(i, buf, sizeof(buf));
					break;
				case WBQtMapIniHighlighter::KindCommandButton:
					WBQtMapIniEditorData_GetCommandButton(i, buf, sizeof(buf));
					break;
				case WBQtMapIniHighlighter::KindScience:
					WBQtMapIniEditorData_GetScience(i, buf, sizeof(buf));
					break;
				case WBQtMapIniHighlighter::KindCommandSet:
					WBQtMapIniEditorData_GetCommandSet(i, buf, sizeof(buf));
					break;
				case WBQtMapIniHighlighter::KindSide:
					WBQtMapIniEditorData_GetSide(i, buf, sizeof(buf));
					break;
				default:
					break;
			}
			if (buf[0] != 0)
			{
				catalog.append(QString::fromLocal8Bit(buf));
			}
		}
		return catalog;
	}

	// The closest names to `target` from `kind`'s catalog, best-first, capped at kMaxSuggestions.
	QStringList suggestionsFor(const QString &target, int kind)
	{
		const QStringList &catalog = catalogFor(kind);
		QList<QPair<float, QString> > scored;
		for (int i = 0; i < catalog.size(); ++i)
		{
			const QString &candidate = catalog.at(i);
			const float base = WBQtNameMatch::similarity(target, candidate);
			if (base < WBQtNameMatch::kSuggestThreshold)
			{
				continue;	// admit on raw similarity, as bestMatch does
			}
			scored.append(qMakePair(WBQtNameMatch::matchScoreFromBase(target, candidate, base),
				candidate));
		}
		// Higher score first; std::stable_sort keeps catalog order on ties.
		std::stable_sort(scored.begin(), scored.end(),
			[](const QPair<float, QString> &a, const QPair<float, QString> &b)
			{
				return a.first > b.first;
			});
		QStringList out;
		for (int i = 0; i < scored.size() && out.size() < kMaxSuggestions; ++i)
		{
			out.append(scored.at(i).second);
		}
		return out;
	}
}

//----------------------------------------------------------------------------------------
// WBQtMapIniHighlighter
//----------------------------------------------------------------------------------------

WBQtMapIniHighlighter::WBQtMapIniHighlighter(QTextDocument *doc)
	: QSyntaxHighlighter(doc),
	  m_checkNames(true)
{
}

void WBQtMapIniHighlighter::setCheckNames(bool on)
{
	if (m_checkNames == on)
	{
		return;
	}
	m_checkNames = on;
	rehighlight();
}

void WBQtMapIniHighlighter::clearNameCache()
{
	s_knownCache.clear();
	for (int i = 0; i < kNameKindCount; ++i)
	{
		s_catalogs[i].clear();
	}
}

void WBQtMapIniHighlighter::setLocalNames(const QSet<QString> &names)
{
	s_localNames = names;
}

bool WBQtMapIniHighlighter::isLocallyDeclared(const QString &name)
{
	return s_localNames.contains(name);
}

// The block names this file opens. NOTE these are not necessarily NEW names: map.ini loads with
// INI_LOAD_CREATE_OVERRIDES, so "Object Foo" means "override Foo if it exists, otherwise create
// it". That is why isKnownName consults the game data FIRST and only falls back to this set --
// treating every block header as a declaration would self-certify every object in the file and
// stop checking them entirely.
// A "Key = Value" line is a REFERENCE, never a block header, so the '=' form is excluded.
QSet<QString> WBQtMapIniHighlighter::scanLocalNames(const QString &text)
{
	QSet<QString> names;
	static const char *const kDeclKeywords[] = {
		"Object", "ChildObject", "ObjectReskin", "Upgrade", "CommandSet", "CommandButton",
		"Science", "Weapon", "Armor", "Locomotor", "FXList", "ObjectCreationList",
		NULL
	};
	const QStringList lines = text.split('\n');
	for (int i = 0; i < lines.size(); ++i)
	{
		QString scan = lines.at(i);
		const int commentAt = scan.indexOf(';');
		if (commentAt >= 0)
		{
			scan = scan.left(commentAt);
		}
		// Split into exactly two tokens once, then match the first against the keyword list --
		// a regex per keyword per line would be a dozen compilations for every line of the file.
		// A block header is exactly `Keyword Name` with no '=' (that would be a reference).
		const QStringList tokens = scan.simplified().split(' ', QString::SkipEmptyParts);
		if (tokens.size() != 2 || tokens.at(1).startsWith('='))
		{
			continue;
		}
		for (int k = 0; kDeclKeywords[k] != NULL; ++k)
		{
			if (tokens.at(0).compare(QString::fromLatin1(kDeclKeywords[k]),
					Qt::CaseInsensitive) == 0)
			{
				names.insert(tokens.at(1));
				break;
			}
		}
	}
	return names;
}

bool WBQtMapIniHighlighter::isKnownName(const QString &name, NameKind kind)
{
	if (name.isEmpty() || kind == KindNone)
	{
		return true;	// nothing to flag
	}
	// Key on the kind too: the same string can be a valid upgrade and an unknown object.
	const QString cacheKey = QString::number((int)kind) + ":" + name;
	QHash<QString, bool>::const_iterator it = s_knownCache.constFind(cacheKey);
	if (it != s_knownCache.constEnd())
	{
		return it.value();
	}
	const QByteArray raw = name.toLocal8Bit();
	bool known = true;
	switch (kind)
	{
		case KindObject:
			known = (WBQtMapIniEditorData_IsTemplate(raw.constData()) != 0);
			break;
		case KindUpgrade:
			known = (WBQtMapIniEditorData_IsUpgrade(raw.constData()) != 0);
			break;
		case KindCommandButton:
			known = (WBQtMapIniEditorData_IsCommandButton(raw.constData()) != 0);
			break;
		case KindScience:
			known = (WBQtMapIniEditorData_IsScience(raw.constData()) != 0);
			break;
		case KindCommandSet:
			known = (WBQtMapIniEditorData_IsCommandSet(raw.constData()) != 0);
			break;
		case KindSide:
			known = (WBQtMapIniEditorData_IsSide(raw.constData()) != 0);
			break;
		default:
			break;
	}
	s_knownCache.insert(cacheKey, known);
	if (known)
	{
		return true;
	}
	// Not in the game data. It is still valid if THIS FILE creates it -- map.ini loads with
	// INI_LOAD_CREATE_OVERRIDES, so a block header for a name the data lacks defines a new one.
	// Checked after the catalog (and deliberately not cached) so that deleting the block
	// re-flags every reference to it on the next rescan.
	return isLocallyDeclared(name);
}

int WBQtMapIniHighlighter::contextAfterLine(const QString &line, int previousContext)
{
	QString scan = line;
	const int commentAt = scan.indexOf(';');
	if (commentAt >= 0)
	{
		scan = scan.left(commentAt);
	}
	// A CommandSet block opens with `CommandSet <name>` -- no '=', which is what separates the
	// block header from a `CommandSet = <name>` reference to one. Both static: this runs per
	// line per repaint.
	static QRegExp commandSet("^\\s*CommandSet\\s+([^=\\s]\\S*)\\s*$", Qt::CaseInsensitive);
	if (commandSet.indexIn(scan) >= 0)
	{
		return ContextCommandSet;
	}
	static QRegExp end("^\\s*End\\s*$", Qt::CaseInsensitive);
	if (end.indexIn(scan) >= 0)
	{
		// CommandSet blocks do not nest, so the first End closes the one we are in. (Blocks that
		// DO nest -- ReplaceModule > Behavior -- are not tracked as contexts at all: the keys
		// inside them are matched by key name, wherever they sit.)
		return ContextOther;
	}
	return previousContext;
}

// The checkable name on a line, if any. Three shapes:
//   `Object Foo`      -- a declaration keyword followed by a template name
//   `Upgrade = Foo`   -- an upgrade-valued key
//   `Command = Foo`   -- a command button, but ONLY inside a CommandSet block, which is why the
//                        caller passes the enclosing-block context
// A trailing comment is stripped before matching, so "Object Foo ; note" still matches.
bool WBQtMapIniHighlighter::checkableNameOnLine(const QString &line, QString *nameOut,
	int *startOut, int *lengthOut, NameKind *kindOut)
{
	QString scan = line;
	const int commentAt = scan.indexOf(';');
	if (commentAt >= 0)
	{
		scan = scan.left(commentAt);
	}

	// Split the line ONCE into its leading word and what follows, then look the word up. The
	// earlier form built a fresh QRegExp per candidate key -- twenty-odd regex compilations per
	// line, re-run for every block Qt repaints, which made scrolling hitch.
	int at = 0;
	while (at < scan.length() && scan.at(at).isSpace())
	{
		++at;
	}
	const int wordStart = at;
	while (at < scan.length() && !scan.at(at).isSpace() && scan.at(at) != '=')
	{
		++at;
	}
	if (at == wordStart)
	{
		return false;		// blank or starts with '='
	}
	const QString word = scan.mid(wordStart, at - wordStart);

	// Skip the run of space between the word and whatever follows it.
	int afterWord = at;
	while (afterWord < scan.length() && scan.at(afterWord).isSpace())
	{
		++afterWord;
	}
	const bool isAssignment = (afterWord < scan.length() && scan.at(afterWord) == '=');

	// The first token of the remainder, and where it starts.
	int valueStart = isAssignment ? afterWord + 1 : afterWord;
	while (valueStart < scan.length() && scan.at(valueStart).isSpace())
	{
		++valueStart;
	}
	int valueEnd = valueStart;
	while (valueEnd < scan.length() && !scan.at(valueEnd).isSpace())
	{
		++valueEnd;
	}
	if (valueEnd == valueStart)
	{
		return false;		// nothing after the key/keyword
	}
	const QString value = scan.mid(valueStart, valueEnd - valueStart);

	if (isAssignment)
	{
		// `Key = Value`. Only the FIRST value is checked when a key takes a list -- flagging one
		// name per line keeps the underline and the replacement unambiguous.
		for (int k = 0; kValueKeys[k].m_key != NULL; ++k)
		{
			if (word.compare(QString::fromLatin1(kValueKeys[k].m_key), Qt::CaseInsensitive) != 0)
			{
				continue;
			}
			if (nameOut != NULL)   { *nameOut = value; }
			if (startOut != NULL)  { *startOut = valueStart; }
			if (lengthOut != NULL) { *lengthOut = value.length(); }
			if (kindOut != NULL)   { *kindOut = (NameKind)kValueKeys[k].m_kind; }
			return true;
		}
		return false;
	}

	// `Keyword Name` block header -- and nothing may follow the name (a header is exactly two
	// tokens), which is what separates "CommandSet Foo" from a stray line that merely starts
	// with the same word.
	int afterValue = valueEnd;
	while (afterValue < scan.length() && scan.at(afterValue).isSpace())
	{
		++afterValue;
	}
	if (afterValue != scan.length())
	{
		return false;
	}
	for (int k = 0; kBlockKeywords[k].m_keyword != NULL; ++k)
	{
		if (word.compare(QString::fromLatin1(kBlockKeywords[k].m_keyword),
				Qt::CaseInsensitive) != 0)
		{
			continue;
		}
		if (nameOut != NULL)   { *nameOut = value; }
		if (startOut != NULL)  { *startOut = valueStart; }
		if (lengthOut != NULL) { *lengthOut = value.length(); }
		if (kindOut != NULL)   { *kindOut = (NameKind)kBlockKeywords[k].m_kind; }
		return true;
	}

	// The numbered entries of a CommandSet ("1 = Command_Foo") need the enclosing-block context,
	// so they are handled by the context-taking overload below.
	return false;
}

// As above, plus the CommandSet-only `Command = Foo` case.
static bool checkableNameOnLineIn(const QString &line, int context, QString *nameOut,
	int *startOut, int *lengthOut, WBQtMapIniHighlighter::NameKind *kindOut)
{
	if (WBQtMapIniHighlighter::checkableNameOnLine(line, nameOut, startOut, lengthOut, kindOut))
	{
		return true;
	}
	if (context != WBQtMapIniHighlighter::ContextCommandSet)
	{
		return false;
	}
	QString scan = line;
	const int commentAt = scan.indexOf(';');
	if (commentAt >= 0)
	{
		scan = scan.left(commentAt);
	}
	// A CommandSet's entries are NUMBERED keys ("1 = Command_AmbushFromShortcut"), not
	// "Command = ...". Some sets also use the literal key, so accept both. Static: this runs
	// per line per repaint, and recompiling it each time is what made scrolling hitch.
	static QRegExp re("^\\s*(?:\\d+|Command)\\s*=\\s*(\\S+)", Qt::CaseInsensitive);
	if (re.indexIn(scan) < 0)
	{
		return false;
	}
	const QString name = re.cap(1);
	if (name.isEmpty())
	{
		return false;
	}
	if (nameOut != NULL)   { *nameOut = name; }
	if (startOut != NULL)  { *startOut = re.pos(1); }
	if (lengthOut != NULL) { *lengthOut = name.length(); }
	if (kindOut != NULL)   { *kindOut = WBQtMapIniHighlighter::KindCommandButton; }
	return true;
}

QString WBQtMapIniHighlighter::unknownNameAt(const QString &blockText, int posInBlock,
	int context, int *startOut, int *lengthOut, NameKind *kindOut)
{
	QString name;
	int start = 0;
	int length = 0;
	NameKind kind = KindNone;
	if (!checkableNameOnLineIn(blockText, context, &name, &start, &length, &kind))
	{
		return QString();
	}
	if (posInBlock < start || posInBlock > start + length)
	{
		return QString();	// the cursor is not on the name itself
	}
	if (isKnownName(name, kind))
	{
		return QString();
	}
	if (startOut != NULL)
	{
		*startOut = start;
	}
	if (lengthOut != NULL)
	{
		*lengthOut = length;
	}
	if (kindOut != NULL)
	{
		*kindOut = kind;
	}
	return name;
}

void WBQtMapIniHighlighter::highlightBlock(const QString &text)
{
	// Comments first: everything from ';' to end of line, whatever else is on it.
	const int commentAt = text.indexOf(';');
	if (commentAt >= 0)
	{
		QTextCharFormat fmt;
		fmt.setForeground(commentColour());
		setFormat(commentAt, text.length() - commentAt, fmt);
	}
	const QString code = (commentAt >= 0) ? text.left(commentAt) : text;

	// The leading word: a block keyword (Object, Weapon, End, ...) or a key name.
	static QRegExp leading("^\\s*([A-Za-z_][A-Za-z0-9_]*)");
	if (leading.indexIn(code) >= 0)
	{
		const QString word = leading.cap(1);
		bool isBlockKeyword = false;
		for (int k = 0; kBlockKeywords[k].m_keyword != NULL; ++k)
		{
			if (word.compare(QString::fromLatin1(kBlockKeywords[k].m_keyword),
					Qt::CaseInsensitive) == 0)
			{
				isBlockKeyword = true;
				break;
			}
		}
		static const char *const kOtherBlockWords[] = {
			"End", "CommandSet", "SkillSet1", "SkillSet2", "SkillSet3", "SkillSet4", "SkillSet5",
			"SideInfo", "ReplaceModule", "Behavior", "Weapon", "Armor", "Upgrade", "Science",
			NULL
		};
		for (int b = 0; !isBlockKeyword && kOtherBlockWords[b] != NULL; ++b)
		{
			if (word.compare(QString::fromLatin1(kOtherBlockWords[b]), Qt::CaseInsensitive) == 0)
			{
				isBlockKeyword = true;
			}
		}
		QTextCharFormat fmt;
		fmt.setForeground(isBlockKeyword ? keywordColour() : keyColour());
		if (isBlockKeyword)
		{
			fmt.setFontWeight(QFont::Bold);
		}
		setFormat(leading.pos(1), word.length(), fmt);
	}

	// Carry the enclosing-block context forward. This runs whether or not name checking is on,
	// so toggling the check back on does not need a full re-scan to know where the blocks are.
	const int previousContext = (currentBlock().previous().isValid()
		&& previousBlockState() >= 0) ? previousBlockState() : (int)ContextOther;
	const int context = contextAfterLine(text, previousContext);
	setCurrentBlockState(context);

	if (!m_checkNames)
	{
		return;
	}

	// The checkable name on this line, underlined when its catalog does not have it.
	QString name;
	int start = 0;
	int length = 0;
	NameKind kind = KindNone;
	if (checkableNameOnLineIn(text, context, &name, &start, &length, &kind)
		&& !isKnownName(name, kind))
	{
		// Orange, not red: an unrecognised name is usually a typo or a name from data that is
		// not loaded, which is a warning rather than a definite error.
		QTextCharFormat fmt;
		fmt.setUnderlineColor(QColor(220, 140, 40));
		fmt.setUnderlineStyle(QTextCharFormat::WaveUnderline);
		fmt.setForeground(QColor(220, 140, 40));
		setFormat(start, length, fmt);
	}
}

//----------------------------------------------------------------------------------------
// WBQtMapIniEditorDialog
//----------------------------------------------------------------------------------------

WBQtMapIniEditorDialog *WBQtMapIniEditorDialog::s_instance = NULL;

WBQtMapIniEditorDialog::WBQtMapIniEditorDialog(void *frameHwnd)
	: QWidget(NULL, Qt::Window),
	  m_ui(new Ui::WBQtMapIniEditorDialog),
	  m_highlighter(NULL),
	  m_editor(NULL)
{
	s_instance = this;

	// The static widget tree lives in WBQtMapIniEditorDialog.ui; wire what Designer can't express.
	m_ui->setupUi(this);
	m_editor = m_ui->editor;

	// A fixed-pitch font: INI files are column-aligned by hand and a proportional font makes
	// that alignment unreadable.
	QFont mono("Consolas");
	mono.setStyleHint(QFont::Monospace);
	mono.setFixedPitch(true);
	mono.setPointSize(10);
	m_editor->setFont(mono);
	m_editor->setTabStopWidth(4 * QFontMetrics(mono).width(' '));

	m_highlighter = new WBQtMapIniHighlighter(m_editor->document());

	// Rescan the file's own declarations after a pause in typing: declaring an object partway
	// through typing its name must not re-highlight the whole document on every keystroke.
	m_rescanTimer = new QTimer(this);
	m_rescanTimer->setSingleShot(true);
	m_rescanTimer->setInterval(400);
	connect(m_rescanTimer, SIGNAL(timeout()), this, SLOT(rescanLocalNames()));
	connect(m_editor, SIGNAL(textChanged()), this, SLOT(onTextChanged()));

	m_editor->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_editor, SIGNAL(customContextMenuRequested(QPoint)),
			this, SLOT(onEditorContextMenu(QPoint)));
	connect(m_editor, SIGNAL(cursorPositionChanged()), this, SLOT(onCursorMoved()));
	connect(m_editor->document(), SIGNAL(modificationChanged(bool)),
			this, SLOT(onModificationChanged(bool)));

	connect(m_ui->saveButton, SIGNAL(clicked()), this, SLOT(onSave()));
	connect(m_ui->reloadButton, SIGNAL(clicked()), this, SLOT(onReload()));
	connect(m_ui->undoButton, SIGNAL(clicked()), m_editor, SLOT(undo()));
	connect(m_ui->redoButton, SIGNAL(clicked()), m_editor, SLOT(redo()));
	connect(m_ui->closeButton, SIGNAL(clicked()), this, SLOT(close()));
	connect(m_ui->findNextButton, SIGNAL(clicked()), this, SLOT(onFindNext()));
	connect(m_ui->findPrevButton, SIGNAL(clicked()), this, SLOT(onFindPrevious()));
	connect(m_ui->findEdit, SIGNAL(returnPressed()), this, SLOT(onFindNext()));
	connect(m_ui->checkNamesBox, SIGNAL(toggled(bool)), this, SLOT(onCheckNamesToggled(bool)));

	// Undo/Redo follow the document's own stack, so they cover typing and applied fixes alike.
	m_ui->undoButton->setEnabled(false);
	m_ui->redoButton->setEnabled(false);
	connect(m_editor->document(), SIGNAL(undoAvailable(bool)),
			m_ui->undoButton, SLOT(setEnabled(bool)));
	connect(m_editor->document(), SIGNAL(redoAvailable(bool)),
			m_ui->redoButton, SLOT(setEnabled(bool)));

	// Ctrl+S / Ctrl+F / F3 / Shift+F3 while anywhere in the window.
	installEventFilter(this);
	m_editor->installEventFilter(this);
	m_ui->findEdit->installEventFilter(this);

	// Own the window without taking the modal/parent relationship: a raw GWLP_HWNDPARENT write
	// gets reset by the Windows QPA, so use the transient parent when the Qt main window exists.
	QWidget *mainWidget = WBQt_MainWindowWidget();
	if (mainWidget != NULL && mainWidget->windowHandle() != NULL)
	{
		winId();	// realize, so windowHandle() is non-NULL
		windowHandle()->setTransientParent(mainWidget->windowHandle());
	}
	else if (frameHwnd != NULL)
	{
		::SetWindowLongPtr(reinterpret_cast<HWND>(winId()), GWLP_HWNDPARENT,
			reinterpret_cast<LONG_PTR>(WBQt_EffectiveOwnerHwnd(frameHwnd)));
	}

	const int left = WBQtMapIniEditorData_GetProfileInt("Left", -1);
	const int top = WBQtMapIniEditorData_GetProfileInt("Top", -1);
	const int width = WBQtMapIniEditorData_GetProfileInt("Width", -1);
	const int height = WBQtMapIniEditorData_GetProfileInt("Height", -1);
	if (width > 0 && height > 0)
	{
		resize(width, height);
	}
	if (left != -1 && top != -1)
	{
		move(left, top);
	}
}

WBQtMapIniEditorDialog::~WBQtMapIniEditorDialog()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

void WBQtMapIniEditorDialog::loadFile(const QString &path)
{
	m_path = path;
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		// An absent file is not an error here -- the caller creates an empty one on demand, and
		// starting from a blank buffer is the sane fallback either way.
		m_editor->setPlainText(QString());
		m_editor->document()->setModified(false);
		updateTitle();
		updateStatus(tr("Could not read %1 -- starting from an empty file.")
			.arg(QFileInfo(path).fileName()));
		return;
	}
	QTextStream in(&file);
	const QString text = in.readAll();
	file.close();

	// Seed the file's own declarations BEFORE the text goes in, so the first highlight pass
	// already knows what this map.ini defines for itself.
	WBQtMapIniHighlighter::setLocalNames(WBQtMapIniHighlighter::scanLocalNames(text));

	m_editor->setPlainText(text);
	// setPlainText leaves an undo entry for the load itself; drop it so Ctrl+Z cannot wipe the
	// file back to empty, and so "modified" starts false.
	m_editor->document()->clearUndoRedoStacks();
	m_editor->document()->setModified(false);
	updateTitle();
	updateStatus(tr("Loaded %1 line(s).").arg(m_editor->document()->blockCount()));
}

bool WBQtMapIniEditorDialog::maybeSave()
{
	if (!m_editor->document()->isModified())
	{
		return true;
	}
	const int rc = QMessageBox::question(this, tr("Map.ini Editor"),
		tr("Save the changes to %1?").arg(QFileInfo(m_path).fileName()),
		QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
	if (rc == QMessageBox::Cancel)
	{
		return false;
	}
	if (rc == QMessageBox::Save)
	{
		onSave();
		return !m_editor->document()->isModified();		// a failed write must not close
	}
	return true;
}

void WBQtMapIniEditorDialog::onSave()
{
	if (m_path.isEmpty())
	{
		return;
	}
	QFile file(m_path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		QMessageBox::warning(this, tr("Map.ini Editor"),
			tr("Could not write %1.\n\nIs the file read-only, or the map folder locked?")
				.arg(m_path));
		return;
	}
	QTextStream out(&file);
	out << m_editor->toPlainText();
	file.close();

	m_editor->document()->setModified(false);
	updateTitle();
	updateStatus(tr("Saved %1.").arg(QFileInfo(m_path).fileName()));
}

void WBQtMapIniEditorDialog::onReload()
{
	if (m_editor->document()->isModified())
	{
		if (QMessageBox::question(this, tr("Map.ini Editor"),
				tr("Discard your unsaved changes and re-read the file from disk?"),
				QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
		{
			return;
		}
	}
	// The data set may have been reloaded since the file was opened, so drop the cached
	// known-template answers too.
	WBQtMapIniHighlighter::clearNameCache();
	loadFile(m_path);
}

void WBQtMapIniEditorDialog::onCheckNamesToggled(bool on)
{
	m_highlighter->setCheckNames(on);
}

void WBQtMapIniEditorDialog::onModificationChanged(bool modified)
{
	Q_UNUSED(modified);
	updateTitle();
}

void WBQtMapIniEditorDialog::onTextChanged()
{
	m_rescanTimer->start();		// restarts the countdown; fires once typing pauses
}

void WBQtMapIniEditorDialog::rescanLocalNames()
{
	WBQtMapIniHighlighter::setLocalNames(
		WBQtMapIniHighlighter::scanLocalNames(m_editor->toPlainText()));
	m_highlighter->rehighlight();
}

void WBQtMapIniEditorDialog::onCursorMoved()
{
	const QTextCursor cursor = m_editor->textCursor();
	updateStatus(tr("Line %1, column %2")
		.arg(cursor.blockNumber() + 1)
		.arg(cursor.positionInBlock() + 1));
}

void WBQtMapIniEditorDialog::onFindNext()
{
	find(true);
}

void WBQtMapIniEditorDialog::onFindPrevious()
{
	find(false);
}

void WBQtMapIniEditorDialog::find(bool forward)
{
	const QString needle = m_ui->findEdit->text();
	if (needle.isEmpty())
	{
		return;
	}
	QTextDocument::FindFlags flags;
	if (!forward)
	{
		flags |= QTextDocument::FindBackward;
	}
	if (m_ui->matchCaseBox->isChecked())
	{
		flags |= QTextDocument::FindCaseSensitively;
	}

	if (m_editor->find(needle, flags))
	{
		return;
	}
	// Wrap around, so Find Next keeps cycling rather than dead-ending at the last hit.
	QTextCursor cursor = m_editor->textCursor();
	QTextCursor wrapped = cursor;
	wrapped.movePosition(forward ? QTextCursor::Start : QTextCursor::End);
	m_editor->setTextCursor(wrapped);
	if (m_editor->find(needle, flags))
	{
		updateStatus(tr("Wrapped to the %1 of the file.")
			.arg(forward ? tr("start") : tr("end")));
		return;
	}
	m_editor->setTextCursor(cursor);	// nothing anywhere -- leave the cursor where it was
	updateStatus(tr("\"%1\" not found.").arg(needle));
}

void WBQtMapIniEditorDialog::onEditorContextMenu(const QPoint &pos)
{
	QMenu *menu = m_editor->createStandardContextMenu();
	const QTextCursor atPoint = m_editor->cursorForPosition(pos);
	const QString blockText = atPoint.block().text();

	int start = 0;
	int length = 0;
	WBQtMapIniHighlighter::NameKind kind = WBQtMapIniHighlighter::KindNone;
	// The highlighter left each block's enclosing-block context in its user state, so the menu
	// resolves "Command =" the same way the underline did.
	const QTextBlock previous = atPoint.block().previous();
	const int context = (previous.isValid() && previous.userState() >= 0)
		? previous.userState() : (int)WBQtMapIniHighlighter::ContextOther;
	const QString unknown = WBQtMapIniHighlighter::unknownNameAt(blockText,
		atPoint.positionInBlock(), context, &start, &length, &kind);
	if (!unknown.isEmpty())
	{
		const QStringList picks = suggestionsFor(unknown, (int)kind);
		QAction *first = menu->actions().isEmpty() ? NULL : menu->actions().first();
		QAction *header = new QAction(tr("Replace \"%1\" with").arg(unknown), menu);
		header->setEnabled(false);
		menu->insertAction(first, header);

		if (picks.isEmpty())
		{
			QAction *none = new QAction(tr("    (no close match found)"), menu);
			none->setEnabled(false);
			menu->insertAction(first, none);
		}
		else
		{
			const int blockNumber = atPoint.blockNumber();
			for (int i = 0; i < picks.size(); ++i)
			{
				const QString pick = picks.at(i);
				QAction *action = new QAction("    " + pick, menu);
				connect(action, &QAction::triggered, this,
					[this, blockNumber, start, length, pick]()
					{
						replaceToken(blockNumber, start, length, pick);
					});
				menu->insertAction(first, action);
			}
		}
		menu->insertSeparator(first);
	}

	menu->exec(m_editor->viewport()->mapToGlobal(pos));
	delete menu;
}

void WBQtMapIniEditorDialog::replaceToken(int blockNumber, int start, int length,
	const QString &replacement)
{
	QTextBlock block = m_editor->document()->findBlockByNumber(blockNumber);
	if (!block.isValid())
	{
		return;
	}
	QTextCursor cursor(block);
	cursor.setPosition(block.position() + start);
	cursor.setPosition(block.position() + start + length, QTextCursor::KeepAnchor);
	// One edit block, so the replacement is a single Ctrl+Z rather than a delete plus an insert.
	cursor.beginEditBlock();
	cursor.insertText(replacement);
	cursor.endEditBlock();
	m_editor->setTextCursor(cursor);
	updateStatus(tr("Replaced with \"%1\".").arg(replacement));
}

void WBQtMapIniEditorDialog::updateTitle()
{
	QString title = tr("Map.ini Editor");
	if (!m_path.isEmpty())
	{
		title += " - " + QFileInfo(m_path).fileName();
		// Show the folder too: every map's file is called "map.ini", so the name alone does not
		// say which map is being edited.
		title += " (" + QFileInfo(m_path).absolutePath() + ")";
	}
	if (m_editor->document()->isModified())
	{
		title += " *";
	}
	setWindowTitle(title);
}

void WBQtMapIniEditorDialog::updateStatus(const QString &message)
{
	m_ui->statusLabel->setText(message);
}

bool WBQtMapIniEditorDialog::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::KeyPress)
	{
		QKeyEvent *key = static_cast<QKeyEvent *>(event);
		if (key->matches(QKeySequence::Save))
		{
			onSave();
			return true;
		}
		if (key->matches(QKeySequence::Find))
		{
			m_ui->findEdit->setFocus();
			m_ui->findEdit->selectAll();
			return true;
		}
		if (key->key() == Qt::Key_F3)
		{
			if ((key->modifiers() & Qt::ShiftModifier) != 0)
			{
				onFindPrevious();
			}
			else
			{
				onFindNext();
			}
			return true;
		}
	}
	return QWidget::eventFilter(watched, event);
}

void WBQtMapIniEditorDialog::savePosition()
{
	WBQtMapIniEditor_SetProfileInt("Left", x());
	WBQtMapIniEditor_SetProfileInt("Top", y());
	WBQtMapIniEditor_SetProfileInt("Width", width());
	WBQtMapIniEditor_SetProfileInt("Height", height());
}

void WBQtMapIniEditorDialog::closeEvent(QCloseEvent *event)
{
	if (!maybeSave())
	{
		event->ignore();
		return;
	}
	savePosition();
	event->accept();
	// Modeless and owned by nothing: drop it so the next open starts from a clean document.
	deleteLater();
}

//----------------------------------------------------------------------------------------
// the entry point
//----------------------------------------------------------------------------------------

extern "C" int WBQtMapIniEditor_Open(void *frameHwnd, const char *iniPath)
{
	if (qApp == NULL)
	{
		return 0;	// Qt not up -- the caller falls back to the system editor
	}
	const QString path = QString::fromLocal8Bit(iniPath ? iniPath : "");
	if (path.isEmpty())
	{
		return 0;
	}

	WBQtMapIniEditorDialog *dlg = WBQtMapIniEditorDialog::instance();
	if (dlg == NULL)
	{
		dlg = new WBQtMapIniEditorDialog(frameHwnd);
		dlg->setAttribute(Qt::WA_DeleteOnClose, false);		// closeEvent does the deleteLater
		dlg->loadFile(path);
	}
	dlg->show();
	dlg->raise();
	dlg->activateWindow();
	return 1;
}
