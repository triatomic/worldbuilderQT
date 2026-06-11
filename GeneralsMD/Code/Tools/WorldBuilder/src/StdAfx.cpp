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

// stdafx.cpp : source file that includes just the standard includes
//	WorldBuilder.pch will be the pre-compiled header
//	stdafx.obj will contain the pre-compiled type information

#include "StdAfx.h"

// Build toggle for modern Windows controls.
// 1: pull in ComCtl32 v6 (visual styles) so the MFC dialogs/toolbars render with
//    modern themed controls (emits an embedded manifest dependency at link time;
//    MFC's _AFXDLL runtime initializes the common controls, so theming engages
//    without an explicit InitCommonControlsEx call).
// 0: keep the classic flat ComCtl32 v5 look.
#define WB_MODERN_CONTROLS 0

#if WB_MODERN_CONTROLS
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif



