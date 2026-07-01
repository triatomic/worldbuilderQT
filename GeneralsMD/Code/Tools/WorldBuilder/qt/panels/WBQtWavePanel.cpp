// WBQtWavePanel.cpp -- see WBQtWavePanel.h.
#include "WBQtWavePanel.h"
#include "WBQtWaveBridge.h"
#include "WBQtTreeStyle.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QTreeWidget>
#include <QVBoxLayout>

// Bucket brush-size slider range (world units) -- matches the MFC WAVE_BRUSH_MIN/MAX.
static const int kBrushMin = 30;
static const int kBrushMax = 5000;

WBQtWavePanel *WBQtWavePanel::s_instance = NULL;

WBQtWavePanel::WBQtWavePanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_updating(false)
{
	setWindowTitle("Wave Editor Options");
	resize(380, 560);

	QVBoxLayout *root = new QVBoxLayout(this);

	// Current type + the four command buttons.
	m_typeLabel = new QLabel("Type:", this);
	root->addWidget(m_typeLabel);

	QGridLayout *cmdGrid = new QGridLayout();
	QPushButton *cycleBtn = new QPushButton("Cycle Type", this);
	QPushButton *undoBtn = new QPushButton("Undo", this);
	QPushButton *saveBtn = new QPushButton("Save", this);
	QPushButton *reloadBtn = new QPushButton("Reload", this);
	cmdGrid->addWidget(cycleBtn, 0, 0);
	cmdGrid->addWidget(undoBtn, 0, 1);
	cmdGrid->addWidget(saveBtn, 1, 0);
	cmdGrid->addWidget(reloadBtn, 1, 1);
	root->addLayout(cmdGrid);

	// Editor mode: four push-like toggles (exactly one pressed, kept in sync by hand like the
	// MFC BS_PUSHLIKE radios).
	QGroupBox *modeBox = new QGroupBox("Mode", this);
	QHBoxLayout *modeLay = new QHBoxLayout(modeBox);
	m_modeCreate = new QPushButton("Create", modeBox);
	m_modeManipulate = new QPushButton("Manipulate", modeBox);
	m_modePaint = new QPushButton("Paint", modeBox);
	m_modeBucket = new QPushButton("Bucket", modeBox);
	m_modeCreate->setCheckable(true);
	m_modeManipulate->setCheckable(true);
	m_modePaint->setCheckable(true);
	m_modeBucket->setCheckable(true);
	modeLay->addWidget(m_modeCreate);
	modeLay->addWidget(m_modeManipulate);
	modeLay->addWidget(m_modePaint);
	modeLay->addWidget(m_modeBucket);
	root->addWidget(modeBox);

	// Bucket brush size (only visible in Bucket mode).
	QHBoxLayout *brushRow = new QHBoxLayout();
	m_brushLabel = new QLabel("Bucket brush: 120", this);
	m_brushSlider = new QSlider(Qt::Horizontal, this);
	m_brushSlider->setRange(kBrushMin, kBrushMax);
	brushRow->addWidget(m_brushLabel);
	brushRow->addWidget(m_brushSlider, 1);
	root->addLayout(brushRow);

	// Help text (mirrors the MFC statics).
	QLabel *help = new QLabel(
		"Create: drag on water to place a single wave.\n"
		"Manipulate: drag a wave to move it, Ctrl+drag to rotate it, Shift+click to multi-select.\n"
		"Paint: hold and drag to lay a trail of waves toward shore.\n"
		"Bucket: drag along the coast to auto-fill the shoreline with waves.\n"
		"Right-click to discard the waves you just added.\n"
		"* * Remember to press the Save button to save your waves.", this);
	help->setWordWrap(true);
	root->addWidget(help);

	// The wave list: multi-select, 6 columns like the MFC report view.
	m_list = new QTreeWidget(this);
	m_list->setRootIsDecorated(false);
	m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_list->setContextMenuPolicy(Qt::CustomContextMenu);
	m_list->setMinimumHeight(220);
	m_list->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	QStringList headers;
	headers << "#" << "Start X" << "Start Y" << "End X" << "End Y" << "Type";
	m_list->setHeaderLabels(headers);
	m_list->header()->resizeSection(0, 44);
	m_list->header()->resizeSection(1, 62);
	m_list->header()->resizeSection(2, 62);
	m_list->header()->resizeSection(3, 62);
	m_list->header()->resizeSection(4, 62);
	WBQtTreeStyle::applyTreeLines(m_list);
	root->addWidget(m_list, 1);

	// Delete buttons + overlay checkboxes.
	QGridLayout *bottomGrid = new QGridLayout();
	QPushButton *delBtn = new QPushButton("Delete Selected", this);
	QPushButton *delAllBtn = new QPushButton("Delete All", this);
	m_showLines = new QCheckBox("Show wave lines", this);
	m_showShoreline = new QCheckBox("Show shoreline", this);
	bottomGrid->addWidget(delBtn, 0, 0);
	bottomGrid->addWidget(m_showLines, 0, 1);
	bottomGrid->addWidget(delAllBtn, 1, 0);
	bottomGrid->addWidget(m_showShoreline, 1, 1);
	root->addLayout(bottomGrid);

	// Seed everything under the guard.
	m_updating = true;
	updateTypeLabel();
	populateList();
	syncModeButtons(WBQtWave_GetEditorMode());
	int sz = WBQtWave_GetBrushSize();
	if (sz < kBrushMin) { sz = kBrushMin; }
	if (sz > kBrushMax) { sz = kBrushMax; }
	m_brushSlider->setValue(sz);
	WBQtWave_SetBrushSize(sz);	// clamp the tool to the slider's range, like the MFC panel
	updateBrushLabel();
	m_showLines->setChecked(WBQtWave_GetShowWaveLines() != 0);
	m_showShoreline->setChecked(WBQtWave_GetShowShoreline() != 0);
	m_updating = false;

	connect(cycleBtn, SIGNAL(clicked()), this, SLOT(onCycleType()));
	connect(undoBtn, SIGNAL(clicked()), this, SLOT(onUndo()));
	connect(saveBtn, SIGNAL(clicked()), this, SLOT(onSave()));
	connect(reloadBtn, SIGNAL(clicked()), this, SLOT(onReload()));
	connect(m_modeCreate, SIGNAL(clicked()), this, SLOT(onModeClicked()));
	connect(m_modeManipulate, SIGNAL(clicked()), this, SLOT(onModeClicked()));
	connect(m_modePaint, SIGNAL(clicked()), this, SLOT(onModeClicked()));
	connect(m_modeBucket, SIGNAL(clicked()), this, SLOT(onModeClicked()));
	connect(m_brushSlider, SIGNAL(valueChanged(int)), this, SLOT(onBrushSliderMoved(int)));
	connect(delBtn, SIGNAL(clicked()), this, SLOT(onDeleteSelected()));
	connect(delAllBtn, SIGNAL(clicked()), this, SLOT(onDeleteAll()));
	connect(m_showLines, SIGNAL(clicked()), this, SLOT(onShowWaveLinesToggled()));
	connect(m_showShoreline, SIGNAL(clicked()), this, SLOT(onShowShorelineToggled()));
	connect(m_list, SIGNAL(itemSelectionChanged()), this, SLOT(onListSelectionChanged()));
	connect(m_list, SIGNAL(customContextMenuRequested(const QPoint &)),
		this, SLOT(onListContextMenu(const QPoint &)));

	s_instance = this;
}

