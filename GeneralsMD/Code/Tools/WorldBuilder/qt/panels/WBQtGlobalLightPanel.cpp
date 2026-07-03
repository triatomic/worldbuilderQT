// WBQtGlobalLightPanel.cpp -- see WBQtGlobalLightPanel.h.
#include "WBQtGlobalLightPanel.h"
#include "WBQtGlobalLightBridge.h"
#include "WBQtScrubSpinBox.h"
#include "WBQtWindowPos.h"
#include "qwinwidget.h"

#include <QColor>
#include <QColorDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <qt_windows.h>

WBQtGlobalLightPanel *WBQtGlobalLightPanel::s_instance = NULL;

// Defined in WBQtBridge.cpp: the main window when inverted, else an invisible
// QWinWidget bridge rooted in the MFC frame. Never hide() the result.
QWidget *WBQt_CreateOwnerBridgeWidget(void *frameHwnd);

namespace
{
	// The window's owner (created on first open, like the panel host).
	QWidget *s_owner = NULL;

	QSpinBox *makeColorSpin(QWidget *parent)
	{
		QSpinBox *s = new QSpinBox(parent);
		s->setRange(0, 255);
		return s;
	}
}

WBQtGlobalLightPanel::WBQtGlobalLightPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_updating(false)
{
	setWindowTitle("Global Light Options");
	WBQtWindowPos_Track(this, "GlobalLight");

	QVBoxLayout *root = new QVBoxLayout(this);

	QPushButton *resetBtn = new QPushButton("Restore To Default", this);
	QHBoxLayout *resetRow = new QHBoxLayout();
	resetRow->addWidget(resetBtn);
	resetRow->addStretch(1);
	root->addLayout(resetRow);

	// The four groups side by side: Ambient | Sun | Accent 1 | Accent 2 (matching the MFC row).
	QHBoxLayout *groupsRow = new QHBoxLayout();

	// Ambient (Sun ambient color: swatch + RGB).
	QGroupBox *ambientBox = new QGroupBox("Ambient", this);
	QGridLayout *ambGrid = new QGridLayout(ambientBox);
	m_ambientSwatch = new QPushButton(ambientBox);
	m_ambientSwatch->setFixedSize(28, 20);
	ambGrid->addWidget(m_ambientSwatch, 0, 0, 1, 2);
	ambGrid->addWidget(new QLabel("R:", ambientBox), 1, 0);
	m_ambR = makeColorSpin(ambientBox);
	ambGrid->addWidget(m_ambR, 1, 1);
	ambGrid->addWidget(new QLabel("G:", ambientBox), 2, 0);
	m_ambG = makeColorSpin(ambientBox);
	ambGrid->addWidget(m_ambG, 2, 1);
	ambGrid->addWidget(new QLabel("B:", ambientBox), 3, 0);
	m_ambB = makeColorSpin(ambientBox);
	ambGrid->addWidget(m_ambB, 3, 1);
	ambGrid->setRowStretch(4, 1);
	groupsRow->addWidget(ambientBox);

	// Sun / Accent 1 / Accent 2: azimuth + elevation scrub-spinboxes and diffuse RGB.
	static const char *kGroupNames[3] = { "Sun", "Accent 1", "Accent 2" };
	for (int light = 0; light < 3; ++light)
	{
		QGroupBox *box = new QGroupBox(QString::fromLatin1(kGroupNames[light]), this);
		QGridLayout *grid = new QGridLayout(box);

		grid->addWidget(new QLabel("Az:", box), 0, 0);
		m_azimuth[light] = new WBQtScrubSpinBox(box);
		m_azimuth[light]->setDecimals(0);
		m_azimuth[light]->setRange(0.0, 359.0);
		m_azimuth[light]->setSingleStep(1.0);
		m_azimuth[light]->setWrapping(true);
		m_azimuth[light]->setToolTip("Azimuth (drag left/right to scrub)");
		grid->addWidget(m_azimuth[light], 0, 1);

		grid->addWidget(new QLabel("El:", box), 1, 0);
		m_elevation[light] = new WBQtScrubSpinBox(box, /*axisVertical=*/true);
		m_elevation[light]->setDecimals(0);
		m_elevation[light]->setRange(0.0, 90.0);
		m_elevation[light]->setSingleStep(1.0);
		m_elevation[light]->setToolTip("Elevation (drag up/down to scrub)");
		grid->addWidget(m_elevation[light], 1, 1);

		grid->addWidget(new QLabel("R:", box), 2, 0);
		m_diffR[light] = makeColorSpin(box);
		grid->addWidget(m_diffR[light], 2, 1);
		grid->addWidget(new QLabel("G:", box), 3, 0);
		m_diffG[light] = makeColorSpin(box);
		grid->addWidget(m_diffG[light], 3, 1);
		grid->addWidget(new QLabel("B:", box), 4, 0);
		m_diffB[light] = makeColorSpin(box);
		grid->addWidget(m_diffB[light], 4, 1);
		grid->setRowStretch(5, 1);
		groupsRow->addWidget(box);
	}
	root->addLayout(groupsRow);

	m_xyzLabel = new QLabel("XYZ:", this);
	root->addWidget(m_xyzLabel);

	QGroupBox *applyBox = new QGroupBox("Lighting applies to:", this);
	QVBoxLayout *applyLay = new QVBoxLayout(applyBox);
	m_radioTerrain = new QRadioButton("Terrain (ground, roads, etc.)", applyBox);
	m_radioObjects = new QRadioButton("Objects (buildings, tanks etc.)", applyBox);
	m_radioEverything = new QRadioButton("Everything (Terrain && Objects).", applyBox);
	applyLay->addWidget(m_radioTerrain);
	applyLay->addWidget(m_radioObjects);
	applyLay->addWidget(m_radioEverything);
	root->addWidget(applyBox);

	m_todLabel = new QLabel("Time of day: Morning.", this);
	root->addWidget(m_todLabel);
	root->addStretch(1);

	seedFromGlobals();

	connect(resetBtn, SIGNAL(clicked()), this, SLOT(onReset()));
	connect(m_radioTerrain, SIGNAL(clicked()), this, SLOT(onLightingRadio()));
	connect(m_radioObjects, SIGNAL(clicked()), this, SLOT(onLightingRadio()));
	connect(m_radioEverything, SIGNAL(clicked()), this, SLOT(onLightingRadio()));
	connect(m_ambientSwatch, SIGNAL(clicked()), this, SLOT(onAmbientSwatch()));
	connect(m_ambR, SIGNAL(valueChanged(int)), this, SLOT(onAmbientChanged()));
	connect(m_ambG, SIGNAL(valueChanged(int)), this, SLOT(onAmbientChanged()));
	connect(m_ambB, SIGNAL(valueChanged(int)), this, SLOT(onAmbientChanged()));
	for (int light = 0; light < 3; ++light)
	{
		connect(m_azimuth[light], SIGNAL(valueChanged(double)), this, SLOT(onAngleChanged()));
		connect(m_elevation[light], SIGNAL(valueChanged(double)), this, SLOT(onAngleChanged()));
		connect(m_diffR[light], SIGNAL(valueChanged(int)), this, SLOT(onDiffuseChanged()));
		connect(m_diffG[light], SIGNAL(valueChanged(int)), this, SLOT(onDiffuseChanged()));
		connect(m_diffB[light], SIGNAL(valueChanged(int)), this, SLOT(onDiffuseChanged()));
	}

	s_instance = this;
}

