// WBQtMapIniEditorDialog.cpp -- see WBQtMapIniEditorDialog.h.
#include "WBQtMapIniEditorDialog.h"
#include "ui_WBQtMapIniEditorDialog.h"
#include "WBQtMapIniEditorBridge.h"
#include "../WBQtNameMatch.h"

#include <QAction>
#include <QApplication>
#include <QAbstractItemView>
#include <QCloseEvent>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QFont>
#include <QHash>
#include <QKeyEvent>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRegExp>
#include <QScrollBar>
#include <QSplitter>
#include <QStringListModel>
#include <QSet>
#include <QTextBlock>
#include <QTextOption>
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
			case WBQtMapIniHighlighter::KindAnyIniName:
				count = WBQtMapIniEditorData_BuildIniNames();
				break;
			case WBQtMapIniHighlighter::KindAnyIniKey:
				count = WBQtMapIniEditorData_BuildIniKeys();
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
				case WBQtMapIniHighlighter::KindAnyIniName:
					WBQtMapIniEditorData_GetIniName(i, buf, sizeof(buf));
					break;
				case WBQtMapIniHighlighter::KindAnyIniKey:
					WBQtMapIniEditorData_GetIniKey(i, buf, sizeof(buf));
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
	  m_checkNames(true),
	  m_checkSyntax(true)
{
}

void WBQtMapIniHighlighter::setCheckSyntax(bool on)
{
	if (m_checkSyntax == on)
	{
		return;
	}
	m_checkSyntax = on;
	rehighlight();
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
	m_localNames = names;
}

const QSet<QString> &WBQtMapIniHighlighter::localNames() const
{
	return m_localNames;
}

bool WBQtMapIniHighlighter::isLocallyDeclared(const QString &name) const
{
	return m_localNames.contains(name);
}

// The block names this file opens. NOTE these are not necessarily NEW names: map.ini loads with
// INI_LOAD_CREATE_OVERRIDES, so "Object Foo" means "override Foo if it exists, otherwise create
// it". That is why isKnownName consults the game data FIRST and only falls back to this set --
// treating every block header as a declaration would self-certify every object in the file and
// stop checking them entirely.
// A "Key = Value" line is a REFERENCE, never a block header, so the '=' form is excluded.
namespace
{
	// The one line-handler both scanLocalNames overloads use: insert the declared name, if any.
	void collectDeclaredName(const QString &line, QSet<QString> &names)
	{
		static const char *const kDeclKeywords[] = {
			"Object", "ChildObject", "ObjectReskin", "Upgrade", "CommandSet", "CommandButton",
			"Science", "Weapon", "Armor", "Locomotor", "FXList", "ObjectCreationList",
			NULL
		};
		QString scan = line;
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
			return;
		}
		for (int k = 0; kDeclKeywords[k] != NULL; ++k)
		{
			if (tokens.at(0).compare(QString::fromLatin1(kDeclKeywords[k]),
					Qt::CaseInsensitive) == 0)
			{
				names.insert(tokens.at(1));
				return;
			}
		}
	}
}

QSet<QString> WBQtMapIniHighlighter::scanLocalNames(const QTextDocument *doc)
{
	QSet<QString> names;
	if (doc == NULL)
	{
		return names;
	}
	for (QTextBlock block = doc->firstBlock(); block.isValid(); block = block.next())
	{
		collectDeclaredName(block.text(), names);
	}
	return names;
}

QSet<QString> WBQtMapIniHighlighter::scanLocalNames(const QString &text)
{
	QSet<QString> names;
	const QStringList lines = text.split('\n');
	for (int i = 0; i < lines.size(); ++i)
	{
		collectDeclaredName(lines.at(i), names);
	}
	return names;
}

bool WBQtMapIniHighlighter::isKnownName(const QString &name, NameKind kind) const
{
	if (isInCatalog(name, kind))
	{
		return true;
	}
	// Not in the game data. It is still valid if THIS FILE creates it -- map.ini loads with
	// INI_LOAD_CREATE_OVERRIDES, so a block header for a name the data lacks defines a new one.
	// Checked after the catalog (and deliberately not cached) so that deleting the block
	// re-flags every reference to it on the next rescan.
	return isLocallyDeclared(name);
}

bool WBQtMapIniHighlighter::isInCatalog(const QString &name, NameKind kind)
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
	return known;
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

bool WBQtMapIniHighlighter::isEndLine(const QString &line)
{
	QString scan = line;
	const int commentAt = scan.indexOf(';');
	if (commentAt >= 0)
	{
		scan = scan.left(commentAt);
	}
	// Case-insensitive: real map.inis use End, end and END interchangeably.
	return scan.trimmed().compare("End", Qt::CaseInsensitive) == 0;
}

// Whether a line opens a block that an "End" must close. Two shapes, both verified against real
// map.inis (whose Ends balance to exactly zero under this rule):
//
//   `Object Foo` / `SkillSet1`   -- a bare keyword, optionally with a name
//   `Behavior = Xyz ModuleTag_5` -- a MODULE, which despite the '=' is a block
//
// The module case is why "no '=' means no block" is wrong. But two tokens after the '=' is not
// enough either: `Locomotor = SET_NORMAL LimoLocomotor` has two and opens nothing. What separates
// them is the module-tag convention -- a module block's last token is its tag.
bool WBQtMapIniHighlighter::opensBlock(const QString &line)
{
	QString scan = line;
	const int commentAt = scan.indexOf(';');
	if (commentAt >= 0)
	{
		scan = scan.left(commentAt);
	}
	scan = scan.trimmed();
	if (scan.isEmpty() || scan.compare("End", Qt::CaseInsensitive) == 0)
	{
		return false;
	}

	const int eq = scan.indexOf('=');
	if (eq >= 0)
	{
		// A module: `Key = ModuleType ModuleTag_N`. Anything else with an '=' is a plain
		// assignment, however many tokens it has.
		const QStringList rhs = scan.mid(eq + 1).simplified()
			.split(' ', QString::SkipEmptyParts);
		if (rhs.size() < 2)
		{
			return false;
		}
		return rhs.last().startsWith("ModuleTag", Qt::CaseInsensitive);
	}

	// Bare keyword, or keyword + name. Anything with three or more tokens is not a header.
	const QStringList tokens = scan.simplified().split(' ', QString::SkipEmptyParts);
	if (tokens.size() > 2 || tokens.isEmpty())
	{
		return false;
	}
	// One-line directives that take a name but open nothing ("RemoveModule ModuleTag_DIE").
	static const char *const kOneLiners[] = { "RemoveModule", "InheritableModule", NULL };
	for (int k = 0; kOneLiners[k] != NULL; ++k)
	{
		if (tokens.at(0).compare(QString::fromLatin1(kOneLiners[k]), Qt::CaseInsensitive) == 0)
		{
			return false;
		}
	}
	// The first token must look like an identifier (a stray word or a number is not a block).
	const QString word = tokens.at(0);
	if (!word.at(0).isLetter())
	{
		return false;
	}
	for (int i = 0; i < word.length(); ++i)
	{
		if (!word.at(i).isLetterOrNumber() && word.at(i) != '_')
		{
			return false;
		}
	}
	return true;
}

