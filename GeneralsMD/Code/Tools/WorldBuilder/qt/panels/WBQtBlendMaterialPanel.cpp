// WBQtBlendMaterialPanel.cpp -- see WBQtBlendMaterialPanel.h.
#include "WBQtBlendMaterialPanel.h"
#include "ui_WBQtBlendMaterialPanel.h"
#include "WBQtBlendMaterialBridge.h"

#include <QCheckBox>

WBQtBlendMaterialPanel *WBQtBlendMaterialPanel::s_instance = NULL;

WBQtBlendMaterialPanel::WBQtBlendMaterialPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtBlendMaterialPanel),
	  m_updating(false)
{
	// The static widget tree lives in WBQtBlendMaterialPanel.ui; bind the members the
	// logic below uses, then wire what Designer can't express.
	m_ui->setupUi(this);

	// The two "add one tile between gaps" checkboxes (IDC_HVGAP / IDC_DGAP) and the
	// re-revalidate-blends checkbox (IDC_REVALIDATEBLENDS).
	m_hvGap = m_ui->hvGap;
	m_dGap = m_ui->dGap;
	m_revalBlends = m_ui->revalBlends;
	// The four mirror toggles (IDC_TOGGLE_MIRROR/X/Y/XY). These drive AutoEdgeOutTool's
	// mirror statics, the same tool the Feather panel's mirror row drives.
	m_mirror = m_ui->mirror;
	m_mirrorX = m_ui->mirrorX;
	m_mirrorY = m_ui->mirrorY;
	m_mirrorXY = m_ui->mirrorXY;

	// Seed from the current tool state under the guard so it doesn't echo back to the tool.
	m_updating = true;
	m_hvGap->setChecked(WBQtBlendMaterial_GetHorizVertGap() != 0);
	m_dGap->setChecked(WBQtBlendMaterial_GetDiagGap() != 0);
	m_revalBlends->setChecked(WBQtBlendMaterial_GetRevalBlends() != 0);
	m_mirror->setChecked(WBQtBlendMaterial_GetMirror() != 0);
	m_mirrorX->setChecked(WBQtBlendMaterial_GetMirrorX() != 0);
	m_mirrorY->setChecked(WBQtBlendMaterial_GetMirrorY() != 0);
	m_mirrorXY->setChecked(WBQtBlendMaterial_GetMirrorXY() != 0);
	m_updating = false;

	// Checkboxes use clicked() so the programmatic setChecked() during seeding doesn't fire
	// the tool.
	connect(m_hvGap, SIGNAL(clicked()), this, SLOT(onHorizVertGapToggled()));
	connect(m_dGap, SIGNAL(clicked()), this, SLOT(onDiagGapToggled()));
	connect(m_revalBlends, SIGNAL(clicked()), this, SLOT(onRevalBlendsToggled()));
	connect(m_mirror, SIGNAL(clicked()), this, SLOT(onMirror()));
	connect(m_mirrorX, SIGNAL(clicked()), this, SLOT(onMirrorX()));
	connect(m_mirrorY, SIGNAL(clicked()), this, SLOT(onMirrorY()));
	connect(m_mirrorXY, SIGNAL(clicked()), this, SLOT(onMirrorXY()));

	s_instance = this;
}

WBQtBlendMaterialPanel::~WBQtBlendMaterialPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

// The three gap checkboxes push their explicit state (the setters also refresh the tooltip).
void WBQtBlendMaterialPanel::onHorizVertGapToggled()
{
	if (m_updating)
	{
		return;
	}
	WBQtBlendMaterial_SetHorizVertGap(m_hvGap->isChecked() ? 1 : 0);
}

void WBQtBlendMaterialPanel::onDiagGapToggled()
{
	if (m_updating)
	{
		return;
	}
	WBQtBlendMaterial_SetDiagGap(m_dGap->isChecked() ? 1 : 0);
}

void WBQtBlendMaterialPanel::onRevalBlendsToggled()
{
	if (m_updating)
	{
		return;
	}
	WBQtBlendMaterial_SetRevalBlends(m_revalBlends->isChecked() ? 1 : 0);
}

// The four mirror checkboxes toggle the tool statics, matching the MFC OnToggleMirror*.
void WBQtBlendMaterialPanel::onMirror()
{
	WBQtBlendMaterial_ToggleMirror();
}

void WBQtBlendMaterialPanel::onMirrorX()
{
	WBQtBlendMaterial_ToggleMirrorX();
}

void WBQtBlendMaterialPanel::onMirrorY()
{
	WBQtBlendMaterial_ToggleMirrorY();
}

void WBQtBlendMaterialPanel::onMirrorXY()
{
	WBQtBlendMaterial_ToggleMirrorXY();
}
