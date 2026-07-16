// WBQtBrushPanel.cpp -- see WBQtBrushPanel.h.
#include "WBQtBrushPanel.h"
#include "ui_WBQtBrushPanel.h"
#include "WBQtPanelBridge.h"

#include <QCheckBox>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>

// Cells -> feet conversions from the engine header the Qt lib must not include (see
// Common/MapObject.h): MAP_XY_FACTOR (width/feather) is 10.0; MAP_HEIGHT_SCALE (height) is
// MAP_XY_FACTOR/16 = 0.625.
static const double kFeetPerCell = 10.0;
static const double kFeetPerHeight = 10.0 / 16.0;

WBQtBrushPanel *WBQtBrushPanel::s_instance = NULL;

WBQtBrushPanel::WBQtBrushPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtBrushPanel),
	  m_updating(false)
{
	// The static widget tree lives in WBQtBrushPanel.ui. Each slider+spinbox row is
	// kept in lockstep by the slots below; the spin boxes accept typed values past the
	// slider caps (the vanilla WB pop-slider stopped at the cap, but its edit box parsed
	// any int) -- the slider just pegs.
	m_ui->setupUi(this);

	m_widthSlider = m_ui->widthSlider;
	m_widthSpin = m_ui->widthSpin;
	m_widthLabel = m_ui->widthLabel;
	m_featherSlider = m_ui->featherSlider;
	m_featherSpin = m_ui->featherSpin;
	m_featherLabel = m_ui->featherLabel;
	m_heightSlider = m_ui->heightSlider;
	m_heightSpin = m_ui->heightSpin;
	m_heightLabel = m_ui->heightLabel;
	m_mirror = m_ui->mirror;
	m_mirrorX = m_ui->mirrorX;
	m_mirrorY = m_ui->mirrorY;
	m_mirrorXY = m_ui->mirrorXY;

	// Seed from the current tool state under the guard so it doesn't echo back to the tool.
	m_updating = true;
	setRow(m_widthSlider, m_widthSpin, WBQtBrush_GetWidth());
	setRow(m_featherSlider, m_featherSpin, WBQtBrush_GetFeather());
	setRow(m_heightSlider, m_heightSpin, WBQtBrush_GetHeight());
	m_widthLabel->setText(QString::asprintf("%.1f FEET.", m_widthSpin->value() * kFeetPerCell));
	m_featherLabel->setText(QString::asprintf("%.1f FEET.", m_featherSpin->value() * kFeetPerCell));
	m_heightLabel->setText(QString::asprintf("%.1f FEET.", m_heightSpin->value() * kFeetPerHeight));
	m_mirror->setChecked(WBQtBrush_GetMirror() != 0);
	m_mirrorX->setChecked(WBQtBrush_GetMirrorX() != 0);
	m_mirrorY->setChecked(WBQtBrush_GetMirrorY() != 0);
	m_mirrorXY->setChecked(WBQtBrush_GetMirrorXY() != 0);
	m_updating = false;

	// Slider and spin drive each other (via the slots) and the tool. Checkboxes use
	// clicked() so the programmatic setChecked() during seeding doesn't fire the tool.
	connect(m_widthSlider, SIGNAL(valueChanged(int)), this, SLOT(onWidthChanged(int)));
	connect(m_widthSpin, SIGNAL(valueChanged(int)), this, SLOT(onWidthChanged(int)));
	connect(m_featherSlider, SIGNAL(valueChanged(int)), this, SLOT(onFeatherChanged(int)));
	connect(m_featherSpin, SIGNAL(valueChanged(int)), this, SLOT(onFeatherChanged(int)));
	connect(m_heightSlider, SIGNAL(valueChanged(int)), this, SLOT(onHeightChanged(int)));
	connect(m_heightSpin, SIGNAL(valueChanged(int)), this, SLOT(onHeightChanged(int)));
	connect(m_mirror, SIGNAL(clicked()), this, SLOT(onMirror()));
	connect(m_mirrorX, SIGNAL(clicked()), this, SLOT(onMirrorX()));
	connect(m_mirrorY, SIGNAL(clicked()), this, SLOT(onMirrorY()));
	connect(m_mirrorXY, SIGNAL(clicked()), this, SLOT(onMirrorXY()));

	s_instance = this;
}

WBQtBrushPanel::~WBQtBrushPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

void WBQtBrushPanel::setRow(QSlider *slider, QSpinBox *spin, int v)
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

void WBQtBrushPanel::onWidthChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_widthSlider, m_widthSpin, v);
	m_widthLabel->setText(QString::asprintf("%.1f FEET.", v * kFeetPerCell));
	WBQtBrush_SetWidth(v);	// the echo-back push is suppressed while m_updating
	m_updating = false;
}

void WBQtBrushPanel::onFeatherChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_featherSlider, m_featherSpin, v);
	m_featherLabel->setText(QString::asprintf("%.1f FEET.", v * kFeetPerCell));
	WBQtBrush_SetFeather(v);
	m_updating = false;
}

void WBQtBrushPanel::onHeightChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_heightSlider, m_heightSpin, v);
	m_heightLabel->setText(QString::asprintf("%.1f FEET.", v * kFeetPerHeight));
	WBQtBrush_SetHeight(v);
	m_updating = false;
}

void WBQtBrushPanel::onMirror()
{
	WBQtBrush_ToggleMirror();
}

void WBQtBrushPanel::onMirrorX()
{
	WBQtBrush_ToggleMirrorX();
}

void WBQtBrushPanel::onMirrorY()
{
	WBQtBrush_ToggleMirrorY();
}

void WBQtBrushPanel::onMirrorXY()
{
	WBQtBrush_ToggleMirrorXY();
}

void WBQtBrushPanel::pushWidth(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_widthSlider, m_widthSpin, v);
	m_widthLabel->setText(QString::asprintf("%.1f FEET.", v * kFeetPerCell));
	m_updating = false;
}

void WBQtBrushPanel::pushFeather(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_featherSlider, m_featherSpin, v);
	m_featherLabel->setText(QString::asprintf("%.1f FEET.", v * kFeetPerCell));
	m_updating = false;
}

void WBQtBrushPanel::pushHeight(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_heightSlider, m_heightSpin, v);
	m_heightLabel->setText(QString::asprintf("%.1f FEET.", v * kFeetPerHeight));
	m_updating = false;
}

// --- Forward push functions (tool -> widget), the Qt-side of WBQtPanelBridge.h ----------
extern "C" void WBQtBrush_PushWidth(int v)
{
	if (WBQtBrushPanel::instance() != NULL)
	{
		WBQtBrushPanel::instance()->pushWidth(v);
	}
}

extern "C" void WBQtBrush_PushFeather(int v)
{
	if (WBQtBrushPanel::instance() != NULL)
	{
		WBQtBrushPanel::instance()->pushFeather(v);
	}
}

extern "C" void WBQtBrush_PushHeight(int v)
{
	if (WBQtBrushPanel::instance() != NULL)
	{
		WBQtBrushPanel::instance()->pushHeight(v);
	}
}