void WBQtGlobalLightPanel::seedFromGlobals()
{
	m_updating = true;

	int mode = WBQtGlobalLight_GetLighting();
	m_radioTerrain->setChecked(mode == 1);
	m_radioObjects->setChecked(mode == 2);
	m_radioEverything->setChecked(mode == 3);

	for (int light = 0; light < 3; ++light)
	{
		int az = 0, el = 0;
		WBQtGlobalLight_GetAngles(light, &az, &el);
		m_azimuth[light]->setValue((double)az);
		m_elevation[light]->setValue((double)el);

		int r = 0, g = 0, b = 0;
		WBQtGlobalLight_GetDiffuse(light, &r, &g, &b);
		m_diffR[light]->setValue(r);
		m_diffG[light]->setValue(g);
		m_diffB[light]->setValue(b);
	}

	int ar = 0, ag = 0, ab = 0;
	WBQtGlobalLight_GetAmbient(&ar, &ag, &ab);
	m_ambR->setValue(ar);
	m_ambG->setValue(ag);
	m_ambB->setValue(ab);
	updateSwatch();

	const int cap = 64;
	char buf[cap];
	WBQtGlobalLight_GetTimeOfDayName(buf, cap);
	m_todLabel->setText(QString("Time of day: %1.").arg(QString::fromLatin1(buf)));

	updateXYZLabel(0);

	m_updating = false;
}

