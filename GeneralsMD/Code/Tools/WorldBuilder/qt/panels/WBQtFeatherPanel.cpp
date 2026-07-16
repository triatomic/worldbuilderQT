// WBQtFeatherPanel.cpp -- see WBQtFeatherPanel.h.
#include "WBQtFeatherPanel.h"
#include "ui_WBQtFeatherPanel.h"
#include "WBQtPanelBridge.h"

#include <QCheckBox>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>

// MAP_XY_FACTOR (cells -> feet) is 10.0. It lives in an engine header the Qt lib must not
// include, so mirror the constant here (see Common/MapObject.h).
static const double kFeetPerCell = 10.0;

WBQtFeatherPanel *WBQtFeatherPanel::s_instance = NULL;

WBQtFeatherPanel::WBQtFeatherPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtFeatherPanel),
	  m_updating(false)
{
	// The static widget tree lives in WBQtFeatherPanel.ui. Each slider+spinbox row is
	// kept in lockstep by the slots below; the spin boxes accept typed values past the
	// slider caps (the vanilla WB pop-slider stopped at the cap, but its edit box parsed
	// any int) -- the slider just pegs.
	m_ui->setupUi(this);

	m_featherSlider = m_ui->featherSlider;
	m_featherSpin = m_ui->featherSpin;
	m_feetLabel = m_ui->feetLabel;
	m_radiusSlider = m_ui->radiusSlider;
	m_radiusSpin = m_ui->radiusSpin;
	m_rateSlider = m_ui->rateSlider;
	m_rateSpin = m_ui->rateSpin;
	m_mirror = m_ui->mirror;
	m_mirrorX = m_ui->mirrorX;
	m_mirrorY = m_ui->mirrorY;
	m_mirrorXY = m_ui->mirrorXY;

	// Seed from the current tool state under the guard so it doesn't echo back to the tool.
	m_updating = true;
	setRow(m_featherSlider, m_featherSpin, WBQtFeather_GetFeather());
	setRow(m_radiusSlider, m_radiusSpin, WBQtFeather_GetRadius());
	setRow(m_rateSlider, m_rateSpin, WBQtFeather_GetRate());
	m_feetLabel->setText(QString::asprintf("%.1f FEET.", m_featherSpin->value() * kFeetPerCell));
	m_mirror->setChecked(WBQtFeather_GetMirror() != 0);
	m_mirrorX->setChecked(WBQtFeather_GetMirrorX() != 0);
	m_mirrorY->setChecked(WBQtFeather_GetMirrorY() != 0);
	m_mirrorXY->setChecked(WBQtFeather_GetMirrorXY() != 0);
	m_updating = false;

	// Slider and spin drive each other (via the slots) and the tool. Checkboxes use
	// clicked() so the programmatic setChecked() during seeding doesn't fire the tool.
	connect(m_featherSlider, SIGNAL(valueChanged(int)), this, SLOT(onFeatherChanged(int)));
	connect(m_featherSpin, SIGNAL(valueChanged(int)), this, SLOT(onFeatherChanged(int)));
	connect(m_radiusSlider, SIGNAL(valueChanged(int)), this, SLOT(onRadiusChanged(int)));
	connect(m_radiusSpin, SIGNAL(valueChanged(int)), this, SLOT(onRadiusChanged(int)));
	connect(m_rateSlider, SIGNAL(valueChanged(int)), this, SLOT(onRateChanged(int)));
	connect(m_rateSpin, SIGNAL(valueChanged(int)), this, SLOT(onRateChanged(int)));
	connect(m_mirror, SIGNAL(clicked()), this, SLOT(onMirror()));
	connect(m_mirrorX, SIGNAL(clicked()), this, SLOT(onMirrorX()));
	connect(m_mirrorY, SIGNAL(clicked()), this, SLOT(onMirrorY()));
	connect(m_mirrorXY, SIGNAL(clicked()), this, SLOT(onMirrorXY()));

	s_instance = this;
}

WBQtFeatherPanel::~WBQtFeatherPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

void WBQtFeatherPanel::setRow(QSlider *slider, QSpinBox *spin, int v)
{
	// Caller has set m_updating. Only the slider position is clamped -- the spin box
	// keeps the full typed value (its own wider range clamps it), like the vanilla
	// edit box next to the capped pop-slider.
	int sliderV = v;
	if (sliderV < slider->minimum())
	{
		sliderV = slider->minimum();
	}
	if (sliderV > slider->maximum())
	{
		sliderV = slider->maximum();
	}
	slider->setValue(sliderV);
	spin->setValue(v);
}

void WBQtFeatherPanel::onFeatherChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_featherSlider, m_featherSpin, v);
	m_feetLabel->setText(QString::asprintf("%.1f FEET.", v * kFeetPerCell));
	WBQtFeather_SetFeather(v);	// the echo-back push is suppressed while m_updating
	m_updating = false;
}

void WBQtFeatherPanel::onRadiusChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_radiusSlider, m_radiusSpin, v);
	WBQtFeather_SetRadius(v);
	m_updating = false;
}

void WBQtFeatherPanel::onRateChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_rateSlider, m_rateSpin, v);
	WBQtFeather_SetRate(v);
	m_updating = false;
}

void WBQtFeatherPanel::onMirror()
{
	WBQtFeather_ToggleMirror();
}

void WBQtFeatherPanel::onMirrorX()
{
	WBQtFeather_ToggleMirrorX();
}

void WBQtFeatherPanel::onMirrorY()
{
	WBQtFeather_ToggleMirrorY();
}

void WBQtFeatherPanel::onMirrorXY()
{
	WBQtFeather_ToggleMirrorXY();
}

void WBQtFeatherPanel::pushFeather(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_featherSlider, m_featherSpin, v);
	m_feetLabel->setText(QString::asprintf("%.1f FEET.", v * kFeetPerCell));
	m_updating = false;
}

void WBQtFeatherPanel::pushRadius(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_radiusSlider, m_radiusSpin, v);
	m_updating = false;
}

void WBQtFeatherPanel::pushRate(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_rateSlider, m_rateSpin, v);
	m_updating = false;
}

// --- Forward push functions (tool -> widget), the Qt-side of WBQtPanelBridge.h ----------
extern "C" void WBQtFeather_PushFeather(int v)
{
	if (WBQtFeatherPanel::instance() != NULL)
	{
		WBQtFeatherPanel::instance()->pushFeather(v);
	}
}

extern "C" void WBQtFeather_PushRadius(int v)
{
	if (WBQtFeatherPanel::instance() != NULL)
	{
		WBQtFeatherPanel::instance()->pushRadius(v);
	}
}

extern "C" void WBQtFeather_PushRate(int v)
{
	if (WBQtFeatherPanel::instance() != NULL)
	{
		WBQtFeatherPanel::instance()->pushRate(v);
	}
}