void WBQtWavePanel::updateTypeLabel()
{
	const int cap = 256;
	char buf[cap];
	if (WBQtWave_GetTypeName(buf, cap))
	{
		m_typeLabel->setText(QString("Type: %1").arg(QString::fromLatin1(buf)));
	}
}

void WBQtWavePanel::updateBrushLabel()
{
	m_brushLabel->setText(QString("Bucket brush: %1").arg(WBQtWave_GetBrushSize()));
}

void WBQtWavePanel::syncModeButtons(int mode)
{
	m_modeCreate->setChecked(mode == 0);
	m_modeManipulate->setChecked(mode == 1);
	m_modePaint->setChecked(mode == 2);
	m_modeBucket->setChecked(mode == 3);

	// The brush slider only applies to Bucket mode, like the MFC panel.
	bool bucket = (mode == 3);
	m_brushLabel->setVisible(bucket);
	m_brushSlider->setVisible(bucket);
}

void WBQtWavePanel::populateList()
{
	// Caller has set m_updating (programmatic selection must not re-fire the sync).
	m_list->clear();

	const int cap = 128;
	char typeBuf[cap];
	int count = WBQtWave_GetWaveCount();
	for (int i = 0; i < count; ++i)
	{
		float sx = 0.0f, sy = 0.0f, ex = 0.0f, ey = 0.0f;
		if (!WBQtWave_GetWaveRow(i, &sx, &sy, &ex, &ey, typeBuf, cap))
		{
			continue;
		}
		QTreeWidgetItem *item = new QTreeWidgetItem(m_list);
		item->setText(0, QString("%1").arg(i + 1, 3, 10, QChar('0')));
		item->setText(1, QString::number(sx, 'f', 3));
		item->setText(2, QString::number(sy, 'f', 3));
		item->setText(3, QString::number(ex, 'f', 3));
		item->setText(4, QString::number(ey, 'f', 3));
		item->setText(5, QString::fromLatin1(typeBuf));
		item->setSelected(WBQtWave_IsWaveSelected(i) != 0);
	}

	// Focus + reveal the anchor row, matching the tool's selection.
	int anchor = WBQtWave_GetSelectedWave();
	if (anchor >= 0 && anchor < m_list->topLevelItemCount())
	{
		m_list->setCurrentItem(m_list->topLevelItem(anchor), 0, QItemSelectionModel::NoUpdate);
		m_list->scrollToItem(m_list->topLevelItem(anchor));
	}
}

