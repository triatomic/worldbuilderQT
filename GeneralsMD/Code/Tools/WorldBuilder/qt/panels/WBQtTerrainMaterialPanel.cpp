// WBQtTerrainMaterialPanel.cpp -- see WBQtTerrainMaterialPanel.h.
#include "WBQtTerrainMaterialPanel.h"
#include "WBQtTerrainMaterialBridge.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QTreeWidget>
#include <QVBoxLayout>

WBQtTerrainMaterialPanel *WBQtTerrainMaterialPanel::s_instance = NULL;

// A tree leaf stores its texture-class index in this role (>=0 for a real texture, -1 for a
// grouping node).
static const int kTexClassRole = Qt::UserRole + 1;

// The MFC combo's Scatter-B mode is the one that uses a paint density (mode 3, 1-based).
static const int kScatterBMode = 3;

WBQtTerrainMaterialPanel::WBQtTerrainMaterialPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_updating(false)
{
	setWindowTitle("Terrain Material Options");
	resize(340, 820);

	QVBoxLayout *root = new QVBoxLayout(this);

	// --- Search + texture tree -----------------------------------------------------------
	QHBoxLayout *searchRow = new QHBoxLayout();
	m_search = new QLineEdit(this);
	m_search->setPlaceholderText("Search textures...");
	m_searchBtn = new QPushButton("Search", this);
	m_resetBtn = new QPushButton("Reset", this);
	searchRow->addWidget(m_search, 1);
	searchRow->addWidget(m_searchBtn);
	searchRow->addWidget(m_resetBtn);
	root->addLayout(searchRow);

	m_tree = new QTreeWidget(this);
	m_tree->setHeaderHidden(true);
	m_tree->setColumnCount(1);
	// The texture tree is the primary control (like the MFC dialog) -- give it a large minimum
	// so the many groups below can't squeeze it down to a few rows, and let it take the slack.
	m_tree->setMinimumHeight(240);
	m_tree->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	root->addWidget(m_tree, 3);

	m_nameLabel = new QLabel("No Selection", this);
	root->addWidget(m_nameLabel);

	// --- Swatches (foreground / background) + swap ---------------------------------------
	QGroupBox *swatchBox = new QGroupBox("Textures", this);
	QHBoxLayout *swatchLay = new QHBoxLayout(swatchBox);
	QVBoxLayout *fgCol = new QVBoxLayout();
	fgCol->addWidget(new QLabel("Foreground", swatchBox), 0, Qt::AlignHCenter);
	m_fgSwatch = new QLabel(swatchBox);
	m_fgSwatch->setFixedSize(64, 64);
	m_fgSwatch->setFrameShape(QFrame::Box);
	fgCol->addWidget(m_fgSwatch, 0, Qt::AlignHCenter);
	QVBoxLayout *bgCol = new QVBoxLayout();
	bgCol->addWidget(new QLabel("Background", swatchBox), 0, Qt::AlignHCenter);
	m_bgSwatch = new QLabel(swatchBox);
	m_bgSwatch->setFixedSize(64, 64);
	m_bgSwatch->setFrameShape(QFrame::Box);
	bgCol->addWidget(m_bgSwatch, 0, Qt::AlignHCenter);
	m_swapBtn = new QPushButton("Swap", swatchBox);
	swatchLay->addLayout(fgCol);
	swatchLay->addWidget(m_swapBtn, 0, Qt::AlignVCenter);
	swatchLay->addLayout(bgCol);
	root->addWidget(swatchBox);

	// --- Favorites -----------------------------------------------------------------------
	QGroupBox *favBox = new QGroupBox("Favorites", this);
	QVBoxLayout *favLay = new QVBoxLayout(favBox);
	m_favTree = new QTreeWidget(favBox);
	m_favTree->setHeaderHidden(true);
	m_favTree->setColumnCount(1);
	m_favTree->setMaximumHeight(120);
	favLay->addWidget(m_favTree);
	QHBoxLayout *favBtnRow = new QHBoxLayout();
	m_setFavBtn = new QPushButton("Set", favBox);
	m_delFavBtn = new QPushButton("Delete", favBox);
	m_importFavBtn = new QPushButton("Import", favBox);
	favBtnRow->addWidget(m_setFavBtn);
	favBtnRow->addWidget(m_delFavBtn);
	favBtnRow->addWidget(m_importFavBtn);
	favLay->addLayout(favBtnRow);
	root->addWidget(favBox);

	// --- Brush size + z-height -----------------------------------------------------------
	QGroupBox *sizeBox = new QGroupBox("Brush Size", this);
	QVBoxLayout *sizeLay = new QVBoxLayout(sizeBox);
	QHBoxLayout *wRow = new QHBoxLayout();
	wRow->addWidget(new QLabel("Size in cells:", sizeBox));
	m_widthSlider = new QSlider(Qt::Horizontal, sizeBox);
	m_widthSlider->setRange(WBQtTerrainMaterial_GetMinTileSize(), WBQtTerrainMaterial_GetMaxTileSize());
	m_widthSpin = new QSpinBox(sizeBox);
	m_widthSpin->setRange(WBQtTerrainMaterial_GetMinTileSize(), WBQtTerrainMaterial_GetMaxTileSize());
	wRow->addWidget(m_widthSlider, 1);
	wRow->addWidget(m_widthSpin);
	sizeLay->addLayout(wRow);
	m_widthLabel = new QLabel("0.0 FEET.", sizeBox);
	sizeLay->addWidget(m_widthLabel);

	QHBoxLayout *zRow = new QHBoxLayout();
	zRow->addWidget(new QLabel("Z height:", sizeBox));
	m_heightSlider = new QSlider(Qt::Horizontal, sizeBox);
	m_heightSlider->setRange(WBQtTerrainMaterial_GetMinZHeight(), WBQtTerrainMaterial_GetMaxZHeight());
	m_heightSpin = new QSpinBox(sizeBox);
	m_heightSpin->setRange(WBQtTerrainMaterial_GetMinZHeight(), WBQtTerrainMaterial_GetMaxZHeight());
	zRow->addWidget(m_heightSlider, 1);
	zRow->addWidget(m_heightSpin);
	sizeLay->addLayout(zRow);
	root->addWidget(sizeBox);

	// --- Pathing (passable / impassable) -------------------------------------------------
	QGroupBox *pathBox = new QGroupBox("Pathing", this);
	QVBoxLayout *pathLay = new QVBoxLayout(pathBox);
	m_paintPathing = new QCheckBox("Paint passable / impassable", pathBox);
	pathLay->addWidget(m_paintPathing);
	QHBoxLayout *passRow = new QHBoxLayout();
	m_passable = new QRadioButton("Passable", pathBox);
	m_impassable = new QRadioButton("Impassable", pathBox);
	m_impassable->setChecked(true);
	passRow->addWidget(m_passable);
	passRow->addWidget(m_impassable);
	pathLay->addLayout(passRow);
	root->addWidget(pathBox);

	// --- Pattern paint mode --------------------------------------------------------------
	QGroupBox *patBox = new QGroupBox("Pattern Paint", this);
	QVBoxLayout *patLay = new QVBoxLayout(patBox);
	m_patternPaint = new QCheckBox("Pattern paint", patBox);
	patLay->addWidget(m_patternPaint);
	m_paintMode = new QComboBox(patBox);
	int modeCount = WBQtTerrainMaterial_GetPaintModeCount();
	for (int i = 0; i < modeCount; ++i)
	{
		char buf[128];
		if (WBQtTerrainMaterial_GetPaintModeName(i, buf, sizeof(buf)))
		{
			m_paintMode->addItem(QString::fromLatin1(buf));
		}
	}
	patLay->addWidget(m_paintMode);
	QHBoxLayout *densRow = new QHBoxLayout();
	m_densityLabel = new QLabel("Density:", patBox);
	densRow->addWidget(m_densityLabel);
	m_density = new QSlider(Qt::Horizontal, patBox);
	m_density->setRange(0, 100);
	densRow->addWidget(m_density, 1);
	patLay->addLayout(densRow);
	root->addWidget(patBox);

	// --- No mixing -----------------------------------------------------------------------
	m_noMixing = new QCheckBox("No mixing (don't auto-blend different classes)", this);
	root->addWidget(m_noMixing);

	// --- Copy mode -----------------------------------------------------------------------
	QGroupBox *copyBox = new QGroupBox("Copy Mode", this);
	QVBoxLayout *copyLay = new QVBoxLayout(copyBox);
	m_copyTexture = new QCheckBox("Copy texture", copyBox);
	m_copyTerrain = new QCheckBox("Copy terrain", copyBox);
	m_raiseOnly = new QCheckBox("Raise only", copyBox);
	copyLay->addWidget(m_copyTexture);
	copyLay->addWidget(m_copyTerrain);
	copyLay->addWidget(m_raiseOnly);
	QHBoxLayout *saRow = new QHBoxLayout();
	m_copySelect = new QRadioButton("Select", copyBox);
	m_copyApply = new QRadioButton("Apply", copyBox);
	saRow->addWidget(m_copySelect);
	saRow->addWidget(m_copyApply);
	copyLay->addLayout(saRow);
	QHBoxLayout *rotRow = new QHBoxLayout();
	m_rot0 = new QRadioButton("0", copyBox);
	m_rot90 = new QRadioButton("90", copyBox);
	m_rot180 = new QRadioButton("180", copyBox);
	m_rot270 = new QRadioButton("270", copyBox);
	m_rot0->setChecked(true);
	rotRow->addWidget(new QLabel("Rotate:", copyBox));
	rotRow->addWidget(m_rot0);
	rotRow->addWidget(m_rot90);
	rotRow->addWidget(m_rot180);
	rotRow->addWidget(m_rot270);
	copyLay->addLayout(rotRow);
	root->addWidget(copyBox);

	// --- Mirror --------------------------------------------------------------------------
	QGroupBox *mirrorBox = new QGroupBox("Mirror", this);
	QVBoxLayout *mirrorLay = new QVBoxLayout(mirrorBox);
	m_mirror = new QCheckBox("Toggle", mirrorBox);
	m_mirrorX = new QCheckBox("Mirror X", mirrorBox);
	m_mirrorY = new QCheckBox("Mirror Y", mirrorBox);
	m_mirrorXY = new QCheckBox("Diagonal", mirrorBox);
	mirrorLay->addWidget(m_mirror);
	mirrorLay->addWidget(m_mirrorX);
	mirrorLay->addWidget(m_mirrorY);
	mirrorLay->addWidget(m_mirrorXY);
	root->addWidget(mirrorBox);

	root->addStretch(1);

	// Seed everything under the guard so nothing echoes back to the tool while we populate.
	m_updating = true;
	rebuildTextureTree(QString());
	rebuildFavoritesTree();
	refreshSwatches();
	refreshName();
	setWidthRow(WBQtTerrainMaterial_GetWidth());
	m_heightSlider->setValue(WBQtTerrainMaterial_GetHeight());
	m_heightSpin->setValue(WBQtTerrainMaterial_GetHeight());
	m_paintPathing->setChecked(WBQtTerrainMaterial_IsPaintingPathing() != 0);
	m_passable->setChecked(WBQtTerrainMaterial_IsPassable() != 0);
	m_impassable->setChecked(WBQtTerrainMaterial_IsPassable() == 0);
	m_patternPaint->setChecked(WBQtTerrainMaterial_IsPatternPaint() != 0);
	{
		int mode = WBQtTerrainMaterial_GetPaintMode();		// 1-based
		if (mode >= 1 && mode <= m_paintMode->count())
		{
			m_paintMode->setCurrentIndex(mode - 1);
		}
	}
	m_density->setValue(WBQtTerrainMaterial_GetPaintDensity());
	m_noMixing->setChecked(WBQtTerrainMaterial_IsNoMixing() != 0);
	m_copyTexture->setChecked(WBQtTerrainMaterial_IsCopyTextureMode() != 0);
	m_copyTerrain->setChecked(WBQtTerrainMaterial_IsCopyTerrainMode() != 0);
	m_raiseOnly->setChecked(WBQtTerrainMaterial_IsRaiseOnly() != 0);
	m_copySelect->setChecked(WBQtTerrainMaterial_IsCopySelectMode() != 0);
	m_copyApply->setChecked(WBQtTerrainMaterial_IsCopyApplyMode() != 0);
	{
		int rot = WBQtTerrainMaterial_GetCopyRotation();
		m_rot0->setChecked(rot == 0);
		m_rot90->setChecked(rot == 90);
		m_rot180->setChecked(rot == 180);
		m_rot270->setChecked(rot == 270);
	}
	m_mirror->setChecked(WBQtTerrainMaterial_GetMirror() != 0);
	m_mirrorX->setChecked(WBQtTerrainMaterial_GetMirrorX() != 0);
	m_mirrorY->setChecked(WBQtTerrainMaterial_GetMirrorY() != 0);
	m_mirrorXY->setChecked(WBQtTerrainMaterial_GetMirrorXY() != 0);
	updateEnableState();
	m_updating = false;

	connect(m_tree, SIGNAL(itemSelectionChanged()), this, SLOT(onTextureSelectionChanged()));
	connect(m_searchBtn, SIGNAL(clicked()), this, SLOT(onSearch()));
	connect(m_resetBtn, SIGNAL(clicked()), this, SLOT(onReset()));
	connect(m_search, SIGNAL(returnPressed()), this, SLOT(onSearch()));
	connect(m_swapBtn, SIGNAL(clicked()), this, SLOT(onSwap()));
	connect(m_favTree, SIGNAL(itemSelectionChanged()), this, SLOT(onFavoriteSelectionChanged()));
	connect(m_setFavBtn, SIGNAL(clicked()), this, SLOT(onSetFavorite()));
	connect(m_delFavBtn, SIGNAL(clicked()), this, SLOT(onDeleteFavorite()));
	connect(m_importFavBtn, SIGNAL(clicked()), this, SLOT(onImportFavorites()));
	connect(m_widthSlider, SIGNAL(valueChanged(int)), this, SLOT(onWidthChanged(int)));
	connect(m_widthSpin, SIGNAL(valueChanged(int)), this, SLOT(onWidthChanged(int)));
	connect(m_heightSlider, SIGNAL(valueChanged(int)), this, SLOT(onHeightChanged(int)));
	connect(m_heightSpin, SIGNAL(valueChanged(int)), this, SLOT(onHeightChanged(int)));
	connect(m_paintPathing, SIGNAL(clicked()), this, SLOT(onPathingToggled()));
	connect(m_passable, SIGNAL(clicked()), this, SLOT(onPassableChanged()));
	connect(m_impassable, SIGNAL(clicked()), this, SLOT(onPassableChanged()));
	connect(m_patternPaint, SIGNAL(clicked()), this, SLOT(onPatternPaintToggled()));
	connect(m_paintMode, SIGNAL(currentIndexChanged(int)), this, SLOT(onPaintModeChanged(int)));
	connect(m_density, SIGNAL(valueChanged(int)), this, SLOT(onDensityChanged(int)));
	connect(m_noMixing, SIGNAL(clicked()), this, SLOT(onNoMixingToggled()));
	connect(m_copyTexture, SIGNAL(clicked()), this, SLOT(onCopyTextureToggled()));
	connect(m_copyTerrain, SIGNAL(clicked()), this, SLOT(onCopyTerrainToggled()));
	connect(m_raiseOnly, SIGNAL(clicked()), this, SLOT(onRaiseOnlyToggled()));
	connect(m_copySelect, SIGNAL(clicked()), this, SLOT(onCopyModeChanged()));
	connect(m_copyApply, SIGNAL(clicked()), this, SLOT(onCopyModeChanged()));
	connect(m_rot0, SIGNAL(clicked()), this, SLOT(onRotationChanged()));
	connect(m_rot90, SIGNAL(clicked()), this, SLOT(onRotationChanged()));
	connect(m_rot180, SIGNAL(clicked()), this, SLOT(onRotationChanged()));
	connect(m_rot270, SIGNAL(clicked()), this, SLOT(onRotationChanged()));
	connect(m_mirror, SIGNAL(clicked()), this, SLOT(onMirror()));
	connect(m_mirrorX, SIGNAL(clicked()), this, SLOT(onMirrorX()));
	connect(m_mirrorY, SIGNAL(clicked()), this, SLOT(onMirrorY()));
	connect(m_mirrorXY, SIGNAL(clicked()), this, SLOT(onMirrorXY()));

	s_instance = this;
}

