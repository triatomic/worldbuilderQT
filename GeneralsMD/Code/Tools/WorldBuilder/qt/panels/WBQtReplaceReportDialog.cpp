// WBQtReplaceReportDialog.cpp -- see WBQtReplaceReportDialog.h.
#include "WBQtReplaceReportDialog.h"
#include "ui_WBQtReplaceReportDialog.h"
#include "WBQtPickUnitBridge.h"
#include "WBQtTreeStyle.h"

#include <QApplication>		// qApp, in the _Run entry point
#include <QHeaderView>
#include <QPushButton>
#include <QShortcut>
#include <QTreeWidget>
#include <QTreeWidgetItem>

QWidget *WBQt_DialogParent(void);

namespace
{
	// Shown in the "Replaced with" column when a missing name had no close enough match and was
	// left alone. Bracketed so it cannot be mistaken for a template name.
	const char *const kUnresolvedText = "(not replaced)";

	enum { kColMissing = 0, kColReplacement = 1, kColObjects = 2 };
}

WBQtReplaceReportDialog::WBQtReplaceReportDialog(QWidget *parent)
	: QDialog(parent),
	  m_ui(new Ui::WBQtReplaceReportDialog)
{
	m_ui->setupUi(this);
	WBQtTreeStyle::applyTreeLines(m_ui->rowTree);
	// The same report serves both sources; only the wording differs. Script rows name object
	// types used in condition/action parameters, so there is nothing to select in the viewport.
	if (WBQtReplaceReport_GetSource() == WBQT_REPLACE_SOURCE_SCRIPTS)
	{
		setWindowTitle(tr("Replaced Missing Script Entries"));
		m_ui->introLabel->setText(tr("Each missing object type in the scripts was replaced with "
			"its closest name match. Change a replacement if the guess is wrong."));
		m_ui->rowTree->headerItem()->setText(kColMissing, tr("Missing object type"));
		m_ui->rowTree->headerItem()->setText(kColObjects, tr("Uses"));
	}
	else if (WBQtReplaceReport_GetSource() == WBQT_REPLACE_SOURCE_BUILDLIST)
	{
		setWindowTitle(tr("Replaced Missing Buildings"));
		m_ui->introLabel->setText(tr("Each missing building in the build lists was replaced with "
			"its closest name match, keeping its position and settings. Change a replacement if "
			"the guess is wrong."));
		m_ui->rowTree->headerItem()->setText(kColMissing, tr("Missing building"));
		m_ui->rowTree->headerItem()->setText(kColObjects, tr("Entries"));
	}
	else if (WBQtReplaceReport_GetSource() == WBQT_REPLACE_SOURCE_TEAMS)
	{
		setWindowTitle(tr("Replaced Missing Team Units"));
		m_ui->introLabel->setText(tr("Each missing unit type in the team templates was replaced "
			"with its closest name match. Change a replacement if the guess is wrong."));
		m_ui->rowTree->headerItem()->setText(kColMissing, tr("Missing unit type"));
		m_ui->rowTree->headerItem()->setText(kColObjects, tr("Slots"));
	}
	m_ui->rowTree->header()->setStretchLastSection(false);
	m_ui->rowTree->header()->setSectionResizeMode(kColMissing, QHeaderView::Stretch);
	m_ui->rowTree->header()->setSectionResizeMode(kColReplacement, QHeaderView::Stretch);
	m_ui->rowTree->header()->setSectionResizeMode(kColObjects, QHeaderView::ResizeToContents);

	connect(m_ui->rowTree, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
			this, SLOT(onCurrentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));
	// Double-clicking a row is the same as "Change Replacement" -- it is the obvious gesture for
	// "this one is wrong".
	connect(m_ui->rowTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
			this, SLOT(onChangeReplacement()));
	connect(m_ui->changeButton, SIGNAL(clicked()), this, SLOT(onChangeReplacement()));
	connect(m_ui->closeButton, SIGNAL(clicked()), this, SLOT(accept()));

	// Row stepper == the Replace Missing Unit dialog's "Find Next" / "^": walk the report in
	// order so a long list can be reviewed without reaching for the mouse. Selecting a row
	// already selects its objects on the map, so F3 doubles as "show me the next one".
	connect(m_ui->findNextButton, SIGNAL(clicked()), this, SLOT(onFindNextRow()));
	connect(m_ui->findPrevButton, SIGNAL(clicked()), this, SLOT(onFindPrevRow()));
	// F3 / Shift+F3 == the standard find-next convention, matching the pick/replace dialog.
	// This dialog is modal, so the main window's F3 (Border Tool) accelerator can't fire.
	QShortcut *nextSc = new QShortcut(QKeySequence(Qt::Key_F3), this);
	connect(nextSc, SIGNAL(activated()), this, SLOT(onFindNextRow()));
	QShortcut *prevSc = new QShortcut(QKeySequence(Qt::SHIFT + Qt::Key_F3), this);
	connect(prevSc, SIGNAL(activated()), this, SLOT(onFindPrevRow()));

	reload();
}

WBQtReplaceReportDialog::~WBQtReplaceReportDialog()
{
	delete m_ui;
}

