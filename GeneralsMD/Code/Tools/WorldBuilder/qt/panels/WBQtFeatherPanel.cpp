// WBQtFeatherPanel.cpp -- see WBQtFeatherPanel.h.
#include "WBQtFeatherPanel.h"
#include "WBQtPanelBridge.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

// MAP_XY_FACTOR (cells -> feet) is 10.0. It lives in an engine header the Qt lib must not
// include, so mirror the constant here (see Common/MapObject.h).
static const double kFeetPerCell = 10.0;

WBQtFeatherPanel *WBQtFeatherPanel::s_instance = NULL;

namespace
{
	// Build one labelled "slider + spinbox" row (kept in lockstep by the owner's slots).
	void makeRow(QWidget *parent, const char *caption, int lo, int hi,
		QSlider **outSlider, QSpinBox **outSpin, QBoxLayout *into)
	{
		QHBoxLayout *row = new QHBoxLayout();
		row->addWidget(new QLabel(QString::fromLatin1(caption), parent));
		QSlider *s = new QSlider(Qt::Horizontal, parent);
		s->setRange(lo, hi);
		QSpinBox *b = new QSpinBox(parent);
		b->setRange(lo, hi);
		row->addWidget(s, 1);
		row->addWidget(b);
		into->addLayout(row);
		*outSlider = s;
		*outSpin = b;
	}
}

WBQtFeatherPanel::WBQtFeatherPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_updating(false)
{
	setWindowTitle("Feather Brush Options");

	QVBoxLayout *root = new QVBoxLayout(this);

	// Brush width (the editable "feather/size" row) with a live "FEET" label.
	QGroupBox *sizeBox = new QGroupBox("Brush width", this);
	QVBoxLayout *sizeLay = new QVBoxLayout(sizeBox);
	makeRow(this, "Size in cells:", 2, 51, &m_featherSlider, &m_featherSpin, sizeLay);
	m_feetLabel = new QLabel("0.0 FEET.", sizeBox);
	sizeLay->addWidget(m_feetLabel);
	root->addWidget(sizeBox);

	// Filter radius.
	QGroupBox *radiusBox = new QGroupBox("Filter Radius", this);
	QVBoxLayout *radiusLay = new QVBoxLayout(radiusBox);
	makeRow(this, "Radius:", 1, 5, &m_radiusSlider, &m_radiusSpin, radiusLay);
	radiusLay->addWidget(new QLabel("A large value tends to flatten more of the terrain.", radiusBox));
	root->addWidget(radiusBox);

	// Feather rate.
	QGroupBox *rateBox = new QGroupBox("Feather Rate", this);
	QVBoxLayout *rateLay = new QVBoxLayout(rateBox);
	makeRow(this, "Rate:", 1, 10, &m_rateSlider, &m_rateSpin, rateLay);
	rateLay->addWidget(new QLabel("A high value flattens faster.", rateBox));
	root->addWidget(rateBox);

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

void WBQtFeatherPanel::setRow(QSlider *slider, QSpinBox *spin, int v)
{
	// Caller has set m_updating. Clamp to range to avoid Qt warnings.
	if (v < slider->minimum())
	{
		v = slider->minimum();
	}
	if (v > slider->maximum())
	{
		v = slider->maximum();
	}
	slider->setValue(v);
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
