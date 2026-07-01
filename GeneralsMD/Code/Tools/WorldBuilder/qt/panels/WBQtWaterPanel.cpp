// WBQtWaterPanel.cpp -- see WBQtWaterPanel.h.
#include "WBQtWaterPanel.h"
#include "WBQtWaterBridge.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

WBQtWaterPanel *WBQtWaterPanel::s_instance = NULL;

WBQtWaterPanel::WBQtWaterPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_updating(false)
{
	setWindowTitle("Water Options");
	resize(320, 300);

	QVBoxLayout *root = new QVBoxLayout(this);

	// Global tool state: the "Water Polygon" toggle (flood-fill water areas vs single points)
	// and the point spacing used when placing points. These are meaningful with or without a
	// selection, so they live outside the selection group.
	QGroupBox *toolBox = new QGroupBox("Tool", this);
	QVBoxLayout *toolLay = new QVBoxLayout(toolBox);
	m_waterPolygon = new QCheckBox("Water Polygon", toolBox);
	toolLay->addWidget(m_waterPolygon);

	QHBoxLayout *spacingRow = new QHBoxLayout();
	spacingRow->addWidget(new QLabel("Point Spacing:", toolBox));
	m_spacing = new QSpinBox(toolBox);
	m_spacing->setRange(0, 1000000);
	spacingRow->addWidget(m_spacing, 1);
	toolLay->addLayout(spacingRow);
	root->addWidget(toolBox);

	// Selected water-area trigger: its name, water height, and the Make River toggle. Shown only
	// when a single water-area PolygonTrigger is selected (mirrors updateTheUI enabling these).
	m_selectionBox = new QGroupBox("Selected Water Area", this);
	QVBoxLayout *selLay = new QVBoxLayout(m_selectionBox);

	// Name (editable, like the MFC CBS_DROPDOWN combo).
	QHBoxLayout *nameRow = new QHBoxLayout();
	nameRow->addWidget(new QLabel("Name:", m_selectionBox));
	m_name = new QComboBox(m_selectionBox);
	m_name->setEditable(true);
	m_name->setInsertPolicy(QComboBox::NoInsert);
	nameRow->addWidget(m_name, 1);
	selLay->addLayout(nameRow);

	// Water height: slider + spinbox in lockstep. The slider spans the popup-slider range
	// (0 .. 255*MAP_HEIGHT_SCALE); the spinbox range is a little wider so a hand-typed value that
	// exceeds the slider (as the MFC edit control allowed) is still accepted.
	QHBoxLayout *heightRow = new QHBoxLayout();
	heightRow->addWidget(new QLabel("Height:", m_selectionBox));
	m_heightSlider = new QSlider(Qt::Horizontal, m_selectionBox);
	m_heightSlider->setRange(WBQtWater_GetHeightMin(), WBQtWater_GetHeightMax());
	m_heightSpin = new QSpinBox(m_selectionBox);
	m_heightSpin->setRange(0, 65535);
	heightRow->addWidget(m_heightSlider, 1);
	heightRow->addWidget(m_heightSpin);
	selLay->addLayout(heightRow);

	m_makeRiver = new QCheckBox("Make River", m_selectionBox);
	selLay->addWidget(m_makeRiver);

	root->addWidget(m_selectionBox);
	root->addStretch(1);

	// Seed from the current tool + selection state under the guard so nothing echoes back while
	// populating.
	pushRefresh();

	connect(m_waterPolygon, SIGNAL(clicked()), this, SLOT(onWaterPolygonToggled()));
	connect(m_spacing, SIGNAL(valueChanged(int)), this, SLOT(onSpacingChanged(int)));
	connect(m_name, SIGNAL(activated(int)), this, SLOT(onNameChanged()));
	connect(m_name->lineEdit(), SIGNAL(editingFinished()), this, SLOT(onNameChanged()));
	connect(m_heightSlider, SIGNAL(valueChanged(int)), this, SLOT(onHeightChanged(int)));
	connect(m_heightSpin, SIGNAL(valueChanged(int)), this, SLOT(onHeightChanged(int)));
	connect(m_makeRiver, SIGNAL(clicked()), this, SLOT(onMakeRiverToggled()));

	s_instance = this;
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

	bool hasSel = (WBQtWater_HasSelection() != 0);
	m_selectionBox->setVisible(hasSel);

	if (hasSel)
	{
		const int cap = 256;
		char buf[cap];
		WBQtWater_GetName(buf, cap);
		m_name->setEditText(QString::fromLatin1(buf));
		setHeightRow(WBQtWater_GetHeight());
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
	WBQtWater_SetHeight(v);	// drives the MovePolygonUndoable on the selected water-area trigger
	m_updating = false;
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
