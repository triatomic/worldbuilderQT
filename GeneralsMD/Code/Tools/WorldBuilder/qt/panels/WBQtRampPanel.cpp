// WBQtRampPanel.cpp -- see WBQtRampPanel.h.
#include "WBQtRampPanel.h"
#include "WBQtPanelBridge.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QVBoxLayout>

WBQtRampPanel *WBQtRampPanel::s_instance = NULL;

WBQtRampPanel::WBQtRampPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_updating(false)
{
	setWindowTitle("Ramp Options");

	QVBoxLayout *root = new QVBoxLayout(this);

	// Ramp width + the Apply button. RampTool draws feedback from the width live and applies
	// the ramp when the latch is set (Apply); pressing Apply after dragging out the ramp line
	// commits it, matching the MFC dialog's Apply button.
	QGroupBox *rampBox = new QGroupBox("Ramp", this);
	QVBoxLayout *rampLay = new QVBoxLayout(rampBox);

	QHBoxLayout *widthRow = new QHBoxLayout();
	widthRow->addWidget(new QLabel("Width:", rampBox));
	m_width = new QDoubleSpinBox(rampBox);
	m_width->setDecimals(2);
	m_width->setRange(0.0, 1000000.0);
	widthRow->addWidget(m_width, 1);
	rampLay->addLayout(widthRow);

	QPushButton *apply = new QPushButton("Apply", rampBox);
	rampLay->addWidget(apply);
	root->addWidget(rampBox);

	// Advanced mirror options.
	QGroupBox *mirrorBox = new QGroupBox("Advanced Mirror Options", this);
	QVBoxLayout *mirrorLay = new QVBoxLayout(mirrorBox);
	m_mirror = new QCheckBox("Toggle", mirrorBox);
	m_mirrorX = new QCheckBox("Mirror X", mirrorBox);
	m_mirrorY = new QCheckBox("Mirror Y", mirrorBox);
	m_mirrorXY = new QCheckBox("Diagonal", mirrorBox);
	mirrorLay->addWidget(m_mirror);
	mirrorLay->addWidget(m_mirrorX);
	mirrorLay->addWidget(m_mirrorY);
	mirrorLay->addWidget(m_mirrorXY);
	root->addWidget(mirrorBox);

	root->addStretch(1);

	// Seed from the current tool/panel state under the guard so it doesn't echo back.
	m_updating = true;
	m_width->setValue(WBQtRamp_GetWidth());
	m_mirror->setChecked(WBQtRamp_GetMirror() != 0);
	m_mirrorX->setChecked(WBQtRamp_GetMirrorX() != 0);
	m_mirrorY->setChecked(WBQtRamp_GetMirrorY() != 0);
	m_mirrorXY->setChecked(WBQtRamp_GetMirrorXY() != 0);
	m_updating = false;

	connect(m_width, SIGNAL(valueChanged(double)), this, SLOT(onWidthChanged(double)));
	connect(apply, SIGNAL(clicked()), this, SLOT(onApply()));
	connect(m_mirror, SIGNAL(clicked()), this, SLOT(onMirror()));
	connect(m_mirrorX, SIGNAL(clicked()), this, SLOT(onMirrorX()));
	connect(m_mirrorY, SIGNAL(clicked()), this, SLOT(onMirrorY()));
	connect(m_mirrorXY, SIGNAL(clicked()), this, SLOT(onMirrorXY()));

	s_instance = this;
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
