// WBQtMapGenDialog.cpp -- the Map Generator settings modal.

#include "WBQtMapGenDialog.h"
#include "ui_WBQtMapGenDialog.h"
#include "WBQtMapGenBridge.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSpinBox>
#include <QTime>

// Defined in WBQtBridge.cpp -- the main window, or the active modal when nested.
QWidget *WBQt_DialogParent(void);

namespace
{
	// Same discipline as the other workflow modals: parent to the main window and
	// go application modal, which fences the hosted viewport too.
	int runMapGenModal(QDialog &dlg)
	{
		dlg.setParent(WBQt_DialogParent(), dlg.windowFlags());
		dlg.setWindowModality(Qt::ApplicationModal);
		int rc = dlg.exec();
		return (rc == QDialog::Accepted) ? 1 : 0;
	}

	// The players combo lists 2/4/6/8, so the index and the count are not the same.
	int playerCountFromIndex(int index)
	{
		return 2 + (index * 2);
	}

	int indexFromPlayerCount(int count)
	{
		int index = (count - 2) / 2;
		if (index < 0)
		{
			index = 0;
		}
		if (index > 3)
		{
			index = 3;
		}
		return index;
	}
}

//=============================================================================
WBQtMapGenDialog::WBQtMapGenDialog(int seed, int numPlayers, int baseHeight,
																	 int doCliffs, int cliffDensity, int doTextures,
																	 int doTrees, int treeDensity, int doRocks,
																	 QWidget *parent)
	: QDialog(parent),
		m_ui(new Ui::WBQtMapGenDialog)
{
	m_ui->setupUi(this);

	m_seed = m_ui->seed;
	m_players = m_ui->players;
	m_baseHeight = m_ui->baseHeight;
	m_doCliffs = m_ui->doCliffs;
	m_cliffDensity = m_ui->cliffDensity;
	m_doTextures = m_ui->doTextures;
	m_doTrees = m_ui->doTrees;
	m_treeDensity = m_ui->treeDensity;
	m_doRocks = m_ui->doRocks;
	m_sizeNote = m_ui->noteLabel;

	m_seed->setValue(seed);
	m_players->setCurrentIndex(indexFromPlayerCount(numPlayers));
	m_baseHeight->setValue(baseHeight);
	m_doCliffs->setChecked(doCliffs != 0);
	m_cliffDensity->setCurrentIndex(cliffDensity);
	m_cliffDensity->setEnabled(doCliffs != 0);
	m_doTextures->setChecked(doTextures != 0);
	m_doTrees->setChecked(doTrees != 0);
	m_treeDensity->setCurrentIndex(treeDensity);
	m_treeDensity->setEnabled(doTrees != 0);
	m_doRocks->setChecked(doRocks != 0);

	refreshSizeNote();

	connect(m_ui->randomize, SIGNAL(clicked()), this, SLOT(onRandomize()));
	connect(m_players, SIGNAL(currentIndexChanged(int)), this, SLOT(onPlayersChanged(int)));
	connect(m_doCliffs, SIGNAL(toggled(bool)), this, SLOT(onCliffsToggled(bool)));
	connect(m_doTrees, SIGNAL(toggled(bool)), this, SLOT(onTreesToggled(bool)));
	connect(m_ui->ok, SIGNAL(clicked()), this, SLOT(accept()));
	connect(m_ui->cancel, SIGNAL(clicked()), this, SLOT(reject()));
}

//=============================================================================
WBQtMapGenDialog::~WBQtMapGenDialog()
{
	delete m_ui;
}

//=============================================================================
int WBQtMapGenDialog::seed(void) const
{
	return m_seed->value();
}

//=============================================================================
int WBQtMapGenDialog::numPlayers(void) const
{
	return playerCountFromIndex(m_players->currentIndex());
}

//=============================================================================
int WBQtMapGenDialog::baseHeight(void) const
{
	return m_baseHeight->value();
}

//=============================================================================
int WBQtMapGenDialog::doCliffs(void) const
{
	return m_doCliffs->isChecked() ? 1 : 0;
}

//=============================================================================
int WBQtMapGenDialog::cliffDensity(void) const
{
	return m_cliffDensity->currentIndex();
}