void WBQtReplaceReportDialog::reload()
{
	const int wasRow = currentRow();
	m_ui->rowTree->clear();
	const int count = WBQtReplaceReport_GetCount();
	for (int i = 0; i < count; i++)
	{
		char missing[256];
		char replacement[256];
		int objectCount = 0;
		WBQtReplaceReport_GetRow(i, missing, sizeof(missing),
			replacement, sizeof(replacement), &objectCount);

		QTreeWidgetItem *item = new QTreeWidgetItem(m_ui->rowTree);
		item->setText(kColMissing, QString::fromLocal8Bit(missing));
		const bool resolved = (replacement[0] != 0);
		item->setText(kColReplacement, resolved
			? QString::fromLocal8Bit(replacement) : QString(kUnresolvedText));
		if (!resolved)
		{
			// Grey the placeholder so an unreplaced row reads as "needs attention" at a glance.
			item->setForeground(kColReplacement, palette().brush(QPalette::Disabled, QPalette::Text));
		}
		item->setText(kColObjects, QString::number(objectCount));
		item->setTextAlignment(kColObjects, Qt::AlignRight | Qt::AlignVCenter);
	}
	if (wasRow >= 0 && wasRow < m_ui->rowTree->topLevelItemCount())
	{
		m_ui->rowTree->setCurrentItem(m_ui->rowTree->topLevelItem(wasRow));
	}
	else if (m_ui->rowTree->topLevelItemCount() > 0)
	{
		m_ui->rowTree->setCurrentItem(m_ui->rowTree->topLevelItem(0));
	}
	// Nothing to step through with a single row (or none) -- == the pick dialog's arming rule.
	const bool canStep = (m_ui->rowTree->topLevelItemCount() > 1);
	m_ui->findNextButton->setEnabled(canStep);
	m_ui->findPrevButton->setEnabled(canStep);
	refreshSummary();
}

void WBQtReplaceReportDialog::refreshSummary()
{
	const int count = WBQtReplaceReport_GetCount();
	int unresolved = 0;
	for (int i = 0; i < count; i++)
	{
		char missing[256];
		char replacement[256];
		int objectCount = 0;
		WBQtReplaceReport_GetRow(i, missing, sizeof(missing),
			replacement, sizeof(replacement), &objectCount);
		if (replacement[0] == 0)
		{
			++unresolved;
		}
	}
	QString text;
	if (WBQtReplaceReport_GetSource() == WBQT_REPLACE_SOURCE_SCRIPTS)
	{
		text = tr("%1 missing object type(s)").arg(count);
	}
	else if (WBQtReplaceReport_GetSource() == WBQT_REPLACE_SOURCE_TEAMS)
	{
		text = tr("%1 missing unit type(s)").arg(count);
	}
	else if (WBQtReplaceReport_GetSource() == WBQT_REPLACE_SOURCE_BUILDLIST)
	{
		text = tr("%1 missing building(s)").arg(count);
	}
	else
	{
		text = tr("%1 missing unit(s)").arg(count);
	}
	if (unresolved > 0)
	{
		text += tr(" -- %1 with no close match").arg(unresolved);
	}
	m_ui->summaryLabel->setText(text);
}

int WBQtReplaceReportDialog::currentRow() const
{
	QTreeWidgetItem *item = m_ui->rowTree->currentItem();
	if (item == NULL)
	{
		return -1;
	}
	return m_ui->rowTree->indexOfTopLevelItem(item);
}

void WBQtReplaceReportDialog::onCurrentItemChanged(QTreeWidgetItem *current,
	QTreeWidgetItem *previous)
{
	Q_UNUSED(previous);
	if (current == NULL)
	{
		return;
	}
	// Select this row's objects on the map and centre on the first, so the replacement can be
	// judged in place rather than by name alone.
	WBQtReplaceReport_SelectRow(m_ui->rowTree->indexOfTopLevelItem(current));
}

// Move the selection by `dir` (+1 next, -1 previous) with wrap-around. Setting the current item
// fires onCurrentItemChanged, which selects that row's objects on the map -- so stepping the
// report also walks the viewport through them.
void WBQtReplaceReportDialog::stepRow(int dir)
{
	const int count = m_ui->rowTree->topLevelItemCount();
	if (count <= 0)
	{
		return;
	}
	const int cur = currentRow();
	// No selection yet: enter at the top going forward, at the bottom going back.
	int next = (cur < 0) ? ((dir > 0) ? 0 : count - 1) : ((cur + dir + count) % count);
	m_ui->rowTree->setCurrentItem(m_ui->rowTree->topLevelItem(next));
	m_ui->rowTree->scrollToItem(m_ui->rowTree->topLevelItem(next));
}

void WBQtReplaceReportDialog::onFindNextRow()
{
	stepRow(1);
}

void WBQtReplaceReportDialog::onFindPrevRow()
{
	stepRow(-1);
}

void WBQtReplaceReportDialog::onChangeReplacement()
{
	const int row = currentRow();
	if (row < 0)
	{
		return;
	}
	char missing[256];
	char replacement[256];
	int objectCount = 0;
	WBQtReplaceReport_GetRow(row, missing, sizeof(missing),
		replacement, sizeof(replacement), &objectCount);

	// Reuse the normal replace dialog, seeded with the ORIGINAL missing name so its suggestion
	// and Find Next cycle the same candidates the batch pass ranked. NULL allowable == "every
	// editor sorting", which keeps the ES_* enum on the MFC side of the seam.
	char picked[256];
	picked[0] = 0;
	const int rc = WBQtReplaceUnit_Run(NULL, missing, NULL, 0, 0, picked, sizeof(picked));
	if (rc == WBQT_REPLACE_OK && picked[0] != 0)
	{
		WBQtReplaceReport_SetReplacement(row, picked);
	}
	else if (rc == WBQT_REPLACE_IGNORE)
	{
		// "Continue without replacing" here means "put the missing name back".
		WBQtReplaceReport_SetReplacement(row, "");
	}
	reload();
}

extern "C" void WBQtReplaceReport_Run(void *frameHwnd)
{
	Q_UNUSED(frameHwnd);
	if (qApp == NULL || WBQtReplaceReport_HasRows() == 0)
	{
		return;
	}
	WBQtReplaceReportDialog dlg(WBQt_DialogParent());
	dlg.setWindowModality(Qt::ApplicationModal);
	dlg.exec();
}
