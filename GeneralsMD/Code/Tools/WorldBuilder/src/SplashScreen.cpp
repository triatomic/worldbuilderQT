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

	m_loadString = "";


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

			// Bottom-center band for the loading-progress label (full width so DT_CENTER
			// centers it horizontally, just above the bottom edge of the image).
			m_rect = CRect(0, h - 28, w, h - 8);

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
// Paint the loading label on top of the splash.  The bitmap is a child Static control that
// covers the whole client area, so it would obscure anything drawn on the dialog itself -
// we therefore draw the text directly onto the Static's DC so it sits over the image.
void SplashScreen::drawProgressText(void)
{
	if (m_loadString.IsEmpty())
		return;

	// Prefer the bitmap static (the image is on top); fall back to the dialog.
	CWnd *pTarget = GetDlgItem(IDC_STATIC);
	if (!pTarget)
		pTarget = this;

	CClientDC dc(pTarget);
	CFont *oldFont = dc.SelectObject(&m_font);
	int oldBk = dc.SetBkMode(TRANSPARENT);
	COLORREF oldRef = dc.SetTextColor(RGB(255, 255, 255));

	dc.DrawText(m_loadString, m_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	dc.SetTextColor(oldRef);
	dc.SetBkMode(oldBk);
	dc.SelectObject(oldFont);
}

//-------------------------------------------------------------------------------------------------
// Update the bottom-center loading label and repaint it immediately.  The UI thread is
// busy in InitInstance during startup, so we force a synchronous paint rather than wait
// for the (idle) message loop.
void SplashScreen::setProgress(LPCTSTR text)
{
	m_loadString = text;

	if (!::IsWindow(m_hWnd))
		return;

	// Invalidate the label band on the bitmap static so the image under the old text is
	// repainted, then draw the new text on top.
	CWnd *pPic = GetDlgItem(IDC_STATIC);
	if (pPic)
	{
		pPic->InvalidateRect(&m_rect, FALSE);
		pPic->UpdateWindow();	// let the static repaint the bitmap in that band
	}
	drawProgressText();			// then stamp the text over it
}

//-------------------------------------------------------------------------------------------------
void SplashScreen::OnPaint()
{
	// Let the dialog (and its bitmap static) paint the splash image first.
	CDialog::OnPaint();

	// Then draw the loading label on top of the image.
	drawProgressText();
}

BEGIN_MESSAGE_MAP(SplashScreen, CDialog)
	ON_WM_PAINT()
END_MESSAGE_MAP()