void WBQtWavePanel::pushRefresh()
{
	m_updating = true;
	updateTypeLabel();
	populateList();
	m_updating = false;
}

void WBQtWavePanel::pushBrushSize()
{
	m_updating = true;
	int sz = WBQtWave_GetBrushSize();
	if (sz < kBrushMin) { sz = kBrushMin; }
	if (sz > kBrushMax) { sz = kBrushMax; }
	m_brushSlider->setValue(sz);
	updateBrushLabel();
	m_updating = false;
}

void WBQtWavePanel::hideEvent(QHideEvent *event)
{
	// Mirrors MFC OnShowWindow(FALSE): clear the wave highlight when the panel goes away.
	WBQtWave_ClearSelection();
	WBQtWave_InvalidateView();
	QWidget::hideEvent(event);
}

void WBQtWavePanel::onCycleType()
{
	WBQtWave_CycleType();
	updateTypeLabel();
}

void WBQtWavePanel::onUndo()
{
	WBQtWave_Undo();
	pushRefresh();
}

void WBQtWavePanel::onSave()
{
	WBQtWave_Save();
}

void WBQtWavePanel::onReload()
{
	WBQtWave_Reload();
	pushRefresh();
}

void WBQtWavePanel::onModeClicked()
{
	if (m_updating)
	{
		return;
	}
	int mode = 0;
	QObject *src = sender();
	if (src == m_modeManipulate)  { mode = 1; }
	else if (src == m_modePaint)  { mode = 2; }
	else if (src == m_modeBucket) { mode = 3; }
	WBQtWave_SetEditorMode(mode);
	syncModeButtons(mode);
}

void WBQtWavePanel::onBrushSliderMoved(int v)
{
	if (m_updating)
	{
		return;
	}
	WBQtWave_SetBrushSize(v);
	updateBrushLabel();
}

void WBQtWavePanel::onDeleteSelected()
{
	WBQtWave_DeleteSelected();
	pushRefresh();
}