WBQtMapIniHighlighter::SyntaxProblem WBQtMapIniHighlighter::checkLineSyntax(const QString &line,
	int depthBefore, int *depthAfterOut)
{
	int depth = depthBefore;
	SyntaxProblem problem = SyntaxOk;

	QString scan = line;
	const int commentAt = scan.indexOf(';');
	if (commentAt >= 0)
	{
		scan = scan.left(commentAt);
	}
	scan = scan.trimmed();

	if (isEndLine(line))
	{
		if (depth <= 0)
		{
			problem = SyntaxStrayEnd;	// nothing open for this End to close
		}
		else
		{
			--depth;
		}
	}
	else if (opensBlock(line))
	{
		++depth;
	}
	else if (!scan.isEmpty())
	{
		// `Key =` with nothing after it. The engine's parsers read the value straight off the
		// line, so an empty one is a silent default rather than an error it reports.
		const int eq = scan.indexOf('=');
		if (eq >= 0 && scan.mid(eq + 1).trimmed().isEmpty())
		{
			problem = SyntaxMissingValue;
		}
	}

	if (depthAfterOut != NULL)
	{
		*depthAfterOut = depth;
	}
	return problem;
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

	// Carry the enclosing-block context AND the nesting depth forward, packed into the one int
	// of per-block state Qt gives us. This runs whether or not the checks are on, so toggling
	// one back on does not need a full re-scan to know where the blocks are.
	const int previousState = previousBlockState();
	const int previousContext = stateContext(previousState);
	const int depthBefore = stateDepth(previousState);
	const int context = contextAfterLine(text, previousContext);

	int depthAfter = depthBefore;
	const SyntaxProblem problem = checkLineSyntax(text, depthBefore, &depthAfter);
	setCurrentBlockState(packState(context, depthAfter));

	if (m_checkSyntax && problem != SyntaxOk)
	{
		// Red for syntax: unlike an unrecognised name (which may just be data that is not
		// loaded), a stray End or a valueless key is wrong in the file itself.
		QTextCharFormat fmt;
		fmt.setUnderlineColor(QColor(200, 60, 60));
		fmt.setUnderlineStyle(QTextCharFormat::WaveUnderline);
		int from = 0;
		int span = text.length();
		// Underline just the offending token where there is one.
		const int firstNonSpace = text.length() - QString(text).remove(QRegExp("^\\s*")).length();
		if (problem == SyntaxStrayEnd)
		{
			from = firstNonSpace;
			span = 3;		// "End"
		}
		if (span > 0)
		{
			setFormat(from, span, fmt);
		}
	}

	// An unclosed block can only be known at the END of the file, so it is flagged by the
	// dialog's own pass (which knows where the last line is), not from here.

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
// WBQtMapIniValuePicker -- Ctrl+Space's searchable list
//----------------------------------------------------------------------------------------

WBQtMapIniValuePicker::WBQtMapIniValuePicker(const QStringList &candidates,
	const QString &current, QWidget *parent)
	: QDialog(parent),
	  m_candidates(candidates),
	  m_filter(NULL),
	  m_list(NULL)
{
	setWindowTitle(tr("Insert Value"));
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	resize(420, 460);

	QVBoxLayout *layout = new QVBoxLayout(this);

	m_filter = new QLineEdit(this);
	m_filter->setPlaceholderText(tr("type to filter (matches anywhere in the name)"));
	layout->addWidget(m_filter);

	m_list = new QListWidget(this);
	m_list->setAlternatingRowColors(true);
	// Monospaced, like the editor: these are identifiers, and the shared prefixes line up.
	QFont mono("Consolas");
	mono.setStyleHint(QFont::Monospace);
	mono.setFixedPitch(true);
	m_list->setFont(mono);
	layout->addWidget(m_list, 1);

	QDialogButtonBox *buttons = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
	layout->addWidget(buttons);

	connect(m_filter, SIGNAL(textChanged(QString)), this, SLOT(onFilterChanged(QString)));
	connect(m_list, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(onRowActivated()));
	connect(buttons, SIGNAL(accepted()), this, SLOT(onRowActivated()));
	connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));

	// Enter in the filter box takes the highlighted row, so the whole flow is keyboard-only:
	// Ctrl+Space, type a fragment, Enter.
	connect(m_filter, SIGNAL(returnPressed()), this, SLOT(onRowActivated()));

	// Seed with everything, preselecting whatever was already typed.
	onFilterChanged(QString());
	if (!current.isEmpty())
	{
		m_filter->setText(current);
	}
	m_filter->installEventFilter(this);		// arrow keys drive the list, not the text cursor
	m_filter->setFocus();
}

void WBQtMapIniValuePicker::onFilterChanged(const QString &text)
{
	m_list->clear();
	// "Contains", not "starts with": the inline popup already does prefix matching, so the point
	// of this dialog is finding a name by a fragment from the middle of it.
	for (int i = 0; i < m_candidates.size(); ++i)
	{
		const QString &candidate = m_candidates.at(i);
		if (text.isEmpty() || candidate.contains(text, Qt::CaseInsensitive))
		{
			new QListWidgetItem(candidate, m_list);
		}
	}
	if (m_list->count() > 0)
	{
		m_list->setCurrentRow(0);
	}
}