QTreeWidgetItem *WBQtTerrainMaterialPanel::findOrAddChild(QTreeWidgetItem *parent, const QString &label)
{
	if (parent == NULL)
	{
		for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
		{
			QTreeWidgetItem *child = m_tree->topLevelItem(i);
			if (child->data(0, kTexClassRole).toInt() < 0 && child->text(0) == label)
			{
				return child;
			}
		}
		QTreeWidgetItem *node = new QTreeWidgetItem(m_tree);
		node->setText(0, label);
		node->setData(0, kTexClassRole, -1);
		return node;
	}

	for (int i = 0; i < parent->childCount(); ++i)
	{
		QTreeWidgetItem *child = parent->child(i);
		if (child->data(0, kTexClassRole).toInt() < 0 && child->text(0) == label)
		{
			return child;
		}
	}
	QTreeWidgetItem *node = new QTreeWidgetItem(parent);
	node->setText(0, label);
	node->setData(0, kTexClassRole, -1);
	return node;
}

void WBQtTerrainMaterialPanel::rebuildTextureTree(const QString &filter)
{
	m_tree->clear();

	const int cap = 512;
	char pathBuf[cap];
	char leafBuf[cap];
	char nameBuf[cap];

	QString lowered = filter.toLower();
	bool expandAll = !lowered.isEmpty();

	int count = WBQtTerrainMaterial_GetTexClassCount();
	int fgClass = WBQtTerrainMaterial_GetFgTexClass();
	QTreeWidgetItem *selectItem = NULL;

	for (int i = 0; i < count; ++i)
	{
		if (!WBQtTerrainMaterial_GetTexClassEntry(i, pathBuf, leafBuf, cap))
		{
			continue;	// skipped (blend edge / no data)
		}

		if (!lowered.isEmpty())
		{
			if (!WBQtTerrainMaterial_GetTexClassUiName(i, nameBuf, cap))
			{
				continue;
			}
			QString full = QString::fromLatin1(nameBuf).toLower();
			if (!full.contains(lowered))
			{
				continue;
			}
		}

		// Split the "\"-joined category path into nested grouping nodes.
		QTreeWidgetItem *parent = NULL;
		QString path = QString::fromLatin1(pathBuf);
		if (!path.isEmpty())
		{
			QStringList segs = path.split(QChar('\\'), QString::SkipEmptyParts);
			for (int s = 0; s < segs.size(); ++s)
			{
				parent = findOrAddChild(parent, segs.at(s));
			}
		}

		QTreeWidgetItem *leaf;
		if (parent == NULL)
		{
			leaf = new QTreeWidgetItem(m_tree);
		}
		else
		{
			leaf = new QTreeWidgetItem(parent);
		}
		leaf->setText(0, QString::fromLatin1(leafBuf));
		leaf->setData(0, kTexClassRole, i);
		if (i == fgClass)
		{
			selectItem = leaf;
		}
	}

	m_tree->sortItems(0, Qt::AscendingOrder);
	if (expandAll)
	{
		m_tree->expandAll();
	}
	if (selectItem != NULL)
	{
		m_tree->setCurrentItem(selectItem);
	}
}