void WBQtWavePanel::onDeleteAll()
{
	int count = WBQtWave_GetWaveCount();
	if (count <= 0)
	{
		return;
	}
	// Not undoable (delete clears the wave undo stack), so confirm first like the MFC panel.
	QString msg = QString("Delete all %1 wave%2? This cannot be undone.")
		.arg(count).arg(count == 1 ? "" : "s");
	if (QMessageBox::warning(this, "Delete All Waves", msg,
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
	{
		return;
	}
	WBQtWave_DeleteAll();
	pushRefresh();
}

void WBQtWavePanel::onShowWaveLinesToggled()
{
	WBQtWave_SetShowWaveLines(m_showLines->isChecked() ? 1 : 0);
}

void WBQtWavePanel::onShowShorelineToggled()
{
	WBQtWave_SetShowShoreline(m_showShoreline->isChecked() ? 1 : 0);
}

void WBQtWavePanel::syncToolSelectionFromList()
{
	// Mirror the list's selected rows into the tool's selection set. selectedItems() returns a
	// QList of POINTERS (safe with WB's global allocator override -- never use value-type index
	// lists like selectedIndexes() here).
	WBQtWave_BeginListSelection();

	int anchor = -1;
	QList<QTreeWidgetItem*> sel = m_list->selectedItems();
	for (int i = 0; i < sel.size(); ++i)
	{
		int row = m_list->indexOfTopLevelItem(sel.at(i));
		if (row >= 0)
		{
			WBQtWave_AddListSelection(row);
			if (anchor < 0)
			{
				anchor = row;
			}
		}
	}

	// Prefer the focused (current) row as the anchor, like the MFC panel.
	QTreeWidgetItem *cur = m_list->currentItem();
	if (cur != NULL && cur->isSelected())
	{
		int row = m_list->indexOfTopLevelItem(cur);
		if (row >= 0)
		{
			anchor = row;
		}
	}

	WBQtWave_EndListSelection(anchor);
}

void WBQtWavePanel::onListSelectionChanged()
{
	if (m_updating)
	{
		return;
	}
	// Qt emits this once per user action (no per-row LVN_ITEMCHANGED storm), so sync directly.
	syncToolSelectionFromList();
}

void WBQtWavePanel::onListContextMenu(const QPoint &pos)
{
	// Right-click: menu of wave types; retype every selected wave (mirrors OnWaveListRClick).
	QTreeWidgetItem *clicked = m_list->itemAt(pos);
	if (WBQtWave_GetSelectionCount() <= 0 ||
			(clicked != NULL && !clicked->isSelected()))
	{
		if (clicked == NULL)
		{
			return;	// right-clicked empty space with no selection
		}
		m_updating = true;
		m_list->clearSelection();
		clicked->setSelected(true);
		m_list->setCurrentItem(clicked, 0, QItemSelectionModel::NoUpdate);
		m_updating = false;
		syncToolSelectionFromList();
	}

	int typeCount = WBQtWave_GetTypeCount();
	if (typeCount <= 0 || WBQtWave_GetSelectionCount() <= 0)
	{
		return;
	}

	QMenu menu(this);
	const int cap = 128;
	char buf[cap];
	for (int i = 0; i < typeCount; ++i)
	{
		if (WBQtWave_GetTypeNameAt(i, buf, cap))
		{
			QAction *act = menu.addAction(QString::fromLatin1(buf));
			act->setData(i);
		}
	}

	QAction *chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
	if (chosen != NULL)
	{
		WBQtWave_SetSelectedWavesType(chosen->data().toInt());
		pushRefresh();	// reflect the new type names (and keep the selection highlit)
	}
}

// --- Forward pushes (tool -> widget), the Qt side of WBQtWaveBridge.h ------------------------
extern "C" void WBQtWave_PushRefresh(void)
{
	if (WBQtWavePanel::instance() != NULL)
	{
		WBQtWavePanel::instance()->pushRefresh();
	}
}

extern "C" void WBQtWave_PushBrushSize(void)
{
	if (WBQtWavePanel::instance() != NULL)
	{
		WBQtWavePanel::instance()->pushBrushSize();
	}
}
