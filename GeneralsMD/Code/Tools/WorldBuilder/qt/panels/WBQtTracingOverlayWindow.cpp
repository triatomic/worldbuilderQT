// WBQtTracingOverlayWindow.cpp -- see WBQtTracingOverlayWindow.h.
#include "WBQtTracingOverlayWindow.h"
#include "ui_WBQtTracingOverlayWindow.h"
#include "WBQtTracingOverlayBridge.h"
#include "WBQtWindowPos.h"
#include "qwinwidget.h"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QSlider>

#include <qt_windows.h>

WBQtTracingOverlayWindow *WBQtTracingOverlayWindow::s_instance = NULL;

// Defined in WBQtBridge.cpp: the main window when inverted, else an invisible
// QWinWidget bridge rooted in the MFC frame. Never hide() the result.
QWidget *WBQt_CreateOwnerBridgeWidget(void *frameHwnd);

namespace
{
	// The window's owner (created on first open, like the other Qt tool windows).
	QWidget *s_ownerBridge = NULL;
}

WBQtTracingOverlayWindow::WBQtTracingOverlayWindow(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtTracingOverlayWindow),
	  m_updating(false)
{
	// The static widget tree lives in WBQtTracingOverlayWindow.ui; bind the members the
	// logic below uses, then wire what Designer can't express.
	m_ui->setupUi(this);
	WBQtWindowPos_Track(this, "TracingOverlay");

	m_opacitySlider = m_ui->opacitySlider;
	m_opacityLabel = m_ui->opacityLabel;
	m_filterCombo = m_ui->filterCombo;

	// The item ORDER is load-bearing: currentIndex() is passed straight through as the
	// bridge filter value, so keep the enum mapping visible here.
	m_filterCombo->addItem("Default");		// FILTER_DEFAULT (linear)
	m_filterCombo->addItem("Nearest");		// FILTER_NEAREST (point sampling)

	connect(m_opacitySlider, SIGNAL(valueChanged(int)), this, SLOT(onOpacityChanged(int)));
	connect(m_opacitySlider, SIGNAL(sliderReleased()), this, SLOT(onOpacityReleased()));
	connect(m_filterCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onFilterChanged(int)));
	connect(m_ui->okBtn, SIGNAL(clicked()), this, SLOT(onOkClicked()));

	s_instance = this;
}

WBQtTracingOverlayWindow::~WBQtTracingOverlayWindow()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

void WBQtTracingOverlayWindow::seedFromSettings()
{
	m_updating = true;
	int pct = WBQtTracingOverlayData_GetOpacityPct();
	m_opacitySlider->setValue(pct);
	updateOpacityLabel(pct);
	m_filterCombo->setCurrentIndex(WBQtTracingOverlayData_GetFilter());
	m_updating = false;
}

void WBQtTracingOverlayWindow::updateOpacityLabel(int pct)
{
	m_opacityLabel->setText(QString("%1%").arg(pct));
}

// Slider moved (drag tick or keyboard nudge): update the readout and apply the new
// opacity to the overlay live, but do NOT persist -- valueChanged fires on every drag
// tick, and writing the profile each tick hammers the INI (== the MFC OnHScroll note).
void WBQtTracingOverlayWindow::onOpacityChanged(int value)
{
	updateOpacityLabel(value);
	if (m_updating)
	{
		return;
	}
	WBQtTracingOverlayData_SetFromUi(value, m_filterCombo->currentIndex(), 0);
}

// Drag ended (mouse released) -- persist the final value once.
void WBQtTracingOverlayWindow::onOpacityReleased()
{
	if (m_updating)
	{
		return;
	}
	WBQtTracingOverlayData_SetFromUi(m_opacitySlider->value(), m_filterCombo->currentIndex(), 1);
}

// Combo change is a discrete event -- apply AND persist (DrawObject re-decodes the
// overlay with the new D3DX filter, so it updates on the fly).
void WBQtTracingOverlayWindow::onFilterChanged(int index)
{
	if (m_updating)
	{
		return;
	}
	WBQtTracingOverlayData_SetFromUi(m_opacitySlider->value(), index, 1);
}

void WBQtTracingOverlayWindow::commitCurrent()
{
	WBQtTracingOverlayData_SetFromUi(m_opacitySlider->value(), m_filterCombo->currentIndex(), 1);
}

// OK just hides the cached window; everything was applied live already. Persist once,
// like the MFC OnOK/OnDestroy pair.
void WBQtTracingOverlayWindow::onOkClicked()
{
	commitCurrent();
	hide();
}

void WBQtTracingOverlayWindow::closeEvent(QCloseEvent *event)
{
	// The window is a cached singleton: closing (X) hides it, keeping the instance.
	commitCurrent();
	hide();
	event->ignore();
}

void WBQtTracingOverlayWindow::keyPressEvent(QKeyEvent *event)
{
	// Esc closes too (== the MFC OnCancel), keeping whatever was applied live.
	if (event->key() == Qt::Key_Escape)
	{
		commitCurrent();
		hide();
		return;
	}
	QWidget::keyPressEvent(event);
}

// --- The C entry points (Qt side of WBQtTracingOverlayBridge.h) -------------------------

extern "C" int WBQtTracingOverlay_Open(void *frameHwnd)
{
	if (qApp == NULL || frameHwnd == NULL)
	{
		return 0;	// Qt not up -- caller falls back to the MFC dialog
	}
	if (s_ownerBridge == NULL)
	{
		s_ownerBridge = WBQt_CreateOwnerBridgeWidget(frameHwnd);
	}
	WBQtTracingOverlayWindow *w = WBQtTracingOverlayWindow::instance();
	if (w == NULL)
	{
		w = new WBQtTracingOverlayWindow(s_ownerBridge);
	}
	w->seedFromSettings();
	// The MFC dialog opened activated (SW_SHOW / SetForegroundWindow on raise) -- it is
	// a direct response to the user's menu pick, so activating is expected.
	w->show();
	w->raise();
	w->activateWindow();
	return 1;
}

extern "C" void WBQtTracingOverlay_Close(void)
{
	WBQtTracingOverlayWindow *w = WBQtTracingOverlayWindow::instance();
	if (w != NULL && w->isVisible())
	{
		w->commitCurrent();
		w->hide();
	}
}
