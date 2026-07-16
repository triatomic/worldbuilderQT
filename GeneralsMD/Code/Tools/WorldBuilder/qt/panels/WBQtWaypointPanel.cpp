// WBQtWaypointPanel.cpp -- see WBQtWaypointPanel.h.
#include "WBQtWaypointPanel.h"
#include "ui_WBQtWaypointPanel.h"
#include "WBQtWaypointBridge.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLineEdit>

WBQtWaypointPanel *WBQtWaypointPanel::s_instance = NULL;

WBQtWaypointPanel::WBQtWaypointPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtWaypointPanel),
	  m_updating(false)
{
	// The static widget tree lives in WBQtWaypointPanel.ui: the dual-purpose name combo
	// (editable, like the MFC CBS_DROPDOWN; its preset list is repopulated per selection
	// kind in pushRefresh()), the waypoint location X/Y pair, the path labels +
	// bi-directional block, and the read-only helper text the MFC dialog shows for a
	// waypoint (and hides for a trigger). Bind the members the logic below uses.
	m_ui->setupUi(this);

	m_nameBox = m_ui->nameBox;
	m_name = m_ui->name;
	m_locationBox = m_ui->locationBox;
	m_locX = m_ui->locX;
	m_locY = m_ui->locY;
	m_labelsBox = m_ui->labelsBox;
	m_label1 = m_ui->label1;
	m_label2 = m_ui->label2;
	m_label3 = m_ui->label3;
	m_biDirectional = m_ui->biDirectional;
	m_helpBox = m_ui->helpBox;

	// Seed from the current selection under the guard so nothing echoes back while populating.
	pushRefresh();

	connect(m_name, SIGNAL(activated(int)), this, SLOT(onNameChanged()));
	connect(m_name->lineEdit(), SIGNAL(editingFinished()), this, SLOT(onNameChanged()));
	connect(m_locX, SIGNAL(valueChanged(double)), this, SLOT(onLocationXChanged(double)));
	connect(m_locY, SIGNAL(valueChanged(double)), this, SLOT(onLocationYChanged(double)));
	connect(m_label1, SIGNAL(editingFinished()), this, SLOT(onLabel1Changed()));
	connect(m_label2, SIGNAL(editingFinished()), this, SLOT(onLabel2Changed()));
	connect(m_label3, SIGNAL(editingFinished()), this, SLOT(onLabel3Changed()));
	connect(m_biDirectional, SIGNAL(clicked()), this, SLOT(onBiDirectionalToggled()));

	s_instance = this;
}

WBQtWaypointPanel::~WBQtWaypointPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

// Repopulate the name combo's preset drop-down for the current selection kind. Caller has set
// m_updating so this does not fire onNameChanged.
void WBQtWaypointPanel::rebuildNamePresets(int kind)
{
	// Preserve the editable text (the current name) across the repopulation.
	QString current = m_name->currentText();
	m_name->clear();

	const int cap = 256;
	char nameBuf[cap];
	int count = WBQtWaypoint_GetNamePresetCount();
	for (int i = 0; i < count; ++i)
	{
		if (WBQtWaypoint_GetNamePreset(i, nameBuf, cap))
		{
			m_name->addItem(QString::fromLatin1(nameBuf));
		}
	}
	m_name->setEditText(current);
}

void WBQtWaypointPanel::pushRefresh()
{
	m_updating = true;

	int kind = WBQtWaypoint_GetKind();

	const int cap = 256;
	char buf[cap];

	// Name field: always shown; drop-down presets depend on the kind.
	WBQtWaypoint_GetName(buf, cap);
	rebuildNamePresets(kind);
	m_name->setEditText(QString::fromLatin1(buf));
	m_name->setEnabled(kind != WBQT_WAYPOINT_KIND_NONE);
	m_nameBox->setTitle((kind == WBQT_WAYPOINT_KIND_TRIGGER) ? "Area Name" : "Waypoint Name");

	bool isWaypoint = (kind == WBQT_WAYPOINT_KIND_WAYPOINT);
	bool isLinked = isWaypoint && (WBQtWaypoint_IsLinked() != 0);

	// Location: waypoints only.
	m_locationBox->setVisible(isWaypoint);
	if (isWaypoint)
	{
		m_locX->setValue(WBQtWaypoint_GetLocationX());
		m_locY->setValue(WBQtWaypoint_GetLocationY());
	}

	// Path labels + bi-directional: waypoints, and only when linked (mirrors updateTheUI).
	m_labelsBox->setVisible(isWaypoint);
	m_label1->setVisible(isLinked);
	m_label2->setVisible(isLinked);
	m_label3->setVisible(isLinked);
	m_biDirectional->setVisible(isWaypoint);
	if (isLinked)
	{
		WBQtWaypoint_GetLabel(1, buf, cap);
		m_label1->setText(QString::fromLatin1(buf));
		WBQtWaypoint_GetLabel(2, buf, cap);
		m_label2->setText(QString::fromLatin1(buf));
		WBQtWaypoint_GetLabel(3, buf, cap);
		m_label3->setText(QString::fromLatin1(buf));
		m_biDirectional->setChecked(WBQtWaypoint_GetBiDirectional() != 0);
	}
	m_biDirectional->setEnabled(isLinked);

	// The read-only helper text is a waypoint-side reference (hidden for a trigger area).
	m_helpBox->setVisible(isWaypoint);

	m_updating = false;
}

void WBQtWaypointPanel::onNameChanged()
{
	if (m_updating)
	{
		return;
	}
	QByteArray name = m_name->currentText().toLatin1();
	if (WBQtWaypoint_SetName(name.constData()))
	{
		// A rename can toggle link state / re-title the box; re-seed to stay in step.
		pushRefresh();
	}
	else
	{
		// Duplicate name (bridge already showed the message box) -- restore the stored name.
		m_updating = true;
		const int cap = 256;
		char buf[cap];
		WBQtWaypoint_GetName(buf, cap);
		m_name->setEditText(QString::fromLatin1(buf));
		m_updating = false;
	}
}

void WBQtWaypointPanel::onLocationXChanged(double v)
{
	if (m_updating)
	{
		return;
	}
	WBQtWaypoint_SetLocationX(v);
}

void WBQtWaypointPanel::onLocationYChanged(double v)
{
	if (m_updating)
	{
		return;
	}
	WBQtWaypoint_SetLocationY(v);
}

void WBQtWaypointPanel::applyLabel(int labelIndex, QLineEdit *edit)
{
	if (m_updating)
	{
		return;
	}
	QByteArray text = edit->text().toLatin1();
	WBQtWaypoint_SetLabel(labelIndex, text.constData());
}

void WBQtWaypointPanel::onLabel1Changed()
{
	applyLabel(1, m_label1);
}

void WBQtWaypointPanel::onLabel2Changed()
{
	applyLabel(2, m_label2);
}

void WBQtWaypointPanel::onLabel3Changed()
{
	applyLabel(3, m_label3);
}

void WBQtWaypointPanel::onBiDirectionalToggled()
{
	if (m_updating)
	{
		return;
	}
	WBQtWaypoint_SetBiDirectional(m_biDirectional->isChecked() ? 1 : 0);
}

// --- Forward push function (MFC selection -> widget), the Qt-side of WBQtWaypointBridge.h ----
extern "C" void WBQtWaypoint_PushRefresh(void)
{
	if (WBQtWaypointPanel::instance() != NULL)
	{
		WBQtWaypointPanel::instance()->pushRefresh();
	}
}
