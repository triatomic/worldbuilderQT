// WBQtRampPanel.cpp -- see WBQtRampPanel.h.
#include "WBQtRampPanel.h"
#include "ui_WBQtRampPanel.h"
#include "WBQtPanelBridge.h"

#include <QCheckBox>
#include <QPushButton>
#include <QDoubleSpinBox>

WBQtRampPanel *WBQtRampPanel::s_instance = NULL;

WBQtRampPanel::WBQtRampPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtRampPanel),
	  m_updating(false)
{
	// The static widget tree lives in WBQtRampPanel.ui; bind the members the
	// logic below uses, then wire what Designer can't express.
	m_ui->setupUi(this);

	m_width = m_ui->width;
	m_mirror = m_ui->mirror;
	m_mirrorX = m_ui->mirrorX;
	m_mirrorY = m_ui->mirrorY;
	m_mirrorXY = m_ui->mirrorXY;

	// Seed from the current tool/panel state under the guard so it doesn't echo back.
	m_updating = true;
	m_width->setValue(WBQtRamp_GetWidth());
	m_mirror->setChecked(WBQtRamp_GetMirror() != 0);
	m_mirrorX->setChecked(WBQtRamp_GetMirrorX() != 0);
	m_mirrorY->setChecked(WBQtRamp_GetMirrorY() != 0);
	m_mirrorXY->setChecked(WBQtRamp_GetMirrorXY() != 0);
	m_updating = false;

	// RampTool draws feedback from the width live and applies the ramp when the latch is set
	// (Apply); pressing Apply after dragging out the ramp line commits it, matching the MFC
	// dialog's Apply button.
	connect(m_width, SIGNAL(valueChanged(double)), this, SLOT(onWidthChanged(double)));
	connect(m_ui->applyBtn, SIGNAL(clicked()), this, SLOT(onApply()));
	connect(m_mirror, SIGNAL(clicked()), this, SLOT(onMirror()));
	connect(m_mirrorX, SIGNAL(clicked()), this, SLOT(onMirrorX()));
	connect(m_mirrorY, SIGNAL(clicked()), this, SLOT(onMirrorY()));
	connect(m_mirrorXY, SIGNAL(clicked()), this, SLOT(onMirrorXY()));

	s_instance = this;
}

WBQtRampPanel::~WBQtRampPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

void WBQtRampPanel::onWidthChanged(double v)
{
	if (m_updating)
	{
		return;
	}
	WBQtRamp_SetWidth(v);
}

void WBQtRampPanel::onApply()
{
	WBQtRamp_Apply();
}

void WBQtRampPanel::onMirror()
{
	WBQtRamp_ToggleMirror();
}

void WBQtRampPanel::onMirrorX()
{
	WBQtRamp_ToggleMirrorX();
}

void WBQtRampPanel::onMirrorY()
{
	WBQtRamp_ToggleMirrorY();
}

void WBQtRampPanel::onMirrorXY()
{
	WBQtRamp_ToggleMirrorXY();
}