void WBQtMapIniValuePicker::onRowActivated()
{
	QListWidgetItem *item = m_list->currentItem();
	if (item == NULL)
	{
		return;		// nothing matched the filter -- leave the dialog open
	}
	m_picked = item->text();
	accept();
}

// The filter box keeps focus, so route the arrow keys to the list: type a fragment, arrow to the
// row you want, Enter. Without this the arrows would only move the text cursor.
bool WBQtMapIniValuePicker::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == m_filter && event->type() == QEvent::KeyPress)
	{
		QKeyEvent *key = static_cast<QKeyEvent *>(event);
		switch (key->key())
		{
			case Qt::Key_Up:
			case Qt::Key_Down:
			case Qt::Key_PageUp:
			case Qt::Key_PageDown:
				QApplication::sendEvent(m_list, event);
				return true;
			default:
				break;
		}
	}
	return QDialog::eventFilter(watched, event);
}

//----------------------------------------------------------------------------------------
// WBQtMapIniEditorDialog
//----------------------------------------------------------------------------------------

WBQtMapIniEditorDialog *WBQtMapIniEditorDialog::s_instance = NULL;

WBQtMapIniEditorDialog::WBQtMapIniEditorDialog(void *frameHwnd)
	: QWidget(NULL, Qt::Window),
	  m_ui(new Ui::WBQtMapIniEditorDialog),
	  m_highlighter(NULL),
	  m_editor(NULL),
	  m_rescanTimer(NULL),
	  m_errorList(NULL),
	  m_split(NULL),
	  m_completer(NULL),
	  m_recentMenu(NULL)
{
	s_instance = this;

	// The static widget tree lives in WBQtMapIniEditorDialog.ui; wire what Designer can't express.
	m_ui->setupUi(this);

	buildMenuBar();

	// The editor, its highlighter, the error pane and the rescan timer.
	createEditor();

	connect(m_ui->saveButton, SIGNAL(clicked()), this, SLOT(onSave()));
	connect(m_ui->reloadButton, SIGNAL(clicked()), this, SLOT(onReload()));
	connect(m_ui->undoButton, SIGNAL(clicked()), this, SLOT(onUndo()));
	connect(m_ui->redoButton, SIGNAL(clicked()), this, SLOT(onRedo()));
	connect(m_ui->closeButton, SIGNAL(clicked()), this, SLOT(close()));
	connect(m_ui->findNextButton, SIGNAL(clicked()), this, SLOT(onFindNext()));
	connect(m_ui->findPrevButton, SIGNAL(clicked()), this, SLOT(onFindPrevious()));
	connect(m_ui->findEdit, SIGNAL(returnPressed()), this, SLOT(onFindNext()));
	connect(m_ui->checkNamesBox, SIGNAL(toggled(bool)), this, SLOT(onCheckNamesToggled(bool)));
	connect(m_ui->checkSyntaxBox, SIGNAL(toggled(bool)), this, SLOT(onCheckSyntaxToggled(bool)));
	connect(m_ui->whitespaceBox, SIGNAL(toggled(bool)), this, SLOT(onWhitespaceToggled(bool)));
	connect(m_ui->showErrorsBox, SIGNAL(toggled(bool)), this, SLOT(onShowErrorsToggled(bool)));

	// Autocomplete over the same catalogs the checking uses, so it can only offer names that
	// would also validate. The model is swapped per line kind (object / upgrade / science / ...).
	m_completer = new QCompleter(this);
	// MUST be set before anything touches popup(): a widget-less QCompleter has no popup to
	// return. (The multi-tab version set this per tab; with one editor it is set once, here.)
	m_completer->setWidget(m_editor);
	m_completer->setCompletionMode(QCompleter::PopupCompletion);
	m_completer->setCaseSensitivity(Qt::CaseInsensitive);
	m_completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
	connect(m_completer, SIGNAL(activated(QString)), this, SLOT(onCompletionChosen(QString)));
	connect(m_ui->autoCompleteBox, SIGNAL(toggled(bool)), this, SLOT(onAutoCompleteToggled(bool)));

	// Ctrl+F / F3 / Shift+F3 while anywhere in the window. createEditor installs the filter on
	// the editor itself.
	installEventFilter(this);
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
	m_highlighter->setLocalNames(WBQtMapIniHighlighter::scanLocalNames(text));

	m_editor->setPlainText(text);
	// setPlainText leaves an undo entry for the load itself; drop it so Ctrl+Z cannot wipe the
	// file back to empty, and so "modified" starts false.
	m_editor->document()->clearUndoRedoStacks();
	m_editor->document()->setModified(false);
	updateTitle();

	// Back off the rescan on a very large file. Every fire walks all the lines, and the work is
	// proportional to the file, so the same 400ms that feels instant on a few hundred lines is
	// noticeable on ten thousand.
	const int lineCount = m_editor->document()->blockCount();
	m_rescanTimer->setInterval(lineCount > 5000 ? 1200 : 400);

	noteRecentFile(path);
	updateStatus(tr("Loaded %1 line(s).").arg(lineCount));
}

