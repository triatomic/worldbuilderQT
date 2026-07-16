// WBQtMapIniReport.cpp -- see WBQtMapIniReport.h.
#include "WBQtMapIniReport.h"

#ifdef RTS_HAS_QT

#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "WBQtTreeStyle.h"

// Stage 1 phase 3: the parent for a modal Qt dialog (active modal if nested, else the
// main window). Defined in WBQtBridge.cpp.
QWidget *WBQt_DialogParent(void);

// The dialog class lives in its own header (WBQtMapIniReportPrivate.h) so AUTOMOC generates
// its moc the standard way; the public WBQtMapIniReport.h stays a pure C facade for the MFC
// side. See the include below.
#include "WBQtMapIniReportPrivate.h"

namespace
{
	// A ';' comment line that names a section (not the ==== banner rule, not a detail
	// comment). These become the collapsible top-level nodes. The producer (doLoadMapIni)
	// writes section headers as "; Text" (a single space after the ';') and detail /
	// continuation comments as ";   text" (two or more spaces), so the space run after
	// the ';' is the discriminator.
	bool isSectionHeader(const QString &line)
	{
		QString t = line.trimmed();
		if (!t.startsWith(';'))
		{
			return false;
		}
		QString rest = t.mid(1);		// after the ';', spacing intact
		QString body = rest.trimmed();
		if (body.isEmpty())
		{
			return false;
		}
		if (body.startsWith('='))
		{
			return false;	// banner rule
		}
		if (rest.startsWith(QLatin1String("  ")))
		{
			return false;	// ";   detail" line -> child of the current section
		}
		return true;
	}
}

WBQtMapIniReportDialog::WBQtMapIniReportDialog(const QString &title, const QString &text,
	bool applyMode, QWidget *parent)
	: QDialog(parent),
	  m_filter(NULL),
	  m_tree(NULL),
	  m_rawText(text)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle(title);

	QVBoxLayout *root = new QVBoxLayout(this);

	// Filter + expand/collapse controls.
	QHBoxLayout *topRow = new QHBoxLayout();
	topRow->addWidget(new QLabel("Filter:", this));
	m_filter = new QLineEdit(this);
	m_filter->setPlaceholderText("substring (object / module / store name)...");
	topRow->addWidget(m_filter, 1);
	QPushButton *expandBtn = new QPushButton("Expand all", this);
	expandBtn->setAutoDefault(false);
	QPushButton *collapseBtn = new QPushButton("Collapse all", this);
	collapseBtn->setAutoDefault(false);
	topRow->addWidget(expandBtn);
	topRow->addWidget(collapseBtn);
	root->addLayout(topRow);

	// The report tree: section headers as parents, their lines as monospace children.
	m_tree = new QTreeWidget(this);
	m_tree->setHeaderHidden(true);
	m_tree->setColumnCount(1);
	m_tree->setUniformRowHeights(true);
	m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
	QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	m_tree->setFont(mono);
	WBQtTreeStyle::applyTreeLines(m_tree);	// MFC-style branch lines, like every other WB tree
	root->addWidget(m_tree, 1);

	buildTree(text);

	// Buttons. Apply mode (open/Reload) offers OK (load) / Cancel (don't); informational
	// mode (Check) just closes. Copy is always available.
	QHBoxLayout *buttons = new QHBoxLayout();
	QPushButton *copyBtn = new QPushButton("Copy report", this);
	copyBtn->setAutoDefault(false);
	buttons->addWidget(copyBtn);
	buttons->addStretch(1);
	if (applyMode)
	{
		QPushButton *okButton = new QPushButton("OK (load map.ini)", this);
		okButton->setDefault(true);
		buttons->addWidget(okButton);
		QPushButton *cancelButton = new QPushButton("Cancel", this);
		cancelButton->setAutoDefault(false);
		buttons->addWidget(cancelButton);
		connect(okButton, SIGNAL(clicked()), this, SLOT(accept()));
		connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));
	}
	else
	{
		QPushButton *closeBtn = new QPushButton("Close", this);
		closeBtn->setDefault(true);
		buttons->addWidget(closeBtn);
		connect(closeBtn, SIGNAL(clicked()), this, SLOT(accept()));
	}
	root->addLayout(buttons);

	connect(m_filter, SIGNAL(textChanged(QString)), this, SLOT(onFilterChanged(QString)));
	connect(expandBtn, SIGNAL(clicked()), this, SLOT(onExpandAll()));
	connect(collapseBtn, SIGNAL(clicked()), this, SLOT(onCollapseAll()));
	connect(copyBtn, SIGNAL(clicked()), this, SLOT(onCopy()));

	resize(720, 560);
}

