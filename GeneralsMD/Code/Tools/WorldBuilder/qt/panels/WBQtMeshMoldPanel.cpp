// WBQtMeshMoldPanel.cpp -- see WBQtMeshMoldPanel.h.
#include "WBQtMeshMoldPanel.h"
#include "ui_WBQtMeshMoldPanel.h"
#include "WBQtMeshMoldBridge.h"

#include <QButtonGroup>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>

// The MFC MeshMoldOptions ranges (MeshMoldOptions.h): angle -180..180 degrees, scale 1..200
// percent, height -10..256 raw slider units (the engine cells->feet MAP_HEIGHT_SCALE multiply
// is applied MFC-side, so the panel edits the raw unit exactly like the MFC popup slider).
// The ranges themselves now live on the slider/spin widgets in WBQtMeshMoldPanel.ui.

WBQtMeshMoldPanel *WBQtMeshMoldPanel::s_instance = NULL;

WBQtMeshMoldPanel::WBQtMeshMoldPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtMeshMoldPanel),
	  m_updating(false)
{
	// The static widget tree lives in WBQtMeshMoldPanel.ui; bind the members the
	// logic below uses.
	m_ui->setupUi(this);

	m_moldList = m_ui->moldList;
	m_angleSlider = m_ui->angleSlider;
	m_angleSpin = m_ui->angleSpin;
	m_scaleSlider = m_ui->scaleSlider;
	m_scaleSpin = m_ui->scaleSpin;
	m_heightSlider = m_ui->heightSlider;
	m_heightSpin = m_ui->heightSpin;
	m_preview = m_ui->preview;
	m_applyMesh = m_ui->applyMesh;
	m_raise = m_ui->raise;
	m_raiseLower = m_ui->raiseLower;
	m_lower = m_ui->lower;

	QButtonGroup *modeGroup = new QButtonGroup(this);
	modeGroup->addButton(m_raise);
	modeGroup->addButton(m_raiseLower);
	modeGroup->addButton(m_lower);

	// Seed everything under the guard so nothing echoes back to the tool while we populate.
	m_updating = true;
	rebuildMoldList();
	setRow(m_angleSlider, m_angleSpin, WBQtMeshMold_GetAngle());
	setRow(m_scaleSlider, m_scaleSpin, WBQtMeshMold_GetScalePercent());
	setRow(m_heightSlider, m_heightSpin, WBQtMeshMold_GetHeightRaw());
	m_preview->setChecked(WBQtMeshMold_GetPreview() != 0);
	switch (WBQtMeshMold_GetRaiseMode())
	{
		case 0:
			m_raise->setChecked(true);
			break;
		case 2:
			m_lower->setChecked(true);
			break;
		default:
			m_raiseLower->setChecked(true);
			break;
	}
	m_updating = false;

	// Slider and spin drive each other (via the slots) and the tool. The list selection sets
	// the mold name; the radios re-fire on any pick.
	connect(m_moldList, SIGNAL(itemSelectionChanged()), this, SLOT(onMoldSelectionChanged()));
	connect(m_angleSlider, SIGNAL(valueChanged(int)), this, SLOT(onAngleChanged(int)));
	connect(m_angleSpin, SIGNAL(valueChanged(int)), this, SLOT(onAngleChanged(int)));
	connect(m_scaleSlider, SIGNAL(valueChanged(int)), this, SLOT(onScaleChanged(int)));
	connect(m_scaleSpin, SIGNAL(valueChanged(int)), this, SLOT(onScaleChanged(int)));
	connect(m_heightSlider, SIGNAL(valueChanged(int)), this, SLOT(onHeightChanged(int)));
	connect(m_heightSpin, SIGNAL(valueChanged(int)), this, SLOT(onHeightChanged(int)));
	connect(m_preview, SIGNAL(clicked()), this, SLOT(onPreviewToggled()));
	connect(m_applyMesh, SIGNAL(clicked()), this, SLOT(onApplyMesh()));
	connect(m_raise, SIGNAL(clicked()), this, SLOT(onRaiseModeChanged()));
	connect(m_raiseLower, SIGNAL(clicked()), this, SLOT(onRaiseModeChanged()));
	connect(m_lower, SIGNAL(clicked()), this, SLOT(onRaiseModeChanged()));
	connect(m_ui->openFolderBtn, SIGNAL(clicked()), this, SLOT(onOpenMoldsFolder()));
	connect(m_ui->openLinkBtn, SIGNAL(clicked()), this, SLOT(onOpenLink()));

	s_instance = this;
}