bool WBQtMapIniEditorDialog::maybeSave()
{
	if (m_editor == NULL)
	{
		return true;		// no tab open: nothing to save, so never block
	}
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

//----------------------------------------------------------------------------------------
// The editor
//
// One file open at a time. The editor, its highlighter, the error pane and the rescan timer are
// built once into the .ui's editorHost; everything else in this class acts on those members.
//----------------------------------------------------------------------------------------

void WBQtMapIniEditorDialog::createEditor()
{
	m_split = new QSplitter(Qt::Vertical, m_ui->editorHost);
	m_editor = new QPlainTextEdit(m_split);
	m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
	m_errorList = new QListWidget(m_split);
	m_errorList->setAlternatingRowColors(true);
	m_errorList->setMaximumHeight(200);
	m_errorList->setToolTip(tr("Lines with a problem: red for syntax, orange for an "
		"unrecognised name. Click one to jump to it in the file above."));
	m_errorList->hide();		// the pane only appears once "Show errors" is ticked
	m_split->addWidget(m_editor);
	m_split->addWidget(m_errorList);
	m_split->setStretchFactor(0, 1);	// the text takes the slack when the window resizes
	m_split->setStretchFactor(1, 0);
	m_split->setChildrenCollapsible(false);
	m_ui->editorHostLay->addWidget(m_split);

	// Fixed-pitch: INI files are column-aligned by hand and a proportional font ruins that.
	QFont mono("Consolas");
	mono.setStyleHint(QFont::Monospace);
	mono.setFixedPitch(true);
	mono.setPointSize(10);
	m_editor->setFont(mono);
	m_editor->setTabStopWidth(4 * QFontMetrics(mono).width(' '));

	m_highlighter = new WBQtMapIniHighlighter(m_editor->document());

	// Rescan the file's own declarations after a pause in typing: declaring an object partway
	// through typing its name must not re-highlight the whole document on every keystroke. The
	// interval is raised for very large files in loadFile.
	m_rescanTimer = new QTimer(this);
	m_rescanTimer->setSingleShot(true);
	m_rescanTimer->setInterval(400);

	m_editor->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_editor, SIGNAL(customContextMenuRequested(QPoint)),
			this, SLOT(onEditorContextMenu(QPoint)));
	connect(m_editor, SIGNAL(cursorPositionChanged()), this, SLOT(onCursorMoved()));
	connect(m_editor->document(), SIGNAL(modificationChanged(bool)),
			this, SLOT(onModificationChanged(bool)));
	connect(m_rescanTimer, SIGNAL(timeout()), this, SLOT(rescanLocalNames()));
	connect(m_errorList, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
			this, SLOT(onErrorRowChanged(QListWidgetItem*,QListWidgetItem*)));
	connect(m_editor, SIGNAL(textChanged()), this, SLOT(onTextChanged()));
	m_editor->installEventFilter(this);

	// Undo/redo follow the document's own stack, so they cover typing and applied fixes alike.
	m_ui->undoButton->setEnabled(false);
	m_ui->redoButton->setEnabled(false);
	connect(m_editor->document(), SIGNAL(undoAvailable(bool)),
			m_ui->undoButton, SLOT(setEnabled(bool)));
	connect(m_editor->document(), SIGNAL(redoAvailable(bool)),
			m_ui->redoButton, SLOT(setEnabled(bool)));
}

// Routed rather than connected straight to the editor's own undo/redo slots, so the buttons keep
// working if the editor is ever rebuilt underneath them.
void WBQtMapIniEditorDialog::onUndo()
{
	if (m_editor != NULL)
	{
		m_editor->undo();
	}
}

void WBQtMapIniEditorDialog::onRedo()
{
	if (m_editor != NULL)
	{
		m_editor->redo();
	}
}


// The File menu. Everything in the editor already works off m_path, so opening an arbitrary .ini
// needs no other change -- the name checking, autocomplete and syntax checking are about INI
// structure and the loaded game data, not about this file being a map.ini.
void WBQtMapIniEditorDialog::buildMenuBar()
{
	QMenuBar *bar = new QMenuBar(this);
	QMenu *fileMenu = bar->addMenu(tr("&File"));

	QAction *open = fileMenu->addAction(tr("&Open INI..."), this, SLOT(onOpen()));
	open->setShortcut(QKeySequence::Open);
	fileMenu->addAction(tr("Open This &Map's map.ini"), this, SLOT(onOpenMapIni()));

	m_recentMenu = fileMenu->addMenu(tr("Open &Recent"));
	rebuildRecentMenu();

	fileMenu->addSeparator();
	QAction *save = fileMenu->addAction(tr("&Save"), this, SLOT(onSave()));
	save->setShortcut(QKeySequence::Save);
	fileMenu->addAction(tr("Save &As..."), this, SLOT(onSaveAs()));
	fileMenu->addAction(tr("&Reload"), this, SLOT(onReload()));
	fileMenu->addSeparator();
	fileMenu->addAction(tr("&Close"), this, SLOT(close()));

	// A QMenuBar dropped into a QVBoxLayout stretches to the full width and reads like a toolbar
	// rather than a menu bar. This is a QWidget, not a QMainWindow, so there is no native menu
	// bar slot -- put it in a row of its own, left-aligned, and let a spacer take the slack.
	QHBoxLayout *menuRow = new QHBoxLayout();
	menuRow->setContentsMargins(0, 0, 0, 0);
	menuRow->addWidget(bar, 0, Qt::AlignLeft);
	menuRow->addStretch(1);
	bar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
	m_ui->rootLayout->insertLayout(0, menuRow);
}

void WBQtMapIniEditorDialog::rebuildRecentMenu()
{
	if (m_recentMenu == NULL)
	{
		return;
	}
	m_recentMenu->clear();
	char buf[kTextCap];
	int added = 0;
	for (int i = 0; i < 10; ++i)
	{
		buf[0] = 0;
		const QByteArray key = QString("Recent%1").arg(i).toLatin1();
		WBQtMapIniEditorData_GetProfileString(key.constData(), buf, sizeof(buf));
		const QString path = QString::fromLocal8Bit(buf);
		if (path.isEmpty())
		{
			continue;
		}
		// Show the folder too: a menu full of identical "map.ini" entries would be useless.
		QAction *action = m_recentMenu->addAction(
			QString("%1  (%2)").arg(QFileInfo(path).fileName(), QFileInfo(path).absolutePath()),
			this, SLOT(onOpenRecent()));
		action->setData(path);
		++added;
	}
	if (added == 0)
	{
		QAction *none = m_recentMenu->addAction(tr("(nothing yet)"));
		none->setEnabled(false);
	}
}