void WBQtGlobalLightPanel::updateXYZLabel(int light)
{
	float x = 0.0f, y = 0.0f, z = 0.0f;
	WBQtGlobalLight_GetLightPos(light, &x, &y, &z);
	m_xyzLabel->setText(QString("XYZ: %1, %2, %3")
		.arg(x, 0, 'f', 2).arg(y, 0, 'f', 2).arg(z, 0, 'f', 2));
}

void WBQtGlobalLightPanel::updateSwatch()
{
	QColor c(m_ambR->value(), m_ambG->value(), m_ambB->value());
	m_ambientSwatch->setStyleSheet(
		QString("background-color: %1; border: 1px solid palette(mid);").arg(c.name()));
}

int WBQtGlobalLightPanel::senderLight(QObject *src) const
{
	for (int light = 0; light < 3; ++light)
	{
		if (src == m_azimuth[light] || src == m_elevation[light] ||
				src == m_diffR[light] || src == m_diffG[light] || src == m_diffB[light])
		{
			return light;
		}
	}
	return 0;
}

void WBQtGlobalLightPanel::showEvent(QShowEvent *event)
{
	// Mirrors the MFC OnShowWindow(TRUE): re-read the globals into the hidden dialog, then seed.
	WBQtGlobalLight_SyncFromGlobals();
	seedFromGlobals();
	QWidget::showEvent(event);
}

void WBQtGlobalLightPanel::hideEvent(QHideEvent *event)
{
	// Mirrors the MFC OnClose/OnShowWindow(FALSE): drop the in-view light-direction arrows.
	WBQtGlobalLight_FeedbackOff();
	QWidget::hideEvent(event);
}

void WBQtGlobalLightPanel::onReset()
{
	WBQtGlobalLight_ResetLights();
	seedFromGlobals();
}

void WBQtGlobalLightPanel::onLightingRadio()
{
	if (m_updating)
	{
		return;
	}
	int mode = 3;
	if (m_radioTerrain->isChecked())
	{
		mode = 1;
	}
	else if (m_radioObjects->isChecked())
	{
		mode = 2;
	}
	WBQtGlobalLight_SetLighting(mode);
	// The mode switch re-reads a different lighting table; re-seed all fields from it.
	seedFromGlobals();
}

void WBQtGlobalLightPanel::onAngleChanged()
{
	if (m_updating)
	{
		return;
	}
	int light = senderLight(sender());
	WBQtGlobalLight_SetAngles(light,
		(int)m_azimuth[light]->value(), (int)m_elevation[light]->value());
	updateXYZLabel(light);
}

void WBQtGlobalLightPanel::onAmbientChanged()
{
	if (m_updating)
	{
		return;
	}
	WBQtGlobalLight_SetAmbient(m_ambR->value(), m_ambG->value(), m_ambB->value());
	updateSwatch();
}

void WBQtGlobalLightPanel::onDiffuseChanged()
{
	if (m_updating)
	{
		return;
	}
	int light = senderLight(sender());
	WBQtGlobalLight_SetDiffuse(light,
		m_diffR[light]->value(), m_diffG[light]->value(), m_diffB[light]->value());
}

void WBQtGlobalLightPanel::onAmbientSwatch()
{
	QColor initial(m_ambR->value(), m_ambG->value(), m_ambB->value());
	QColor chosen = QColorDialog::getColor(initial, this, "Ambient Color");
	if (!chosen.isValid())
	{
		return;
	}
	m_updating = true;
	m_ambR->setValue(chosen.red());
	m_ambG->setValue(chosen.green());
	m_ambB->setValue(chosen.blue());
	m_updating = false;
	WBQtGlobalLight_SetAmbient(chosen.red(), chosen.green(), chosen.blue());
	updateSwatch();
}

// --- Open / focus hooks (the Qt side of WBQtGlobalLightBridge.h) ------------------------------

extern "C" void WBQtGlobalLight_Open(void *frameHwnd)
{
	if (frameHwnd == NULL)
	{
		return;
	}
	if (s_owner == NULL)
	{
		s_owner = WBQt_CreateOwnerBridgeWidget(frameHwnd);
	}
	WBQtGlobalLightPanel *panel = WBQtGlobalLightPanel::instance();
	if (panel == NULL)
	{
		panel = new WBQtGlobalLightPanel(s_owner);
	}
	// Show WITHOUT activating (== the MFC SW_SHOWNA) so the viewport keeps focus.
	panel->setAttribute(Qt::WA_ShowWithoutActivating);
	panel->show();
	panel->raise();
}
