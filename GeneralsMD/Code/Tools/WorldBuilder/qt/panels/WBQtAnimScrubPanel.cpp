// WBQtAnimScrubPanel.cpp -- see WBQtAnimScrubPanel.h.
#include "WBQtAnimScrubPanel.h"
#include "ui_WBQtAnimScrubPanel.h"
#include "WBQtAnimScrubBridge.h"
#include "WBQtWindowPos.h"
#include "qwinwidget.h"

#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QtGlobal>

#include <qt_windows.h>

WBQtAnimScrubPanel *WBQtAnimScrubPanel::s_instance = NULL;

// Defined in WBQtBridge.cpp: the main window when inverted, else an invisible
// QWinWidget bridge rooted in the MFC frame. Never hide() the result.
QWidget *WBQt_CreateOwnerBridgeWidget(void *frameHwnd);

namespace
{
	QWidget *s_owner = NULL;	// owner for the floating panel (created on first open)
}

WBQtAnimScrubPanel::WBQtAnimScrubPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtAnimScrubPanel),
	  m_frameCount(0),
	  m_updating(false)
{
	// The static widget tree lives in WBQtAnimScrubPanel.ui; bind the members the logic
	// below uses, then wire what Designer can't express.
	m_ui->setupUi(this);
	WBQtWindowPos_Track(this, "AnimScrubber");

	m_scrub = m_ui->scrub;
	m_objectText = m_ui->objectText;
	m_modulesText = m_ui->modulesText;
	m_frameText = m_ui->frameText;

	// The pose applies live, on every step of the drag, so the animation plays under the handle.
	// That is affordable because WbView3d::repositionAnimationScrub re-poses the render objects the
	// scene already holds instead of rebuilding it; only arming a different object costs a rebuild.
	connect(m_scrub, SIGNAL(valueChanged(int)), this, SLOT(onScrub(int)));
	connect(m_ui->startBtn, SIGNAL(clicked()), this, SLOT(onStart()));
	connect(m_ui->endBtn, SIGNAL(clicked()), this, SLOT(onEnd()));
	connect(m_ui->restBtn, SIGNAL(clicked()), this, SLOT(onRelease()));

	pushRefresh();

	s_instance = this;
}

WBQtAnimScrubPanel::~WBQtAnimScrubPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

void WBQtAnimScrubPanel::pushRefresh()
{
	m_updating = true;

	char name[256];
	int frames = 0;
	int modules = 0;
	const int haveSelection =
		WBQtAnimScrub_GetSelection(name, sizeof(name), &frames, &modules);

	m_frameCount = frames;

	if (!haveSelection)
	{
		m_objectText->setText(tr("(nothing selected)"));
		m_modulesText->setText(QString());
	}
	else
	{
		m_objectText->setText(QString::fromLatin1(name));
		if (modules > 0)
		{
			m_modulesText->setText(tr("%1 (longest %2 frames)").arg(modules).arg(frames));
		}
		else
		{
			m_modulesText->setText(tr("none animate"));
		}
	}

	// Nothing to scrub unless something selected actually carries an animation.
	const bool scrubbable = (haveSelection != 0 && frames > 0);
	m_scrub->setEnabled(scrubbable);
	m_ui->startBtn->setEnabled(scrubbable);
	m_ui->endBtn->setEnabled(scrubbable);
	m_ui->restBtn->setEnabled(scrubbable);

	// One slider step == one frame of the longest animation, so the handle can only ever sit on a
	// frame that exists. The range is per selection; a selection with no animation collapses to the
	// single position 0 (and the slider is disabled anyway).
	m_scrub->setMaximum(lastFrame());
	// Dragging the handle a screenful should cover a useful span of a long animation without
	// crawling frame by frame, but never less than one frame on a short one.
	m_scrub->setPageStep(qMax(1, (lastFrame() + 9) / 10));
	m_scrub->setValue(fractionToFrame(WBQtAnimScrub_GetFraction()));

	updateReadout();
	m_updating = false;
}

// The frame the slider position stands for, within the LONGEST animation on the object -- the one
// that bounds the whole motion. Shorter modules are at the same fraction of their own, fewer frames.
int WBQtAnimScrubPanel::lastFrame() const
{
	if (m_frameCount > 1)
	{
		return m_frameCount - 1;
	}
	return 0;
}

double WBQtAnimScrubPanel::frameToFraction(int frame) const
{
	if (lastFrame() <= 0)
	{
		return 0.0;
	}
	return (double)frame / (double)lastFrame();
}

int WBQtAnimScrubPanel::fractionToFrame(double fraction) const
{
	return (int)(fraction * lastFrame() + 0.5);
}

void WBQtAnimScrubPanel::updateReadout()
{
	if (m_frameCount > 0)
	{
		m_frameText->setText(tr("frame %1 / %2").arg(m_scrub->value()).arg(lastFrame()));
	}
	else
	{
		m_frameText->setText(QString());
	}
}

void WBQtAnimScrubPanel::showEvent(QShowEvent *event)
{
	pushRefresh();
	QWidget::showEvent(event);
}

void WBQtAnimScrubPanel::hideEvent(QHideEvent *event)
{
	// Closing the window must not leave the map holding a scrubbed pose -- that would look like a
	// broken object with no visible cause once the panel is gone.
	WBQtAnimScrub_Clear();
	QWidget::hideEvent(event);
}

void WBQtAnimScrubPanel::onScrub(int value)
{
	updateReadout();
	if (m_updating)
	{
		return;
	}
	// Every step poses, drag included -- that is what makes the animation play under the handle.
	WBQtAnimScrub_SetFraction(frameToFraction(value));
}

void WBQtAnimScrubPanel::onStart()
{
	m_scrub->setValue(0);
}

void WBQtAnimScrubPanel::onEnd()
{
	m_scrub->setValue(lastFrame());
}

void WBQtAnimScrubPanel::onRelease()
{
	WBQtAnimScrub_Clear();
	m_updating = true;
	m_scrub->setValue(0);
	updateReadout();
	m_updating = false;
}

// --- Open / push hooks (the Qt side of WBQtAnimScrubBridge.h) --------------------------------

extern "C" void WBQtAnimScrub_PushRefresh(void)
{
	// Called on selection change; stay cheap when the window is closed.
	WBQtAnimScrubPanel *panel = WBQtAnimScrubPanel::instance();
	if (panel != NULL && panel->isVisible())
	{
		panel->pushRefresh();
	}
}

extern "C" void WBQtAnimScrub_Open(void *frameHwnd)
{
	if (frameHwnd == NULL)
	{
		return;
	}
	if (s_owner == NULL)
	{
		s_owner = WBQt_CreateOwnerBridgeWidget(frameHwnd);
	}
	WBQtAnimScrubPanel *panel = WBQtAnimScrubPanel::instance();
	if (panel == NULL)
	{
		panel = new WBQtAnimScrubPanel(s_owner);
	}
	// Show WITHOUT activating (== the MFC SW_SHOWNA) so the viewport keeps focus.
	panel->setAttribute(Qt::WA_ShowWithoutActivating);
	panel->show();
	panel->raise();
}