void WBQtMapIniEditorDialog::noteRecentFile(const QString &path)
{
	if (path.isEmpty())
	{
		return;
	}
	// Read the current list, move this path to the front, drop duplicates, keep 10.
	QStringList recent;
	recent.append(QDir::toNativeSeparators(path));
	char buf[kTextCap];
	for (int i = 0; i < 10; ++i)
	{
		buf[0] = 0;
		const QByteArray key = QString("Recent%1").arg(i).toLatin1();
		WBQtMapIniEditorData_GetProfileString(key.constData(), buf, sizeof(buf));
		const QString existing = QString::fromLocal8Bit(buf);
		if (!existing.isEmpty()
			&& QString::compare(existing, recent.first(), Qt::CaseInsensitive) != 0
			&& !recent.contains(existing, Qt::CaseInsensitive))
		{
			recent.append(existing);
		}
	}
	while (recent.size() > 10)
	{
		recent.removeLast();
	}
	for (int i = 0; i < 10; ++i)
	{
		const QByteArray key = QString("Recent%1").arg(i).toLatin1();
		const QByteArray value = (i < recent.size())
			? recent.at(i).toLocal8Bit() : QByteArray();
		WBQtMapIniEditor_SetProfileString(key.constData(), value.constData());
	}
	rebuildRecentMenu();
}

void WBQtMapIniEditorDialog::onOpen()
{
	if (!maybeSave())
	{
		return;
	}
	// Start where the current file is, or the game's Data\INI if there is none.
	const QString startIn = m_path.isEmpty() ? QString() : QFileInfo(m_path).absolutePath();
	const QString path = QFileDialog::getOpenFileName(this, tr("Open INI File"), startIn,
		tr("INI files (*.ini);;All files (*.*)"));
	if (path.isEmpty())
	{
		return;
	}
	loadFile(QDir::toNativeSeparators(path));
}

void WBQtMapIniEditorDialog::onOpenMapIni()
{
	char buf[kTextCap];
	buf[0] = 0;
	WBQtMapIniEditorData_GetPath(buf, sizeof(buf));
	const QString path = QString::fromLocal8Bit(buf);
	if (path.isEmpty())
	{
		QMessageBox::information(this, tr("Map.ini Editor"),
			tr("No map is open, so there is no map.ini to go back to."));
		return;
	}
	if (!maybeSave())
	{
		return;
	}
	loadFile(path);
}

void WBQtMapIniEditorDialog::onOpenRecent()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (action == NULL)
	{
		return;
	}
	const QString path = action->data().toString();
	if (path.isEmpty())
	{
		return;
	}
	if (!QFile::exists(path))
	{
		QMessageBox::warning(this, tr("Map.ini Editor"),
			tr("%1 is no longer there.").arg(path));
		return;
	}
	if (!maybeSave())
	{
		return;
	}
	loadFile(path);
}

void WBQtMapIniEditorDialog::onSaveAs()
{
	const QString startIn = m_path.isEmpty() ? QString() : m_path;
	const QString path = QFileDialog::getSaveFileName(this, tr("Save INI File As"), startIn,
		tr("INI files (*.ini);;All files (*.*)"));
	if (path.isEmpty())
	{
		return;
	}
	m_path = QDir::toNativeSeparators(path);
	onSave();
	noteRecentFile(m_path);
	updateTitle();
}

void WBQtMapIniEditorDialog::onSave()
{
	if (m_editor == NULL)
	{
		return;		// no tab open: nothing to act on
	}
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
	if (m_editor == NULL)
	{
		return;		// no tab open: nothing to act on
	}
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
	if (m_editor == NULL)
	{
		return;		// no tab open: nothing to act on
	}
	m_highlighter->setCheckNames(on);
	if (m_ui->showErrorsBox->isChecked())
	{
		rebuildErrorList();
	}
}

// Render spaces and tabs. INI files are hand-aligned, so a tab where the rest of the block uses
// spaces is invisible until you turn this on.
void WBQtMapIniEditorDialog::onWhitespaceToggled(bool on)
{
	if (m_editor == NULL)
	{
		return;		// no tab open: nothing to act on
	}
	QTextOption options = m_editor->document()->defaultTextOption();
	QTextOption::Flags flags = options.flags();
	if (on)
	{
		flags |= QTextOption::ShowTabsAndSpaces;
	}
	else
	{
		flags &= ~QTextOption::ShowTabsAndSpaces;
	}
	options.setFlags(flags);
	m_editor->document()->setDefaultTextOption(options);
	// setDefaultTextOption does not repaint laid-out blocks on its own.
	m_editor->viewport()->update();
}

void WBQtMapIniEditorDialog::onCheckSyntaxToggled(bool on)
{
	if (m_editor == NULL)
	{
		return;		// no tab open: nothing to act on
	}
	m_highlighter->setCheckSyntax(on);
	if (m_ui->showErrorsBox->isChecked())
	{
		rebuildErrorList();
	}
}

void WBQtMapIniEditorDialog::onModificationChanged(bool modified)
{
	Q_UNUSED(modified);
	updateTitle();
	if (m_editor != NULL)
	{
		m_ui->undoButton->setEnabled(m_editor->document()->isUndoAvailable());
		m_ui->redoButton->setEnabled(m_editor->document()->isRedoAvailable());
	}
}

