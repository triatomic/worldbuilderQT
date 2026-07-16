// WBQtCameraPanel.cpp -- see WBQtCameraPanel.h.
#include "WBQtCameraPanel.h"
#include "ui_WBQtCameraPanel.h"
#include "WBQtCameraBridge.h"
#include "WBQtScrubSpinBox.h"
#include "WBQtWindowPos.h"
#include "qwinwidget.h"

#include <QLabel>
#include <QPushButton>

#include <qt_windows.h>

WBQtCameraPanel *WBQtCameraPanel::s_instance = NULL;

// Defined in WBQtBridge.cpp: the main window when inverted, else an invisible
// QWinWidget bridge rooted in the MFC frame. Never hide() the result.
QWidget *WBQt_CreateOwnerBridgeWidget(void *frameHwnd);

namespace
{
	QWidget *s_owner = NULL;	// owner for the floating panel (created on first open)
}

WBQtCameraPanel::WBQtCameraPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtCameraPanel),
	  m_updating(false)
{
	// The static widget tree lives in WBQtCameraPanel.ui; bind the members the
	// logic below uses, then wire what Designer can't express.
	m_ui->setupUi(this);
	WBQtWindowPos_Track(this, "CameraOptions");

	m_pitch = m_ui->pitch;
	m_heightText = m_ui->heightText;
	m_zoomText = m_ui->zoomText;
	m_posText = m_ui->posText;
	m_targetText = m_ui->targetText;

	m_pitch->setAxisVertical(true);		// promotion can't pass the ctor arg

	pushRefresh();

	connect(m_ui->resetBtn, SIGNAL(clicked()), this, SLOT(onReset()));
	connect(m_pitch, SIGNAL(valueChanged(double)), this, SLOT(onPitchChanged()));
	connect(m_ui->dropBtn, SIGNAL(clicked()), this, SLOT(onDropWaypoint()));
	connect(m_ui->centerBtn, SIGNAL(clicked()), this, SLOT(onCenterOnSelected()));

	s_instance = this;
}

WBQtCameraPanel::~WBQtCameraPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

void WBQtCameraPanel::pushRefresh()
{
	m_updating = true;

	m_pitch->setValue(WBQtCamera_GetPitch());

	float height = 0.0f, zoom = 0.0f, posX = 0.0f, posY = 0.0f, tgtX = 0.0f, tgtY = 0.0f;
	if (WBQtCamera_GetInfo(&height, &zoom, &posX, &posY, &tgtX, &tgtY))
	{
		m_heightText->setText(QString::number(height, 'g'));
		m_zoomText->setText(QString::number(zoom, 'g'));
		m_posText->setText(QString("(%1, %2)").arg(posX, 0, 'g').arg(posY, 0, 'g'));
		m_targetText->setText(QString("(%1, %2)").arg(tgtX, 0, 'g').arg(tgtY, 0, 'g'));
	}

	m_updating = false;
}

void WBQtCameraPanel::showEvent(QShowEvent *event)
{
	pushRefresh();
	QWidget::showEvent(event);
}

void WBQtCameraPanel::onReset()
{
	WBQtCamera_Reset();
	pushRefresh();
}

void WBQtCameraPanel::onPitchChanged()
{
	if (m_updating)
	{
		return;
	}
	WBQtCamera_SetPitch(m_pitch->value());
}

void WBQtCameraPanel::onDropWaypoint()
{
	WBQtCamera_DropWaypoint();
}

void WBQtCameraPanel::onCenterOnSelected()
{
	WBQtCamera_CenterOnSelected();
}

// --- Open / push / focus hooks (the Qt side of WBQtCameraBridge.h) ---------------------------

extern "C" void WBQtCamera_PushRefresh(void)
{
	// Called on EVERY camera move (handleCameraChange); stay cheap when the window is closed.
	WBQtCameraPanel *panel = WBQtCameraPanel::instance();
	if (panel != NULL && panel->isVisible())
	{
		panel->pushRefresh();
	}
}

extern "C" void WBQtCamera_Open(void *frameHwnd)
{
	if (frameHwnd == NULL)
	{
		return;
	}
	if (s_owner == NULL)
	{
		s_owner = WBQt_CreateOwnerBridgeWidget(frameHwnd);
	}
	WBQtCameraPanel *panel = WBQtCameraPanel::instance();
	if (panel == NULL)
	{
		panel = new WBQtCameraPanel(s_owner);
	}
	// Show WITHOUT activating (== the MFC SW_SHOWNA) so the viewport keeps focus.
	panel->setAttribute(Qt::WA_ShowWithoutActivating);
	panel->show();
	panel->raise();
}

