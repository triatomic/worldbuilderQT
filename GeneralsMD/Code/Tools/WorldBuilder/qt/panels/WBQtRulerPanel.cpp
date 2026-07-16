// WBQtRulerPanel.cpp -- see WBQtRulerPanel.h.
#include "WBQtRulerPanel.h"
#include "ui_WBQtRulerPanel.h"
#include "WBQtPanelBridge.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>

// Feet per meter, matching RulerOptions' 0.3048 factor. The tool stores lengths in feet;
// this is only applied when the user has picked the "meters" display unit.
static const double kFeetPerMeter = 1.0 / 0.3048;

// RulerTool::getType() returns the RulerTypeEnum value (wbview.h, which the Qt lib must not
// include): RULER_NONE = 0, RULER_LINE = 1, RULER_CIRCLE = 2. Only value 2 is a circle --
// comparing "!= 0" would wrongly treat a line ruler as a circle.
static const int kRulerTypeCircle = 2;

WBQtRulerPanel *WBQtRulerPanel::s_instance = NULL;

WBQtRulerPanel::WBQtRulerPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtRulerPanel),
	  m_updating(false)
{
	// The static widget tree lives in WBQtRulerPanel.ui; bind the members the
	// logic below uses, then wire what Designer can't express.
	m_ui->setupUi(this);

	// "Circular Measurements" (checked) vs a straight line ruler (unchecked). The checkbox
	// state always reflects the tool's real type -- see syncCircleState(), which re-reads it
	// after switchType() since the tool can refuse to flip mid-measure. The width field is
	// only meaningful for a circle ruler.
	m_circle = m_ui->circle;
	m_widthLabel = m_ui->widthLabel;
	m_width = m_ui->width;
	m_useMeters = m_ui->useMeters;
	m_showGrid = m_ui->showGrid;

	// Seed from the current tool state under the guard so it doesn't echo back to the tool.
	m_updating = true;
	m_useMeters->setChecked(WBQtRuler_GetUseMeters() != 0);
	m_showGrid->setChecked(WBQtRuler_GetShowGrid() != 0);
	syncCircleState();
	refreshWidthFromTool();
	m_updating = false;

	connect(m_circle, SIGNAL(clicked()), this, SLOT(onCircleToggled()));
	connect(m_width, SIGNAL(valueChanged(double)), this, SLOT(onWidthChanged(double)));
	connect(m_useMeters, SIGNAL(clicked()), this, SLOT(onUseMetersToggled()));
	connect(m_showGrid, SIGNAL(clicked()), this, SLOT(onShowGridToggled()));

	s_instance = this;
}

WBQtRulerPanel::~WBQtRulerPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

// Checkbox + width-field-enabled reflect the tool's current ruler type (1 == circle).
void WBQtRulerPanel::syncCircleState()
{
	bool isCircle = (WBQtRuler_GetType() == kRulerTypeCircle);
	m_circle->setChecked(isCircle);
	m_width->setEnabled(isCircle);
	m_widthLabel->setEnabled(isCircle);
}

// Show the stored (feet) length as a diameter in the currently-selected display unit.
void WBQtRulerPanel::refreshWidthFromTool()
{
	double diameterFeet = WBQtRuler_GetLengthFeet() * 2.0;
	m_width->setValue(WBQtRuler_ToDisplayUnits(diameterFeet));
}

void WBQtRulerPanel::onCircleToggled()
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	// The tool decides whether the type actually flips (it won't mid-measure); re-read it
	// rather than trusting the checkbox, exactly like the MFC OnChangeCheckRuler.
	WBQtRuler_SwitchType();
	syncCircleState();
	if (WBQtRuler_GetType() == kRulerTypeCircle)
	{
		refreshWidthFromTool();
	}
	m_updating = false;
}

void WBQtRulerPanel::onWidthChanged(double v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	// v is a diameter in the display unit. Convert meters->feet if needed, then halve to a
	// radius (world units) before storing -- the inverse of refreshWidthFromTool().
	double feet = v;
	if (WBQtRuler_GetUseMeters() != 0)
	{
		feet *= kFeetPerMeter;
	}
	WBQtRuler_SetLengthFeet(feet / 2.0);
	m_updating = false;
}

void WBQtRulerPanel::onUseMetersToggled()
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	WBQtRuler_SetUseMeters(m_useMeters->isChecked() ? 1 : 0);
	// Re-display the current length in the new unit (only matters for a circle) and repaint
	// the 3D view so the in-view length label updates right away.
	if (WBQtRuler_GetType() == kRulerTypeCircle)
	{
		refreshWidthFromTool();
	}
	m_updating = false;
	WBQtRuler_RepaintViews();
}

void WBQtRulerPanel::onShowGridToggled()
{
	if (m_updating)
	{
		return;
	}
	WBQtRuler_SetShowGrid(m_showGrid->isChecked() ? 1 : 0);
}
