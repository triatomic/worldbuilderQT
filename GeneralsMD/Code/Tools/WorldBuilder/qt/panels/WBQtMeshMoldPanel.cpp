// WBQtMeshMoldPanel.cpp -- see WBQtMeshMoldPanel.h.
#include "WBQtMeshMoldPanel.h"
#include "WBQtMeshMoldBridge.h"

#include <QButtonGroup>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

// The MFC MeshMoldOptions ranges (MeshMoldOptions.h): angle -180..180 degrees, scale 1..200
// percent, height -10..256 raw slider units (the engine cells->feet MAP_HEIGHT_SCALE multiply
// is applied MFC-side, so the panel edits the raw unit exactly like the MFC popup slider).
static const int kMinAngle = -180;
static const int kMaxAngle = 180;
static const int kMinScale = 1;
static const int kMaxScale = 200;
static const int kMinHeight = -10;
static const int kMaxHeight = 256;

WBQtMeshMoldPanel *WBQtMeshMoldPanel::s_instance = NULL;

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

WBQtMeshMoldPanel::WBQtMeshMoldPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_updating(false)
{
	setWindowTitle("Mesh Mold Options");
	resize(320, 560);

	QVBoxLayout *root = new QVBoxLayout(this);

	// The mold model list (flat, mirroring the MFC CTreeCtrl of .w3d models).
	QGroupBox *moldBox = new QGroupBox("Mold Models", this);
	QVBoxLayout *moldLay = new QVBoxLayout(moldBox);
	m_moldList = new QListWidget(moldBox);
	moldLay->addWidget(m_moldList, 1);

	QHBoxLayout *moldBtnRow = new QHBoxLayout();
	QPushButton *openFolderBtn = new QPushButton("Open Molds Folder", moldBox);
	QPushButton *openLinkBtn = new QPushButton("How To Create Molds", moldBox);
	moldBtnRow->addWidget(openFolderBtn);
	moldBtnRow->addWidget(openLinkBtn);
	moldLay->addLayout(moldBtnRow);
	root->addWidget(moldBox, 1);

	// Angle / scale / height rows.
	QGroupBox *xformBox = new QGroupBox("Transform", this);
	QVBoxLayout *xformLay = new QVBoxLayout(xformBox);
	makeRow(this, "Angle:", kMinAngle, kMaxAngle, &m_angleSlider, &m_angleSpin, xformLay);
	makeRow(this, "Scale %:", kMinScale, kMaxScale, &m_scaleSlider, &m_scaleSpin, xformLay);
	makeRow(this, "Height:", kMinHeight, kMaxHeight, &m_heightSlider, &m_heightSpin, xformLay);
	root->addWidget(xformBox);

	// Preview toggle + Apply.
	QHBoxLayout *actionRow = new QHBoxLayout();
	m_preview = new QPushButton("Preview", this);
	m_preview->setCheckable(true);
	m_applyMesh = new QPushButton("Apply Mesh", this);
	actionRow->addWidget(m_preview);
	actionRow->addWidget(m_applyMesh);
	root->addLayout(actionRow);

	// Raise / Raise+Lower / Lower radios.
	QGroupBox *modeBox = new QGroupBox("Mesh Mode", this);
	QVBoxLayout *modeLay = new QVBoxLayout(modeBox);
	m_raise = new QRadioButton("Raise only", modeBox);
	m_raiseLower = new QRadioButton("Raise and lower", modeBox);
	m_lower = new QRadioButton("Lower only", modeBox);
	modeLay->addWidget(m_raise);
	modeLay->addWidget(m_raiseLower);
	modeLay->addWidget(m_lower);
	QButtonGroup *modeGroup = new QButtonGroup(this);
	modeGroup->addButton(m_raise);
	modeGroup->addButton(m_raiseLower);
	modeGroup->addButton(m_lower);
	root->addWidget(modeBox);

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
	connect(openFolderBtn, SIGNAL(clicked()), this, SLOT(onOpenMoldsFolder()));
	connect(openLinkBtn, SIGNAL(clicked()), this, SLOT(onOpenLink()));

	s_instance = this;
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
