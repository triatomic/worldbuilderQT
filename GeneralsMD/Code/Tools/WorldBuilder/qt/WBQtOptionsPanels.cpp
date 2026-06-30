// WBQtOptionsPanels.cpp -- the generic Qt host for migrated option panels.
//
// Implements WBQt_ShowOptionsPanel / WBQt_HideOptionsPanel (called from
// CMainFrame::showOptionsDialog). A single cached QWinWidget rooted in the MFC frame owns
// every migrated panel (each a top-level Qt::Tool window floating over the frame). The
// dialogID->panel registry below is the ONE place new panels are wired in (Phase 3 has
// only Feather); everything else in showOptionsDialog stays generic.
//
// Qt + Win32 only (no afx). resource.h is pure #defines (Qt-safe) -- it gives the dialog
// IDs without coupling to MFC.
#include "WBQtPanelBridge.h"
#include "qwinwidget.h"
#include "panels/WBQtFeatherPanel.h"
#include "resource.h"

#include <QApplication>
#include <QWidget>

#include <qt_windows.h>

static QWinWidget *g_panelOwner = NULL;	// invisible owner bridge, rooted in the MFC frame
static QWidget    *g_currentPanel = NULL;
static int         g_currentDialogID = 0;

// Map a dialog ID to its (lazily created, cached) Qt panel, or NULL if not migrated.
static QWidget *wbQtPanelFor(int dialogID, QWidget *owner)
{
	static WBQtFeatherPanel *featherPanel = NULL;

	switch (dialogID)
	{
		case IDD_FEATHER_OPTIONS:
			if (featherPanel == NULL)
			{
				featherPanel = new WBQtFeatherPanel(owner);
			}
			return featherPanel;

		default:
			return NULL;
	}
}

extern "C" int WBQt_ShowOptionsPanel(void *frameHwnd, int dialogID, int x, int y, int w, int h)
{
	if (qApp == NULL || frameHwnd == NULL)
	{
		return 0;
	}

	if (g_panelOwner == NULL)
	{
		g_panelOwner = new QWinWidget(reinterpret_cast<HWND>(frameHwnd));
	}

	QWidget *panel = wbQtPanelFor(dialogID, g_panelOwner);
	if (panel == NULL)
	{
		return 0;	// not a migrated panel -- let the caller show the MFC dialog
	}

	// Show WITHOUT stealing activation from the 3D viewport, matching the MFC panels'
	// ShowWindow(SW_SHOWNA). Otherwise the viewport goes to the background and its brush
	// cursor / feedback doesn't refresh on a tool switch until the viewport is clicked.
	panel->setAttribute(Qt::WA_ShowWithoutActivating, true);

	// Already showing this panel? Do nothing -- re-positioning on every tool re-activation
	// (e.g. clicking the 3D viewport) would fight the user's drag. Matches the MFC
	// showOptionsDialog "dialogID == m_curDialogID && visible -> return" early-out.
	if (panel == g_currentPanel && panel->isVisible())
	{
		return 1;
	}

	if (g_currentPanel != NULL && g_currentPanel != panel)
	{
		g_currentPanel->hide();
	}

	// Position via Qt setGeometry in SCREEN coords (this is a top-level Qt::Tool window),
	// NOT Win32 SetWindowPos -- see the Phase 2 viewport-host lesson.
	panel->setGeometry(x, y, w, h);
	panel->show();
	panel->raise();

	g_currentPanel = panel;
	g_currentDialogID = dialogID;
	return 1;
}

extern "C" void WBQt_HideOptionsPanel(void)
{
	if (g_currentPanel != NULL)
	{
		g_currentPanel->hide();
		g_currentPanel = NULL;
		g_currentDialogID = 0;
	}
}