//=============================================================================
// WBQtMapGenDialog::onRandomize
//=============================================================================
/** Picks a fresh seed. Derived from the clock rather than a random generator so
	it doesn't depend on any global seeding the rest of the editor may have done. */
//=============================================================================
void WBQtMapGenDialog::onRandomize()
{
	const int msecs = QTime::currentTime().msecsSinceStartOfDay();
	m_seed->setValue(msecs % (m_seed->maximum() + 1));
}

//=============================================================================
void WBQtMapGenDialog::onCliffsToggled(bool checked)
{
	m_cliffDensity->setEnabled(checked);
}

//=============================================================================
int WBQtMapGenDialog::doTextures(void) const
{
	return m_doTextures->isChecked() ? 1 : 0;
}

//=============================================================================
int WBQtMapGenDialog::doTrees(void) const
{
	return m_doTrees->isChecked() ? 1 : 0;
}

//=============================================================================
int WBQtMapGenDialog::treeDensity(void) const
{
	return m_treeDensity->currentIndex();
}

//=============================================================================
int WBQtMapGenDialog::doRocks(void) const
{
	return m_doRocks->isChecked() ? 1 : 0;
}

//=============================================================================
void WBQtMapGenDialog::onTreesToggled(bool checked)
{
	m_treeDensity->setEnabled(checked);
}

//=============================================================================
void WBQtMapGenDialog::onPlayersChanged(int /*index*/)
{
	refreshSizeNote();
}

//=============================================================================
// WBQtMapGenDialog::refreshSizeNote
//=============================================================================
/** Says up front whether generating is going to enlarge the map.

	More players means the start positions sit further apart, which needs a bigger
	map; rather than refuse, the generator grows the map to fit. That is a big
	enough change to the document to warn about before it happens rather than
	after.
*/
//=============================================================================
void WBQtMapGenDialog::refreshSizeNote()
{
	if (m_sizeNote == NULL)
	{
		return;
	}

	const int minSize = WBQtMapGen_GetMinimumSize(numPlayers());
	int curWidth = 0;
	int curHeight = 0;
	WBQtMapGen_GetCurrentSize(&curWidth, &curHeight);

	QString text = tr("Generates into the current map. One Undo reverses it.");
	if (curWidth > 0 && curHeight > 0)
	{
		if (curWidth < minSize || curHeight < minSize)
		{
			text = tr("The map is %1 x %2. For %3 players it will be enlarged to at "
								"least %4 x %4. One Undo reverses it.")
							 .arg(curWidth).arg(curHeight).arg(numPlayers()).arg(minSize);
		}
		else
		{
			text = tr("Generates into the current map (%1 x %2). One Undo reverses it.")
							 .arg(curWidth).arg(curHeight);
		}
	}
	m_sizeNote->setText(text);
}

//=============================================================================
// WBQtMapGen_Run
//=============================================================================
extern "C" int WBQtMapGen_Run(void * /*frameHwnd*/, int *seed, int *numPlayers,
	int *baseHeight, int *doCliffs, int *cliffDensity, int *doTextures, int *doTrees,
	int *treeDensity, int *doRocks)
{
	if (seed == NULL || numPlayers == NULL || baseHeight == NULL ||
			doCliffs == NULL || cliffDensity == NULL || doTextures == NULL ||
			doTrees == NULL || treeDensity == NULL || doRocks == NULL)
	{
		return 0;
	}

	WBQtMapGenDialog dlg(*seed, *numPlayers, *baseHeight, *doCliffs, *cliffDensity,
											 *doTextures, *doTrees, *treeDensity, *doRocks);
	if (runMapGenModal(dlg) == 0)
	{
		return 0;
	}

	*seed = dlg.seed();
	*numPlayers = dlg.numPlayers();
	*baseHeight = dlg.baseHeight();
	*doCliffs = dlg.doCliffs();
	*cliffDensity = dlg.cliffDensity();
	*doTextures = dlg.doTextures();
	*doTrees = dlg.doTrees();
	*treeDensity = dlg.treeDensity();
	*doRocks = dlg.doRocks();
	return 1;
}