void WBQtTerrainMaterialPanel::rebuildFavoritesTree()
{
	m_favTree->clear();
	const int cap = 512;
	char nameBuf[cap];
	int count = WBQtTerrainMaterial_GetFavoriteCount();
	for (int i = 0; i < count; ++i)
	{
		int texClass = -1;
		if (WBQtTerrainMaterial_GetFavorite(i, nameBuf, cap, &texClass))
		{
			QTreeWidgetItem *item = new QTreeWidgetItem(m_favTree);
			item->setText(0, QString::fromLatin1(nameBuf));
			item->setData(0, kTexClassRole, texClass);
		}
	}
}

void WBQtTerrainMaterialPanel::setSwatch(QLabel *label, int texClass)
{
	int extent = WBQtTerrainMaterial_GetSwatchExtent();
	if (extent <= 0)
	{
		label->setText("(n/a)");
		return;
	}
	QByteArray bgra(extent * extent * 4, 0);
	if (!WBQtTerrainMaterial_GetSwatchPixels(texClass, reinterpret_cast<unsigned char*>(bgra.data()), bgra.size()))
	{
		// No tile data: draw a solid placeholder, matching TerrainSwatches' FillSolidRect.
		QPixmap pm(extent, extent);
		pm.fill(QColor(0, 128, 0));
		label->setPixmap(pm.scaled(label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
		return;
	}

	QImage img(extent, extent, QImage::Format_RGB888);
	// The MFC swatch blits the tile as a positive-height (bottom-up) DIB, so read source rows
	// bottom-to-top; the source pixels are BGRA (drop A).
	for (int y = 0; y < extent; ++y)
	{
		const unsigned char *src = reinterpret_cast<const unsigned char*>(bgra.constData()) + (extent - 1 - y) * extent * 4;
		unsigned char *dst = img.scanLine(y);
		for (int x = 0; x < extent; ++x)
		{
			dst[x * 3 + 0] = src[x * 4 + 2];	// R <- B
			dst[x * 3 + 1] = src[x * 4 + 1];	// G
			dst[x * 3 + 2] = src[x * 4 + 0];	// B <- R
		}
	}
	label->setPixmap(QPixmap::fromImage(img).scaled(label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void WBQtTerrainMaterialPanel::refreshSwatches()
{
	setSwatch(m_fgSwatch, WBQtTerrainMaterial_GetFgTexClass());
	setSwatch(m_bgSwatch, WBQtTerrainMaterial_GetBgTexClass());
}

void WBQtTerrainMaterialPanel::refreshName()
{
	const int cap = 512;
	char buf[cap];
	if (WBQtTerrainMaterial_GetFgLeafName(buf, cap) && buf[0] != 0)
	{
		m_nameLabel->setText(QString::fromLatin1(buf));
	}
	else
	{
		m_nameLabel->setText("No Selection");
	}
}

void WBQtTerrainMaterialPanel::setWidthRow(int v)
{
	// Caller has set m_updating. Clamp to range to avoid Qt warnings.
	if (v < m_widthSlider->minimum())
	{
		v = m_widthSlider->minimum();
	}
	if (v > m_widthSlider->maximum())
	{
		v = m_widthSlider->maximum();
	}
	m_widthSlider->setValue(v);
	m_widthSpin->setValue(v);
	m_widthLabel->setText(QString::asprintf("%.1f FEET.", v * WBQtTerrainMaterial_GetFeetPerCell()));
}

void WBQtTerrainMaterialPanel::updateEnableState()
{
	// Mirrors setToolOptions / OnPassableCheck / OnTogglePaintMode / copyMode / OnToggleMirror:
	// the size, copy, pattern, no-mixing and mirror controls only apply to the multi-tile tool;
	// pathing / copy / pattern paint are mutually exclusive, and mirror disables pattern paint.
	bool single = (WBQtTerrainMaterial_IsSingleCell() != 0);
	bool pathing = m_paintPathing->isChecked();
	bool pattern = m_patternPaint->isChecked();
	bool mirror = m_mirror->isChecked();
	bool copyChecked = m_copyTexture->isChecked() || m_copyTerrain->isChecked();

	// Brush size: enabled only for multi-tile.
	m_widthSlider->setEnabled(!single);
	m_widthSpin->setEnabled(!single);
	m_widthLabel->setEnabled(!single);

	// Passable/impassable radios follow the pathing checkbox.
	m_passable->setEnabled(pathing);
	m_impassable->setEnabled(pathing);
	// The texture tree/swatches are disabled while painting pathing or in copy mode.
	bool texEnabled = !pathing && !copyChecked;
	m_tree->setEnabled(texEnabled);
	m_fgSwatch->setEnabled(texEnabled);
	m_bgSwatch->setEnabled(texEnabled);

	// Pattern paint: multi-tile only, and off while mirror is active. Its combo/density follow.
	m_patternPaint->setEnabled(!single && !mirror);
	m_paintMode->setEnabled(!single && !mirror && pattern);
	bool densityOn = (!single && !mirror && pattern && WBQtTerrainMaterial_GetPaintMode() == kScatterBMode);
	m_density->setEnabled(densityOn);
	m_densityLabel->setEnabled(densityOn);

	// No mixing: multi-tile only, off while mirror active (matches setToolOptions).
	m_noMixing->setEnabled(!single && !mirror);

	// Copy mode: multi-tile only, and off while pattern paint or pathing active.
	bool copyEnabled = !single && !pattern && !pathing;
	m_copyTexture->setEnabled(copyEnabled);
	m_copyTerrain->setEnabled(copyEnabled);
	m_raiseOnly->setEnabled(copyEnabled && m_copyTerrain->isChecked());
	m_copySelect->setEnabled(copyEnabled && copyChecked);
	m_copyApply->setEnabled(copyEnabled && copyChecked);
	bool rotEnabled = copyEnabled && copyChecked;
	m_rot0->setEnabled(rotEnabled);
	m_rot90->setEnabled(rotEnabled);
	m_rot180->setEnabled(rotEnabled);
	m_rot270->setEnabled(rotEnabled);

	// Mirror: multi-tile only, off while pattern paint active.
	bool mirrorEnabled = !single && !pattern;
	m_mirror->setEnabled(mirrorEnabled);
	m_mirrorX->setEnabled(mirrorEnabled);
	m_mirrorY->setEnabled(mirrorEnabled);
	m_mirrorXY->setEnabled(mirrorEnabled);
}

// --- Texture browser slots ------------------------------------------------------------------
void WBQtTerrainMaterialPanel::onTextureSelectionChanged()
{
	if (m_updating)
	{
		return;
	}
	QList<QTreeWidgetItem*> sel = m_tree->selectedItems();
	if (sel.isEmpty())
	{
		return;
	}
	int texClass = sel.first()->data(0, kTexClassRole).toInt();
	if (texClass < 0)
	{
		return;	// grouping node
	}

	if (!WBQtTerrainMaterial_SelectFgTexClass(texClass))
	{
		// Too large to fit -- warn like OnNotify, but the class is still selected.
		QMessageBox::warning(this, "Terrain Material",
			"That texture is too large to fit in the remaining texture space.");
	}
	m_updating = true;
	refreshSwatches();
	refreshName();
	m_updating = false;
}

void WBQtTerrainMaterialPanel::onSearch()
{
	QString text = m_search->text().trimmed();
	m_updating = true;
	rebuildTextureTree(text);
	m_updating = false;
}

void WBQtTerrainMaterialPanel::onReset()
{
	m_search->clear();
	m_updating = true;
	rebuildTextureTree(QString());
	m_updating = false;
}

void WBQtTerrainMaterialPanel::onSwap()
{
	WBQtTerrainMaterial_SwapTextures();
	m_updating = true;
	refreshSwatches();
	refreshName();
	rebuildTextureTree(m_search->text().trimmed());
	m_updating = false;
}

// --- Favorites slots ------------------------------------------------------------------------
void WBQtTerrainMaterialPanel::onSetFavorite()
{
	QList<QTreeWidgetItem*> sel = m_tree->selectedItems();
	if (sel.isEmpty())
	{
		return;
	}
	int texClass = sel.first()->data(0, kTexClassRole).toInt();
	if (texClass < 0)
	{
		return;
	}
	QByteArray label = sel.first()->text(0).toLatin1();
	WBQtTerrainMaterial_AddFavorite(texClass, label.constData());
	m_updating = true;
	rebuildFavoritesTree();
	m_updating = false;
}

void WBQtTerrainMaterialPanel::onDeleteFavorite()
{
	// NOTE: do NOT hold a QModelIndexList here. WorldBuilder globally overrides operator
	// new/delete to route through the game's MemoryPool; a QModelIndexList's heap node array
	// is allocated by Qt but freed via that override on scope exit, which frees a block the
	// game pool never owned -> access violation (crash on delete-favorite). Use selectedItems().
	QList<QTreeWidgetItem*> sel = m_favTree->selectedItems();
	if (sel.isEmpty())
	{
		return;
	}
	int row = m_favTree->indexOfTopLevelItem(sel.first());
	if (row < 0)
	{
		return;
	}
	WBQtTerrainMaterial_DeleteFavorite(row);
	m_updating = true;
	rebuildFavoritesTree();
	m_updating = false;
}

void WBQtTerrainMaterialPanel::onImportFavorites()
{
	if (QMessageBox::question(this, "Import Favorites",
		"This will import the custom favorite textures from this map if there's one.\n\n"
		"Do you want to continue?",
		QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
	{
		return;
	}
	WBQtTerrainMaterial_ImportFavorites();
	m_updating = true;
	rebuildFavoritesTree();
	m_updating = false;
}

void WBQtTerrainMaterialPanel::onFavoriteSelectionChanged()
{
	if (m_updating)
	{
		return;
	}
	QList<QTreeWidgetItem*> sel = m_favTree->selectedItems();
	if (sel.isEmpty())
	{
		return;
	}
	int texClass = sel.first()->data(0, kTexClassRole).toInt();
	if (texClass < 0)
	{
		return;
	}
	if (!WBQtTerrainMaterial_SelectFgTexClass(texClass))
	{
		QMessageBox::warning(this, "Terrain Material",
			"That texture is too large to fit in the remaining texture space.");
	}
	m_updating = true;
	refreshSwatches();
	refreshName();
	m_updating = false;
}

// --- Brush size / z-height slots ------------------------------------------------------------
void WBQtTerrainMaterialPanel::onWidthChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setWidthRow(v);
	WBQtTerrainMaterial_SetWidth(v);
	m_updating = false;
}

void WBQtTerrainMaterialPanel::onHeightChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	m_heightSlider->setValue(v);
	m_heightSpin->setValue(v);
	WBQtTerrainMaterial_SetHeight(v);
	m_updating = false;
}

// --- Pathing slots --------------------------------------------------------------------------
void WBQtTerrainMaterialPanel::onPathingToggled()
{
	WBQtTerrainMaterial_SetPaintPathing(m_paintPathing->isChecked() ? 1 : 0);
	m_updating = true;
	updateEnableState();
	m_updating = false;
}

void WBQtTerrainMaterialPanel::onPassableChanged()
{
	WBQtTerrainMaterial_SetPassable(m_passable->isChecked() ? 1 : 0);
}

// --- Pattern paint slots --------------------------------------------------------------------
void WBQtTerrainMaterialPanel::onPatternPaintToggled()
{
	WBQtTerrainMaterial_SetPatternPaint(m_patternPaint->isChecked() ? 1 : 0);
	m_updating = true;
	updateEnableState();
	m_updating = false;
}

void WBQtTerrainMaterialPanel::onPaintModeChanged(int index)
{
	if (m_updating)
	{
		return;
	}
	WBQtTerrainMaterial_SetPaintMode(index + 1);	// modes are 1-based, like the MFC combo
	m_updating = true;
	updateEnableState();
	m_updating = false;
}

void WBQtTerrainMaterialPanel::onDensityChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	WBQtTerrainMaterial_SetPaintDensity(v);
}

// --- No mixing slot -------------------------------------------------------------------------
void WBQtTerrainMaterialPanel::onNoMixingToggled()
{
	WBQtTerrainMaterial_SetNoMixing(m_noMixing->isChecked() ? 1 : 0);
}

// --- Copy mode slots ------------------------------------------------------------------------
void WBQtTerrainMaterialPanel::onCopyTextureToggled()
{
	WBQtTerrainMaterial_SetCopyTextureMode(m_copyTexture->isChecked() ? 1 : 0);
	onCopyModeChanged();	// enable Select/Apply + default to Select, like copyMode()
}

void WBQtTerrainMaterialPanel::onCopyTerrainToggled()
{
	WBQtTerrainMaterial_SetCopyTerrainMode(m_copyTerrain->isChecked() ? 1 : 0);
	onCopyModeChanged();
}

void WBQtTerrainMaterialPanel::onRaiseOnlyToggled()
{
	WBQtTerrainMaterial_SetRaiseOnly(m_raiseOnly->isChecked() ? 1 : 0);
}

void WBQtTerrainMaterialPanel::onCopyModeChanged()
{
	// When copy mode becomes active default to Select (matches copyMode()); the actual
	// select/apply choice drives the tool.
	bool copyChecked = m_copyTexture->isChecked() || m_copyTerrain->isChecked();
	if (copyChecked && !m_copySelect->isChecked() && !m_copyApply->isChecked())
	{
		m_updating = true;
		m_copySelect->setChecked(true);
		m_updating = false;
	}
	if (m_copyApply->isChecked())
	{
		WBQtTerrainMaterial_SetCopyApplyMode();
	}
	else if (m_copySelect->isChecked())
	{
		WBQtTerrainMaterial_SetCopySelectMode();
	}
	m_updating = true;
	updateEnableState();
	m_updating = false;
}

void WBQtTerrainMaterialPanel::onRotationChanged()
{
	int rot = 0;
	if (m_rot90->isChecked())
	{
		rot = 90;
	}
	else if (m_rot180->isChecked())
	{
		rot = 180;
	}
	else if (m_rot270->isChecked())
	{
		rot = 270;
	}
	WBQtTerrainMaterial_SetCopyRotation(rot);
}

// --- Mirror slots ---------------------------------------------------------------------------
void WBQtTerrainMaterialPanel::onMirror()
{
	WBQtTerrainMaterial_ToggleMirror();
	m_updating = true;
	updateEnableState();
	m_updating = false;
}

void WBQtTerrainMaterialPanel::onMirrorX()
{
	WBQtTerrainMaterial_ToggleMirrorX();
}

void WBQtTerrainMaterialPanel::onMirrorY()
{
	WBQtTerrainMaterial_ToggleMirrorY();
}

void WBQtTerrainMaterialPanel::onMirrorXY()
{
	WBQtTerrainMaterial_ToggleMirrorXY();
}

// --- Push refresh (MFC/tool -> widget) ------------------------------------------------------
void WBQtTerrainMaterialPanel::refreshFromState()
{
	m_updating = true;
	rebuildTextureTree(m_search->text().trimmed());
	rebuildFavoritesTree();
	refreshSwatches();
	refreshName();
	setWidthRow(WBQtTerrainMaterial_GetWidth());
	m_heightSlider->setValue(WBQtTerrainMaterial_GetHeight());
	m_heightSpin->setValue(WBQtTerrainMaterial_GetHeight());
	m_paintPathing->setChecked(WBQtTerrainMaterial_IsPaintingPathing() != 0);
	m_passable->setChecked(WBQtTerrainMaterial_IsPassable() != 0);
	m_impassable->setChecked(WBQtTerrainMaterial_IsPassable() == 0);
	m_patternPaint->setChecked(WBQtTerrainMaterial_IsPatternPaint() != 0);
	{
		int mode = WBQtTerrainMaterial_GetPaintMode();
		if (mode >= 1 && mode <= m_paintMode->count())
		{
			m_paintMode->setCurrentIndex(mode - 1);
		}
	}
	m_density->setValue(WBQtTerrainMaterial_GetPaintDensity());
	m_noMixing->setChecked(WBQtTerrainMaterial_IsNoMixing() != 0);
	m_copyTexture->setChecked(WBQtTerrainMaterial_IsCopyTextureMode() != 0);
	m_copyTerrain->setChecked(WBQtTerrainMaterial_IsCopyTerrainMode() != 0);
	m_raiseOnly->setChecked(WBQtTerrainMaterial_IsRaiseOnly() != 0);
	m_copySelect->setChecked(WBQtTerrainMaterial_IsCopySelectMode() != 0);
	m_copyApply->setChecked(WBQtTerrainMaterial_IsCopyApplyMode() != 0);
	{
		int rot = WBQtTerrainMaterial_GetCopyRotation();
		m_rot0->setChecked(rot == 0);
		m_rot90->setChecked(rot == 90);
		m_rot180->setChecked(rot == 180);
		m_rot270->setChecked(rot == 270);
	}
	m_mirror->setChecked(WBQtTerrainMaterial_GetMirror() != 0);
	m_mirrorX->setChecked(WBQtTerrainMaterial_GetMirrorX() != 0);
	m_mirrorY->setChecked(WBQtTerrainMaterial_GetMirrorY() != 0);
	m_mirrorXY->setChecked(WBQtTerrainMaterial_GetMirrorXY() != 0);
	updateEnableState();
	m_updating = false;
}

// --- Forward push function (MFC/tool -> widget), the Qt-side of WBQtTerrainMaterialBridge.h --
extern "C" void WBQtTerrainMaterial_PushRefresh(void)
{
	if (WBQtTerrainMaterialPanel::instance() != NULL)
	{
		WBQtTerrainMaterialPanel::instance()->refreshFromState();
	}
}