WBQtMeshMoldPanel::~WBQtMeshMoldPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

void WBQtMeshMoldPanel::rebuildMoldList()
{
	// Caller has set m_updating (list repopulation must not fire onMoldSelectionChanged).
	m_moldList->clear();

	const int cap = 256;
	char nameBuf[cap];
	int count = WBQtMeshMold_GetCount();
	for (int i = 0; i < count; ++i)
	{
		if (WBQtMeshMold_GetName(i, nameBuf, cap))
		{
			m_moldList->addItem(QString::fromLatin1(nameBuf));
		}
	}

	// Reflect the mold the tool already has selected (the MFC panel selects the last-added
	// item on init); otherwise select the first so a name is always set.
	char selBuf[cap];
	if (WBQtMeshMold_GetSelectedName(selBuf, cap) && selBuf[0] != 0)
	{
		QList<QListWidgetItem*> hits = m_moldList->findItems(QString::fromLatin1(selBuf), Qt::MatchExactly);
		if (!hits.isEmpty())
		{
			m_moldList->setCurrentItem(hits.first());
		}
	}
	if (m_moldList->currentRow() < 0 && m_moldList->count() > 0)
	{
		m_moldList->setCurrentRow(0);
		WBQtMeshMold_SelectName(m_moldList->item(0)->text().toLatin1().constData());
	}
}

void WBQtMeshMoldPanel::setRow(QSlider *slider, QSpinBox *spin, int v)
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

void WBQtMeshMoldPanel::onMoldSelectionChanged()
{
	if (m_updating)
	{
		return;
	}
	QListWidgetItem *item = m_moldList->currentItem();
	if (item == NULL)
	{
		return;
	}
	WBQtMeshMold_SelectName(item->text().toLatin1().constData());
}

void WBQtMeshMoldPanel::onAngleChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_angleSlider, m_angleSpin, v);
	WBQtMeshMold_SetAngle(v);
	m_updating = false;
}

void WBQtMeshMoldPanel::onScaleChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_scaleSlider, m_scaleSpin, v);
	WBQtMeshMold_SetScalePercent(v);
	m_updating = false;
}

void WBQtMeshMoldPanel::onHeightChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	m_updating = true;
	setRow(m_heightSlider, m_heightSpin, v);
	WBQtMeshMold_SetHeightRaw(v);
	m_updating = false;
}

void WBQtMeshMoldPanel::onPreviewToggled()
{
	WBQtMeshMold_SetPreview(m_preview->isChecked() ? 1 : 0);
}

void WBQtMeshMoldPanel::onApplyMesh()
{
	WBQtMeshMold_ApplyMesh();
}

void WBQtMeshMoldPanel::onRaiseModeChanged()
{
	if (m_updating)
	{
		return;
	}
	int mode = 1;	// raise+lower
	if (m_raise->isChecked())
	{
		mode = 0;
	}
	else if (m_lower->isChecked())
	{
		mode = 2;
	}
	WBQtMeshMold_SetRaiseMode(mode);
}

void WBQtMeshMoldPanel::onOpenMoldsFolder()
{
	WBQtMeshMold_OpenMoldsFolder();
}

void WBQtMeshMoldPanel::onOpenLink()
{
	WBQtMeshMold_OpenLink();
}
