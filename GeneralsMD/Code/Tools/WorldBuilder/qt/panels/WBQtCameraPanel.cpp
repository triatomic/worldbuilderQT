// WBQtCameraPanel.cpp -- see WBQtCameraPanel.h.
#include "WBQtCameraPanel.h"
#include "WBQtCameraBridge.h"
#include "WBQtScrubSpinBox.h"
#include "WBQtWindowPos.h"
#include "qwinwidget.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

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
	  m_updating(false)
{
	setWindowTitle("Camera Options");
	WBQtWindowPos_Track(this, "CameraOptions");

	QVBoxLayout *root = new QVBoxLayout(this);

	// Top row: reset + the pitch group (like the MFC layout).
	QHBoxLayout *topRow = new QHBoxLayout();
	QPushButton *resetBtn = new QPushButton("Restore To Default", this);
	topRow->addWidget(resetBtn);
	topRow->addStretch(1);
	QGroupBox *pitchBox = new QGroupBox("Pitch", this);
	QHBoxLayout *pitchLay = new QHBoxLayout(pitchBox);
	m_pitch = new WBQtScrubSpinBox(pitchBox, /*axisVertical=*/true);
	m_pitch->setDecimals(2);
	m_pitch->setRange(0.0, 1.0);
	m_pitch->setSingleStep(0.01);
	m_pitch->setToolTip("Camera pitch (drag up/down to scrub)");
	pitchLay->addWidget(m_pitch);
	topRow->addWidget(pitchBox);
	root->addLayout(topRow);

	// Read-only camera info.
	QGridLayout *infoGrid = new QGridLayout();
	infoGrid->addWidget(new QLabel("Height above ground:", this), 0, 0);
	m_heightText = new QLabel(this);
	infoGrid->addWidget(m_heightText, 0, 1);
	infoGrid->addWidget(new QLabel("Zoom:", this), 1, 0);
	m_zoomText = new QLabel(this);
	infoGrid->addWidget(m_zoomText, 1, 1);
	infoGrid->addWidget(new QLabel("(1.0 == max, 0.0 == min)", this), 1, 2);
	infoGrid->addWidget(new QLabel("Position:", this), 2, 0);
	m_posText = new QLabel(this);
	infoGrid->addWidget(m_posText, 2, 1, 1, 2);
	infoGrid->addWidget(new QLabel("Target:", this), 3, 0);
	m_targetText = new QLabel(this);
	infoGrid->addWidget(m_targetText, 3, 1, 1, 2);
	infoGrid->setColumnStretch(2, 1);
	root->addLayout(infoGrid);

	QPushButton *dropBtn = new QPushButton("Drop Waypoint", this);
	root->addWidget(dropBtn);
	QPushButton *centerBtn = new QPushButton("Center Camera On Selected Object", this);
	root->addWidget(centerBtn);
	root->addStretch(1);

	pushRefresh();

	connect(resetBtn, SIGNAL(clicked()), this, SLOT(onReset()));
	connect(m_pitch, SIGNAL(valueChanged(double)), this, SLOT(onPitchChanged()));
	connect(dropBtn, SIGNAL(clicked()), this, SLOT(onDropWaypoint()));
	connect(centerBtn, SIGNAL(clicked()), this, SLOT(onCenterOnSelected()));

	s_instance = this;
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

