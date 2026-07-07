// WBQtTerrainMaterialPanel.h -- Qt replacement for the MFC TerrainMaterial dialog (the terrain
// texture browser shown by the Tile / Flood-Fill / Eyedropper tools).
//
// A top-level Qt::Tool window owned by the shared QWinWidget bridge (see WBQtOptionsPanels.cpp)
// so it floats over the MFC main window with correct stacking and gets the dark title bar
// automatically. Reproduces the MFC dialog's controls: a categorised texture tree + search, a
// favorites tree with set/delete/import, foreground/background tile swatches + swap + name
// label, the brush-size and z-height sliders, the passable/impassable pathing controls, the
// pattern-paint mode + density, no-mixing, copy mode (texture/terrain/raise-only + select/apply
// + rotation) and the mirror toggles.
//
// It edits none of its own state -- every control drives the TerrainMaterial selection statics /
// BigTileTool / FloodFillTool that the tools read, via the reverse callbacks in
// WBQtTerrainMaterialBridge.h. The MFC TerrainMaterial stays created as the toggle-OFF fallback
// (it owns the favorites tree + width/height edit boxes those callbacks reach); this is the
// RTS_HAS_QT path. It refreshes itself (texture list + enable state) when the MFC side pushes
// WBQtTerrainMaterial_PushRefresh (map load, tool switch).
#ifndef WB_QT_TERRAIN_MATERIAL_PANEL_H
#define WB_QT_TERRAIN_MATERIAL_PANEL_H

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSlider;
class QTreeWidget;
class QTreeWidgetItem;
class WBQtScrubSpinBox;

class WBQtTerrainMaterialPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtTerrainMaterialPanel(QWidget *owner);

	// Re-seed the whole panel from the current MFC/tool state (WBQtTerrainMaterial_PushRefresh).
	void refreshFromState();

	// Light push (WBQtTerrainMaterial_PushSelection): the fg selection changed outside the panel
	// (eyedropper pick) -- re-select the tree item and refresh the swatches/name, no tree rebuild.
	void refreshSelectionFromState();

	// Light push (WBQtTerrainMaterial_PushToolState): everything EXCEPT the tree/favorites
	// rebuild -- swatches, name, sliders, toggles, enable state. setToolOptions runs on every
	// tool activation (including transient Ctrl/Alt swaps), so it must not rebuild the tree:
	// that is per-activation O(N) work and resets the user's scroll position.
	void refreshToolState();

	static WBQtTerrainMaterialPanel *instance() { return s_instance; }

private slots:
	void onTextureSelectionChanged();
	void onSearch();
	void onReset();
	void onSwap();
	void onSetFavorite();
	void onDeleteFavorite();
	void onImportFavorites();
	void onFavoriteSelectionChanged();
	void onWidthChanged(int v);
	void onWidthSpinChanged(double v);
	void onHeightChanged(int v);
	void onHeightSpinChanged(double v);
	void onPathingToggled();
	void onPassableChanged();
	void onPatternPaintToggled();
	void onPaintModeChanged(int index);
	void onDensityChanged(int v);
	void onNoMixingToggled();
	void onCopyTextureToggled();
	void onCopyTerrainToggled();
	void onRaiseOnlyToggled();
	void onCopyModeChanged();
	void onRotationChanged();
	void onMirror();
	void onMirrorX();
	void onMirrorY();
	void onMirrorXY();

private:
	void rebuildTextureTree(const QString &filter);	// filter empty => full list
	void rebuildFavoritesTree();
	void refreshSwatches();
	void refreshName();
	void updateEnableState();
	void setWidthRow(int v);
	void setSwatch(QLabel *label, int texClass);
	QTreeWidgetItem *findOrAddChild(QTreeWidgetItem *parent, const QString &label);

	// Texture browser.
	QLineEdit    *m_search;
	QPushButton  *m_searchBtn;
	QPushButton  *m_resetBtn;
	QTreeWidget  *m_tree;
	QLabel       *m_nameLabel;

	// Swatches.
	QLabel       *m_fgSwatch;
	QLabel       *m_bgSwatch;
	QPushButton  *m_swapBtn;

	// Favorites.
	QTreeWidget  *m_favTree;
	QPushButton  *m_setFavBtn;
	QPushButton  *m_delFavBtn;
	QPushButton  *m_importFavBtn;

	// Brush size / z-height. The spinboxes scrub on a vertical click-drag (WBQtScrubSpinBox),
	// like the Z / Angle fields in Object Properties.
	QSlider           *m_widthSlider;
	WBQtScrubSpinBox  *m_widthSpin;
	QLabel            *m_widthLabel;
	QSlider           *m_heightSlider;
	WBQtScrubSpinBox  *m_heightSpin;

	// Pathing.
	QCheckBox    *m_paintPathing;
	QRadioButton *m_passable;
	QRadioButton *m_impassable;

	// Pattern paint.
	QCheckBox    *m_patternPaint;
	QComboBox    *m_paintMode;
	QSlider      *m_density;
	QLabel       *m_densityLabel;

	// No mixing.
	QCheckBox    *m_noMixing;

	// Copy mode.
	QCheckBox    *m_copyTexture;
	QCheckBox    *m_copyTerrain;
	QCheckBox    *m_raiseOnly;
	QRadioButton *m_copySelect;
	QRadioButton *m_copyApply;
	QRadioButton *m_rot0;
	QRadioButton *m_rot90;
	QRadioButton *m_rot180;
	QRadioButton *m_rot270;

	// Mirror.
	QCheckBox    *m_mirror;
	QCheckBox    *m_mirrorX;
	QCheckBox    *m_mirrorY;
	QCheckBox    *m_mirrorXY;

	bool m_updating;	// re-entrancy guard, mirrors MFC TerrainMaterial::m_updating

	static WBQtTerrainMaterialPanel *s_instance;
};

#endif // WB_QT_TERRAIN_MATERIAL_PANEL_H