// Walk the whole file, collecting the lines whose name does not resolve. Runs the same matcher
// and the same block-context tracking the highlighter uses, so the list and the underlines can
// never disagree.
void WBQtMapIniEditorDialog::rebuildErrorList()
{
	if (m_editor == NULL)
	{
		return;		// no tab open: nothing to act on
	}
	// The list refills on every edit; re-selecting by ROW would jump around as lines are fixed,
	// so remember which file line was selected and restore that instead. Blocked so the restore
	// does not fire the navigation slot and yank the cursor away from where you are typing.
	int wasLine = -1;
	if (m_errorList->currentItem() != NULL)
	{
		wasLine = m_errorList->currentItem()->data(Qt::UserRole).toInt();
	}
	const bool blocked = m_errorList->blockSignals(true);

	m_errorList->clear();
	// Walk the document's blocks rather than toPlainText().split('\n'): that copies the entire
	// buffer and then allocates a QString per line, which on a 10,000-line file is megabytes of
	// churn every time this runs.
	int context = WBQtMapIniHighlighter::ContextOther;
	int flagged = 0;
	int i = -1;
	int depth = 0;
	// Where each still-open block was opened, so an unclosed one can name its own line.
	QList<int> openLines;
	for (QTextBlock block = m_editor->document()->firstBlock();
			block.isValid();
			block = block.next())
	{
		++i;
		const QString line = block.text();
		context = WBQtMapIniHighlighter::contextAfterLine(line, context);

		// Structural problems first -- they are about the line itself, not what it names.
		if (m_ui->checkSyntaxBox->isChecked())
		{
			int depthAfter = depth;
			const WBQtMapIniHighlighter::SyntaxProblem problem =
				WBQtMapIniHighlighter::checkLineSyntax(line, depth, &depthAfter);
			if (WBQtMapIniHighlighter::opensBlock(line))
			{
				openLines.append(i);
			}
			else if (WBQtMapIniHighlighter::isEndLine(line) && !openLines.isEmpty())
			{
				openLines.removeLast();
			}
			depth = depthAfter;

			if (problem != WBQtMapIniHighlighter::SyntaxOk)
			{
				const QString what = (problem == WBQtMapIniHighlighter::SyntaxStrayEnd)
					? tr("End with no block open")
					: tr("key with no value");
				QListWidgetItem *item = new QListWidgetItem(
					tr("%1:  %2  --  %3").arg(i + 1, 5).arg(line.trimmed()).arg(what),
					m_errorList);
				item->setData(Qt::UserRole, i);
				item->setForeground(QBrush(QColor(200, 60, 60)));
				++flagged;
			}
		}

		QString name;
		int start = 0;
		int length = 0;
		WBQtMapIniHighlighter::NameKind kind = WBQtMapIniHighlighter::KindNone;
		if (!checkableNameOnLineIn(line, context, &name, &start, &length, &kind))
		{
			continue;
		}
		if (m_highlighter->isKnownName(name, kind))
		{
			continue;
		}
		QListWidgetItem *item = new QListWidgetItem(
			tr("%1:  %2").arg(i + 1, 5).arg(line.trimmed()), m_errorList);
		// The line number the row jumps to; the text itself is only for reading.
		item->setData(Qt::UserRole, i);
		item->setForeground(QBrush(QColor(220, 140, 40)));
		++flagged;
	}
	// Blocks still open at end of file: each one is missing its End. Only knowable here, since
	// the highlighter sees a line at a time and cannot tell "not closed yet" from "never closed".
	if (m_ui->checkSyntaxBox->isChecked())
	{
		for (int n = 0; n < openLines.size(); ++n)
		{
			const int lineNumber = openLines.at(n);
			const QString lineText =
				m_editor->document()->findBlockByNumber(lineNumber).text().trimmed();
			QListWidgetItem *item = new QListWidgetItem(
				tr("%1:  %2  --  block never closed (missing End)")
					.arg(lineNumber + 1, 5).arg(lineText),
				m_errorList);
			item->setData(Qt::UserRole, lineNumber);
			item->setForeground(QBrush(QColor(200, 60, 60)));
			++flagged;
		}
	}

	if (flagged == 0)
	{
		QListWidgetItem *item = new QListWidgetItem(
			tr("No problems found."), m_errorList);
		item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
		item->setForeground(palette().brush(QPalette::Disabled, QPalette::Text));
	}

	// Restore the previously selected FILE LINE if it is still flagged.
	if (wasLine >= 0)
	{
		for (int i = 0; i < m_errorList->count(); ++i)
		{
			QListWidgetItem *item = m_errorList->item(i);
			if (item->data(Qt::UserRole).toInt() == wasLine)
			{
				m_errorList->setCurrentItem(item);
				break;
			}
		}
	}
	m_errorList->blockSignals(blocked);

	// The count goes on the checkbox, NOT the status label: that label follows the cursor, and
	// with the pane open the cursor moves constantly, so a count there would be wiped the moment
	// you clicked a row.
	m_ui->showErrorsBox->setText(flagged > 0
		? tr("Show errors (%1)").arg(flagged) : tr("Show errors"));
}

void WBQtMapIniEditorDialog::onShowErrorsToggled(bool on)
{
	if (m_editor == NULL)
	{
		return;		// no tab open: nothing to act on
	}
	if (on)
	{
		// Listing the flagged lines while name checking is off would read as a contradiction
		// (an empty list next to an unticked "Check names"), so switch it back on.
		m_ui->checkNamesBox->setChecked(true);
		rebuildErrorList();
	}
	// A second pane BELOW the text rather than a mode that replaces it (== the script editor's
	// comment pane): the file stays visible and editable while the list is up, so clicking
	// through the flagged lines does not keep closing the thing you are trying to fix.
	m_errorList->setVisible(on);
	if (!on)
	{
		m_editor->setFocus();
	}
}

void WBQtMapIniEditorDialog::onErrorRowChanged(QListWidgetItem *item, QListWidgetItem *previous)
{
	Q_UNUSED(previous);
	if (item == NULL)
	{
		return;
	}
	bool ok = false;
	const int lineNumber = item->data(Qt::UserRole).toInt(&ok);
	if (!ok)
	{
		return;		// the "nothing found" placeholder row
	}
	// The pane stays open -- selecting a row only moves the cursor in the editor above it.
	QTextBlock block = m_editor->document()->findBlockByNumber(lineNumber);
	if (!block.isValid())
	{
		return;
	}
	QTextCursor cursor(block);
	cursor.select(QTextCursor::LineUnderCursor);
	m_editor->setTextCursor(cursor);
	m_editor->centerCursor();
	// Focus deliberately stays on the list: with the pane open you step through the flagged
	// lines with the arrow keys, and stealing focus to the editor would break that after one row.
}

// The partial word left of the cursor. Stops at whitespace and '=' so "Upgrade = Upgr" offers
// completions for "Upgr" rather than the whole line.
QString WBQtMapIniEditorDialog::wordUnderCursor(int *startInBlockOut) const
{
	const QTextCursor cursor = m_editor->textCursor();
	const QString block = cursor.block().text();
	int at = cursor.positionInBlock();
	int end = at;
	while (at > 0)
	{
		const QChar ch = block.at(at - 1);
		if (ch.isSpace() || ch == '=')
		{
			break;
		}
		--at;
	}
	if (startInBlockOut != NULL)
	{
		*startInBlockOut = at;
	}
	return block.mid(at, end - at);
}

