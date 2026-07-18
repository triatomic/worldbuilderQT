// WBQtWaterPanel.cpp -- see WBQtWaterPanel.h.
#include "WBQtWaterPanel.h"
#include "ui_WBQtWaterPanel.h"
#include "WBQtComboStyle.h"
#include "WBQtWaterBridge.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>

WBQtWaterPanel *WBQtWaterPanel::s_instance = NULL;

WBQtWaterPanel::WBQtWaterPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtWaterPanel),
	  m_updating(false),
	  m_heightDragging(false)
{
	// The static widget tree lives in WBQtWaterPanel.ui; bind the members the
	// logic below uses, then wire what Designer can't express.
	m_ui->setupUi(this);

	m_waterPolygon = m_ui->waterPolygon;
	m_spacing = m_ui->spacing;
	m_selectionBox = m_ui->selectionBox;
	m_name = m_ui->name;
	m_heightSlider = m_ui->heightSlider;
	m_heightSpin = m_ui->heightSpin;
	m_makeRiver = m_ui->makeRiver;

	// MFC's combos are WS_VSCROLL: give every drop-down here a scrolling popup.
	WBQtComboStyle::applyPopupScrollRecursive(this);

	// Water height: slider + spinbox in lockstep. The slider spans the popup-slider range
	// (0 .. 255*MAP_HEIGHT_SCALE) -- runtime values from the bridge, so set here; the spinbox
	// range (in the .ui) is a little wider so a hand-typed value that exceeds the slider (as
	// the MFC edit control allowed) is still accepted.
	m_heightSlider->setRange(WBQtWater_GetHeightMin(), WBQtWater_GetHeightMax());

	// Seed from the current tool + selection state under the guard so nothing echoes back while
	// populating.
	pushRefresh();

	connect(m_waterPolygon, SIGNAL(clicked()), this, SLOT(onWaterPolygonToggled()));
	connect(m_spacing, SIGNAL(valueChanged(int)), this, SLOT(onSpacingChanged(int)));
	connect(m_name, SIGNAL(activated(int)), this, SLOT(onNameChanged()));
	connect(m_name->lineEdit(), SIGNAL(editingFinished()), this, SLOT(onNameChanged()));
	connect(m_heightSlider, SIGNAL(valueChanged(int)), this, SLOT(onHeightChanged(int)));
	connect(m_heightSpin, SIGNAL(valueChanged(int)), this, SLOT(onHeightChanged(int)));
	// A slider drag batches all its ticks into one undoable (== the MFC popup-slider protocol);
	// spinbox / typed changes stay one commit each.
	connect(m_heightSlider, SIGNAL(sliderPressed()), this, SLOT(onHeightSliderPressed()));
	connect(m_heightSlider, SIGNAL(sliderReleased()), this, SLOT(onHeightSliderReleased()));
	connect(m_makeRiver, SIGNAL(clicked()), this, SLOT(onMakeRiverToggled()));

	s_instance = this;
}

WBQtWaterPanel::~WBQtWaterPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

void WBQtWaterPanel::setHeightRow(int v)
{
	// Caller has set m_updating. Clamp the slider to its range (the spinbox is wider) so Qt does
	// not warn, but let the spinbox hold the real value.
	int sv = v;
	if (sv < m_heightSlider->minimum())
	{
		sv = m_heightSlider->minimum();
	}
	if (sv > m_heightSlider->maximum())
	{
		sv = m_heightSlider->maximum();
	}
	m_heightSlider->setValue(sv);
	m_heightSpin->setValue(v);
}

void WBQtWaterPanel::pushRefresh()
{
	m_updating = true;

	// Global tool state -- always valid.
	m_waterPolygon->setChecked(WBQtWater_GetCreatingWaterAreas() != 0);
	m_spacing->setValue(WBQtWater_GetSpacing());

	// Name + Make River show for ANY single selected polygon (== MFC updateTheUI); the height
	// row is water-area-only, hidden for a plain polygon you might be about to Make River.
	bool hasSel = (WBQtWater_HasSelection() != 0);
	bool isWater = hasSel && (WBQtWater_IsWaterArea() != 0);
	m_selectionBox->setVisible(hasSel);

	m_ui->heightLabel->setVisible(isWater);
	m_heightSlider->setVisible(isWater);
	m_heightSpin->setVisible(isWater);

	if (hasSel)
	{
		const int cap = 256;
		char buf[cap];
		WBQtWater_GetName(buf, cap);
		m_name->setEditText(QString::fromLatin1(buf));
		if (isWater)
		{
			setHeightRow(WBQtWater_GetHeight());
		}
		m_makeRiver->setChecked(WBQtWater_GetRiver() != 0);
	}

	m_updating = false;
}

void WBQtWaterPanel::onWaterPolygonToggled()
{
	if (m_updating)
	{
		return;
	}
	WBQtWater_SetCreatingWaterAreas(m_waterPolygon->isChecked() ? 1 : 0);
}

void WBQtWaterPanel::onSpacingChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	WBQtWater_SetSpacing(v);
}

void WBQtWaterPanel::onNameChanged()
{
	if (m_updating)
	{
		return;
	}
	QByteArray name = m_name->currentText().toLatin1();
	if (WBQtWater_SetName(name.constData()))
	{
		// A rename can re-title / re-key things; re-seed to stay in step.
		pushRefresh();
	}
	else
	{
		// Duplicate name (bridge already showed the message box) -- restore the stored name.
		m_updating = true;
		const int cap = 256;
		char buf[cap];
		WBQtWater_GetName(buf, cap);
		m_name->setEditText(QString::fromLatin1(buf));
		m_updating = false;
	}
}

void WBQtWaterPanel::onHeightChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setHeightRow(v);
	if (m_heightDragging)
	{
		// Mid-drag: reuse one MovePolygonUndoable across every tick (closed on release).
		WBQtWater_SetHeightDragStep(v);
	}
	else
	{
		// Spinbox / typed / keyboard step: one commit, like the MFC edit-box path.
		WBQtWater_SetHeight(v);
	}
	m_updating = false;
}

void WBQtWaterPanel::onHeightSliderPressed()
{
	m_heightDragging = true;
}

void WBQtWaterPanel::onHeightSliderReleased()
{
	m_heightDragging = false;
	WBQtWater_EndHeightScrub();	// == PopSliderFinished: close the drag's single undoable
}

void WBQtWaterPanel::onMakeRiverToggled()
{
	if (m_updating)
	{
		return;
	}
	WBQtWater_SetRiver(m_makeRiver->isChecked() ? 1 : 0);
}

// --- Forward push function (MFC selection -> widget), the Qt-side of WBQtWaterBridge.h -------
extern "C" void WBQtWater_PushRefresh(void)
{
	if (WBQtWaterPanel::instance() != NULL)
	{
		WBQtWaterPanel::instance()->pushRefresh();
	}
}
