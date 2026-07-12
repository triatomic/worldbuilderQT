// WBQtBrushPanel.cpp -- see WBQtBrushPanel.h.
#include "WBQtBrushPanel.h"
#include "WBQtPanelBridge.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

// Cells -> feet conversions from the engine header the Qt lib must not include (see
// Common/MapObject.h): MAP_XY_FACTOR (width/feather) is 10.0; MAP_HEIGHT_SCALE (height) is
// MAP_XY_FACTOR/16 = 0.625.
static const double kFeetPerCell = 10.0;
static const double kFeetPerHeight = 10.0 / 16.0;

WBQtBrushPanel *WBQtBrushPanel::s_instance = NULL;

namespace
{
	// Build one labelled "slider + spinbox" row (kept in lockstep by the owner's slots).
	// typedHi: the spin box accepts typed values past the slider cap (the vanilla WB
	// pop-slider stopped at hi, but its edit box parsed any int); the slider just pegs.
	void makeRow(QWidget *parent, const char *caption, int lo, int hi, int typedHi,
		QSlider **outSlider, QSpinBox **outSpin, QBoxLayout *into)
	{
		QHBoxLayout *row = new QHBoxLayout();
		row->addWidget(new QLabel(QString::fromLatin1(caption), parent));
		QSlider *s = new QSlider(Qt::Horizontal, parent);
		s->setRange(lo, hi);
		QSpinBox *b = new QSpinBox(parent);
		b->setRange(lo, (typedHi > hi) ? typedHi : hi);
		row->addWidget(s, 1);
		row->addWidget(b);
		into->addLayout(row);
		*outSlider = s;
		*outSpin = b;
	}
}

WBQtBrushPanel::WBQtBrushPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_updating(false)
{
	setWindowTitle("Height Brush Options");

	QVBoxLayout *root = new QVBoxLayout(this);

	// Brush width, with a live "FEET" label (value x MAP_XY_FACTOR).
	QGroupBox *widthBox = new QGroupBox("Brush Width", this);
	QVBoxLayout *widthLay = new QVBoxLayout(widthBox);
	makeRow(this, "Size in cells:", 1, 30, 999, &m_widthSlider, &m_widthSpin, widthLay);
	m_widthLabel = new QLabel("0.0 FEET.", widthBox);
	widthLay->addWidget(m_widthLabel);
	root->addWidget(widthBox);

	// Feather width (value x MAP_XY_FACTOR).
	QGroupBox *featherBox = new QGroupBox("Feather Width", this);
	QVBoxLayout *featherLay = new QVBoxLayout(featherBox);
	makeRow(this, "Size in cells:", 0, 20, 999, &m_featherSlider, &m_featherSpin, featherLay);
	m_featherLabel = new QLabel("0.0 FEET.", featherBox);
	featherLay->addWidget(m_featherLabel);
	root->addWidget(featherBox);

	// Brush height (value x MAP_HEIGHT_SCALE).
	QGroupBox *heightBox = new QGroupBox("Brush Height", this);
	QVBoxLayout *heightLay = new QVBoxLayout(heightBox);
	makeRow(this, "Height:", 0, 255, 255, &m_heightSlider, &m_heightSpin, heightLay);
	m_heightLabel = new QLabel("0.0 FEET.", heightBox);
	heightLay->addWidget(m_heightLabel);
	root->addWidget(heightBox);

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
