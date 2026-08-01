// WBQtAnimScrubPanel.cpp -- see WBQtAnimScrubPanel.h.
#include "WBQtAnimScrubPanel.h"
#include "ui_WBQtAnimScrubPanel.h"
#include "WBQtAnimScrubBridge.h"
#include "WBQtWindowPos.h"
#include "qwinwidget.h"

#include <QLabel>
#include <QPushButton>
#include <QSlider>

#include <qt_windows.h>

WBQtAnimScrubPanel *WBQtAnimScrubPanel::s_instance = NULL;

// Defined in WBQtBridge.cpp: the main window when inverted, else an invisible
// QWinWidget bridge rooted in the MFC frame. Never hide() the result.
QWidget *WBQt_CreateOwnerBridgeWidget(void *frameHwnd);

namespace
{
	QWidget *s_owner = NULL;	// owner for the floating panel (created on first open)

	const int SCRUB_RESOLUTION = 1000;	// slider steps; matches the .ui maximum
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
	m_percentText = m_ui->percentText;
	m_frameText = m_ui->frameText;

	// Applying costs a FULL scene rebuild (see WbView3d::setAnimationScrub -- a per-object teardown
	// is not something WB supports), so the pose follows the handle rather than every pixel of the
	// drag: valueChanged only moves the readout, and sliderReleased is what actually poses. Keyboard
	// and Start/End still apply immediately, since those are single discrete changes.
	connect(m_scrub, SIGNAL(valueChanged(int)), this, SLOT(onScrub(int)));
	connect(m_scrub, SIGNAL(sliderReleased()), this, SLOT(onScrubCommitted()));
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

	m_scrub->setValue((int)(WBQtAnimScrub_GetFraction() * SCRUB_RESOLUTION + 0.5));

	updateReadout();
	m_updating = false;
}

void WBQtAnimScrubPanel::updateReadout()
{
	const double fraction = (double)m_scrub->value() / (double)SCRUB_RESOLUTION;
	m_percentText->setText(QString::number(fraction * 100.0, 'f', 1) + QLatin1String("%"));

	if (m_frameCount > 0)
	{
		// The frame within the LONGEST animation on the object -- the one that bounds the whole
		// motion. Shorter modules are at the same fraction of their own, fewer, frames.
		const int frame = (int)(fraction * (m_frameCount - 1) + 0.5);
		m_frameText->setText(tr("frame %1 / %2").arg(frame).arg(m_frameCount - 1));
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
	// Readout always tracks the handle so the drag still feels live.
	updateReadout();
	if (m_updating)
	{
		return;
	}
	// Mid-drag: don't pose yet (a full scene rebuild per pixel). onScrubCommitted does it on
	// release. When the value changed WITHOUT a drag -- arrow keys, page up/down, Start/End,
	// clicking the groove -- there is no release to wait for, so apply straight away.
	if (m_scrub->isSliderDown())
	{
		return;
	}
	WBQtAnimScrub_SetFraction((double)value / (double)SCRUB_RESOLUTION);
}

void WBQtAnimScrubPanel::onScrubCommitted()
{
	if (m_updating)
	{
		return;
	}
	WBQtAnimScrub_SetFraction((double)m_scrub->value() / (double)SCRUB_RESOLUTION);
}

void WBQtAnimScrubPanel::onStart()
{
	m_scrub->setValue(0);
}

void WBQtAnimScrubPanel::onEnd()
{
	m_scrub->setValue(SCRUB_RESOLUTION);
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