void WBQtMapIniReportDialog::buildTree(const QString &text)
{
	m_tree->clear();
	QStringList lines = text.split('\n');

	QTreeWidgetItem *intro = NULL;		// leading banner lines before the first section header
	QTreeWidgetItem *section = NULL;	// current ';'-header section
	QTreeWidgetItem *object = NULL;		// current "Object <name>" block (verbose), if any

	for (int i = 0; i < lines.size(); ++i)
	{
		QString line = lines.at(i);
		line.replace('\r', QString());
		QString trimmed = line.trimmed();
		if (trimmed.isEmpty())
		{
			continue;
		}

		if (isSectionHeader(line))
		{
			// Start a new collapsible section; its title is the comment text.
			QString titleText = trimmed.mid(1).trimmed();	// drop leading ';'
			section = new QTreeWidgetItem(m_tree, QStringList(titleText));
			section->setExpanded(true);
			QFont f = section->font(0);
			f.setBold(true);
			section->setFont(0, f);
			object = NULL;
			continue;
		}

		// Verbose per-object block: "Object <name>   ; overridden|new" opens an object node
		// under the section; its indented module lines become that object's own collapsible
		// children, so each object (and its individual module changes) collapse separately.
		if (trimmed.startsWith("Object ") && section != NULL)
		{
			object = new QTreeWidgetItem(section, QStringList(trimmed));
			object->setExpanded(false);	// collapsed by default: one line per object until opened
			continue;
		}
		if (trimmed == "End")
		{
			object = NULL;	// close the current object block
			continue;
		}

		if (object != NULL)
		{
			// A module change line inside the current object -> its own child row.
			new QTreeWidgetItem(object, QStringList(trimmed));
		}
		else if (section != NULL)
		{
			new QTreeWidgetItem(section, QStringList(trimmed));
		}
		else
		{
			// Pre-section banner / summary lines: gather under a "Summary" node.
			if (intro == NULL)
			{
				intro = new QTreeWidgetItem(m_tree, QStringList("Summary"));
				intro->setExpanded(true);
				QFont f = intro->font(0);
				f.setBold(true);
				intro->setFont(0, f);
			}
			new QTreeWidgetItem(intro, QStringList(line));
		}
	}

	m_tree->resizeColumnToContents(0);
}

// Recursive filter: an item is shown if it (or an ancestor already matched) matches, or
// any descendant matches. Returns true if this item stays visible. forcedByAncestor keeps
// a whole subtree visible once a parent matched. Auto-expands matched branches.
static bool filterItem(QTreeWidgetItem *item, const QString &needle, bool forcedByAncestor)
{
	bool selfMatch = needle.isEmpty() || forcedByAncestor ||
		item->text(0).contains(needle, Qt::CaseInsensitive);

	bool anyChildShown = false;
	for (int c = 0; c < item->childCount(); ++c)
	{
		if (filterItem(item->child(c), needle, selfMatch))
		{
			anyChildShown = true;
		}
	}

	bool visible = selfMatch || anyChildShown;
	item->setHidden(!visible);
	if (!needle.isEmpty() && anyChildShown)
	{
		item->setExpanded(true);
	}
	return visible;
}

void WBQtMapIniReportDialog::onFilterChanged(const QString &text)
{
	QString needle = text.trimmed();
	for (int s = 0; s < m_tree->topLevelItemCount(); ++s)
	{
		filterItem(m_tree->topLevelItem(s), needle, false);
	}
}

void WBQtMapIniReportDialog::onExpandAll()
{
	m_tree->expandAll();
}

void WBQtMapIniReportDialog::onCollapseAll()
{
	m_tree->collapseAll();
}

void WBQtMapIniReportDialog::onCopy()
{
	QApplication::clipboard()->setText(m_rawText);
}

// ===================== the modal entry point =====================

extern "C" int WBQtMapIniReport_Show(const char *title, const char *text, int applyMode)
{
	if (qApp == NULL)
	{
		return 0;	// Qt not up yet -- the caller falls back to the MFC dialog
	}
	WBQtMapIniReportDialog dlg(
		QString::fromLocal8Bit(title ? title : "Map.ini"),
		QString::fromLocal8Bit(text ? text : ""),
		applyMode != 0,
		WBQt_DialogParent());
	dlg.setWindowModality(Qt::ApplicationModal);
	int rc = dlg.exec();
	return (rc == QDialog::Accepted) ? 2 : 1;	// 2 = OK/accepted, 1 = Cancel/closed
}

#else	// !RTS_HAS_QT

extern "C" int WBQtMapIniReport_Show(const char * /*title*/, const char * /*text*/, int /*applyMode*/)
{
	return 0;	// no Qt -- caller uses the MFC dialog
}

#endif // RTS_HAS_QT
