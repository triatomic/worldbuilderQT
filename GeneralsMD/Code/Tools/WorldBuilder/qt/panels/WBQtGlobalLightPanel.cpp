// WBQtGlobalLightPanel.cpp -- see WBQtGlobalLightPanel.h.
#include "WBQtGlobalLightPanel.h"
#include "ui_WBQtGlobalLightPanel.h"
#include "WBQtGlobalLightBridge.h"
#include "WBQtScrubSpinBox.h"
#include "WBQtWindowPos.h"
#include "qwinwidget.h"

#include <QColor>
#include <QColorDialog>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>

#include <qt_windows.h>

WBQtGlobalLightPanel *WBQtGlobalLightPanel::s_instance = NULL;

// Defined in WBQtBridge.cpp: the main window when inverted, else an invisible
// QWinWidget bridge rooted in the MFC frame. Never hide() the result.
QWidget *WBQt_CreateOwnerBridgeWidget(void *frameHwnd);

namespace
{
	// The window's owner (created on first open, like the panel host).
	QWidget *s_owner = NULL;
}

WBQtGlobalLightPanel::WBQtGlobalLightPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtGlobalLightPanel),
	  m_updating(false)
{
	// The static widget tree lives in WBQtGlobalLightPanel.ui (Ambient | Sun | Accent 1 |
	// Accent 2 side by side, matching the MFC row); bind the members the logic below uses,
	// then wire what Designer can't express.
	m_ui->setupUi(this);
	WBQtWindowPos_Track(this, "GlobalLight");

	// Ambient (Sun ambient color: swatch + RGB).
	m_ambientSwatch = m_ui->ambientSwatch;
	m_ambR = m_ui->ambR;
	m_ambG = m_ui->ambG;
	m_ambB = m_ui->ambB;

	// Sun / Accent 1 / Accent 2: azimuth + elevation scrub-spinboxes and diffuse RGB.
	m_azimuth[0] = m_ui->azimuth0;
	m_azimuth[1] = m_ui->azimuth1;
	m_azimuth[2] = m_ui->azimuth2;
	m_elevation[0] = m_ui->elevation0;
	m_elevation[1] = m_ui->elevation1;
	m_elevation[2] = m_ui->elevation2;
	m_diffR[0] = m_ui->diffR0;
	m_diffR[1] = m_ui->diffR1;
	m_diffR[2] = m_ui->diffR2;
	m_diffG[0] = m_ui->diffG0;
	m_diffG[1] = m_ui->diffG1;
	m_diffG[2] = m_ui->diffG2;
	m_diffB[0] = m_ui->diffB0;
	m_diffB[1] = m_ui->diffB1;
	m_diffB[2] = m_ui->diffB2;

	m_radioTerrain = m_ui->radioTerrain;
	m_radioObjects = m_ui->radioObjects;
	m_radioEverything = m_ui->radioEverything;

	m_xyzLabel = m_ui->xyzLabel;
	m_todLabel = m_ui->todLabel;

	for (int light = 0; light < 3; ++light)
	{
		m_elevation[light]->setAxisVertical(true);	// promotion can't pass the ctor arg
	}

	seedFromGlobals();

	connect(m_ui->resetBtn, SIGNAL(clicked()), this, SLOT(onReset()));
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

WBQtGlobalLightPanel::~WBQtGlobalLightPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
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
