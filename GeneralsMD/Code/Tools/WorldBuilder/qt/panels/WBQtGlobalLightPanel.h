// WBQtGlobalLightPanel.h -- Qt replacement for the MFC GlobalLightOptions dialog.
//
// The modeless Global Light Options window (Edit > Global Light Options): per-light azimuth /
// elevation angles (Sun + two Accents) with drag-scrub spinboxes replacing the MFC popup sliders,
// ambient color (swatch + RGB) on the Sun, diffuse RGB per light, the "Lighting applies to"
// radios, the XYZ readout, the time-of-day caption and Restore To Default. All state lives on the
// hidden MFC GlobalLightOptions (a CMainFrame member, still Create()d as the OFF fallback) behind
// the WBQtGlobalLight_* facade; its applyAngle/applyColor paths drive the 3D view.
//
// Unlike the option panels this window is NOT in the wbQtPanelFor registry -- it opens from the
// Edit menu (WBQtGlobalLight_Open) and can stay up alongside any option panel, like the MFC one.
#ifndef WB_QT_GLOBALLIGHT_PANEL_H
#define WB_QT_GLOBALLIGHT_PANEL_H

#include <QWidget>

class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;
class WBQtScrubSpinBox;

class WBQtGlobalLightPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtGlobalLightPanel(QWidget *owner);

	// Re-seed every control from the hidden MFC dialog / globals.
	void seedFromGlobals();

	static WBQtGlobalLightPanel *instance() { return s_instance; }

protected:
	virtual void showEvent(QShowEvent *event);	// sync hidden dialog from globals + seed
	virtual void hideEvent(QHideEvent *event);	// turn off the in-view light feedback arrows

private slots:
	void onReset();
	void onLightingRadio();
	void onAngleChanged();
	void onAmbientChanged();
	void onDiffuseChanged();
	void onAmbientSwatch();

private:
	void updateXYZLabel(int light);
	void updateSwatch();
	int  senderLight(QObject *src) const;	// which light column a control belongs to

	// Per-light angle + diffuse controls (0 Sun, 1 Accent1, 2 Accent2).
	WBQtScrubSpinBox *m_azimuth[3];
	WBQtScrubSpinBox *m_elevation[3];
	QSpinBox         *m_diffR[3];
	QSpinBox         *m_diffG[3];
	QSpinBox         *m_diffB[3];

	// Ambient (Sun only).
	QPushButton *m_ambientSwatch;
	QSpinBox    *m_ambR;
	QSpinBox    *m_ambG;
	QSpinBox    *m_ambB;

	QRadioButton *m_radioTerrain;
	QRadioButton *m_radioObjects;
	QRadioButton *m_radioEverything;

	QLabel *m_xyzLabel;
	QLabel *m_todLabel;

	bool m_updating;

	static WBQtGlobalLightPanel *s_instance;
};

#endif // WB_QT_GLOBALLIGHT_PANEL_H
