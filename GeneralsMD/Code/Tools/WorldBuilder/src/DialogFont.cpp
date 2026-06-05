/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// DialogFont.cpp  --  see DialogFont.h for the why.

#include "StdAfx.h"
#include "DialogFont.h"

#define APPEARANCE_SECTION  "Appearance"
#define DIALOG_FONT_KEY     "DialogFont"

// Point size matches the .rc dialogs (FONT 8). Index 0 is the default used when no
// choice is saved in the INI -- MS Sans Serif, the original classic face. Segoe UI /
// Calibri / Arial are flagged mayClip so the combo warns the user; the rest have
// metrics close enough to the layouts that they don't clip.
static const DialogFontChoice s_fonts[] = {
	{ "MS Sans Serif (default)", "MS Sans Serif",       false },
	{ "Tahoma",                                 "Tahoma",              false },
	{ "Segoe UI",                               "Segoe UI",            true  },
	{ "Verdana",                                "Verdana",             false },
	{ "Microsoft Sans Serif",                   "Microsoft Sans Serif",false },
	{ "Calibri",                                "Calibri",             true  },
	{ "Arial",                                  "Arial",               true  },
};
static const int s_fontCount = sizeof(s_fonts) / sizeof(s_fonts[0]);

static const int DIALOG_FONT_POINT_SIZE = 8;

static HHOOK  s_hook       = NULL;
static HFONT  s_activeFont = NULL;	// cached font for this session's choice
static bool   s_resolved   = false;	// have we built s_activeFont yet?

int GetDialogFontChoiceCount()
{
	return s_fontCount;
}

const DialogFontChoice& GetDialogFontChoice(int index)
{
	if (index < 0 || index >= s_fontCount)
		index = 0;
	return s_fonts[index];
}

int LoadDialogFontChoice()
{
	int idx = ::AfxGetApp()->GetProfileInt(APPEARANCE_SECTION, DIALOG_FONT_KEY, 0);
	if (idx < 0 || idx >= s_fontCount)
		idx = 0;
	return idx;
}

void SaveDialogFontChoice(int index)
{
	if (index < 0 || index >= s_fontCount)
		index = 0;
	::AfxGetApp()->WriteProfileInt(APPEARANCE_SECTION, DIALOG_FONT_KEY, index);
}

// Build (once) the HFONT for the saved choice, converting the point size to the
// logical height for the screen DC. Named face -> locale-independent resolution.
static HFONT getActiveFont()
{
	if (s_resolved)
		return s_activeFont;
	s_resolved = true;

	const DialogFontChoice &fc = GetDialogFontChoice(LoadDialogFontChoice());

	HDC screen = ::GetDC(NULL);
	int logHeight = -::MulDiv(DIALOG_FONT_POINT_SIZE, ::GetDeviceCaps(screen, LOGPIXELSY), 72);
	::ReleaseDC(NULL, screen);

	LOGFONT lf;
	::memset(&lf, 0, sizeof(lf));
	lf.lfHeight         = logHeight;
	lf.lfWeight         = FW_NORMAL;
	lf.lfCharSet        = DEFAULT_CHARSET;
	lf.lfOutPrecision   = OUT_DEFAULT_PRECIS;
	lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
	lf.lfQuality        = CLEARTYPE_QUALITY;
	lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	::lstrcpyn(lf.lfFaceName, fc.faceName, LF_FACESIZE);

	s_activeFont = ::CreateFontIndirect(&lf);	// NULL on failure -> hook no-ops
	return s_activeFont;
}

static BOOL CALLBACK setFontOnChild(HWND hwnd, LPARAM lParam)
{
	::SendMessage(hwnd, WM_SETFONT, (WPARAM)lParam, MAKELPARAM(TRUE, 0));
	return TRUE;
}

static void applyFontToDialog(HWND dlg)
{
	HFONT f = getActiveFont();
	if (!f || !dlg || !::IsWindow(dlg))
		return;
	::SendMessage(dlg, WM_SETFONT, (WPARAM)f, MAKELPARAM(TRUE, 0));
	::EnumChildWindows(dlg, setFontOnChild, (LPARAM)f);
}

// WM_INITDIALOG fires after controls exist but before the dialog is shown.
static LRESULT CALLBACK callWndProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION) {
		CWPSTRUCT *cwp = (CWPSTRUCT *)lParam;
		if (cwp && cwp->message == WM_INITDIALOG)
			applyFontToDialog(cwp->hwnd);
	}
	return ::CallNextHookEx(s_hook, code, wParam, lParam);
}

void InstallDialogFontHook()
{
	if (s_hook)
		return;
	s_hook = ::SetWindowsHookEx(WH_CALLWNDPROC, callWndProc, NULL, ::GetCurrentThreadId());
}

void RemoveDialogFontHook()
{
	if (s_hook) {
		::UnhookWindowsHookEx(s_hook);
		s_hook = NULL;
	}
	if (s_activeFont) {
		::DeleteObject(s_activeFont);
		s_activeFont = NULL;
	}
	s_resolved = false;
}
