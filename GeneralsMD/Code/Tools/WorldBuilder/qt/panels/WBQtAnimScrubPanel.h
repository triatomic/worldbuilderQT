// WBQtAnimScrubPanel.h -- the Animation Scrubber window (View > Models > Animation Scrubber).
//
// Holds the SELECTED object's draw modules at a chosen point through their animations instead of
// at the resting frame the view normally poses them at. Useful for watching what an animation
// actually does to a structure -- and for bone work, since a published bone moves with the pose
// and its riders follow it (see WbView3d::setAnimationScrub).
//
// The slider is a normalized 0..1 position, not a frame number: one object's modules run
// animations of different lengths, so a single frame number would mean a different point in each.
// All calls go through the WBQtAnimScrub_* facade.
#ifndef WB_QT_ANIM_SCRUB_PANEL_H
#define WB_QT_ANIM_SCRUB_PANEL_H

#include <QWidget>

class QLabel;
class QSlider;

namespace Ui { class WBQtAnimScrubPanel; }	// generated from WBQtAnimScrubPanel.ui

class WBQtAnimScrubPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtAnimScrubPanel(QWidget *owner);
	virtual ~WBQtAnimScrubPanel();

	// Re-read the selection (name, frame count, module count) and re-enable accordingly.
	void pushRefresh();

	static WBQtAnimScrubPanel *instance() { return s_instance; }

protected:
	virtual void showEvent(QShowEvent *event);
	virtual void hideEvent(QHideEvent *event);	// stop scrubbing when the window closes

private slots:
	void onScrub(int value);
	void onScrubCommitted();	// slider released -- apply the pose (see the ctor's connect)
	void onStart();
	void onEnd();
	void onRelease();

private:
	void updateReadout();

	Ui::WBQtAnimScrubPanel *m_ui;	// owns the static widget tree (WBQtAnimScrubPanel.ui)

	QSlider *m_scrub;
	QLabel *m_objectText;
	QLabel *m_modulesText;
	QLabel *m_percentText;
	QLabel *m_frameText;

	int m_frameCount;	// longest animation on the selection, 0 == nothing to scrub
	bool m_updating;

	static WBQtAnimScrubPanel *s_instance;
};

#endif // WB_QT_ANIM_SCRUB_PANEL_H