// Offer names from the catalog the cursor's line expects. `force` is Ctrl+Space, which pops the
// list even for an empty prefix; otherwise a minimum prefix keeps the popup from appearing after
// a single character.
void WBQtMapIniEditorDialog::maybeComplete(bool force)
{
	if (m_editor == NULL)
	{
		return;		// no tab open: nothing to act on
	}
	if (!m_ui->autoCompleteBox->isChecked())
	{
		return;
	}

	const QTextCursor cursor = m_editor->textCursor();
	int wordStart = 0;
	const QString prefix = wordUnderCursor(&wordStart);
	if (!force && prefix.length() < 2)
	{
		m_completer->popup()->hide();
		return;
	}

	// Same narrowing the Ctrl+Space picker uses, so the two never disagree. Keyed on the KEY text
	// rather than the kind, since "values used with Surfaces" and "values used with Science" are
	// different lists that share a kind.
	const QString cacheKey = keyOnCurrentLine(wordStart) + "/"
		+ QString::number(catalogKindAtCursor(wordStart));
	if (cacheKey != m_completerFor)
	{
		const QStringList catalog = candidatesAtCursor(wordStart, NULL);
		if (catalog.isEmpty())
		{
			m_completer->popup()->hide();
			return;		// nothing to offer (e.g. command sets, which cannot be enumerated)
		}
		m_completer->setModel(new QStringListModel(catalog, m_completer));
		m_completerFor = cacheKey;
	}

	m_completer->setCompletionPrefix(prefix);
	if (m_completer->completionCount() == 0)
	{
		m_completer->popup()->hide();
		return;
	}
	// Position the popup at the start of the word being completed.
	QTextCursor rectCursor = cursor;
	rectCursor.setPosition(cursor.block().position() + wordStart);
	QRect rect = m_editor->cursorRect(rectCursor);
	rect.setWidth(m_completer->popup()->sizeHintForColumn(0)
		+ m_completer->popup()->verticalScrollBar()->sizeHint().width());
	m_completer->complete(rect);
}

// Which catalog the cursor's position expects. Shared by the inline popup and the Ctrl+Space
// picker so both offer exactly the same candidates.
int WBQtMapIniEditorDialog::catalogKindAtCursor(int wordStart) const
{
	const QTextCursor cursor = m_editor->textCursor();
	const QTextBlock previous = cursor.block().previous();
	const int context = (previous.isValid() && previous.userState() >= 0)
		? WBQtMapIniHighlighter::stateContext(previous.userState())
		: (int)WBQtMapIniHighlighter::ContextOther;

	// Probe with the partial word replaced, so an incomplete value still matches its key.
	QString probe = cursor.block().text();
	const int prefixLength = cursor.positionInBlock() - wordStart;
	if (prefixLength > 0 && wordStart >= 0 && wordStart + prefixLength <= probe.length())
	{
		probe.replace(wordStart, prefixLength, "x");
	}

	QString name;
	int start = 0;
	int length = 0;
	WBQtMapIniHighlighter::NameKind kind = WBQtMapIniHighlighter::KindNone;
	if (checkableNameOnLineIn(probe, context, &name, &start, &length, &kind)
		&& kind != WBQtMapIniHighlighter::KindNone
		&& start == wordStart)
	{
		return (int)kind;
	}
	// No validated kind here: fall back to the INI-tree catalogs, choosing by which side of the
	// '=' the cursor sits on.
	const QString before = cursor.block().text().left(wordStart);
	return before.contains('=')
		? (int)WBQtMapIniHighlighter::KindAnyIniName
		: (int)WBQtMapIniHighlighter::KindAnyIniKey;
}

// The key whose value the cursor is typing: "Surfaces" from "Surfaces = GROU|". Empty when the
// cursor is on the key side, or the line has no '=' yet.
QString WBQtMapIniEditorDialog::keyOnCurrentLine(int wordStart) const
{
	QString line = m_editor->textCursor().block().text().left(wordStart);
	const int commentAt = line.indexOf(';');
	if (commentAt >= 0)
	{
		line = line.left(commentAt);
	}
	const int eq = line.indexOf('=');
	if (eq < 0)
	{
		return QString();		// still on the key side
	}
	return line.left(eq).trimmed();
}

// Narrowest useful candidate list for the cursor's position.
QStringList WBQtMapIniEditorDialog::candidatesAtCursor(int wordStart, QString *labelOut) const
{
	// 1. The values actually seen with THIS key. Far and away the most useful: "Surfaces ="
	//    offers the five surface names rather than every name in the game data.
	const QString key = keyOnCurrentLine(wordStart);
	if (!key.isEmpty())
	{
		const QByteArray raw = key.toLocal8Bit();
		const int count = WBQtMapIniEditorData_BuildValuesForKey(raw.constData());
		if (count > 0)
		{
			QStringList out;
			char buf[kTextCap];
			for (int i = 0; i < count; ++i)
			{
				buf[0] = 0;
				WBQtMapIniEditorData_GetValueForKey(i, buf, sizeof(buf));
				out.append(QString::fromLocal8Bit(buf));
			}
			// Fold in the engine-validated catalog for this position, so a name that exists but
			// happens never to be used with this key in the shipped data is still offered.
			const int kind = catalogKindAtCursor(wordStart);
			if (kind != WBQtMapIniHighlighter::KindAnyIniName
				&& kind != WBQtMapIniHighlighter::KindAnyIniKey)
			{
				const QStringList &validated = catalogFor(kind);
				for (int i = 0; i < validated.size(); ++i)
				{
					if (!out.contains(validated.at(i)))
					{
						out.append(validated.at(i));
					}
				}
			}
			out.sort();
			if (labelOut != NULL)
			{
				*labelOut = tr("values used with \"%1\"").arg(key);
			}
			return out;
		}
	}

	// 2. Otherwise the catalog for the kind (a validated one where the key is checkable, else the
	//    whole-tree names or keys).
	const int kind = catalogKindAtCursor(wordStart);
	if (labelOut != NULL)
	{
		*labelOut = (kind == WBQtMapIniHighlighter::KindAnyIniKey)
			? tr("keys") : tr("names from the game data");
	}
	return catalogFor(kind);
}

