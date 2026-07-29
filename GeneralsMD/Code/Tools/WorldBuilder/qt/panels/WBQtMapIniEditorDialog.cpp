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
#include <QTextBlock>
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
	const char *const kObjectKeywords[] = {
		"Object",
		"ChildObject",
		"ObjectReskin",
		NULL
	};

	// Keys inside a block whose VALUE is an object name.
	const char *const kObjectValueKeys[] = {
		"BuildVariations",
		"PortraitImageName",		// not a template, but kept out of the check by absence below
		NULL
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

	// Name -> known, so the per-block highlight does not hit the bridge for every repeat.
	QHash<QString, bool> s_knownCache;
	QStringList s_templateCatalog;

	void ensureCatalog()
	{
		if (!s_templateCatalog.isEmpty())
		{
			return;
		}
		char buf[kTextCap];
		const int count = WBQtMapIniEditorData_BuildTemplates();
		for (int i = 0; i < count; ++i)
		{
			buf[0] = 0;
			WBQtMapIniEditorData_GetTemplate(i, buf, sizeof(buf));
			s_templateCatalog.append(QString::fromLocal8Bit(buf));
		}
	}

	// The closest catalog names to `target`, best-first, capped at kMaxSuggestions.
	QStringList suggestionsFor(const QString &target)
	{
		ensureCatalog();
		QList<QPair<float, QString> > scored;
		for (int i = 0; i < s_templateCatalog.size(); ++i)
		{
			const QString &candidate = s_templateCatalog.at(i);
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
	s_templateCatalog.clear();
}

bool WBQtMapIniHighlighter::isKnownTemplate(const QString &name)
{
	if (name.isEmpty())
	{
		return true;	// nothing to flag
	}
	QHash<QString, bool>::const_iterator it = s_knownCache.constFind(name);
	if (it != s_knownCache.constEnd())
	{
		return it.value();
	}
	const QByteArray raw = name.toLocal8Bit();
	const bool known = (WBQtMapIniEditorData_IsTemplate(raw.constData()) != 0);
	s_knownCache.insert(name, known);
	return known;
}

// A declaration line is `Keyword Name` at the start of the line (leading whitespace allowed).
// Anything after the name (a trailing comment) is ignored.
bool WBQtMapIniHighlighter::objectNameOnLine(const QString &line, QString *nameOut,
	int *startOut, int *lengthOut)
{
	// Strip a trailing comment before matching, so "Object Foo ; note" reads as "Object Foo".
	QString scan = line;
	const int commentAt = scan.indexOf(';');
	if (commentAt >= 0)
	{
		scan = scan.left(commentAt);
	}

	for (int k = 0; kObjectKeywords[k] != NULL; ++k)
	{
		const QString keyword = QString::fromLatin1(kObjectKeywords[k]);
		QRegExp re(QString("^\\s*%1\\s+(\\S+)\\s*$").arg(keyword));
		if (re.indexIn(scan) < 0)
		{
			continue;
		}
		const QString name = re.cap(1);
		if (name.isEmpty())
		{
			continue;
		}
		if (nameOut != NULL)
		{
			*nameOut = name;
		}
		if (startOut != NULL)
		{
			*startOut = re.pos(1);
		}
		if (lengthOut != NULL)
		{
			*lengthOut = name.length();
		}
		return true;
	}
	return false;
}

QString WBQtMapIniHighlighter::unknownNameAt(const QString &blockText, int posInBlock,
	int *startOut, int *lengthOut)
{
	QString name;
	int start = 0;
	int length = 0;
	if (!objectNameOnLine(blockText, &name, &start, &length))
	{
		return QString();
	}
	if (posInBlock < start || posInBlock > start + length)
	{
		return QString();	// the cursor is not on the name itself
	}
	if (isKnownTemplate(name))
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
	QRegExp leading("^\\s*([A-Za-z_][A-Za-z0-9_]*)");
	if (leading.indexIn(code) >= 0)
	{
		const QString word = leading.cap(1);
		bool isBlockKeyword = false;
		for (int k = 0; kObjectKeywords[k] != NULL; ++k)
		{
			if (word.compare(QString::fromLatin1(kObjectKeywords[k]), Qt::CaseInsensitive) == 0)
			{
				isBlockKeyword = true;
				break;
			}
		}
		if (word.compare("End", Qt::CaseInsensitive) == 0)
		{
			isBlockKeyword = true;
		}
		QTextCharFormat fmt;
		fmt.setForeground(isBlockKeyword ? keywordColour() : keyColour());
		if (isBlockKeyword)
		{
			fmt.setFontWeight(QFont::Bold);
		}
		setFormat(leading.pos(1), word.length(), fmt);
	}

	if (!m_checkNames)
	{
		return;
	}

	// The object name on a declaration line, underlined when the game data has no such template.
	QString name;
	int start = 0;
	int length = 0;
	if (objectNameOnLine(text, &name, &start, &length) && !isKnownTemplate(name))
	{
		QTextCharFormat fmt;
		fmt.setUnderlineColor(QColor(200, 60, 60));
		fmt.setUnderlineStyle(QTextCharFormat::WaveUnderline);
		fmt.setForeground(QColor(200, 60, 60));
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
	const QString unknown = WBQtMapIniHighlighter::unknownNameAt(blockText,
		atPoint.positionInBlock(), &start, &length);
	if (!unknown.isEmpty())
	{
		const QStringList picks = suggestionsFor(unknown);
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
