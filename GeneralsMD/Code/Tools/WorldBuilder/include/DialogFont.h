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

// DialogFont.h
//
// User-selectable dialog font. The dialog resources hard-code a FONT face at
// compile time; to let the user pick one (Tahoma / Segoe UI / MS Sans Serif)
// and have it apply app-wide, we install a CallWndProc hook that, on every
// dialog's WM_INITDIALOG, sets the chosen font on the dialog and its children.
// The choice is read once at startup from the INI ([Appearance]/DialogFont), so
// changing it takes effect on the next launch. The face is named explicitly via
// CreateFontIndirect, so it resolves regardless of regional/locale settings.

#pragma once

// One source of truth for the selectable fonts. Order == combo index == INI value.
struct DialogFontChoice {
	const char *displayName;	// shown in the combo box
	const char *faceName;		// actual font face passed to CreateFontIndirect
	bool        mayClip;		// true if this face's metrics can clip the layouts
};

int                     GetDialogFontChoiceCount();
const DialogFontChoice& GetDialogFontChoice(int index);

// INI persistence ([Appearance]/DialogFont). Default index 0 (Tahoma).
int  LoadDialogFontChoice();
void SaveDialogFontChoice(int index);

// Install/remove the app-wide dialog-font hook on the calling (UI) thread.
// Call install once from InitInstance, remove from ExitInstance.
void InstallDialogFontHook();
void RemoveDialogFontHook();