void WBQtMapIniEditorDialog::openValuePicker()
{
	if (m_editor == NULL)
	{
		return;		// no tab open: nothing to act on
	}
	int wordStart = 0;
	const QString prefix = wordUnderCursor(&wordStart);

	// The picker is explicit, so build the catalog even when autocomplete is switched off -- the
	// user asked for a list by pressing the key.
	QApplication::setOverrideCursor(Qt::WaitCursor);
	QString label;
	const QStringList catalog = candidatesAtCursor(wordStart, &label);
	QApplication::restoreOverrideCursor();

	if (catalog.isEmpty())
	{
		updateStatus(tr("Nothing to suggest here."));
		return;
	}

	WBQtMapIniValuePicker picker(catalog, prefix, this);
	picker.setWindowTitle(tr("Insert Value - %1 (%2)").arg(label).arg(catalog.size()));
	if (picker.exec() != QDialog::Accepted || picker.picked().isEmpty())
	{
		return;
	}
	// Same replacement path as accepting an inline completion: one undo step.
	QTextCursor cursor = m_editor->textCursor();
	const int blockPos = cursor.block().position();
	cursor.setPosition(blockPos + wordStart);
	cursor.setPosition(blockPos + wordStart + prefix.length(), QTextCursor::KeepAnchor);
	cursor.beginEditBlock();
	cursor.insertText(picker.picked());
	cursor.endEditBlock();
	m_editor->setTextCursor(cursor);
	m_editor->setFocus();
}

void WBQtMapIniEditorDialog::onAutoCompleteToggled(bool on)
{
	if (!on)
	{
		m_completer->popup()->hide();
		return;
	}
	// Warm the INI-tree scan here rather than on the first keystroke: it reads every .ini under
	// Data\INI (thousands of files, inside the .big archives), so doing it lazily would stall
	// the first character typed. Cached after this, so the wait happens once per session.
	QApplication::setOverrideCursor(Qt::WaitCursor);
	const int names = WBQtMapIniEditorData_BuildIniNames();
	const int keys = WBQtMapIniEditorData_BuildIniKeys();
	QApplication::restoreOverrideCursor();
	updateStatus(tr("Autocomplete ready: %1 name(s), %2 key(s) from the game data.")
		.arg(names).arg(keys));
}

void WBQtMapIniEditorDialog::onCompletionChosen(const QString &completion)
{
	// Replace the partial word with the chosen name, as one undo step.
	int wordStart = 0;
	const QString prefix = wordUnderCursor(&wordStart);
	QTextCursor cursor = m_editor->textCursor();
	const int blockPos = cursor.block().position();
	cursor.setPosition(blockPos + wordStart);
	cursor.setPosition(blockPos + wordStart + prefix.length(), QTextCursor::KeepAnchor);
	cursor.beginEditBlock();
	cursor.insertText(completion);
	cursor.endEditBlock();
	m_editor->setTextCursor(cursor);
}

void WBQtMapIniEditorDialog::onTextChanged()
{
	if (m_editor == NULL)
	{
		return;		// no tab open: nothing to act on
	}
	m_rescanTimer->start();		// restarts the countdown; fires once typing pauses
	maybeComplete(false);		// cheap: bails immediately unless the line takes a name
}

void WBQtMapIniEditorDialog::rescanLocalNames()
{
	if (m_editor == NULL)
	{
		return;		// no tab open: nothing to act on
	}
	const QSet<QString> found =
		WBQtMapIniHighlighter::scanLocalNames(m_editor->document());

	// rehighlight() reformats EVERY block, which on a 10,000-line file is the single most
	// expensive thing here. It is only needed when the set of names the file declares actually
	// changed, because that is what can flip a line elsewhere between known and unknown.
	// Ordinary edits (typing inside a value, editing a comment) leave the set alone, and Qt has
	// already re-highlighted the touched block by itself.
	if (found != m_highlighter->localNames())
	{
		m_highlighter->setLocalNames(found);
		m_highlighter->rehighlight();
	}
	if (m_ui->showErrorsBox->isChecked())
	{
		rebuildErrorList();		// keep the filtered list in step with the text behind it
	}
}

void WBQtMapIniEditorDialog::onCursorMoved()
{
	if (m_editor == NULL)
	{
		return;		// no tab open: nothing to act on
	}
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
	if (m_editor == NULL)
	{
		return;		// no tab open: nothing to act on
	}
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
	if (m_editor == NULL)
	{
		return;		// no tab open: nothing to act on
	}
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
	const QString unknown = m_highlighter->unknownNameAt(blockText,
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
	// Generic: the File menu can open any .ini, so naming the window after map.ini would be wrong
	// as soon as you do.
	QString title = tr("INI Editor");
	if (!m_path.isEmpty())
	{
		title += " - " + QFileInfo(m_path).fileName();
		// Show the folder too: every map's file is called "map.ini", so the name alone does not
		// say which map is being edited.
		title += " (" + QFileInfo(m_path).absolutePath() + ")";
	}
	// m_editor is NULL between construction and the first tab, and again after the last tab is
	// closed -- this runs in both windows.
	if (m_editor != NULL && m_editor->document()->isModified())
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

		// While the completion popup is up it owns Enter/Tab/Escape/arrows -- let it consume
		// them, or Enter would insert a newline instead of accepting the highlighted name.
		if (watched == m_editor && m_completer->popup()->isVisible())
		{
			switch (key->key())
			{
				case Qt::Key_Enter:
				case Qt::Key_Return:
				case Qt::Key_Tab:
				case Qt::Key_Backtab:
				case Qt::Key_Escape:
				case Qt::Key_Up:
				case Qt::Key_Down:
				case Qt::Key_PageUp:
				case Qt::Key_PageDown:
					return false;	// the popup is an event filter on the editor; it handles these
				default:
					break;
			}
		}

		// Ctrl+Space opens the searchable picker (the inline popup only prefix-matches, and the
		// catalogs are big enough that finding a name by a middle fragment matters).
		if (watched == m_editor
			&& key->key() == Qt::Key_Space
			&& (key->modifiers() & Qt::ControlModifier) != 0)
		{
			openValuePicker();
			return true;
		}

		// NOT Ctrl+S here: the File menu's Save action owns that shortcut now, and having both
		// claim it makes Qt report an ambiguous shortcut and fire neither.
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
