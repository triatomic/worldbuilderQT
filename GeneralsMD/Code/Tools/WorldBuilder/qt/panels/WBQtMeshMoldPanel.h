// WBQtMeshMoldPanel.h -- Qt replacement for the MFC MeshMoldOptions dialog (the mesh-mold tool).
//
// A QListWidget of the .w3d mold models found under data\Editor\Molds (mirroring the MFC
// CTreeCtrl), three "slider + spinbox" rows (angle / scale% / height), a Preview toggle, an
// Apply Mesh button, Raise / Raise+Lower / Lower radios, and the two "open molds folder" /
// "how to create molds" shell-out buttons. It edits none of its own state -- selecting a mold
// drives the MFC MeshMoldOptions selection statics (via the bridge) that MeshMoldTool reads, so
// mesh molding keeps working. The MFC MeshMoldOptions stays as the toggle-OFF fallback; this is
// the RTS_HAS_QT path. A top-level Qt::Tool window owned by the shared QWinWidget bridge.
#ifndef WB_QT_MESHMOLD_PANEL_H
#define WB_QT_MESHMOLD_PANEL_H

#include <QWidget>

class QListWidget;
class QPushButton;
class QRadioButton;
class QSlider;
class QSpinBox;

class WBQtMeshMoldPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtMeshMoldPanel(QWidget *owner);

	static WBQtMeshMoldPanel *instance() { return s_instance; }

private slots:
	void onMoldSelectionChanged();
	void onAngleChanged(int v);
	void onScaleChanged(int v);
	void onHeightChanged(int v);
	void onPreviewToggled();
	void onApplyMesh();
	void onRaiseModeChanged();
	void onOpenMoldsFolder();
	void onOpenLink();

private:
	void setRow(QSlider *slider, QSpinBox *spin, int v);	// set both without re-entry
	void rebuildMoldList();

	QListWidget  *m_moldList;
	QSlider      *m_angleSlider;
	QSpinBox     *m_angleSpin;
	QSlider      *m_scaleSlider;
	QSpinBox     *m_scaleSpin;
	QSlider      *m_heightSlider;
	QSpinBox     *m_heightSpin;
	QPushButton  *m_preview;		// checkable, mirrors the MFC preview toggle button
	QPushButton  *m_applyMesh;
	QRadioButton *m_raise;
	QRadioButton *m_raiseLower;
	QRadioButton *m_lower;

	bool m_updating;	// re-entrancy guard, mirrors MFC MeshMoldOptions::m_updating

	static WBQtMeshMoldPanel *s_instance;
};

#endif // WB_QT_MESHMOLD_PANEL_H
