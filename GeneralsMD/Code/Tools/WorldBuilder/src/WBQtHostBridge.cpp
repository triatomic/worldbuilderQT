// WBQtHostBridge.cpp -- the MFC side of the Phase 2 viewport-host seam.
//
// This is a plain MFC translation unit (no Qt include). It implements the reverse
// callback that the Qt viewport host (qt/WBQtBridge.cpp) fires on resize, so the D3D
// device tracks the on-screen host size. It lives on the MFC side because
// reset3dEngineDisplaySize is engine/MFC-facing; the Qt static lib resolves this symbol
// against the exe at the final link (extern "C" keeps the name stable).
//
// The whole body is guarded by RTS_HAS_QT, so in the default (Qt-off) build this
// compiles to an empty object and the MFC build is unchanged.
#include "StdAfx.h"
#include "MainFrm.h"
#include "WorldBuilderDoc.h"
#include "wbview3d.h"
#include "OptionsPanel.h"
#include "qt/WBQtBridge.h"
#include "qt/WBQtPanelBridge.h"

#ifdef RTS_HAS_QT
extern "C" void WBQt_OnViewportHostResized(int width, int height)
{
	if (width <= 0 || height <= 0)
	{
		return;
	}

	// HARD GUARD: a windowed D3D8 backbuffer larger than the display mode can never
	// Present (D3DERR_DEVICELOST every frame -> an endless Reset_Device loop that
	// reads as a frozen white viewport, and a crash if WB is closed mid-loop). The
	// MFC world could not hit this (the frame's outer rect was WM-clamped, so the
	// view client always fit); the Qt window's client CAN exceed the screen, so cap
	// the device size here no matter what size the pane reports.
	{
		int maxWidth = ::GetSystemMetrics(SM_CXSCREEN);
		int maxHeight = ::GetSystemMetrics(SM_CYSCREEN);
		if (maxWidth > 0 && width > maxWidth)
		{
			width = maxWidth;
		}
		if (maxHeight > 0 && height > maxHeight)
		{
			height = maxHeight;
		}
	}

	WbView3d *p3d = CWorldBuilderDoc::GetActive3DView();
	if (p3d != NULL)
	{
		// Idempotent: reset3dEngineDisplaySize early-outs when the size is unchanged and
		// no-ops until the device is inited, so an early/duplicate call is harmless.
		p3d->reset3dEngineDisplaySize(width, height);
	}
}

// Tier 5: persist the shared option-panel position exactly like COptionsPanel::OnMove,
// so a dragged Qt panel's Top/Left survives a restart (showOptionsDialog seeds the Qt
// panels from these keys).
extern "C" void WBQtPanels_SaveWindowPos(int top, int left)
{
	::AfxGetApp()->WriteProfileInt(OPTIONS_PANEL_SECTION, "Top", top);
	::AfxGetApp()->WriteProfileInt(OPTIONS_PANEL_SECTION, "Left", left);
}

// Generic per-window position store for the modeless Qt tool windows (Global Light,
// Camera, Layers, Minimap, Script editor, Tracing Overlay). Keyed by a stable name in a
// dedicated [QtWindowPositions] section of the same WorldBuilder.ini every other window
// uses. Modal dialogs are not tracked, so they keep centering fresh. The -32000 sentinel
// (== "never saved", matching the MFC minimap convention) means "no stored position".
#define WB_QT_WINDOW_POS_SECTION "QtWindowPositions"

extern "C" int WBQtWindowPos_Get(const char *name, int *topOut, int *leftOut)
{
	if (name == NULL)
	{
		return 0;
	}
	CString topKey;
	CString leftKey;
	topKey.Format("%s_Top", name);
	leftKey.Format("%s_Left", name);
	int top = ::AfxGetApp()->GetProfileInt(WB_QT_WINDOW_POS_SECTION, topKey, -32000);
	int left = ::AfxGetApp()->GetProfileInt(WB_QT_WINDOW_POS_SECTION, leftKey, -32000);
	if (top == -32000 || left == -32000)
	{
		return 0;
	}
	if (topOut != NULL)
	{
		*topOut = top;
	}
	if (leftOut != NULL)
	{
		*leftOut = left;
	}
	return 1;
}

extern "C" void WBQtWindowPos_Save(const char *name, int top, int left)
{
	if (name == NULL)
	{
		return;
	}
	CString topKey;
	CString leftKey;
	topKey.Format("%s_Top", name);
	leftKey.Format("%s_Left", name);
	::AfxGetApp()->WriteProfileInt(WB_QT_WINDOW_POS_SECTION, topKey, top);
	::AfxGetApp()->WriteProfileInt(WB_QT_WINDOW_POS_SECTION, leftKey, left);
}

// Tier 5: follow a live Windows light/dark switch. The Settings app broadcasts
// WM_SETTINGCHANGE with "ImmersiveColorSet" after flipping the app theme; forward it so
// the Qt side re-applies when the theme mode is System. Declared in MainFrm.h.
void CMainFrame::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
	CFrameWnd::OnSettingChange(uFlags, lpszSection);
	if (lpszSection != NULL && ::lstrcmpi(lpszSection, TEXT("ImmersiveColorSet")) == 0)
	{
		WBQt_OnOsThemeChanged();
	}
}

// Stage 1: the Qt main window persists its placement through the same [MainFrame] keys
// the MFC frame's OnMove/adjustWindowSize used, so the INI stays the single store for
// both builds. Debounced Qt-side (WBQtMainWindow::savePlacement).
extern "C" void WBQt_SaveMainWindowPlacement(int x, int y, int width, int height)
{
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "Left", x);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "Top", y);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "Width", width);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "Height", height);
}

// Stage 1: a .map dropped on the Qt main window -- the hidden frame's OnDropFiles is
// unreachable, so the drop routes here into the same doc-template open.
extern "C" void WBQt_OpenMapFileFromShell(const char *path)
{
	if (path != NULL && path[0] != 0)
	{
		AfxGetApp()->OpenDocumentFile(path);
	}
}
#endif
