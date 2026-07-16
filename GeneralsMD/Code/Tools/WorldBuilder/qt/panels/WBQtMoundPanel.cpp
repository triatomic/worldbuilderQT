// WBQtMoundPanel.cpp -- see WBQtMoundPanel.h.
#include "WBQtMoundPanel.h"
#include "ui_WBQtMoundPanel.h"
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

WBQtMoundPanel *WBQtMoundPanel::s_instance = NULL;

WBQtMoundPanel::WBQtMoundPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtMoundPanel),
	  m_updating(false)
{
	// The static widget tree lives in WBQtMoundPanel.ui; bind the members the
	// logic below uses. Ranges (set in the .ui): width = MIN/MAX_BRUSH_SIZE (1..51,
	// value x MAP_XY_FACTOR feet), feather = MIN/MAX_FEATHER (0..20), height =
	// MIN/MAX_MOUND_HEIGHT (1..21, value x MAP_HEIGHT_SCALE feet). Each spin box
	// accepts typed values past its slider cap (999/999/255) -- the vanilla WB
	// pop-slider stopped at the cap, but its edit box parsed any int; the slider
	// just pegs (see setRow).
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
	setRow(m_widthSlider, m_widthSpin, WBQtMound_GetWidth());
	setRow(m_featherSlider, m_featherSpin, WBQtMound_GetFeather());
	setRow(m_heightSlider, m_heightSpin, WBQtMound_GetHeight());
	m_widthLabel->setText(QString::asprintf("%.1f FEET.", m_widthSpin->value() * kFeetPerCell));
	m_featherLabel->setText(QString::asprintf("%.1f FEET.", m_featherSpin->value() * kFeetPerCell));
	m_heightLabel->setText(QString::asprintf("%.1f FEET.", m_heightSpin->value() * kFeetPerHeight));
	m_mirror->setChecked(WBQtMound_GetMirror() != 0);
	m_mirrorX->setChecked(WBQtMound_GetMirrorX() != 0);
	m_mirrorY->setChecked(WBQtMound_GetMirrorY() != 0);
	m_mirrorXY->setChecked(WBQtMound_GetMirrorXY() != 0);
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

WBQtMoundPanel::~WBQtMoundPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

void WBQtMoundPanel::setRow(QSlider *slider, QSpinBox *spin, int v)
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

void WBQtMoundPanel::onWidthChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_widthSlider, m_widthSpin, v);
	m_widthLabel->setText(QString::asprintf("%.1f FEET.", v * kFeetPerCell));
	WBQtMound_SetWidth(v);	// the echo-back push is suppressed while m_updating
	m_updating = false;
}

void WBQtMoundPanel::onFeatherChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_featherSlider, m_featherSpin, v);
	m_featherLabel->setText(QString::asprintf("%.1f FEET.", v * kFeetPerCell));
	WBQtMound_SetFeather(v);
	m_updating = false;
}

void WBQtMoundPanel::onHeightChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_heightSlider, m_heightSpin, v);
	m_heightLabel->setText(QString::asprintf("%.1f FEET.", v * kFeetPerHeight));
	WBQtMound_SetHeight(v);
	m_updating = false;
}

void WBQtMoundPanel::onMirror()
{
	WBQtMound_ToggleMirror();
}

void WBQtMoundPanel::onMirrorX()
{
	WBQtMound_ToggleMirrorX();
}

void WBQtMoundPanel::onMirrorY()
{
	WBQtMound_ToggleMirrorY();
}

void WBQtMoundPanel::onMirrorXY()
{
	WBQtMound_ToggleMirrorXY();
}

void WBQtMoundPanel::pushWidth(int v)
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

void WBQtMoundPanel::pushFeather(int v)
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

void WBQtMoundPanel::pushHeight(int v)
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
extern "C" void WBQtMound_PushWidth(int v)
{
	if (WBQtMoundPanel::instance() != NULL)
	{
		WBQtMoundPanel::instance()->pushWidth(v);
	}
}

extern "C" void WBQtMound_PushFeather(int v)
{
	if (WBQtMoundPanel::instance() != NULL)
	{
		WBQtMoundPanel::instance()->pushFeather(v);
	}
}

extern "C" void WBQtMound_PushHeight(int v)
{
	if (WBQtMoundPanel::instance() != NULL)
	{
		WBQtMoundPanel::instance()->pushHeight(v);
	}
}
