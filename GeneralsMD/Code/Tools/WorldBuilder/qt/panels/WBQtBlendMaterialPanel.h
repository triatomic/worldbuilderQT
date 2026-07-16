// WBQtBlendMaterialPanel.h -- Qt replacement for the MFC BlendMaterial dialog.
//
// A top-level Qt::Tool window owned by the shared QWinWidget bridge (see
// WBQtOptionsPanels.cpp) so it floats over the MFC main window with correct stacking and gets
// the dark title bar automatically. The AutoEdgeOutTool (blend-edges-out) reads its options
// off two places: the "tile gap" statics on the MFC BlendMaterial dialog (m_hvgap / m_dgap /
// m_revalblends) and the "mirror" statics on AutoEdgeOutTool itself. This panel is a pure
// front-end -- it edits no state of its own; its checkboxes drive those via the reverse
// callbacks in WBQtBlendMaterialBridge.h.
//
// The dialog's disabled terrain tree / swatches (SS_BLACKFRAME placeholders, hidden in the MFC
// OnInitDialog "since we dont have any other options in the tree anyways other than alpha
// blend") carry no live state, so they are intentionally not reproduced. The MFC BlendMaterial
// stays as the toggle-OFF fallback; this is the RTS_HAS_QT path. No forward push: like the MFC
// dialog, the panel only reads tool state at seed time and when the user changes a control.
#ifndef WB_QT_BLEND_MATERIAL_PANEL_H
#define WB_QT_BLEND_MATERIAL_PANEL_H

#include <QWidget>

class QCheckBox;

namespace Ui { class WBQtBlendMaterialPanel; }	// generated from WBQtBlendMaterialPanel.ui

class WBQtBlendMaterialPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtBlendMaterialPanel(QWidget *owner);
	virtual ~WBQtBlendMaterialPanel();

	static WBQtBlendMaterialPanel *instance() { return s_instance; }

private slots:
	void onHorizVertGapToggled();
	void onDiagGapToggled();
	void onRevalBlendsToggled();
	void onMirror();
	void onMirrorX();
	void onMirrorY();
	void onMirrorXY();

private:
	Ui::WBQtBlendMaterialPanel *m_ui;	// owns the static widget tree (WBQtBlendMaterialPanel.ui)

	QCheckBox *m_hvGap;
	QCheckBox *m_dGap;
	QCheckBox *m_revalBlends;
	QCheckBox *m_mirror;
	QCheckBox *m_mirrorX;
	QCheckBox *m_mirrorY;
	QCheckBox *m_mirrorXY;

	bool m_updating;	// re-entrancy guard, mirrors MFC BlendMaterial::m_updating

	static WBQtBlendMaterialPanel *s_instance;
};

#endif // WB_QT_BLEND_MATERIAL_PANEL_H
