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


#include "StdAfx.h"
#include "SplashScreen.h"
#include "resource.h"

//-------------------------------------------------------------------------------------------------
SplashScreen::SplashScreen()
{
	m_rect.left = 0;
	m_rect.right = 0;
	m_rect.top = 0;
	m_rect.bottom = 0;

	m_loadString = "Cock & Beer";


	LOGFONT lf;
	lf.lfHeight = 12;
	lf.lfWidth = 0;
	lf.lfEscapement = 0;
	lf.lfOrientation = 0;
	lf.lfWeight = FW_NORMAL;
	lf.lfItalic = FALSE;
	lf.lfUnderline = FALSE;
	lf.lfStrikeOut = FALSE;
	lf.lfCharSet = ANSI_CHARSET;
	lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
	lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lf.lfQuality = DEFAULT_QUALITY;
	lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	strcpy(lf.lfFaceName, "Arial");
	
	m_font.CreateFontIndirect(&lf);
}

//-------------------------------------------------------------------------------------------------
// Size the dialog (and the bitmap static inside it) to the splash bitmap's exact
// PIXEL dimensions.  The .rc lays the dialog out in dialog units (168 x 217), which
// don't convert to the bitmap's 250 x 350 px at every DPI / font - so a strip of gray
// dialog background "leaks" around the centered image.  Snapping to the real bitmap
// size removes that gap regardless of DPI.
BOOL SplashScreen::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Get the splash bitmap's true pixel size.
	HBITMAP hBmp = (HBITMAP)::LoadImage(AfxGetResourceHandle(),
		MAKEINTRESOURCE(IDB_WORLDBUILDERSPLASH), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
	if (hBmp)
	{
		BITMAP bm;
		if (::GetObject(hBmp, sizeof(bm), &bm))
		{
			int w = bm.bmWidth;
			int h = bm.bmHeight;

			// Resize the static to cover the whole image, anchored at the top-left.
			CWnd *pPic = GetDlgItem(IDC_STATIC);
			if (pPic)
				pPic->SetWindowPos(NULL, 0, 0, w, h, SWP_NOZORDER | SWP_NOACTIVATE);

			// Resize the dialog so its CLIENT area is exactly the bitmap size (no border
			// to account for - this is a WS_POPUP dialog with no frame/caption).
			SetWindowPos(NULL, 0, 0, w, h,
				SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

			// Re-center on the screen now that the size changed.
			CenterWindow();
		}
		::DeleteObject(hBmp);
	}

	return TRUE;
}

//-------------------------------------------------------------------------------------------------
void SplashScreen::setTextOutputLocation(const CRect& rect)
{
	m_rect = rect;
}

//-------------------------------------------------------------------------------------------------
void SplashScreen::outputText(UINT nIDString)
{
	CString str;
	if (!str.LoadString(nIDString)) {
		return;
	}

	m_loadString = str;
	
	RedrawWindow(&m_rect, NULL);
}

//-------------------------------------------------------------------------------------------------
void SplashScreen::OnPaint()
{
	// we're extending the default behavior
	CDialog::OnPaint();

	
	CDC *dc = GetDC();
	
	// Save off the old font
	CFont *oldFont = dc->SelectObject(&m_font);
	COLORREF oldRef = dc->SetTextColor(0x00000000);
	
//	dc->DrawText(m_loadString, m_rect, DT_VCENTER | DT_LEFT);
	
	// restore the old font
	dc->SelectObject(oldFont);
	dc->SetTextColor(oldRef);
}

BEGIN_MESSAGE_MAP(SplashScreen, CDialog)
	ON_WM_PAINT()
END_MESSAGE_MAP()
