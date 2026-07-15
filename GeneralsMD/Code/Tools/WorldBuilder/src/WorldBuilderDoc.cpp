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

// WorldBuilderDoc.cpp : implementation of the CWorldBuilderDoc class
//

#include "StdAfx.h"
#include "WorldBuilder.h"

#include <shlwapi.h> // for PathFileExists
#pragma comment(lib, "shlwapi.lib")

#include <direct.h>
#include <windows.h>
#include <process.h>

#include "Common/Debug.h"
#include "Common/DataChunk.h"
#include "Common/INIException.h"
#include "Common/PlayerTemplate.h"
#include "Common/MapReaderWriterInfo.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
// Stores whose map.ini overrides must be torn down when unloading a map.ini (mirrors
// the WB INI type table in INI.cpp + the game's own between-match reset()).
#include "Common/SpecialPower.h"
#include "Common/Science.h"
#include "Common/ModuleFactory.h"
#include "Common/Module.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Armor.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameClient/FXList.h"
#include "GameClient/Water.h"
#include "Common/WellKnownKeys.h"

#include "GameClient/Line2D.h"
#include "GameClient/View.h"
#include "GameClient/GameText.h"

#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/ScriptEngine.h"


#include "Compression.h"
#include "CUndoable.h"
#include "LayersList.h"
#include "MainFrm.h"
#include "MinimapDialog.h"
#include "NewHeightMap.h"
#ifdef RTS_HAS_QT
#include "qt/panels/WBQtMiscModalsBridge.h"
#include "qt/panels/WBQtMapFileBridge.h"
#include "qt/panels/WBQtPickUnitBridge.h"
#include "qt/panels/WBQtMapIniReport.h"
#endif
#include "SaveMap.h"
#include "ScriptDialog.h"
#include "TerrainMaterial.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "wbview3d.h"
#include "wbview.h"
#include "WHeightMapEdit.h"
#include "WorldBuilderDoc.h"
#include "WorldBuilderView.h"
#include "MapPreview.h"

#include "TileTool.h"

#include <algorithm>
#include <string>
#include <vector>

#ifdef _INTERNAL
// for occasional debugging...
//#pragma optimize("", off)
//#pragma MESSAGE("************************************** WARNING, optimization disabled for debugging purposes")
#endif

// Can't currently have multiple open... jba.
#define notONLY_ONE_AT_A_TIME

#ifdef ONLY_ONE_AT_A_TIME
static Bool gAlreadyOpen = false;
#endif

enum DIRECTION
{
	PREFER_CENTER,
	PREFER_LEFT,
	PREFER_TOP,
	PREFER_RIGHT,
	PREFER_BOTTOM,
};

static Bool g_mapiniloaded = false;
static Bool g_warnedfordupedforthismap = false;

// ----------------------------------------------------------------------------
// Gracefully unload map.ini overrides.
//
// map.ini is loaded with INI_LOAD_CREATE_OVERRIDES, which dangles "override"
// instances off the base templates in each store (the same mechanism the game uses
// for map-specific tweaks). Each store's reset() walks its templates and calls
// Overridable::deleteOverrides(), which deletes ONLY the entries marked as overrides
// and leaves the base game data intact -- exactly what the game does between matches.
//
// We only reset the stores that (a) the WB INI type table (INI.cpp theWbTypeTable)
// can actually create overrides in AND (b) have a real override-only teardown. Object,
// Weapon, Science, SpecialPower and Water/Weather qualify. FXList / OCL / Armor have
// empty reset()s and ParticleSystemManager::reset() is a full wipe (not override-only),
// so we deliberately skip those -- map.ini overrides to them are rare, and calling
// their reset would either do nothing or destroy non-override state.
static void unloadMapIniOverrides(void)
{
	if (!g_mapiniloaded)
		return;

	if (TheThingFactory)       TheThingFactory->reset();        // Object
	if (TheWeaponStore)        TheWeaponStore->reset();         // Weapon
	if (TheScienceStore)       TheScienceStore->reset();        // Science
	if (TheSpecialPowerStore)  TheSpecialPowerStore->reset();   // SpecialPower

	// Water transparency / radar color override (GameLogic does this same dance on its
	// own reset). TheWaterTransparency is an OVERRIDE<> smart pointer.
	if (TheWaterTransparency.getNonOverloadedPointer())
	{
		WaterTransparencySetting *wt =
			(WaterTransparencySetting*)TheWaterTransparency.getNonOverloadedPointer();
		TheWaterTransparency = (WaterTransparencySetting*)wt->deleteOverrides();
	}

	// Re-link object templates after stripping overrides (resolves names, rebuilds the
	// upgrade/module references) -- the same call the WB loader makes after parsing.
	if (TheThingFactory)
		TheThingFactory->postProcessLoad();

	g_mapiniloaded = false;
}

// ----------------------------------------------------------------------------
// Map.ini pre-scan.
//
// The engine treats "RemoveModule <tag>" for a tag the template doesn't have as a
// fatal error (ThingTemplate::parseRemoveModule throws -- "The game will crash
// now!"). Maps are often authored against game data that doesn't match the local
// install (patched INIZH.big, mods), so before handing map.ini to the parser we
// blank out just the RemoveModule lines that would throw and load everything else.
// Mirrors ThingTemplate::removeModuleInfo's search: the behavior, draw and
// clientUpdate module lists.
static Bool templateHasModuleTag(const ThingTemplate *tmpl, const char *tag)
{
	const ModuleInfo *lists[] = {
		&tmpl->getBehaviorModuleInfo(),
		&tmpl->getDrawModuleInfo(),
		&tmpl->getClientUpdateModuleInfo(),
	};
	for (Int li = 0; li < 3; ++li)
		for (Int i = 0; i < lists[li]->getCount(); ++i)
			if (strcmp(lists[li]->getNthTag(i).str(), tag) == 0)
				return true;
	return false;
}

// Map a module block header keyword to the ModuleType the engine parser uses, so we
// can ask TheModuleFactory whether the named module exists in this install. "Body" and
// "ClientBehavior" resolve to BEHAVIOR (see ThingTemplate::parseModuleName). Returns
// false for keywords that are not module headers.
static Bool isModuleHeader(const char *keyword, ModuleType *typeOut)
{
	if (strcmp(keyword, "Behavior") == 0 || strcmp(keyword, "Body") == 0 ||
		strcmp(keyword, "ClientBehavior") == 0)
	{
		*typeOut = MODULETYPE_BEHAVIOR;
		return true;
	}
	if (strcmp(keyword, "Draw") == 0)
	{
		*typeOut = MODULETYPE_DRAW;
		return true;
	}
	if (strcmp(keyword, "ClientUpdate") == 0)
	{
		*typeOut = MODULETYPE_CLIENT_UPDATE;
		return true;
	}
	return false;
}

// Results of the map.ini pre-scan: the sanitized path to actually load, the list of
// neutralized directives, the override/new/module counts for the post-load summary, and
// whether the file touches stores whose overrides cannot be cleanly torn down at runtime
// (FXList / OCL / Armor / ParticleSystem) -- Reload warns about those.
// One object's block in the scan, with the per-module edits under it (for verbose output).
struct MapIniObjectDetail
{
	AsciiString name;
	Bool isNew;								// defined by the map.ini vs. overriding an existing template
	std::vector<AsciiString> moduleLines;	// e.g. "Add Behavior FooUpdate (ModuleTag_Foo)"
	MapIniObjectDetail() : isNew(false) {}
};

struct MapIniScanResult
{
	AsciiString loadPath;						// iniPath, or a sanitized temp copy
	std::vector<AsciiString> skipped;			// one per neutralized directive
	std::vector<AsciiString> overriddenNames;	// existing objects the map.ini overrides
	std::vector<AsciiString> newNames;			// brand-new objects the map.ini defines
	std::vector<MapIniObjectDetail> objects;	// per-object detail, in file order (verbose)
	std::vector<std::pair<AsciiString, Int> > storeCounts;	// non-Object block type -> count
	Int moduleEdits;							// Add/Remove/Replace module directives seen
	Bool hasUntearableOverrides;				// FXList/OCL/Armor/ParticleSystem block present

	MapIniScanResult() : moduleEdits(0), hasUntearableOverrides(false) {}

	// Convenience: derive counts from the name lists (kept identical to the old fields).
	Int objectsOverridden() const { return (Int)overriddenNames.size(); }
	Int objectsNew() const { return (Int)newNames.size(); }

	// Tally a top-level non-Object block ("FXList", "Weapon", ...) for the per-store breakdown.
	void tallyStore(const char *type)
	{
		for (size_t i = 0; i < storeCounts.size(); ++i)
		{
			if (storeCounts[i].first == type)
			{
				storeCounts[i].second++;
				return;
			}
		}
		storeCounts.push_back(std::make_pair(AsciiString(type), 1));
	}
};

// Map.ini pre-scan. Returns a temp copy with fatal-on-mismatch directives blanked (kept
// as empty lines so parser error line numbers still match the real map.ini), plus the
// summary/skip data. Neutralized directives (all throw INI_INVALID_DATA in the engine
// parser on a data/install mismatch, which would abort the whole load):
//   - RemoveModule <tag>  : tag not present on the template
//   - ReplaceModule <tag> : tag not present on the template
//   - a module block (Behavior/Draw/Body/ClientUpdate/ClientBehavior = <Name> <Tag>)
//     naming a module <Name> this build's ModuleFactory doesn't know -- the whole block
//     (header through its matching End) is blanked.
static void sanitizeMapIni(const AsciiString &iniPath, MapIniScanResult &result)
{
	result.loadPath = iniPath;

	FILE *fp = fopen(iniPath.str(), "rt");
	if (fp == NULL)
		return;	// let the real loader produce the error

	std::string output;
	Bool modified = false;

	const ThingTemplate *curTemplate = NULL;
	AsciiString curObjName;
	Int curObjectIndex = -1;				// index into result.objects, or -1 (no object block)
	std::vector<AsciiString> tagsAdded;		// tags introduced inside this block (AddModule headers)
	std::vector<AsciiString> tagsRemoved;	// tags consumed by an earlier Remove/Replace in this block

	// When >0 we are inside a module block being blanked; blank lines through its End.
	Int blankingModuleDepth = 0;
	ModuleType curModuleType = MODULETYPE_BEHAVIOR;	// filled by isModuleHeader per line

	char line[4096];
	Int lineNum = 0;
	while (fgets(line, sizeof(line), fp))
	{
		++lineNum;

		// tokenize a working copy, stripping comments the way INI::readLine does
		char work[4096];
		strcpy(work, line);
		char *cmt = strchr(work, ';');
		if (cmt) *cmt = 0;
		cmt = strstr(work, "//");
		if (cmt) *cmt = 0;
		Bool hasEquals = (strchr(work, '=') != NULL);

		static const char *seps = " \t\n\r=";
		const char *tok1 = strtok(work, seps);
		const char *tok2 = tok1 ? strtok(NULL, seps) : NULL;
		const char *tok3 = tok2 ? strtok(NULL, seps) : NULL;

		Bool keep = true;

		// Blanking a doomed module block: swallow everything up to and including its End.
		if (blankingModuleDepth > 0)
		{
			keep = false;
			if (tok1 && strcmp(tok1, "End") == 0)
				--blankingModuleDepth;
		}
		else if (tok1 && tok2 && !hasEquals && strcmp(tok1, "Object") == 0)
		{
			// block header ("Object <name>"; "Object = <name>" is a field elsewhere)
			curObjName = tok2;
			curTemplate = TheThingFactory ? TheThingFactory->findTemplate(curObjName, FALSE) : NULL;
			MapIniObjectDetail detail;
			detail.name = curObjName;
			detail.isNew = (curTemplate == NULL);
			if (curTemplate != NULL)
			{
				result.overriddenNames.push_back(curObjName);
			}
			else
			{
				result.newNames.push_back(curObjName);
				if (TheThingFactory)
				{
					// object defined by the map.ini itself: ThingFactory::newTemplate seeds it
					// as a copy of DefaultThingTemplate, so those are the module tags a
					// Remove/Replace will actually see (ModuleTag_DefaultInactiveBody etc.)
					curTemplate = TheThingFactory->findTemplate(AsciiString("DefaultThingTemplate"), FALSE);
				}
			}
			result.objects.push_back(detail);
			curObjectIndex = (Int)result.objects.size() - 1;
			tagsAdded.clear();
			tagsRemoved.clear();
		}
		else if (tok1 && tok2 &&
				 (strcmp(tok1, "RemoveModule") == 0 || strcmp(tok1, "ReplaceModule") == 0))
		{
			++result.moduleEdits;
			if (curObjectIndex >= 0) {
				AsciiString ml;
				ml.format("%s %s", tok1, tok2);
				result.objects[curObjectIndex].moduleLines.push_back(ml);
			}
			AsciiString tag(tok2);
			Bool present = false;
			if (std::find(tagsRemoved.begin(), tagsRemoved.end(), tag) == tagsRemoved.end())
			{
				if (curTemplate && templateHasModuleTag(curTemplate, tok2))
					present = true;
				else if (std::find(tagsAdded.begin(), tagsAdded.end(), tag) != tagsAdded.end())
					present = true;
			}
			// ReplaceModule keeps a body (its replacement block follows through an End); if we
			// drop the header we must also drop that body, so treat a doomed ReplaceModule as a
			// module block to blank. RemoveModule is a single line.
			Bool isReplace = (strcmp(tok1, "ReplaceModule") == 0);
			if (present)
			{
				tagsRemoved.push_back(tag);
			}
			else
			{
				keep = false;
				if (isReplace)
					blankingModuleDepth = 1;	// swallow the replacement body too
				AsciiString warn;
				warn.format("line %d: %s %s -- '%s' has no such module", lineNum, tok1, tok2,
					curObjName.isEmpty() ? "(no object)" : curObjName.str());
				result.skipped.push_back(warn);
				// Mark the detail line we just recorded as skipped.
				if (curObjectIndex >= 0 && !result.objects[curObjectIndex].moduleLines.empty()) {
					AsciiString &last = result.objects[curObjectIndex].moduleLines.back();
					last.concat(" [SKIPPED: no such module]");
				}
			}
		}
		else if (tok1 && tok3 && !hasEquals && isModuleHeader(tok1, &curModuleType))
		{
			// module header "Behavior <ModuleName> <Tag>" etc. (the "=" form is a field elsewhere).
			++result.moduleEdits;
			// Unknown module for this build -> the parser throws; blank the whole block.
			if (TheModuleFactory && TheModuleFactory->findModuleInterfaceMask(AsciiString(tok2), curModuleType) == 0)
			{
				keep = false;
				blankingModuleDepth = 1;
				AsciiString warn;
				warn.format("line %d: %s %s -- module type unknown to this build", lineNum, tok1, tok2);
				result.skipped.push_back(warn);
				if (curObjectIndex >= 0) {
					AsciiString ml;
					ml.format("Add %s %s (%s) [SKIPPED: unknown module]", tok1, tok2, tok3);
					result.objects[curObjectIndex].moduleLines.push_back(ml);
				}
			}
			else
			{
				tagsAdded.push_back(AsciiString(tok3));
				if (curObjectIndex >= 0) {
					AsciiString ml;
					ml.format("Add %s %s (%s)", tok1, tok2, tok3);
					result.objects[curObjectIndex].moduleLines.push_back(ml);
				}
			}
		}
		else if (tok1 && tok2 && !hasEquals && blankingModuleDepth == 0)
		{
			// Any other top-level "<Type> <Name>" block header -> tally it by store type for
			// the summary breakdown (Weapon / Science / SpecialPower / FXList / Upgrade / ...).
			// The module-header case above already consumed Behavior/Draw/etc., so what
			// reaches here is a store block, not a nested module.
			static const char *kStoreTypes[] = {
				"Weapon", "Armor", "Science", "SpecialPower", "Upgrade", "FXList",
				"ObjectCreationList", "ParticleSystem", "Locomotor", "DamageFX",
				"CommandButton", "CommandSet", "WeatherData", "Water"
			};
			for (int s = 0; s < (int)(sizeof(kStoreTypes)/sizeof(kStoreTypes[0])); ++s)
			{
				if (strcmp(tok1, kStoreTypes[s]) == 0)
				{
					curObjectIndex = -1;	// left the Object block; module edits below aren't its
					result.tallyStore(tok1);
					// Overrides to these stores can't be cleanly torn down at runtime (see
					// unloadMapIniOverrides); flag it so Reload can warn.
					if (strcmp(tok1, "FXList") == 0 || strcmp(tok1, "ObjectCreationList") == 0 ||
						strcmp(tok1, "Armor") == 0 || strcmp(tok1, "ParticleSystem") == 0)
					{
						result.hasUntearableOverrides = true;
					}
					break;
				}
			}
		}

		if (keep)
		{
			output += line;
		}
		else
		{
			output += "\n";	// keep line numbering intact
			modified = true;
		}
	}
	fclose(fp);

	if (!modified)
		return;

	char tempDir[MAX_PATH];
	::GetTempPathA(MAX_PATH, tempDir);
	AsciiString tempPath;
	tempPath.format("%swb_sanitized_map.ini", tempDir);

	FILE *out = fopen(tempPath.str(), "wt");
	if (out == NULL)
		return;	// can't write the temp copy; let the loader fail loudly (loadPath stays iniPath)
	fwrite(output.data(), 1, output.size(), out);
	fclose(out);
	result.loadPath = tempPath;
}

// ----------------------------------------------------------------------------
// Modal info dialog with a scrollable read-only log. MessageBox grows with its
// text and runs off-screen for long skip lists, so we build a fixed-size dialog
// template in memory (no .rc resource needed) holding a multiline edit.

static const WORD SCROLL_LOG_EDIT_ID = 1001;

static INT_PTR CALLBACK scrollableInfoDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HFONT s_monoFont = NULL;
	switch (msg)
	{
	case WM_INITDIALOG:
		{
			// Monospace so the INI-formatted report (aligned "Object <name> ; tag" columns)
			// lines up. Kept static and freed on WM_DESTROY.
			s_monoFont = ::CreateFontA(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
				DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
				FIXED_PITCH | FF_MODERN, "Consolas");
			HWND edit = ::GetDlgItem(hDlg, SCROLL_LOG_EDIT_ID);
			if (s_monoFont != NULL && edit != NULL)
				::SendMessage(edit, WM_SETFONT, (WPARAM)s_monoFont, TRUE);
			::SetDlgItemTextA(hDlg, SCROLL_LOG_EDIT_ID, (const char *)lParam);
		}
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			::EndDialog(hDlg, IDOK);
			return TRUE;
		}
		break;
	case WM_DESTROY:
		if (s_monoFont != NULL)
		{
			::DeleteObject(s_monoFont);
			s_monoFont = NULL;
		}
		break;
	}
	return FALSE;
}

static void dlgAppend(std::vector<BYTE> &buf, const void *data, size_t len)
{
	const BYTE *p = (const BYTE *)data;
	buf.insert(buf.end(), p, p + len);
}
static void dlgAppendWord(std::vector<BYTE> &buf, WORD w)
{
	dlgAppend(buf, &w, sizeof(w));
}
static void dlgAppendWideString(std::vector<BYTE> &buf, const wchar_t *s)
{
	dlgAppend(buf, s, (wcslen(s) + 1) * sizeof(wchar_t));
}
static void dlgAlign4(std::vector<BYTE> &buf)
{
	while (buf.size() % 4)
		buf.push_back(0);
}

// Show the map.ini report. applyMode: false = informational (Check) with just a Close;
// true = OK/Cancel so the caller applies on OK. Returns 2 if the user accepted (OK / closed
// an informational report), 1 if the user cancelled an apply-mode report.
static int showScrollableInfoDialog(const char *title, const char *text, bool applyMode = false)
{
#ifdef RTS_HAS_QT
	// Prefer the native Qt report viewer (resizable, filter, collapsible sections, Copy,
	// and OK/Cancel in apply mode). Falls through to the MFC path only when Qt is not up.
	int qrc = WBQtMapIniReport_Show(title, text, applyMode ? 1 : 0);
	if (qrc != 0)
		return qrc;	// 2 = accepted, 1 = cancelled
#endif

	// MFC fallback: an apply-mode report can't host OK/Cancel in the read-only viewer, so
	// ask with a Yes/No box after showing it (informational reports just show + return 2).
	// fixed size in dialog units (~570x500 px at 8pt) -- fits any usable screen
	const short DLG_W = 520, DLG_H = 360;

	std::vector<BYTE> buf;
	buf.reserve(512);

	DLGTEMPLATE dt;
	memset(&dt, 0, sizeof(dt));
	dt.style = DS_MODALFRAME | DS_SETFONT | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
	dt.cdit = 2;
	dt.cx = DLG_W;
	dt.cy = DLG_H;
	dlgAppend(buf, &dt, sizeof(dt));
	dlgAppendWord(buf, 0);	// no menu
	dlgAppendWord(buf, 0);	// default dialog class
	{
		wchar_t wtitle[256];
		::MultiByteToWideChar(CP_ACP, 0, title, -1, wtitle, 256);
		wtitle[255] = 0;
		dlgAppendWideString(buf, wtitle);
	}
	dlgAppendWord(buf, 8);	// font size
	dlgAppendWideString(buf, L"MS Shell Dlg");

	// read-only multiline edit with a vertical scrollbar (text set in WM_INITDIALOG)
	dlgAlign4(buf);
	DLGITEMTEMPLATE item;
	memset(&item, 0, sizeof(item));
	item.style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | WS_VSCROLL |
				 ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL;
	item.x = 7;
	item.y = 7;
	item.cx = DLG_W - 14;
	item.cy = DLG_H - 32;
	item.id = SCROLL_LOG_EDIT_ID;
	dlgAppend(buf, &item, sizeof(item));
	dlgAppendWord(buf, 0xFFFF);
	dlgAppendWord(buf, 0x0081);	// EDIT
	dlgAppendWord(buf, 0);		// empty title
	dlgAppendWord(buf, 0);		// no creation data

	// OK button
	dlgAlign4(buf);
	memset(&item, 0, sizeof(item));
	item.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
	item.x = (DLG_W - 50) / 2;
	item.y = DLG_H - 21;
	item.cx = 50;
	item.cy = 14;
	item.id = IDOK;
	dlgAppend(buf, &item, sizeof(item));
	dlgAppendWord(buf, 0xFFFF);
	dlgAppendWord(buf, 0x0080);	// BUTTON
	dlgAppendWideString(buf, L"OK");
	dlgAppendWord(buf, 0);		// no creation data

	::DialogBoxIndirectParamA(::AfxGetInstanceHandle(), (LPCDLGTEMPLATE)&buf[0],
		::AfxGetMainWnd() ? ::AfxGetMainWnd()->GetSafeHwnd() : NULL,
		scrollableInfoDlgProc, (LPARAM)text);

	if (applyMode) {
		// The MFC viewer has no OK/Cancel; confirm with a follow-up prompt.
		int res = ::MessageBox(NULL, "Load this map.ini's overrides?", "Map.ini Loader",
			MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1);
		return (res == IDYES) ? 2 : 1;
	}
	return 2;
}

// "Always load this map's map.ini" list, kept GLOBALLY in WorldBuilder.ini under
// [MapLoaderIni] as numbered AlwaysLoad<N> = <full .map path> keys. A map on the list
// auto-loads its map.ini silently on open; any other map still prompts. (No per-map
// AdrianeMapSettings.ini file, and no "never" state -- unlisted simply means "ask".)
#define MAPLOADER_SECTION "MapLoaderIni"

// Is mapFilePath in the always-load list? Scans AlwaysLoad1.. until the first gap.
static bool isMapIniAlwaysLoad(const char *mapFilePath)
{
	if (mapFilePath == NULL || *mapFilePath == 0)
		return false;
	CString want(mapFilePath);
	for (int i = 1; ; ++i) {
		CString key;
		key.Format("AlwaysLoad%d", i);
		CString val = ::AfxGetApp()->GetProfileString(MAPLOADER_SECTION, key, "");
		if (val.IsEmpty())
			return false;	// first gap -> end of list
		if (val.CompareNoCase(want) == 0)
			return true;
	}
}

// Append mapFilePath to the always-load list (no-op if already present).
static void addMapIniAlwaysLoad(const char *mapFilePath)
{
	if (mapFilePath == NULL || *mapFilePath == 0 || isMapIniAlwaysLoad(mapFilePath))
		return;
	int slot = 1;
	for (; ; ++slot) {
		CString key;
		key.Format("AlwaysLoad%d", slot);
		if (::AfxGetApp()->GetProfileString(MAPLOADER_SECTION, key, "").IsEmpty())
			break;	// first free slot
	}
	CString key;
	key.Format("AlwaysLoad%d", slot);
	::AfxGetApp()->WriteProfileString(MAPLOADER_SECTION, key, mapFilePath);
}

// Append a section of "Object <name>" lines under an INI-style ';' comment header,
// mirroring map.ini's own syntax so the report reads like the file it came from. Each
// line gets a trailing "; <tag>" note. Nothing is appended for an empty list.
static void appendIniObjectSection(CString &msg, const char *header,
	const std::vector<AsciiString> &names, const char *tag)
{
	if (names.empty())
		return;
	CString line;
	line.Format("\r\n; %s (%d)\r\n", header, (Int)names.size());
	msg += line;
	for (size_t i = 0; i < names.size(); ++i) {
		line.Format("Object %-40s ; %s\r\n", names[i].str(), tag);
		msg += line;
	}
}

// Refresh the placed objects in the viewport after overrides change (== the tail of an
// install). Drops cached render objects built from old template data and re-resolves them.
static void refreshMapIniViewport(void)
{
	ObjectOptions::reprocessObjectList();
	WbView3d *p3d = CWorldBuilderDoc::GetActive3DView();
	if (p3d != NULL) {
		p3d->resetRenderObjects();		// == Troubleshooting > Refresh Scene Objects
		p3d->invalObjectInView(NULL);
	}
}

// How doLoadMapIni finishes after a successful parse (which always creates overrides):
enum MapIniLoadMode {
	MAPINI_INSTALL,		// keep overrides + refresh the viewport now (no confirm dialog)
	MAPINI_DRYRUN,		// drop everything (Check): parse only, nothing sticks
	MAPINI_CONFIRM		// keep overrides but DON'T refresh yet -- caller confirms, then
						// calls refreshMapIniViewport() (OK) or unloadMapIniOverrides() (Cancel)
};

// Load a map.ini through the sanitize + engine-parser pipeline. Shared by map-open, Reload
// and Check. reportOut is filled with the summary (counts + skipped, or the error) for the
// caller's dialog. Returns true if the map.ini parsed (even if directives were skipped),
// false on a hard parse error. In MAPINI_CONFIRM the overrides are left installed for the
// caller to keep (refreshMapIniViewport) or discard (unloadMapIniOverrides).
static bool doLoadMapIni(const AsciiString &iniPath, MapIniLoadMode mode, CString &reportOut)
{
	const bool installOverrides = (mode != MAPINI_DRYRUN);
	MapIniScanResult scan;
	sanitizeMapIni(iniPath, scan);

	bool ok = false;
	reportOut.Empty();

	try {
		INI ini;
		ini.loadWB(scan.loadPath, INI_LOAD_CREATE_OVERRIDES, NULL);
		g_mapiniloaded = true;	// overrides now exist; teardown paths must run

		if (mode == MAPINI_INSTALL) {
			// Apply now: rebuild the catalog + refresh the placed objects in the viewport.
			refreshMapIniViewport();
		} else if (mode == MAPINI_DRYRUN) {
			// Check: drop everything we just created; nothing sticks.
			unloadMapIniOverrides();
		}
		// MAPINI_CONFIRM: leave the overrides installed; the caller keeps or discards them
		// based on the report dialog's OK/Cancel.
		ok = true;

		// The report is rendered in map.ini's own syntax: ';' comment headers with the
		// touched objects/stores listed as they'd appear in the file.
		CString msg;
		msg.Format(
			"; ==============================================================\r\n"
			"; %s\r\n"
			"; %d object(s) overridden, %d new object(s) defined, %d module edit(s)\r\n"
			"; ==============================================================\r\n",
			installOverrides ? "map.ini loaded" : "map.ini parses cleanly (no changes applied)",
			scan.objectsOverridden(), scan.objectsNew(), scan.moduleEdits);

		// Per-store breakdown of the non-Object data blocks touched, as commented lines.
		if (!scan.storeCounts.empty()) {
			msg += "\r\n; Data stores touched\r\n";
			for (size_t i = 0; i < scan.storeCounts.size(); ++i) {
				CString line;
				line.Format(";   %-20s %d block(s)\r\n",
					scan.storeCounts[i].first.str(), scan.storeCounts[i].second);
				msg += line;
			}
		}

		// Verbose (File > Map.ini > Verbose report): show each object as an INI block with
		// its per-module edits inside it, mirroring how the change reads in the file itself.
		const bool verbose = (::AfxGetApp()->GetProfileInt("MapIni", "VerboseReport", 0) != 0);
		if (verbose) {
			for (size_t i = 0; i < scan.objects.size(); ++i) {
				const MapIniObjectDetail &o = scan.objects[i];
				CString hdr;
				hdr.Format("\r\nObject %s   ; %s\r\n", o.name.str(), o.isNew ? "new" : "overridden");
				msg += hdr;
				for (size_t m = 0; m < o.moduleLines.size(); ++m) {
					msg += "    ";
					msg += o.moduleLines[m].str();
					msg += "\r\n";
				}
				msg += "End\r\n";
			}
		} else {
			// The objects, as "Object <name>" lines like the file itself.
			appendIniObjectSection(msg, "Objects overridden", scan.overriddenNames, "overridden");
			appendIniObjectSection(msg, "New objects defined", scan.newNames, "new");
		}

		if (!scan.skipped.empty()) {
			msg += "\r\n; ----- Skipped (don't match the installed game data) -----\r\n";
			for (size_t i = 0; i < scan.skipped.size(); ++i) {
				msg += ";   ";
				msg += scan.skipped[i].str();
				msg += "\r\n";
			}
			msg += "; (The game itself would refuse to load this map.ini.)\r\n";
		}
		if (installOverrides && scan.hasUntearableOverrides) {
			msg += "\r\n; Note: this map.ini overrides FXList / ObjectCreationList / Armor /\r\n"
				"; ParticleSystem data. Those can't be cleanly reloaded -- reopen the map\r\n"
				"; to fully reset them.\r\n";
		}
		reportOut = msg;
	}
	catch (const INIException &e) {
		// A hard parse error must not take down the editor: strip any partial overrides.
		g_mapiniloaded = true;	// so the teardown actually runs
		unloadMapIniOverrides();
		reportOut.Format("The map.ini could not be loaded and has been skipped:\r\n\r\n%s\r\n"
			"The map will open without its map.ini overrides.",
			e.mFailureMessage ? e.mFailureMessage : "Unknown INI error.");
	}
	catch (...) {
		g_mapiniloaded = true;
		unloadMapIniOverrides();
		reportOut = "The map.ini could not be loaded and has been skipped (unknown INI error).\r\n"
			"The map will open without its map.ini overrides.";
	}

	// Clean up the sanitized temp copy, if one was made.
	if (strcmp(scan.loadPath.str(), iniPath.str()) != 0)
		::DeleteFileA(scan.loadPath.str());

	return ok;
}

// Parse the map.ini, show its report with OK/Cancel, and apply the overrides only if the
// user clicks OK. On Cancel (or a hard parse error) nothing is left installed. Used by
// map-open and Reload. Returns true if the overrides were applied.
static bool confirmAndLoadMapIni(const AsciiString &iniPath, const char *title)
{
	CString report;
	bool parsed = doLoadMapIni(iniPath, MAPINI_CONFIRM, report);
	if (!parsed) {
		// Hard error: doLoadMapIni already unloaded the partial overrides. Just show why.
		showScrollableInfoDialog(title, report, /*applyMode=*/false);
		return false;
	}
	// Overrides are installed but the viewport hasn't refreshed yet. Let the user decide.
	int choice = showScrollableInfoDialog(title, report, /*applyMode=*/true);
	if (choice == 2) {
		refreshMapIniViewport();	// commit: rebuild catalog + refresh placed objects
		return true;
	}
	unloadMapIniOverrides();		// cancel: discard everything the parse created
	return false;
}

static bool secondGreaterThan(const std::pair<AsciiString, Int>& __t1, const std::pair<AsciiString, Int>& __t2)
{
	return __t1.second > __t2.second;
}

static void FindIndexNearest(CWorldBuilderDoc* pDoc, const Coord3D* point, CPoint* outNdx, DIRECTION pref );
static Bool IndexInRect(CWorldBuilderDoc* pDoc, const Coord3D* bl, const Coord3D* tl, const Coord3D* br, const Coord3D* tr, CPoint* index);
static Bool AddUniqueAndNeighbors(CWorldBuilderDoc* pDoc, const Coord3D* bl, const Coord3D* tl, const Coord3D* br, const Coord3D* tr, CPoint ndx, VecHeightMapIndexes* allIndices);
/////////////////////////////////////////////////////////////////////////////
// CWorldBuilderDoc

IMPLEMENT_DYNCREATE(CWorldBuilderDoc, CDocument)

BEGIN_MESSAGE_MAP(CWorldBuilderDoc, CDocument)
	//{{AFX_MSG_MAP(CWorldBuilderDoc)
	ON_COMMAND(ID_EDIT_REDO, OnEditRedo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_REDO, OnUpdateEditRedo)
	ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
	ON_COMMAND(ID_TS_INFO, OnTsInfo)
	ON_COMMAND(ID_TS_CANONICAL, OnTsCanonical)
	ON_UPDATE_COMMAND_UI(ID_TS_CANONICAL, OnUpdateTsCanonical)
	ON_COMMAND(ID_FILE_RESIZE, OnFileResize)
#ifdef RTS_HAS_QT
	// Intercept ID_FILE_CLOSE at the document (before CDocument's default close, which
	// destroys the Qt-hosted 3D view and fails to recreate it -- "Command failed.").
	ON_COMMAND(ID_FILE_CLOSE, OnFileClose)
#endif
	
	ON_COMMAND(ID_FILE_GENERATE_MAPSTRNINI, OnGenerateMapStrAndIni)
	ON_COMMAND(ID_FILE_RELOAD_MAPINI, OnReloadMapIni)
	ON_COMMAND(ID_FILE_CHECK_MAPINI, OnCheckMapIni)
	ON_COMMAND(ID_FILE_WATCH_MAPINI, OnToggleWatchMapIni)
	ON_UPDATE_COMMAND_UI(ID_FILE_WATCH_MAPINI, OnUpdateWatchMapIni)
	ON_COMMAND(ID_FILE_VERBOSE_MAPINI, OnToggleVerboseMapIni)
	ON_UPDATE_COMMAND_UI(ID_FILE_VERBOSE_MAPINI, OnUpdateVerboseMapIni)
	ON_COMMAND(ID_FILE_WBSETTINGS, OnOpenWorldbuilderSettings)
	ON_COMMAND(ID_FILE_AUTOSAVEFOLDER, OnJumpToAutoSaveFolder)
	ON_COMMAND(ID_FILE_JUMPTOFOLDER, OnJumpToMapFolder)
	ON_COMMAND(ID_FILE_GAMEFOLDERDATA, OnOpenDataFolder)
	ON_COMMAND(ID_FILE_GAMEFOLDER, OnOpenGameFolder)

	ON_COMMAND(ID_FILE_JUMPTOFOLDERDATA, OnJumpToMapFolderWBData)
	
	ON_COMMAND(ID_DISABLEMAPPREVGENERATE, OnViewDisableMapPrevGen)
	// ON_UPDATE_COMMAND_UI(ID_DISABLEMAPPREVGENERATE, OnUpdateDisableMapPrevGen)

	ON_COMMAND(ID_FILE_JUMPTOGAME, OnJumpToGameWithDebug)
	ON_COMMAND(ID_FILE_JUMPTOGAME_WD, OnJumpToGameWithoutDebug)
	ON_COMMAND(ID_FILE_JUMPTOGAME_WM, OnJumpToGameWithWaveEdit)

	ON_COMMAND(ID_TS_REMAP, OnTsRemap)
	ON_COMMAND(ID_EDIT_LINK_CENTERS, OnEditLinkCenters)
	ON_UPDATE_COMMAND_UI(ID_EDIT_LINK_CENTERS, OnUpdateEditLinkCenters)
	ON_COMMAND(ID_VIEW_TIME_OF_DAY, OnViewTimeOfDay)
	ON_COMMAND(ID_WINDOW_2DWINDOW, OnWindow2dwindow)
	ON_UPDATE_COMMAND_UI(ID_WINDOW_2DWINDOW, OnUpdateWindow2dwindow)
	ON_COMMAND(ID_VIEW_RELOADTEXTURES, OnViewReloadtextures)
	ON_COMMAND(ID_EDIT_SCRIPTS, OnEditScripts)
	ON_COMMAND(ID_SCRIPT_EDIT, OnEditScripts)
	ON_COMMAND(ID_VIEWHOME, OnViewHome)
	ON_COMMAND(ID_TEXTURESIZING_TILE4X4, OnTexturesizingTile4x4)
	ON_UPDATE_COMMAND_UI(ID_TEXTURESIZING_TILE4X4, OnUpdateTexturesizingTile4x4)
	ON_COMMAND(ID_TEXTURESIZING_TILE6X6, OnTexturesizingTile6x6)
	ON_UPDATE_COMMAND_UI(ID_TEXTURESIZING_TILE6X6, OnUpdateTexturesizingTile6x6)
	ON_COMMAND(ID_TEXTURESIZING_TILE8X8, OnTexturesizingTile8x8)
	ON_UPDATE_COMMAND_UI(ID_TEXTURESIZING_TILE8X8, OnUpdateTexturesizingTile8x8)
	ON_COMMAND(ID_FILE_DUMPTOFILE, OnDumpDocToText)
	ON_COMMAND(ID_TEXTURESIZING_REMOVECLIFFTEXMAPPING, OnRemoveclifftexmapping)
	ON_COMMAND(ID_TOGGLE_PITCH_AND_ROTATE, OnTogglePitchAndRotation)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CWorldBuilderDoc construction/destruction

CWorldBuilderDoc::CWorldBuilderDoc() :
	m_heightMap(NULL),
	m_undoList(NULL),
	m_maxUndos(MAX_UNDOS),
	m_curRedo(0),
	m_needAutosave(false),
	m_curWaypointID(0),
	m_numWaypointLinks(0),
	m_waypointTableNeedsUpdate(true),
	m_linkCenters(true),
	m_disableMapPrevGeneration(false),
	m_watchMapIni(false)
{
	memset(&m_mapIniLastWrite, 0, sizeof(m_mapIniLastWrite));

	// The old @todo "get from pref": undo depth is a setting now (Entity Finder >
	// Visual Settings), persisted as [MainFrame] MaxUndos.
	setMaxUndos(::AfxGetApp()->GetProfileInt("MainFrame", "MaxUndos", MAX_UNDOS));

	// Auto-reload map.ini watch: app-global toggle in WorldBuilder.ini.
	m_watchMapIni = (::AfxGetApp()->GetProfileInt("MapIni", "AutoWatch", 0) != 0);

    // Attempt to read AdrianeMapSettings.ini here
    if (!m_strPathName.IsEmpty()) {
        char folderPath[_MAX_PATH];
        strcpy(folderPath, m_strPathName);

        char* lastSlash = strrchr(folderPath, '\\');
        if (lastSlash)
            *lastSlash = '\0';

        CString individualMapSettings = CString(folderPath) + "\\AdrianeMapSettings.ini";

        if (PathFileExists(individualMapSettings)) {
            char buffer[8] = {0};
            GetPrivateProfileString("MapSettings", "disableMapPreview", "0", buffer, sizeof(buffer), individualMapSettings);
            m_disableMapPrevGeneration = (atoi(buffer) != 0);
        }
    }
}

CWorldBuilderDoc::~CWorldBuilderDoc()
{
#ifdef ONLY_ONE_AT_A_TIME
	if (m_heightMap != NULL ) {
		gAlreadyOpen = false;
	}
#endif
	REF_PTR_RELEASE(m_heightMap);
	REF_PTR_RELEASE(m_undoList);
}


/////////////////////////////////////////////////////////////////////////////
// CWorldBuilderDoc serialization

class MFCFileOutputStream : public OutputStream
{
protected:
	CFile *m_file;
public:
	MFCFileOutputStream(CFile *pFile):m_file(pFile) {};
	virtual Int write(const void *pData, Int numBytes) {
		Int numBytesWritten = 0;
		try {
			m_file->Write(pData, numBytes);
			numBytesWritten = numBytes;
		} catch(...) {}
		return(numBytesWritten);
	};
};

struct CachedChunk
{
	UnsignedByte *pData;
	Int size;
};

class CachedMFCFileOutputStream : public OutputStream
{
protected:
	CFile *m_file;
	std::list<CachedChunk> m_cachedChunks;
	Int m_totalBytes;
public:
	CachedMFCFileOutputStream(CFile *pFile):m_file(pFile), m_totalBytes(0) {};
	virtual Int write(const void *pData, Int numBytes) {
		UnsignedByte *tmp = new UnsignedByte[numBytes];
		memcpy(tmp, pData, numBytes);
		CachedChunk c;
		c.pData = tmp;
		c.size = numBytes;
		m_cachedChunks.push_back(c);
		DEBUG_LOG(("Caching %d bytes in chunk %d\n", numBytes, m_cachedChunks.size()));
		m_totalBytes += numBytes;
		return(numBytes);
	};
	virtual void flush(void) {
		while (m_cachedChunks.size() != 0)//!m_cachedChunks.empty())
		{
			CachedChunk c = m_cachedChunks.front();
			m_cachedChunks.pop_front();
			try {
				DEBUG_LOG(("Flushing %d bytes\n", c.size));
				m_file->Write(c.pData, c.size);
			} catch(...) {}
			delete[] c.pData;
			m_totalBytes -= c.size;
		}
	}
};

class CompressedCachedMFCFileOutputStream : public OutputStream
{
protected:
	CFile *m_file;
	std::list<CachedChunk> m_cachedChunks;
	Int m_totalBytes;
public:
	CompressedCachedMFCFileOutputStream(CFile *pFile):m_file(pFile), m_totalBytes(0) {};
	virtual Int write(const void *pData, Int numBytes) {
		UnsignedByte *tmp = new UnsignedByte[numBytes];
		memcpy(tmp, pData, numBytes);
		CachedChunk c;
		c.pData = tmp;
		c.size = numBytes;
		m_cachedChunks.push_back(c);
		//DEBUG_LOG(("Caching %d bytes in chunk %d\n", numBytes, m_cachedChunks.size()));
		m_totalBytes += numBytes;
		return(numBytes);
	};
	virtual void flush(void) {
		if (!m_totalBytes)
			return;
		UnsignedByte *srcBuffer = NEW UnsignedByte[m_totalBytes];
		UnsignedByte *insertPos = srcBuffer;
		while (m_cachedChunks.size() != 0)
		{
			CachedChunk c = m_cachedChunks.front();
			m_cachedChunks.pop_front();
			try {
				//DEBUG_LOG(("Flushing %d bytes\n", c.size));
				memcpy(insertPos, c.pData, c.size);
				insertPos += c.size;
			} catch(...) {}
			delete[] c.pData;
		}
		CompressionType compressionToUse = CompressionManager::getPreferredCompression();
		Dict *worldDict = MapObject::getWorldDict();
		if (worldDict)
		{
			Bool exists = FALSE;
			compressionToUse = (CompressionType)worldDict->getInt(TheKey_compression, &exists);
			if (!exists || compressionToUse > COMPRESSION_MAX || compressionToUse < COMPRESSION_MIN)
				compressionToUse = CompressionManager::getPreferredCompression();
		}

		Int compressedLen = CompressionManager::getMaxCompressedSize( m_totalBytes, compressionToUse );
		UnsignedByte *destBuffer = NEW UnsignedByte[compressedLen];
		compressedLen = CompressionManager::compressData( compressionToUse, srcBuffer, m_totalBytes, destBuffer, compressedLen );
		DEBUG_LOG(("Compressed %d bytes to %d bytes - compression of %g%%\n", m_totalBytes, compressedLen,
			compressedLen/(Real)m_totalBytes*100.0f));
		DEBUG_ASSERTCRASH(compressedLen, ("Failed to compress!\n"));
		if (compressedLen)
		{
			m_file->Write(destBuffer, compressedLen);
		}
		else
		{
			m_file->Write(srcBuffer, m_totalBytes);
		}
		delete[] srcBuffer;
		srcBuffer = NULL;
		delete[] destBuffer;
		destBuffer = NULL;
	}
};

// Static helper to get the validated game directory path
static CString GetGameDirectory()
{
	CString gameDir = AfxGetApp()->GetProfileString("WorldbuilderApp", "GameDirectory", "");

	if (gameDir.IsEmpty()) {
		// Try fallback
		gameDir = AfxGetApp()->GetProfileString("WorldbuilderApp", "OpenDirectory", "");
	}

	if (gameDir.IsEmpty()) {
		AfxMessageBox(
			"Unable to locate the game directory because it has not been set in your World Builder settings."
			" To fix this, open your WorldBuilder settings file and add:\n\n"
			"[WorldbuilderApp]\nGameDirectory=YourGameFolderPath\n\n"
			"Example:\nGameDirectory=C:\\Program Files (x86)\\Command and Conquer Generals Zero Hour",
			MB_ICONEXCLAMATION | MB_OK
		);
		return "";
	}

	return gameDir;
}


void CWorldBuilderDoc::OnViewDisableMapPrevGen() 
{
	m_disableMapPrevGeneration = !m_disableMapPrevGeneration;

	if (m_strPathName.IsEmpty()) {
		AfxMessageBox(
			_T("Mate you still havent save the map, please do that first thank you."),
		MB_OK | MB_ICONWARNING);
		return;
	}

	if(m_disableMapPrevGeneration){
		AfxMessageBox(
			_T("Warning: Map preview generation has been disabled for this map.\n\n"
			"You can re-enable it anytime by clicking the toggle button again.\n"
			"This setting is saved with the map and will persist when reopening.\n\n"
			"If you regret this decision, you can always delete the AdrianeMapSettings.ini "
			"file from your map folder... assuming you can actually find it, you caveman."),
		MB_OK | MB_ICONWARNING);
	} else {
		AfxMessageBox(
			_T("Map preview generation has been re-enabled for this map."),
		MB_OK | MB_ICONEXCLAMATION);
	}

	// Build INI path based on current document path
    if (!m_strPathName.IsEmpty()) {
		
        char folderPath[_MAX_PATH];
        strcpy(folderPath, m_strPathName);
        char* lastSlash = strrchr(folderPath, '\\');
        if (lastSlash) *lastSlash = '\0';

        CString individualMapSettings = CString(folderPath) + "\\AdrianeMapSettings.ini";

        if (m_disableMapPrevGeneration) {
            WritePrivateProfileString("MapSettings", "disableMapPreview", "1", individualMapSettings);
        } else {
            WritePrivateProfileString("MapSettings", "disableMapPreview", NULL, individualMapSettings);
        }
    }
}

void CWorldBuilderDoc::OnUpdateDisableMapPrevGen(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_disableMapPrevGeneration?1:0);
}

void CWorldBuilderDoc::Serialize(CArchive& ar)
{
	ar.Flush();
	m_waypointTableNeedsUpdate = true;
	if (ar.IsStoring() && m_heightMap)
	{	
		try {
			Int i;
			MapPreview mPreview;

			char folderPath[_MAX_PATH];
			strcpy(folderPath, m_strPathName);

			// Remove the filename to get the map folder
			char* lastSlash = strrchr(folderPath, '\\');
			if (lastSlash) {
				*lastSlash = '\0';
			}

			DWORD attr = GetFileAttributes(folderPath);
			if (attr == (DWORD)-1 || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
				CreateDirectory(folderPath, NULL); // create folder if missing
			}

			CString individualMapSettings = CString(folderPath) + "\\AdrianeMapSettings.ini";
			bool perMapDisablePreview = false;

			if (PathFileExists(individualMapSettings)) {
				// File exists → read value
				char buffer[8] = {0};
				GetPrivateProfileString("MapSettings", "disableMapPreview", "0", buffer, sizeof(buffer), individualMapSettings);
				perMapDisablePreview = (atoi(buffer) != 0);

				// If user changed state and it's now enabled, clear the key
				if (!perMapDisablePreview) {
					WritePrivateProfileString("MapSettings", "disableMapPreview", NULL, individualMapSettings);
				}
			} 
			else {
				// File doesn't exist → create only if disabling preview
				if (perMapDisablePreview) {
					WritePrivateProfileString("MapSettings", "disableMapPreview", "1", individualMapSettings);
				}
			}

			// Generate preview only if not disabled
			if (!perMapDisablePreview) {
				mPreview.save(ar.GetFile()->GetFilePath());
			}

			CompressedCachedMFCFileOutputStream theStream(ar.GetFile());
			DataChunkOutput *chunkWriter = new DataChunkOutput(&theStream);
			

			m_heightMap->saveToFile(*chunkWriter);
 			/***************WAYPOINTS DATA ***************/
			chunkWriter->openDataChunk("WaypointsList", 	K_WAYPOINTS_VERSION_1);
			chunkWriter->writeInt(this->m_numWaypointLinks);
			for (i=0; i<m_numWaypointLinks; i++) {
				chunkWriter->writeInt(this->m_waypointLinks[i].waypoint1);
				chunkWriter->writeInt(this->m_waypointLinks[i].waypoint2);
			}
			chunkWriter->closeDataChunk();

			delete chunkWriter;
			chunkWriter = NULL;
			theStream.flush();
		} catch(...) {
			const char *msg = "WorldHeightMapEdit::WorldHeightMapEdit  height map file write failed: ";
			AfxMessageBox(msg);
			return;
		}
	}
	else
	{
		WorldHeightMapEdit *pOldHeightMap = m_heightMap;
		CString pth = ar.GetFile()->GetFilePath();
		CachedFileInputStream theInputStream;
		if (theInputStream.open(AsciiString((const char *)pth))) 
		try {

			WbApp()->selectPointerTool();
			PolygonTrigger::deleteTriggers();
			ChunkInputStream *pStrm = &theInputStream;

			// Read the logical data (map objects, waypoints, etc.)
			WorldHeightMap *terrainHeightMap = new WorldHeightMap(pStrm, true);
			REF_PTR_RELEASE(terrainHeightMap);
			pStrm->absoluteSeek(0);
			// Read & keep the graphical data.
			m_heightMap = NEW_REF(WorldHeightMapEdit, (pStrm));
			pStrm->absoluteSeek(0);
			try {
				DataChunkInput file( pStrm );
				if (file.isValidFileType()) {	// Backwards compatible files aren't valid data chunk files.
					// Read the waypoints.
					file.registerParser( AsciiString("WaypointsList"), AsciiString::TheEmptyString, ParseWaypointDataChunk );
					if (!file.parse(this)) {
						throw(ERROR_CORRUPT_FILE_FORMAT);
					}
				}
			} catch(...) {
				// just eat the error - legacy files aren't chunk format.
			}
			theInputStream.close();

			validate();

			compressWaypointIds(); // remove any unused waypoint ids.
			WbView3d * p3View = Get3DView();
			if (p3View) {
				p3View->resetRenderObjects();
			}
			m_heightMap->optimizeTiles(); // force to optimize tileset
			SetHeightMap(m_heightMap, true);
			Coord3D center;
			center.x = MAP_XY_FACTOR*m_heightMap->getXExtent()/2; 
			center.y = MAP_XY_FACTOR*m_heightMap->getYExtent()/2;
			center.x -= m_heightMap->getBorderSize();
			center.y -= m_heightMap->getBorderSize();
			/* update objects. */
			AsciiString startingCamName = TheNameKeyGenerator->keyToName(TheKey_InitialCameraPosition);
			
			TheLayersList->resetLayers();
			AsciiString layerName;
			Bool exists;

			// always assign unique IDs. The things will still live in the correct layers, so this isn't
			// an especially big deal.
			MapObject::fastAssignAllUniqueIDs();

			TheLayersList->disableUpdates();
			MapObject *pMapObj = MapObject::getFirstMapObject();
			while (pMapObj) {
								
				// Then, add it to the Layers List
				layerName = pMapObj->getProperties()->getAsciiString(TheKey_objectLayer, &exists);
				if (exists) {
					TheLayersList->addMapObjectToLayersList(pMapObj, layerName);
				} else {
					TheLayersList->addMapObjectToLayersList(pMapObj);
				}

				MapObject *pTemplateObj = 	ObjectOptions::getObjectNamed(pMapObj->getName());
				if (pTemplateObj) {
					pMapObj->setColor(pTemplateObj->getColor());
				}
				if (pMapObj->isWaypoint()) {
					if (pMapObj->getWaypointID() >= m_curWaypointID) {
						m_curWaypointID = pMapObj->getWaypointID();
					}
					if (startingCamName == pMapObj->getWaypointName()) {
						center = *pMapObj->getLocation();
					}
				}
				pMapObj = pMapObj->getNext();
			}

			PolygonTrigger* polyTrigger = PolygonTrigger::getFirstPolygonTrigger();
			// Add the triggers to the layers list.
			while (polyTrigger) {
				layerName = polyTrigger->getLayerName();
				TheLayersList->addPolygonTriggerToLayersList(polyTrigger, layerName);

				polyTrigger = polyTrigger->getNext();
			}
			
			TheLayersList->enableUpdates();

			TerrainMaterial::updateTextures(m_heightMap);

			REF_PTR_RELEASE(m_undoList);
			m_curRedo = 0;
			POSITION pos = GetFirstViewPosition();
			while (pos != NULL)
			{
				CView* pView = GetNextView(pos);
				WbView* pWView = (WbView *)pView;
				ASSERT_VALID(pWView);
				pWView->setCenterInView(center.x/MAP_XY_FACTOR, center.y/MAP_XY_FACTOR);
			}
			REF_PTR_RELEASE(pOldHeightMap);
			if (p3View) {
				p3View->setDefaultCamera();
			}

		} catch(...) {
			m_heightMap = pOldHeightMap;
		}

		// note - mHeight map has ref count of 1.
	}
}

AsciiString ConvertToNonGCName(AsciiString name, Bool checkTemplate=true)
{
	char oldName[256];
	char newName[256];
	strcpy(oldName, name.str());
	strcpy(newName, oldName+strlen("GC_"));
	AsciiString swapName;
	swapName.set(newName);
	if (checkTemplate)
	{
		const ThingTemplate *tt = TheThingFactory->findTemplate(swapName);
		if (tt) {
			return swapName;
		}
		return AsciiString::TheEmptyString;
	}
	return swapName;
}

AsciiString ConvertName(AsciiString name)
{
	char oldName[256];
	char newName[256];
	strcpy(oldName, name.str());
	strcpy(newName, "GLA");
	strcat(newName, oldName+strlen("Fundamentalist"));
	AsciiString swapName;
	swapName.set(newName);
	const ThingTemplate *tt = TheThingFactory->findTemplate(swapName);
	if (tt) {
		return swapName;
	}
	return AsciiString::TheEmptyString;
}

AsciiString ConvertFaction(AsciiString name)
{
	char oldName[256];
	char newName[256];
	strcpy(oldName, name.str());
	strcpy(newName, "FactionGLA");
	strcat(newName, oldName+strlen("FactionFundamentalist"));
	AsciiString swapName;
	swapName.set(newName);
	const PlayerTemplate* pt = ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY(swapName));
	if (pt) {
		return swapName;
	}
	return AsciiString::TheEmptyString;
}

void CWorldBuilderDoc::validate(void)
{
	DEBUG_LOG(("Validating\n"));

	Dict swapDict;
	Bool changed = false;
	AsciiString swapName;

	Bool needToFixTeams = false;

	// verify/fix the build lists
	for (int side=0; side<TheSidesList->getNumSides(); side++) {
		SidesInfo *pSide = TheSidesList->getSideInfo(side); 

		AsciiString tmplname = pSide->getDict()->getAsciiString(TheKey_playerFaction);
		AsciiString playername = pSide->getDict()->getAsciiString(TheKey_playerName);
		if (tmplname.isEmpty()) {
			continue; // Neutral player has empty template. jba. [8/8/2003]
		}
		const PlayerTemplate* pt = ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY(tmplname));
		if (!pt) {
			DEBUG_LOG(("Player '%s' Faction '%s' could not be found in sides list!\n", playername.str(), tmplname.str()));
			if (tmplname.startsWith("FactionFundamentalist")) {
				swapName = ConvertFaction(tmplname);
				if (swapName != AsciiString::TheEmptyString) {
					DEBUG_LOG(("Changing Faction from %s to %s\n", tmplname.str(), swapName.str()));
					pSide->getDict()->setAsciiString(TheKey_playerFaction, swapName);
				}
			}
		}

		BuildListInfo *pBuild = pSide->getBuildList();
		while (pBuild) {
			AsciiString name = pBuild->getTemplateName();
			if (name.startsWith("Fundamentalist")) {
				swapName = ConvertName(name);
				if (swapName != AsciiString::TheEmptyString) {
					DEBUG_LOG(("Changing BuildList from %s to %s\n", name.str(), swapName.str()));
					pBuild->setTemplateName(swapName);
				}
			}
			pBuild = pBuild->getNext();
		}
	}


#define FIX_TEAM(key)																	\
	type = teamDict->getAsciiString(key, &exists);			\
	if (exists) {																				\
		if (type.startsWith("Fundamentalist")) {					\
			swapName = ConvertName(type);										\
			if (swapName != AsciiString::TheEmptyString) {	\
				DEBUG_LOG(("Changing Team Ref from %s to %s\n", type.str(), swapName.str())); \
				teamDict->setAsciiString(key, swapName);			\
			}																								\
		}																									\
	}																										\

	// verify/fix the team definitions
	Int numTeams = TheSidesList->getNumTeams();
	for (Int team=0; team<numTeams; team++)
	{
		TeamsInfo *ti = TheSidesList->getTeamInfo(team);
		Dict* teamDict = ti->getDict();
		AsciiString type;
		Bool exists;
		FIX_TEAM(TheKey_teamUnitType1)
		FIX_TEAM(TheKey_teamUnitType2)
		FIX_TEAM(TheKey_teamUnitType3)
		FIX_TEAM(TheKey_teamUnitType4)
		FIX_TEAM(TheKey_teamUnitType5)
		FIX_TEAM(TheKey_teamUnitType6)
		FIX_TEAM(TheKey_teamUnitType7)
	}

	MapObject *pMapObj;
	for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext())
	{
		// there is no validation code for these items as of yet.
		if (pMapObj->isScorch() || pMapObj->isWaypoint() || pMapObj->isLight() || pMapObj->getFlag(FLAG_ROAD_FLAGS) || pMapObj->getFlag(FLAG_BRIDGE_FLAGS))
		{
			continue;
		}

		// at this point, only objects with models and teams should be left to process

		// start by verifying the ThingTemplate for the object.
		// swapDict contains a 'history' of missing model swaps done this load, so all objects with a 
		// particular name are replaced with the exact same model.
		AsciiString name = pMapObj->getName();
		if (pMapObj->getThingTemplate() == NULL)
		{
			Bool exists = false;
			swapName = swapDict.getAsciiString(NAMEKEY(name), &exists);

			// quick hack to make loading models with "Fundamentalist" switch to "GLA"
			if (name.startsWith("Fundamentalist")) {
				swapName = ConvertName(name);
				if (swapName != AsciiString::TheEmptyString) {
					swapDict.setAsciiString(NAMEKEY(name), swapName);
					exists = true;
				}
			}

			// quick hack to remove "GC_" objects from Generals mission disk maps.
			if (name.startsWith("GC_")) {
				swapName = ConvertToNonGCName(name);
				if (swapName != AsciiString::TheEmptyString) {
					swapDict.setAsciiString(NAMEKEY(name), swapName);
					exists = true;
				}
			}

			if (!exists) {
#ifdef RTS_HAS_QT
				Bool qtHandled = false;
				Bool qtIgnored = false;
				{
					int allowable[ES_NUM_SORTING_TYPES];
					int allowCount = 0;
					for (int i = ES_FIRST; i<ES_NUM_SORTING_TYPES; i++)	{
						allowable[allowCount++] = i;
					}
					char qtPicked[256];
					qtPicked[0] = 0;
					int qtRc = WBQtReplaceUnit_Run(::AfxGetMainWnd() ? ::AfxGetMainWnd()->GetSafeHwnd() : NULL, name.str(), allowable, allowCount, false, qtPicked, sizeof(qtPicked));
					if (qtRc >= 0) {
						qtHandled = true;
						if (qtRc == 1) {
							const ThingTemplate* qtThing = TheThingFactory->findTemplate(AsciiString(qtPicked));
							if (qtThing) {
								swapName = qtThing->getName();
								swapDict.setAsciiString(NAMEKEY(name), swapName);
							}
						} else if (qtRc == 2) {
							// User clicked "Proceed without replace"
							DEBUG_LOG(("User opted to proceed without replacing unit '%s'\n", name.str()));
							qtIgnored = true;
						}
					}
				}
				if (qtIgnored) {
					break;  // Skip this object and move to the next one
				}
				if (!qtHandled) {
#endif
				ReplaceUnitDialog dlg;
				dlg.setMissing(name);
				for (int i = ES_FIRST; i<ES_NUM_SORTING_TYPES; i++)	{
					dlg.SetAllowableType((EditorSortingType)i);
				}
				dlg.SetFactionOnly(false);
				int result = dlg.DoModal();  // Run the dialog and capture the result
				if (result == IDOK) {
					// User clicked OK and selected a replacement
					const ThingTemplate* thing = dlg.getPickedThing();
					if (thing) {
						swapName = thing->getName();
						swapDict.setAsciiString(NAMEKEY(name), swapName);
					}
				} else if (result == IDIGNORE) {
					// User clicked "Proceed without replace"
					DEBUG_LOG(("User opted to proceed without replacing unit '%s'\n", name.str()));
					// Optionally, you can continue to the next object or handle as necessary
					break;  // Skip this object and move to the next one
				}
#ifdef RTS_HAS_QT
				}
#endif
			}
			swapName = swapDict.getAsciiString(NAMEKEY(name), &exists);
			if (exists) 
			{
				const ThingTemplate *tt = TheThingFactory->findTemplate(swapName);
				if (tt) {
					changed = true;
					pMapObj->setName(swapName);
					pMapObj->setThingTemplate(tt);
					DEBUG_LOG(("Changing Map Object from %s to %s\n", name.str(), swapName.str()));
				}
			}
		}


		// the following code verifies and fixes the team name, player name, and faction linkages
		Bool exists;
		AsciiString teamName = pMapObj->getProperties()->getAsciiString(TheKey_originalOwner, &exists);
		if (exists) {
			TeamsInfo *teamInfo = TheSidesList->findTeamInfo(teamName);
			if (teamInfo) {
				AsciiString teamOwner = teamInfo->getDict()->getAsciiString(TheKey_teamOwner);
				SidesInfo* pSide = TheSidesList->findSideInfo(teamOwner);
				if (pSide) {
//					Bool hasColor = false;
					AsciiString tmplname = pSide->getDict()->getAsciiString(TheKey_playerFaction);
					AsciiString playername = pSide->getDict()->getAsciiString(TheKey_playerName);
					if (tmplname.isEmpty()) {
						continue; // Neutral player has empty template. jba. [8/8/2003]
					}
					const PlayerTemplate* pt = ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY(tmplname));
					if (!pt) {
						DEBUG_LOG(("Player '%s' Faction '%s' could not be found in sides list!\n", playername.str(), tmplname.str()));
						if (tmplname.startsWith("FactionFundamentalist")) {
							swapName = ConvertFaction(tmplname);
							if (swapName != AsciiString::TheEmptyString) {
								DEBUG_LOG(("Changing Faction from %s to %s\n", tmplname.str(), swapName.str()));
								pSide->getDict()->setAsciiString(TheKey_playerFaction, swapName);
							}
						}
					}
				} else {
					needToFixTeams = true;
					DEBUG_LOG(("Side '%s' could not be found in sides list!\n", teamOwner.str()));
				}
			} else {
				needToFixTeams = true;
				DEBUG_LOG(("Team '%s' could not be found in sides list!\n", teamName.str()));
			}
		} else {
			needToFixTeams = true;
			DEBUG_LOG(("Object '%s' does not have a team at all!\n", name.str()));
		}
	}
	if (needToFixTeams) {
		AfxMessageBox(IDS_NEED_TO_FIX_TEAMS, MB_OK|MB_ICONERROR);
	}
}

// Build "<map folder>\map.ini" from the current document path; empty if no map is open.
static AsciiString currentMapIniPath(const CString &mapPathName)
{
	if (mapPathName.IsEmpty())
		return AsciiString::TheEmptyString;
	AsciiString iniPath = (LPCTSTR)mapPathName;
	while (iniPath.getLength() && iniPath.getCharAt(iniPath.getLength()-1) != '\\')
		iniPath.removeLastChar();
	iniPath.concat("map.ini");
	return iniPath;
}

// File > Map.ini > Reload map.ini: unload the current overrides and re-run the loader,
// without reopening the map. Object/Weapon/Science/SpecialPower/Water reload cleanly;
// the report warns if the file also touches stores that can't be cleanly torn down.
void CWorldBuilderDoc::OnReloadMapIni()
{
	AsciiString iniPath = currentMapIniPath(m_strPathName);
	if (iniPath.isEmpty()) {
		AfxMessageBox("Save or open a map first.", MB_ICONEXCLAMATION | MB_OK);
		return;
	}
	if (!TheFileSystem->doesFileExist(iniPath.str())) {
		AfxMessageBox("This map has no map.ini file to reload.", MB_ICONINFORMATION | MB_OK);
		return;
	}
	// Drop the current overrides, then preview the reload and apply only if the user OKs.
	unloadMapIniOverrides();
	confirmAndLoadMapIni(iniPath, "Reload map.ini");
}

// File > Map.ini > Check map.ini: parse the map.ini and report validity + a summary,
// WITHOUT installing any overrides (dry run) -- lets a mapper vet a file before loading.
void CWorldBuilderDoc::OnCheckMapIni()
{
	AsciiString iniPath = currentMapIniPath(m_strPathName);
	if (iniPath.isEmpty()) {
		AfxMessageBox("Save or open a map first.", MB_ICONEXCLAMATION | MB_OK);
		return;
	}
	if (!TheFileSystem->doesFileExist(iniPath.str())) {
		AfxMessageBox("This map has no map.ini file to check.", MB_ICONINFORMATION | MB_OK);
		return;
	}
	// Overrides may already be live from the open-time load; drop them so the dry run
	// starts clean and leaves nothing changed afterward.
	unloadMapIniOverrides();
	CString report;
	doLoadMapIni(iniPath, MAPINI_DRYRUN, report);
	showScrollableInfoDialog("Check map.ini", report, /*applyMode=*/false);
}

// Read <map folder>\map.ini's last-write time into out; false if it can't be stat'd.
static bool getMapIniWriteTime(const AsciiString &iniPath, FILETIME *out)
{
	WIN32_FILE_ATTRIBUTE_DATA fad;
	if (!::GetFileAttributesExA(iniPath.str(), GetFileExInfoStandard, &fad))
		return false;
	*out = fad.ftLastWriteTime;
	return true;
}

// File > Map.ini > Auto-reload map.ini on change: toggle the watch. When turned on we
// snapshot the current mtime so only real edits (not the enabling click) trigger a reload.
// Persisted in WorldBuilder.ini [MapIni] AutoWatch. CMainFrame's timer calls pollMapIniWatch.
void CWorldBuilderDoc::OnToggleWatchMapIni()
{
	m_watchMapIni = !m_watchMapIni;
	::AfxGetApp()->WriteProfileInt("MapIni", "AutoWatch", m_watchMapIni ? 1 : 0);
	if (m_watchMapIni) {
		AsciiString iniPath = currentMapIniPath(m_strPathName);
		if (!iniPath.isEmpty())
			getMapIniWriteTime(iniPath, &m_mapIniLastWrite);
	}
}

void CWorldBuilderDoc::OnUpdateWatchMapIni(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_watchMapIni ? 1 : 0);
}

// File > Map.ini > Verbose report: when on, the load/check report lists each object as an
// INI block with its per-module edits inside. App-global toggle in WorldBuilder.ini.
void CWorldBuilderDoc::OnToggleVerboseMapIni()
{
	int now = ::AfxGetApp()->GetProfileInt("MapIni", "VerboseReport", 0) ? 0 : 1;
	::AfxGetApp()->WriteProfileInt("MapIni", "VerboseReport", now);
}

void CWorldBuilderDoc::OnUpdateVerboseMapIni(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(::AfxGetApp()->GetProfileInt("MapIni", "VerboseReport", 0) ? 1 : 0);
}

// Called from CMainFrame's timer. When watching, reload the moment map.ini's mtime
// advances (external editor saved it). Silent on success; only surfaces errors.
void CWorldBuilderDoc::pollMapIniWatch()
{
	if (!m_watchMapIni)
		return;
	AsciiString iniPath = currentMapIniPath(m_strPathName);
	if (iniPath.isEmpty() || !TheFileSystem->doesFileExist(iniPath.str()))
		return;
	FILETIME now;
	if (!getMapIniWriteTime(iniPath, &now))
		return;
	if (::CompareFileTime(&now, &m_mapIniLastWrite) == 0)
		return;	// unchanged
	m_mapIniLastWrite = now;
	// Auto-reload applies straight away (no confirm dialog -- the mapper's own save is the
	// intent); only interrupt them on a hard parse error.
	unloadMapIniOverrides();
	CString report;
	bool ok = doLoadMapIni(iniPath, MAPINI_INSTALL, report);
	if (!ok)
		showScrollableInfoDialog("Auto-reload map.ini", report, /*applyMode=*/false);
}

void CWorldBuilderDoc::OnJumpToMapFolder()
{
	try {
		// DoFileSave();
		DEBUG_LOG(("strTitle=%s strPathName=%s\n", m_strTitle, m_strPathName));

		char folderPath[_MAX_PATH];
		strcpy(folderPath, m_strPathName);

		// Remove the filename to get the folder path
		char* lastSlash = strrchr(folderPath, '\\');
		if (lastSlash) {
			*lastSlash = '\0';
		}

		DWORD attr = GetFileAttributes(folderPath);
		if (attr != (DWORD)-1 && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
			ShellExecute(NULL, "open", folderPath, NULL, NULL, SW_SHOW);
		} else {
			AfxMessageBox("The map folder does not exist yet. Save the map first.", MB_ICONEXCLAMATION | MB_OK);
		}

	} catch (...) {
	}
}

void CWorldBuilderDoc::OnJumpToMapFolderWBData()
{
    try {
        char documentsPath[MAX_PATH] = {0};

        if (SHGetSpecialFolderPathA(NULL, documentsPath, CSIDL_PERSONAL, FALSE))
        {
            char targetPath[MAX_PATH];
            sprintf(targetPath, "%s\\Command and Conquer Generals Zero Hour Data", documentsPath);

            DWORD attr = GetFileAttributes(targetPath);
            if (attr != (DWORD)-1 && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                ShellExecute(NULL, "open", targetPath, NULL, NULL, SW_SHOW);
            } else {
                AfxMessageBox("The Generals Zero Hour Data folder does not exist.", MB_ICONEXCLAMATION | MB_OK);
            }
        }
        else {
            AfxMessageBox("Unable to locate the Documents folder.", MB_ICONERROR | MB_OK);
        }

    } catch (...) {}
}

void CWorldBuilderDoc::OnJumpToAutoSaveFolder()
{
	try {
		CString folderPath;
		folderPath.Format("%s\\AutoSaves", TheGlobalData->getPath_UserData().str());

		// Ensure the folder exists before trying to open it
		DWORD attr = GetFileAttributes(folderPath);
		if (attr != (DWORD)-1 && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
			ShellExecute(NULL, "open", folderPath, NULL, NULL, SW_SHOWNORMAL);
		} else {
			AfxMessageBox("How the fuck the autosave folder does not exist on your data yet? call adriane.", MB_ICONEXCLAMATION | MB_OK);
		}

	} catch (...) {
		// Optional: handle unexpected errors
	}
}

void CWorldBuilderDoc::OnGenerateMapStrAndIni()
{
	try {
		char folderPath[_MAX_PATH];
		strcpy(folderPath, m_strPathName);

		// Remove the filename to get the map folder
		char* lastSlash = strrchr(folderPath, '\\');
		if (lastSlash) {
			*lastSlash = '\0';
		}

		// Check if the folder exists
		DWORD attr = GetFileAttributes(folderPath);
		if (attr == (DWORD)-1 || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
			AfxMessageBox("Map folder does not exist. Please save the map first.", MB_ICONEXCLAMATION | MB_OK);
			return;
		}

		CString strPath = CString(folderPath) + "\\map.str";
		CString iniPath = CString(folderPath) + "\\map.ini";

		BOOL createdAnyFile = FALSE;

		// Only generate map.str if it doesn't exist
		if (GetFileAttributes(strPath) == (DWORD)-1) {
			CStdioFile strFile;
			if (strFile.Open(strPath, CFile::modeCreate | CFile::modeWrite | CFile::typeText)) {
				strFile.WriteString("//==============================================================================\n");
				strFile.WriteString("// MAP.STR - Custom String Table for Map\n");
				strFile.WriteString("//------------------------------------------------------------------------------\n");
				strFile.WriteString("// Notes:\n");
				strFile.WriteString("// - Each entry starts with a label (e.g., Sample:01), followed by the text in quotes.\n");
				strFile.WriteString("// - You can use up to 3 newline breaks (\\n) in a single string.\n");
				strFile.WriteString("// - SCRIPT: prefixed entries are meant for use in scripting (e.g., timers or UI boxes).\n");
				strFile.WriteString("//==============================================================================\n\n");

				strFile.WriteString("//------------------------------------------------------------------------------\n");
				strFile.WriteString("// Sample simple message (no line breaks)\n");
				strFile.WriteString("//------------------------------------------------------------------------------\n");
				strFile.WriteString("Sample:01\n");
				strFile.WriteString("\"Bold Text\"\n");
				strFile.WriteString("End\n\n");

				strFile.WriteString("//------------------------------------------------------------------------------\n");
				strFile.WriteString("// Sample multiline message (maximum of 3 line breaks)\n");
				strFile.WriteString("//------------------------------------------------------------------------------\n");
				strFile.WriteString("Sample:02\n");
				strFile.WriteString("\"Bold Header:\n\\nSample Message (non-bold)\n\\nAnother message line\n\\nFinal message line\"\n");
				strFile.WriteString("End\n\n");

				strFile.WriteString("//------------------------------------------------------------------------------\n");
				strFile.WriteString("// Script-related message used for UI popups or map timers\n");
				strFile.WriteString("//------------------------------------------------------------------------------\n");
				strFile.WriteString("SCRIPT:_PeaceTimeActivated\n");
				strFile.WriteString("\"Hint:\n\\nGeneral Adriane alt-tabbed to check memes.\n\\nPerfect time for a base tour.\"\n");
				strFile.WriteString("End\n\n");

				strFile.WriteString("SCRIPT:TimerName\n");
				strFile.WriteString("\"Commander Newgate will arrive in:\"\n");
				strFile.WriteString("End\n");

				strFile.Close();
				createdAnyFile = TRUE;
			}
		}

		// Only generate map.ini if it doesn't exist
		if (GetFileAttributes(iniPath) == (DWORD)-1) {
			CStdioFile iniFile;
			if (iniFile.Open(iniPath, CFile::modeCreate | CFile::modeWrite | CFile::typeText)) {
				iniFile.WriteString("; map.ini - custom INI file for map overrides\n");
				iniFile.WriteString("; Add your unit, object, or behavior overrides here.\n");
				iniFile.Close();
				createdAnyFile = TRUE;
			}
		}

		if (createdAnyFile) {
			AfxMessageBox("Template map.str and/or map.ini file(s) have been created.", MB_OK | MB_ICONINFORMATION);
		} else {
			AfxMessageBox("Both map.str and map.ini already exist. No new files were created.", MB_OK | MB_ICONINFORMATION);
		}

		OnJumpToMapFolder();
	} catch (...) {
		AfxMessageBox("An error occurred while generating the template files.", MB_ICONERROR | MB_OK);
	}
}
void CWorldBuilderDoc::OnOpenWorldbuilderSettings()
{
	try {
		// Build the path to the INI file
		CString iniPath;
		iniPath.Format("%sWorldBuilder.ini", TheGlobalData->getPath_UserData().str());

		// Open the file with the default editor (usually Notepad)
		ShellExecute(NULL, "open", iniPath, NULL, NULL, SW_SHOW);

	} catch (...) {
	}
}

void CWorldBuilderDoc::OpenGameFolder(Bool data /*= false*/)
{
	try {
		CString gameDir = GetGameDirectory();

		if (gameDir.IsEmpty()) {
			OnOpenWorldbuilderSettings();
			return;
		}

		CString targetPath = gameDir;
		if (data) {
			targetPath += "\\Data";
		}

		if (!PathFileExists(targetPath)) {
			CString msg;
			msg.Format("The folder was not found:\n%s\n\nPlease make sure it exists in your game directory.", targetPath);
			AfxMessageBox(msg, MB_ICONEXCLAMATION | MB_OK);
			return;
		}

		ShellExecute(NULL, "open", targetPath, NULL, NULL, SW_SHOWNORMAL);

	} catch (...) {
		AfxMessageBox("An unexpected error occurred while trying to open the game folder.", MB_ICONERROR | MB_OK);
	}
}

void CWorldBuilderDoc::OnOpenGameFolder()
{
	OpenGameFolder(false); // opens main game directory
}

void CWorldBuilderDoc::OnOpenDataFolder()
{
	OpenGameFolder(true); // opens Data subfolder
}

void CWorldBuilderDoc::OnJumpToGameWithDebug(){
	OnJumpToGame(true, false);
}

void CWorldBuilderDoc::OnJumpToGameWithoutDebug(){
	OnJumpToGame(false, false);
}

void CWorldBuilderDoc::OnJumpToGameWithWaveEdit(){
	OnJumpToGame(false, true);
}

void CWorldBuilderDoc::OnJumpToGame(Bool withDebug, Bool waveEdit)
{
	try {
		CString gameDir = GetGameDirectory();

		if (gameDir.IsEmpty()) {
			OnOpenWorldbuilderSettings();
			return;
		}

		if (m_strPathName.IsEmpty()) {
			AfxMessageBox(
				"Nice try, genius.\nMaybe save the map before pulling off stunts like this?",
				MB_ICONEXCLAMATION | MB_OK
			);

			if (!DoSave(NULL)) {
				AfxMessageBox(
					"Why are you doing this to me!?, I will not be able to launch the game without you saving the damn map!",
					MB_ICONEXCLAMATION | MB_OK
				);
				return;
			}
		}

		int result = AfxMessageBox(
			"Hold up!\nMonsieur, do you want us to save your map first?\nIf you only want to preview your current map file, then hit No.",
			MB_ICONWARNING | MB_YESNO
		);

		if (result == IDYES) {
			DoFileSave();
		}

		CString filename;
		DEBUG_LOG(("strTitle=%s strPathName=%s\n", m_strTitle, m_strPathName));
		if (strstr(m_strPathName, TheGlobalData->getPath_UserData().str()) != NULL)
			filename.Format("%sMaps\\%s", TheGlobalData->getPath_UserData().str(), m_strTitle);
		else
			filename.Format("Maps\\%s", m_strTitle);

		CString args = CString("-win -file \"") + filename + "\"";
		if (withDebug) {
			args = CString("-scriptDebug ") + args;
		}

		CString gameExePath;
		if (waveEdit) {
			args = CString("-useWaveEditor ") + args;
			gameExePath.Format("%s\\generals_wave.exe", gameDir);

			AfxMessageBox(
				"You are about to run the game with wave edit mode ON. Please take note:\n\n"
				"Hotkeys:\n"
				" 1              : Enable/Disable Wave Edit Mode\n"
				" Ctrl + S       : Save\n"
				" Ctrl + R       : Reload/Clear\n"
				" Ctrl + Z       : Undo (max of 15)\n"
				" Left Click     : Start placing waves\n"
				" 2nd Left Click : Add end of wave point\n"
				" Space          : Cycle Wave Type",
				MB_ICONEXCLAMATION | MB_OK
			);
		} else {
			gameExePath.Format("%s\\generals.exe", gameDir);
		}

		// Check if the executable exists
		if (!PathFileExists(gameExePath)) {
			CString msg;
			msg.Format("The game executable was not found:\n%s\n\nPlease verify your game directory setting.", gameExePath);
			AfxMessageBox(msg, MB_ICONEXCLAMATION | MB_OK);
			return;
		}

		DEBUG_LOG(("Loading gameExePath=%s\n", gameExePath)); 

		ShellExecute(NULL, "open", 
			gameExePath, 
			args, 
			NULL, 
			SW_SHOWNORMAL
		);
		
	} catch (...) {
		// Optional: log or handle exception
	}
}

BOOL CWorldBuilderDoc::DoFileSave()
{
	DWORD dwAttrib = GetFileAttributes(m_strPathName);
	if (dwAttrib & FILE_ATTRIBUTE_READONLY)
	{
		if (dwAttrib != 0xFFFFFFFF) {
			::AfxMessageBox(IDS_FILE_IS_READONLY);
			return FALSE;
		}
		// File does not exist, dwAttrib==0xffffffff
		// we do not have read-write access or the file does not (now) exist
		if (!DoSave(NULL))
		{
			TRACE0("Warning: File save with new name failed.\n");
			return FALSE;
		}
	}
	else
	{
		if (!DoSave(m_strPathName))
		{
			TRACE0("Warning: File save failed.\n");
			return FALSE;
		}
	}
	return TRUE;
}

BOOL CWorldBuilderDoc::DoSave(LPCTSTR lpszPathName, BOOL bReplace)
	// Save the document data to a file
	// lpszPathName = path name where to save document file
	// if lpszPathName is NULL then the user will be prompted (SaveAs)
	// note: lpszPathName can be different than 'm_strPathName'
	// if 'bReplace' is TRUE will change file name if successful (SaveAs)
	// if 'bReplace' is FALSE will not change path name (SaveCopyAs)
{
	// Check current map for duplicates before opening another one
    WorldHeightMapEdit *pMap = GetHeightMap();
    if (pMap != NULL && !g_warnedfordupedforthismap)
    {
        Bool check = pMap->selectDuplicates();
        if (check)
        {
            MessageBeep(MB_ICONWARNING);
			int res = MessageBox(
				AfxGetMainWnd()->GetSafeHwnd(),
				"Duplicate / Overlapping objects were detected in the current map.\n"
				"Are you sure you want to continue saving or fix this monsieur?\n\n"
				"Click OK to save anyway, or Cancel to return and fix the issue.",
				"Duplicate / Overlapping Objects Detected",
				MB_OKCANCEL | MB_ICONERROR | MB_TOPMOST
			);

			g_warnedfordupedforthismap = true;
            if (res == IDCANCEL)
                return FALSE; // user canceled open
        }
    }

	CString newName = lpszPathName;
	if (newName.IsEmpty())
	{
		CDocTemplate* pTemplate = GetDocTemplate();
		ASSERT(pTemplate != NULL);

		newName = m_strPathName;
		if (bReplace && newName.IsEmpty())
		{
			newName = m_strTitle;
			// check for dubious filename
			int iBad = newName.FindOneOf(_T(" #%;/\\"));
			if (iBad != -1)
				newName.ReleaseBuffer(iBad);

			// append the default suffix if there is one
			CString strExt;
			if (pTemplate->GetDocString(strExt, CDocTemplate::filterExt) &&
			  !strExt.IsEmpty())
			{
				ASSERT(strExt[0] == '.');
				newName += strExt;
			}
		}

#ifdef RTS_HAS_QT
		TSaveMapInfo info;
		info.filename = newName;
		{
			char qtFilename[_MAX_PATH];
			int qtBrowse = 0;
			int qtSystemDir = 0;
			if (WBQtSaveMap_Run(::AfxGetMainWnd()->GetSafeHwnd(), (LPCTSTR)info.filename,
					qtFilename, sizeof(qtFilename), &qtBrowse, &qtSystemDir) == 0)
			{
				return FALSE;
			}
			info.filename = qtFilename;
			info.browse = (qtBrowse != 0);
			info.usingSystemDir = (qtSystemDir != 0);
		}
#else
		TSaveMapInfo info;
		info.filename = newName;
		SaveMap saveDlg(&info);
		if (saveDlg.DoModal() == IDCANCEL) {
			return FALSE;
		}
#endif
		if (info.browse) {
			if (!AfxGetApp()->DoPromptFileName(newName,
				bReplace ? AFX_IDS_SAVEFILE : AFX_IDS_SAVEFILECOPY,
				OFN_HIDEREADONLY | OFN_PATHMUSTEXIST, FALSE, pTemplate))
				return FALSE;       // don't even attempt to save
		} else {
			// Construct file name of .\Maps\mapname\mapname.map
			if (info.usingSystemDir)
				newName = ".\\Maps\\";
			else
			{
				newName = TheGlobalData->getPath_UserData().str();
				newName = newName + "Maps\\";
			}
			newName += info.filename;
			// Create directory.
			CFileStatus status;
			if (CFile::GetStatus(newName, status)) {
				if (!(status.m_attribute&CFile::directory)) {
					CString error = "Error: file '" + newName + "' exists, and is not a directory.";
					::AfxMessageBox(error);
					return FALSE;
				}
			} else {
				Int status = ::_mkdir(newName);
				if (status != 0) {
					CString error = "Error: could not create directory '" + newName + "'.";
					::AfxMessageBox(error);
					return FALSE;
				}
			}
			newName += "\\";
			newName += info.filename;
			newName += ".map";
		}
	}

	WbView3d * p3View = Get3DView();
	if (p3View) {
		DWORD editTimeSeconds = p3View->getEditTimeInSeconds();
		
		// Get the map folder path from newName (remove .map extension and get directory)
		CString mapPath = newName;
		int lastSlash = mapPath.ReverseFind('\\');
		if (lastSlash != -1) {
			mapPath = mapPath.Left(lastSlash); // Get folder path
		}
		
		// Create AdrianeMapSettings.ini path
		CString individualMapSettings = mapPath + "\\AdrianeMapSettings.ini";
		
		// Save edit time to the Data section
		CString editTimeStr;
		editTimeStr.Format("%u", editTimeSeconds);
		WritePrivateProfileString("Data", "EditTimeSeconds", editTimeStr, individualMapSettings);
	}

	CWaitCursor wait;

	if (!OnSaveDocument(newName))
	{
		if (lpszPathName == NULL)
		{
			// be sure to delete the file
			try
			{
				CFile::Remove(newName);
			}
			catch(...)
			{
				TRACE0("Warning: failed to delete file after failed SaveAs.\n");
			}
		}
		return FALSE;
	}

	// reset the title and change the document name
	if (bReplace)
		SetPathName(newName);

	return TRUE;        // success
}


/**
* CWorldBuilderDoc::ParseWaypointDataChunk - read a waypoint chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput 
*		
*/
Bool CWorldBuilderDoc::ParseWaypointDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	CWorldBuilderDoc *pThis = (CWorldBuilderDoc *)userData;
	return pThis->ParseWaypointData(file, info, userData);
}

/**
* CWorldBuilderDoc::ParseWaypointData - read waypoint data chunk.
* Format is the newer CHUNKY format.
*	See WorldBuilderDoc.cpp for the writer.
*	Input: DataChunkInput 
*		
*/
Bool CWorldBuilderDoc::ParseWaypointData(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	m_numWaypointLinks = file.readInt();
	Int i;
	for (i=0; i<m_numWaypointLinks; i++) {
		this->m_waypointLinks[i].waypoint1 = file.readInt();
		this->m_waypointLinks[i].waypoint2 = file.readInt();
		//DEBUG_LOG(("Waypoint link from %d to %d\n", m_waypointLinks[i].waypoint1, m_waypointLinks[i].waypoint2));
	}
	DEBUG_ASSERTCRASH(file.atEndOfChunk(), ("Unexpected data left over."));
	return true;
}

static AsciiString IntToAsciiString(int value)
{
	char buffer[16];
	::wsprintf(buffer, "%d", value);
	return AsciiString(buffer);
}

void CWorldBuilderDoc::autoSave(void)
{
	// DEBUG_LOG(("AUTOSAVING...\n"));

	// Build autosave file paths
	const int NUM_SLOTS = 10;
	AsciiString autosavePaths[NUM_SLOTS + 1]; // 1-based indexing for simplicity

	AsciiString autosaveDir = TheGlobalData->getPath_UserData();
	autosaveDir.concat("AutoSaves\\");
	::CreateDirectory(autosaveDir.str(), NULL);

	for (int i = 1; i <= NUM_SLOTS; ++i) {
		autosavePaths[i] = autosaveDir;
		autosavePaths[i].concat("WorldBuilderAutoSave");
		autosavePaths[i].concat(IntToAsciiString(i));
		autosavePaths[i].concat(".map");
	}

	if (m_heightMap) try {
		CFileStatus status;

		// Remove oldest autosave (slot 10)
		try {
			if (CFile::GetStatus(autosavePaths[NUM_SLOTS].str(), status)) {
				CFile::Remove(autosavePaths[NUM_SLOTS].str());
			}
		} catch(...) {}

		// Shift autosaves: 9->10, 8->9, ..., 1->2
		for (int i = NUM_SLOTS - 1; i >= 1; --i) {
			try {
				if (CFile::GetStatus(autosavePaths[i].str(), status)) {
					CFile::Rename(autosavePaths[i].str(), autosavePaths[i + 1].str());
				}
			} catch(...) {}
		}

		// Create the new autosave1.map
		CFile theFile(autosavePaths[1].str(), CFile::modeCreate | CFile::modeWrite | CFile::shareDenyWrite | CFile::typeBinary);
		try {
			MFCFileOutputStream theStream(&theFile);
			DataChunkOutput chunkWriter(&theStream);

			m_heightMap->saveToFile(chunkWriter);

			// Save waypoint data
			chunkWriter.openDataChunk("WaypointsList", K_WAYPOINTS_VERSION_1);
			chunkWriter.writeInt(this->m_numWaypointLinks);
			for (int i = 0; i < m_numWaypointLinks; ++i) {
				chunkWriter.writeInt(this->m_waypointLinks[i].waypoint1);
				chunkWriter.writeInt(this->m_waypointLinks[i].waypoint2);
			}
			chunkWriter.closeDataChunk();
		} catch(...) {}

		theFile.Close();
		m_needAutosave = false;

		// DEBUG_LOG(("AUTOSAVED...\n"));
	} catch(...) {
		// DEBUG_LOG(("AUTOSAVE FAILED...\n"));
		::AfxMessageBox(IDS_NO_AUTOSAVE);
	}
}

/////////////////////////////////////////////////////////////////////////////
// CWorldBuilderDoc diagnostics

#ifdef _DEBUG
void CWorldBuilderDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CWorldBuilderDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CWorldBuilderDoc commands

void CWorldBuilderDoc::SetHeightMap(WorldHeightMapEdit *pMap, Bool doUpdate)
{
	REF_PTR_SET(m_heightMap, pMap);
	if (doUpdate) {
		POSITION pos = GetFirstViewPosition();
		while (pos != NULL)
		{
			CView* pView = GetNextView(pos);
			WbView* pWView = (WbView *)pView;
			IRegion2D partialRange = {0,0,0,0};
			ASSERT_VALID(pWView);
			pWView->updateHeightMapInView(m_heightMap, false, partialRange);
			pWView->Invalidate(false);
		}
		if (TheMinimapDialog && TheMinimapDialog->IsWindowVisible())
			TheMinimapDialog->rebuildTerrain();
	}
}

void CWorldBuilderDoc::AddAndDoUndoable(Undoable *pUndo)
{
	Undoable *pCurUndo = m_undoList;
	Int count = m_curRedo;
	while(count>0 && pCurUndo != NULL) {
		count--;
		pCurUndo = pCurUndo->GetNext();
	}
	m_needAutosave = true;
	// DEBUG_LOG(("NEED AUTOSAVE AddAndDoUndoable ...\n"));
	m_waypointTableNeedsUpdate=true;
	m_curRedo = 0;
	pUndo->LinkNext(pCurUndo);
	REF_PTR_SET(m_undoList, pUndo);
	pUndo->Do();
	SetModifiedFlag();
	pCurUndo = m_undoList;
	count = 0;
	while (pCurUndo) {
		count++;
		if (count >= m_maxUndos) {
			pCurUndo->LinkNext(NULL);
			break;
		}
		pCurUndo = pCurUndo->GetNext();
	}
}

void CWorldBuilderDoc::setMaxUndos(Int count)
{
	if (count < 1) {
		count = 1;
	}
	if (count > 999) {
		count = 999;
	}
	m_maxUndos = count;
}

void CWorldBuilderDoc::OnEditRedo() 
{
	Undoable *pUndo = m_undoList;
	m_needAutosave = true;
	// DEBUG_LOG(("NEED AUTOSAVE OnEditRedo ...\n"));
	m_waypointTableNeedsUpdate=true;
	if (m_curRedo>0) {
		Int count = m_curRedo-1;
		while(count>0) {
			count--;
			pUndo = pUndo->GetNext();
		}
		DEBUG_ASSERTCRASH((pUndo != NULL),("oops"));
		if (pUndo) {
			pUndo->Redo();
			SetModifiedFlag();
			m_curRedo--;
		}
	}
}

void CWorldBuilderDoc::OnUpdateEditRedo(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_undoList!=NULL && m_curRedo>0);
}

void CWorldBuilderDoc::OnEditUndo()
{
	// If the Wave Editor has a pending wave edit, Ctrl+Z (and the Edit ▸ Undo menu)
	// undo that wave action instead of the map undo.  We key off hasUndo() rather
	// than the active tool: holding Ctrl can transiently flip the current tool to the
	// pointer, which would otherwise make the check miss.
	if (WaveEditorTool::hasUndo())
	{
		WaveEditorTool::undoLast();
		return;
	}

	Undoable *pUndo = m_undoList;
	m_needAutosave = true;
	// DEBUG_LOG(("NEED AUTOSAVE OnEditUndo ...\n"));
	m_waypointTableNeedsUpdate=true;
	Int count = m_curRedo;
	while(count>0 && pUndo != NULL) {
		count--;
		pUndo = pUndo->GetNext();
	}
	if (pUndo != NULL) {
		pUndo->Undo();
		SetModifiedFlag();
		m_curRedo++;
	}
}

void CWorldBuilderDoc::OnTogglePitchAndRotation( void )
{
	WbView3d * p3View = Get3DView();
	if (p3View)
	{
		p3View->togglePitchAndRotation();
	}
}

void CWorldBuilderDoc::OnUpdateEditUndo(CCmdUI* pCmdUI) 
{
	Bool canUndo=false;
	if (m_undoList!=NULL) {
		if (m_curRedo == 0) {
			canUndo = true; // haven't undone any yet.
		} else {
			Undoable *pUndo = m_undoList;
			Int count = m_curRedo;
			while(count>0 && pUndo != NULL) {
				count--;
				pUndo = pUndo->GetNext();
			}
			canUndo = pUndo != NULL;
		}
	}
	pCmdUI->Enable(canUndo);
}


void CWorldBuilderDoc::OnTsInfo() 
{
	if (m_heightMap) {
		m_heightMap->showTileStatusInfo();
	}
}


void CWorldBuilderDoc::OnTsCanonical() 
{
	OptimizeTiles();	
}

void CWorldBuilderDoc::OptimizeTiles() 
{
	if (m_heightMap) {

		WorldHeightMapEdit *htMapEditCopy = GetHeightMap()->duplicate();
		if (htMapEditCopy == NULL) return;
		if (htMapEditCopy->optimizeTiles()) {  // does all the work.
			IRegion2D partialRange = {0,0,0,0};
			updateHeightMap(htMapEditCopy, false, partialRange);
			WBDocUndoable *pUndo = new WBDocUndoable(this, htMapEditCopy);
			this->AddAndDoUndoable(pUndo);
			REF_PTR_RELEASE(pUndo); // belongs to this now.	
		} else {
			::Beep(1000,500);
		}
		REF_PTR_RELEASE(htMapEditCopy);
	}	
}

// Adriane[Deathscythe] Hacky cursed code just to refresh the terrain tiles without adding an undo step.
void CWorldBuilderDoc::RefreshAndOptimizeHeightMap()
{
    if (m_heightMap)
    {
        WorldHeightMapEdit* htMapEditCopy = GetHeightMap()->duplicate();
        if (htMapEditCopy == NULL)
            return;

        if (htMapEditCopy->optimizeTiles())  // does all the blend recalculation
        {
            IRegion2D partialRange = {0, 0, 0, 0};
            updateHeightMap(htMapEditCopy, false, partialRange);
        }
        else
        {
            ::Beep(1000, 500);
        }

        REF_PTR_RELEASE(htMapEditCopy);
    }
}

void CWorldBuilderDoc::OnUpdateTsCanonical(CCmdUI* pCmdUI) 
{
}

#ifdef RTS_HAS_QT
// File>Close under the Qt inversion. CDocument's default close destroys the 3D view and
// its frame, but that view is hosted inside the Qt main window, so Create3DView ->
// CreateNewFrame fails to rebuild it and the command reports "Command failed." This is a
// single-document app (the window IS the map), so Close has no distinct meaning from New;
// route it to the File>New path, which REUSES the hosted view (OnNewDocument resets its
// contents in place). The save-modified prompt still fires along the New path. Handled at
// the document because MFC routes ID_FILE_CLOSE to the doc before the app.
void CWorldBuilderDoc::OnFileClose()
{
	// Route Close to the File>New command (CN_COMMAND == 0) through the app's routing: it
	// reuses the hosted view instead of destroying/recreating the frame. This is safe on the
	// legacy/non-inverted path too (New always works), and avoids re-dispatching ID_FILE_CLOSE
	// back into this same override.
	AfxGetApp()->OnCmdMsg(ID_FILE_NEW, 0, NULL, NULL);
}
#endif

void CWorldBuilderDoc::OnFileResize() 
{
	TNewHeightInfo hi;
	hi.initialHeight = 8;
	hi.xExtent = m_heightMap->getXExtent()-2*m_heightMap->getBorderSize();
	hi.yExtent = m_heightMap->getYExtent()-2*m_heightMap->getBorderSize();
	hi.borderWidth = m_heightMap->getBorderSize();
	hi.forResize = true;
	CString label;
	label.LoadString(IDS_RESIZE);
#ifdef RTS_HAS_QT
	{
		int qtHeight = hi.initialHeight;
		int qtX = hi.xExtent;
		int qtY = hi.yExtent;
		int qtBorder = hi.borderWidth;
		int qtTop = 0;
		int qtBottom = 0;
		int qtLeft = 0;
		int qtRight = 0;
		if (WBQtNewHeightMap_Run(::AfxGetMainWnd()->GetSafeHwnd(), (LPCTSTR)label, 1,
				&qtHeight, &qtX, &qtY, &qtBorder, &qtTop, &qtBottom, &qtLeft, &qtRight) == 0)
		{
			return;
		}
		hi.initialHeight = qtHeight;
		hi.xExtent = qtX;
		hi.yExtent = qtY;
		hi.borderWidth = qtBorder;
		hi.anchorTop = (qtTop != 0);
		hi.anchorBottom = (qtBottom != 0);
		hi.anchorLeft = (qtLeft != 0);
		hi.anchorRight = (qtRight != 0);
	}
#else
	CNewHeightMap htDialog(&hi, label);
	if (IDOK == htDialog.DoModal()) {
		htDialog.GetHeightInfo(&hi);
	} else {
		return;
	}
#endif

	WorldHeightMapEdit *htMapEditCopy = GetHeightMap()->duplicate();
	if (htMapEditCopy == NULL) return;
	Coord3D objOffset;
	if (htMapEditCopy->resize(hi.xExtent, hi.yExtent, hi.initialHeight, hi.borderWidth, 
		hi.anchorTop, hi.anchorBottom, hi.anchorLeft, hi.anchorRight, &objOffset)) {  // does all the work.
		WBDocUndoable *pUndo = new WBDocUndoable(this, htMapEditCopy, &objOffset);
		this->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to this now.
		POSITION pos = GetFirstViewPosition();
		IRegion2D partialRange = {0,0,0,0};
		Get3DView()->updateHeightMapInView(m_heightMap, false, partialRange);
		while (pos != NULL)
		{
			CView* pView = GetNextView(pos);
			WbView* pWView = (WbView *)pView;
			ASSERT_VALID(pWView);
			pWView->adjustDocSize();
			pWView->Invalidate();
		}
	} else {
		::Beep(1000,500);
	}
	REF_PTR_RELEASE(htMapEditCopy);

}


void CWorldBuilderDoc::OnTsRemap() 
{
	if (m_heightMap) {
		WorldHeightMapEdit *htMapEditCopy = GetHeightMap()->duplicate();
		if (htMapEditCopy == NULL) return;
		if (htMapEditCopy->remapTextures()) {  // does all the work.
			IRegion2D partialRange = {0,0,0,0};
			updateHeightMap(htMapEditCopy, false, partialRange);
			WBDocUndoable *pUndo = new WBDocUndoable(this, htMapEditCopy);
			this->AddAndDoUndoable(pUndo);
			REF_PTR_RELEASE(pUndo); // belongs to this now.
		} else {
			::Beep(1000,500);
		}
		REF_PTR_RELEASE(htMapEditCopy);
	}	
}

/* static */ CWorldBuilderDoc *CWorldBuilderDoc::GetActiveDoc()
{
#ifdef MDI
	CMDIFrameWnd *pFrame = (CMDIFrameWnd*)AfxGetApp()->m_pMainWnd;
	if (pFrame) {
		DEBUG_ASSERTCRASH((pFrame == CMainFrame::GetMainFrame()),("oops"));
		// Get the active MDI child window.
		CMDIChildWnd *pChild = (CMDIChildWnd *) pFrame->GetActiveFrame();
		if (pChild) {
			// Get the active view attached to the active MDI child
			// window.
			WbView *pView = (WbView *) pChild->GetActiveView();
			if (pView) {
				return pView->WbDoc();
			}
		}
	}

#else 
// only works for SDI, not MDI
	return (CWorldBuilderDoc*)CMainFrame::GetMainFrame()->GetActiveDocument();
#endif
	return NULL;
}

/* static */ CWorldBuilderView *CWorldBuilderDoc::GetActive2DView()
{
	CWorldBuilderDoc* pDoc = GetActiveDoc();
	if (pDoc) {
		return pDoc->Get2DView();
	}
	return NULL;
}

/* static */ WbView3d *CWorldBuilderDoc::GetActive3DView()
{
	CWorldBuilderDoc* pDoc = GetActiveDoc();
	if (pDoc) {
		return pDoc->Get3DView();
	}
	return NULL;
}

CWorldBuilderView *CWorldBuilderDoc::Get2DView()
{
	POSITION pos = GetFirstViewPosition();
	while (pos != NULL)
	{
		CView* pView = GetNextView(pos);
		if (pView->IsKindOf(RUNTIME_CLASS(CWorldBuilderView)))
			return (CWorldBuilderView*)pView;
	}

	return NULL;
}

WbView3d *CWorldBuilderDoc::Get3DView()
{
	POSITION pos = GetFirstViewPosition();
	while (pos != NULL)
	{
		CView* pView = GetNextView(pos);
		if (pView->IsKindOf(RUNTIME_CLASS(WbView3d)))
			return (WbView3d*)pView;
	}

	return NULL;
}

void CWorldBuilderDoc::Create2DView()
{
}

void CWorldBuilderDoc::Create3DView()
{
	if (Get3DView())
		return;
#ifdef ONLY_ONE_AT_A_TIME
	gAlreadyOpen = true;
#endif
#if 1
	CDocTemplate* pTemplate = WbApp()->Get3dTemplate();
	IRegion2D partialRange = {0,0,0,0};
	ASSERT_VALID(pTemplate);
	CFrameWnd* pFrame = pTemplate->CreateNewFrame(this, NULL);
	if (pFrame == NULL)
	{
		TRACE0("Warning: failed to create new frame.\n");
		return;     // command failed
	}
	pTemplate->InitialUpdateFrame(pFrame, this);
	Get3DView()->updateHeightMapInView(m_heightMap, false, partialRange);
#endif
}


BOOL CWorldBuilderDoc::OnNewDocument()
{
#ifdef ONLY_ONE_AT_A_TIME
	if (gAlreadyOpen) {
		::AfxMessageBox(IDS_ONLY_ONE_FILE);
		return FALSE;
	}
#endif
	if (!CDocument::OnNewDocument())
		return FALSE;
	static Bool firstTime = true;

	// clear out map-specific text
	TheGameText->reset();

	TNewHeightInfo hi;
	hi.initialHeight = AfxGetApp()->GetProfileInt("GameOptions", "Default Map Height", 16);
	hi.xExtent = AfxGetApp()->GetProfileInt("GameOptions", "Default Map X-size", 100);
	hi.yExtent = AfxGetApp()->GetProfileInt("GameOptions", "Default Map Y-size", 100);
	hi.borderWidth = AfxGetApp()->GetProfileInt("GameOptions", "Default Map Border", 30);
	hi.forResize = false;
	if (!firstTime) {
		CString label;
		label.LoadString(IDS_NEW);
#ifdef RTS_HAS_QT
		int qtHeight = hi.initialHeight;
		int qtX = hi.xExtent;
		int qtY = hi.yExtent;
		int qtBorder = hi.borderWidth;
		int qtTop = 0;
		int qtBottom = 0;
		int qtLeft = 0;
		int qtRight = 0;
		if (WBQtNewHeightMap_Run(::AfxGetMainWnd()->GetSafeHwnd(), (LPCTSTR)label, 0,
				&qtHeight, &qtX, &qtY, &qtBorder, &qtTop, &qtBottom, &qtLeft, &qtRight) != 0) {
			hi.initialHeight = qtHeight;
			hi.xExtent = qtX;
			hi.yExtent = qtY;
			hi.borderWidth = qtBorder;
#else
		CNewHeightMap htDialog(&hi, label);
		if (IDOK == htDialog.DoModal()) {
			htDialog.GetHeightInfo(&hi);
#endif
			AfxGetApp()->WriteProfileInt("GameOptions", "Default Map Height", hi.initialHeight);
			AfxGetApp()->WriteProfileInt("GameOptions", "Default Map X-size", hi.xExtent);
			AfxGetApp()->WriteProfileInt("GameOptions", "Default Map Y-size", hi.yExtent);
			AfxGetApp()->WriteProfileInt("GameOptions", "Default Map Border", hi.borderWidth);
		} else {
			return(false);
		}
	}
	REF_PTR_RELEASE(m_heightMap);
	REF_PTR_RELEASE(m_undoList);
	m_curRedo = 0;
	m_numWaypointLinks = 0;
	m_waypointTableNeedsUpdate = true;
	m_curWaypointID = 0;
	WbApp()->selectPointerTool();
	PolygonTrigger::deleteTriggers();

	// Make sure that all the old units are removed from the list.
	// Bug fix by MLL 1/14/03
	TheLayersList->enableUpdates();
	TheLayersList->resetLayers();
	TheLayersList->disableUpdates();

	TheSidesList->clear();
	TheSidesList->validateSides();

	WbView3d * p3View = Get3DView();
	if (p3View) {
		p3View->resetRenderObjects();
		p3View->resetEditTimer();
	}
	firstTime = false;
	m_heightMap = NEW_REF(WorldHeightMapEdit,(hi.xExtent,hi.yExtent,hi.initialHeight, hi.borderWidth));
	// note - mHeight map has ref count of 1.

	// Create a default water area.
	PolygonTrigger *pTrig = newInstance(PolygonTrigger)(4); 
	ICoord3D loc;
	pTrig->setWaterArea(true);
	pTrig->setTriggerName(AsciiString("Default Water"));

	const float leftX   = -hi.borderWidth * MAP_XY_FACTOR;
	const float bottomY = -hi.borderWidth * MAP_XY_FACTOR;
	
	// Bottom-left
	loc.x = leftX;
	loc.y = bottomY;
	loc.z = TheGlobalData->m_waterPositionZ;
	pTrig->addPoint(loc);

	// Bottom-right
	loc.x = (hi.xExtent + hi.borderWidth - 1) * MAP_XY_FACTOR;
	pTrig->addPoint(loc);

	// Top-right
	loc.y = (hi.yExtent + hi.borderWidth - 1) * MAP_XY_FACTOR;
	pTrig->addPoint(loc);

	// Top-left
	loc.x = leftX;
	pTrig->addPoint(loc);
	PolygonTrigger::addPolygonTrigger(pTrig);
	TheLayersList->addPolygonTriggerToLayersList(pTrig, pTrig->getLayerName()); 
	SetHeightMap(m_heightMap, true);
	TerrainMaterial::updateTextures(m_heightMap);

	Create3DView();

	POSITION pos = GetFirstViewPosition();
	while (pos != NULL)
	{
		CView* pView = GetNextView(pos);
		WbView* pWView = (WbView *)pView;
		ASSERT_VALID(pWView);
		pWView->setCenterInView(m_heightMap->getXExtent()/2-m_heightMap->getBorderSize(), m_heightMap->getYExtent()/2-m_heightMap->getBorderSize());
	}
	if (p3View) {
		p3View->setDefaultCamera();
	}
	return TRUE;
}

void CWorldBuilderDoc::invalObject(MapObject *pMapObj)
{
	POSITION pos = GetFirstViewPosition();
	while (pos != NULL)
	{
		CView* pView = GetNextView(pos);
		WbView* pWView = (WbView *)pView;
		ASSERT_VALID(pWView);
		pWView->invalObjectInView(pMapObj);
	}
	// Minimap refresh is handled in WbView3d::invalObjectInView (the common funnel for
	// both this path and direct p3View->invalObjectInView callers).
}

void CWorldBuilderDoc::invalCell(int xIndex, int yIndex)
{
	POSITION pos = GetFirstViewPosition();
	while (pos != NULL)
	{
		CView* pView = GetNextView(pos);
		WbView* pWView = (WbView *)pView;
		ASSERT_VALID(pWView);
		pWView->invalidateCellInView(xIndex, yIndex);
	}
}

void CWorldBuilderDoc::syncViewCenters(Real x, Real y)
{
	if (!m_linkCenters)
		return;

	POSITION pos = GetFirstViewPosition();
	while (pos != NULL)
	{
		CView* pView = GetNextView(pos);
		WbView* pWView = (WbView *)pView;
		ASSERT_VALID(pWView);
		pWView->setCenterInView(x, y);
	}
}


void CWorldBuilderDoc::updateAllViews()
{
	POSITION pos = GetFirstViewPosition();
	while (pos != NULL)
	{
		CView* pView = GetNextView(pos);
		WbView* pWView = (WbView *)pView;
		ASSERT_VALID(pWView);
		pWView->UpdateWindow();
	}
}

void CWorldBuilderDoc::updateHeightMap(WorldHeightMap *htMap, Bool partial, const IRegion2D &partialRange)
{
	POSITION pos = GetFirstViewPosition();
	while (pos != NULL)
	{
		CView* pView = GetNextView(pos);
		WbView* pWView = (WbView *)pView;
		ASSERT_VALID(pWView);
		pWView->updateHeightMapInView(htMap, partial, partialRange);
		pWView->Invalidate();
	}

	// Keep the minimap in sync while the user paints/sculpts terrain. Use the
	// throttled request so a continuous brush stroke doesn't resample every frame.
	if (TheMinimapDialog && TheMinimapDialog->IsWindowVisible())
		TheMinimapDialog->requestRebuild();
}

void CWorldBuilderDoc::LoadEditTime(const CString& mapPath)
{
	WbView3d * p3View = Get3DView();
	if (!p3View) return;
	
	// Get the map folder path
	CString folderPath = mapPath;
	int lastSlash = folderPath.ReverseFind('\\');
	if (lastSlash != -1) {
		folderPath = folderPath.Left(lastSlash);
	}
	
	CString individualMapSettings = folderPath + "\\AdrianeMapSettings.ini";
	
	if (PathFileExists(individualMapSettings)) {
		// Read edit time from Data section
		DWORD savedTime = GetPrivateProfileInt("Data", "EditTimeSeconds", 0, individualMapSettings);
		
		// Set the loaded time in the view
		p3View->setEditTime(savedTime); // Subtract 3 seconds to account for load time
	} else {
		// No saved time, start from zero
		p3View->resetEditTimer();
	}
}

BOOL CWorldBuilderDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	// Suppress minimap rebuilds for the whole load. The load pops modal MessageBoxes
	// (map.ini prompts) whose nested message pump would otherwise fire the minimap's
	// pending rebuild timer against a half-swapped document and hang. The guard clears
	// the flag on EVERY return path; clearing (in its dtor) kicks one clean rebuild.
	struct MinimapLoadGuard {
		MinimapLoadGuard()  { MinimapDialog::setLoading(true); }
		~MinimapLoadGuard() { MinimapDialog::setLoading(false); }
	} minimapLoadGuard;

	// If a map.ini override was loaded for the previous map, gracefully tear it down
	// before loading the next map (no more forced restart). This strips only the
	// map.ini-created overrides and leaves the base game data intact.
	if (g_mapiniloaded)
		unloadMapIniOverrides();
#ifdef ONLY_ONE_AT_A_TIME
	if (gAlreadyOpen) {
		::AfxMessageBox(IDS_ONLY_ONE_FILE);
		return FALSE;
	}
#endif
	
	// Open document dialog may change working directory, 
	// let the app know what it was for future opens, and change it back.
	char buf[_MAX_PATH];
	::GetCurrentDirectory(_MAX_PATH, buf);

	// clear out map-specific text
	TheGameText->reset();
	AsciiString s = lpszPathName;
	while (s.getLength() && s.getCharAt(s.getLength()-1) != '\\')
		s.removeLastChar();
	s.concat("map.str");
	DEBUG_LOG(("Looking for map-specific text in [%s]\n", s.str()));
	TheGameText->initMapStringFile(s);

	// TODO: this dude brick the texures when host textures are not in the map...
	// TileTool::clearCopiedTiles();
	// TerrainMaterial::OnImportFavoritesFromMapFolder();
	
	//The dude opened a new map so we set this to false;
	g_warnedfordupedforthismap = false;

	// Adriane [Deathscythe] : Map.ini loader support
	AsciiString iniPath = lpszPathName;
	while (iniPath.getLength() && iniPath.getCharAt(iniPath.getLength()-1) != '\\')
		iniPath.removeLastChar();
	iniPath.concat("map.ini");
	
	if (TheFileSystem->doesFileExist(iniPath.str())) {
		DEBUG_LOG(("Map.ini file detected at [%s]\n", iniPath.str()));

		// Global always-load list ([MapLoaderIni] in WorldBuilder.ini). A listed map loads
		// silently; any other map is previewed in the report dialog first.
		if (isMapIniAlwaysLoad(lpszPathName)) {
			DEBUG_LOG(("Loading map.ini from [%s] (in the always-load list)\n", iniPath.str()));
			CString report;
			bool ok = doLoadMapIni(iniPath, MAPINI_INSTALL, report);
			if (!ok)	// only surface a hard error
				showScrollableInfoDialog("Map.ini Loader (Beta)", report, /*applyMode=*/false);
		} else {
			// Preview the map.ini in the report dialog; OK loads, Cancel skips.
			DEBUG_LOG(("Previewing map.ini from [%s]\n", iniPath.str()));
			MessageBeep(MB_ICONWARNING);
			bool applied = confirmAndLoadMapIni(iniPath, "Map.ini Loader (Beta)");
			if (applied) {
				// Offer to always load this map's map.ini silently from now on.
				if (::MessageBox(NULL,
						"Always load this map's map.ini from now on (no prompt)?",
						"Map.ini Loader (Beta)", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES) {
					addMapIniAlwaysLoad(lpszPathName);
				}
			}
		}

		// Baseline the watch mtime whether or not we loaded, so a later external edit is
		// detected relative to the file as it is now.
		getMapIniWriteTime(iniPath, &m_mapIniLastWrite);
	}

	WbApp()->setCurrentDirectory(AsciiString(buf));
	::GetModuleFileName(NULL, buf, sizeof(buf));
	char *pEnd = buf + strlen(buf);
	while (pEnd != buf) {
		if (*pEnd == '\\') {
			*pEnd = 0;
			break;
		}
		pEnd--;
	}
	::SetCurrentDirectory(buf);

	if (!CDocument::OnOpenDocument(lpszPathName))
		return FALSE;
	
	Create3DView();

	LoadEditTime(lpszPathName);

	if (CMainFrame::GetMainFrame() && CMainFrame::GetMainFrame()->getScriptDialog()) {
		CMainFrame::GetMainFrame()->closeScriptDialog();
	}

	// WbApp()->OnRefreshAppAbout();
	// DEBUG_LOG(("strTitle=%s strPathName=%s\n", lpszPathName, m_strPathName));
	// CString fullPath = lpszPathName;
	// int lastSlash = fullPath.ReverseFind('\\');
	// if (lastSlash != -1)
	// {
	// 	fullPath = fullPath.Left(lastSlash);
	// }
	// TerrainMaterial::ReloadFavorites(fullPath);
	
	return TRUE;
}

//=============================================================================
// CWorldBuilderView::getCellIndexFromPoint
//=============================================================================
/** Given a cursor location, return the x and y index into the height map. 
If the location is outside the height map, returns false. */
//=============================================================================
Bool CWorldBuilderDoc::getCellIndexFromCoord(Coord3D cpt, CPoint *ndxP)
{
	// Set up default return value.
	ndxP->x = -1;
	ndxP->y = -1;	 
	Bool inMap = true;

	WorldHeightMapEdit *pMap = GetHeightMap();
	if (pMap == NULL) return false;

	Int xIndex = floor(cpt.x/MAP_XY_FACTOR);
	xIndex += pMap->getBorderSize();

	// If negative, outside of map so return false.
	if (xIndex<0) {
		inMap = false;
		xIndex = 0;
	}
	// If larger than the map, return default.
	if (xIndex >= pMap->getXExtent()) {
		inMap = false;
		xIndex = pMap->getXExtent()-1;
	}
	Int yIndex = floor(cpt.y/MAP_XY_FACTOR);

	yIndex += pMap->getBorderSize();
	

	// If negative, outside of map so return default.
	if (yIndex<0) {
		inMap = false;
		yIndex = 0;
	}

	// If larger than the map, return default.
	if (yIndex >= pMap->getYExtent())  {
		inMap = false;
		yIndex = pMap->getYExtent()-1;
	}


	ndxP->x = xIndex;
	ndxP->y = yIndex;

	return inMap;
}

void CWorldBuilderDoc::getCoordFromCellIndex(CPoint ndx, Coord3D* pt)
{
	if (!pt) {
		return;
	}
	WorldHeightMap* hm = GetHeightMap();
	if (!hm) {
		return;
	}

	(*pt).x = (ndx.x - hm->getBorderSize()) * MAP_XY_FACTOR;
	(*pt).y = (ndx.y - hm->getBorderSize()) * MAP_XY_FACTOR;
}

//=============================================================================
// CWorldBuilderView::getAllIndexesInRect
//=============================================================================
//=============================================================================
Bool CWorldBuilderDoc::getAllIndexesInRect(const Coord3D* bl, const Coord3D* br, 
																					 const Coord3D* tl, const Coord3D* tr,
																					 Int widthOutside, VecHeightMapIndexes* allIndices)
{
	// given the four corners of this rectangle, find all indices that are within
	// widthOutside of the rect and place them into allIndices.
	if (!(bl && br && tl && tr && allIndices)) {
		return false;
	}

	Coord3D center = { (bl->x + tr->x) / 2, (bl->y + tr->y) / 2, (bl->z + tr->z) / 2 };
	
	allIndices->clear();
	
	CPoint ndx;

	FindIndexNearest(this, &center, &ndx, PREFER_CENTER);
	AddUniqueAndNeighbors(this, bl, br, tl, tr, ndx, allIndices);

	FindIndexNearest(this, &center, &ndx, PREFER_LEFT);
	AddUniqueAndNeighbors(this, bl, br, tl, tr, ndx, allIndices);

	FindIndexNearest(this, &center, &ndx, PREFER_TOP);
	AddUniqueAndNeighbors(this, bl, br, tl, tr, ndx, allIndices);
	
	FindIndexNearest(this, &center, &ndx, PREFER_RIGHT);
	AddUniqueAndNeighbors(this, bl, br, tl, tr, ndx, allIndices);
	
	FindIndexNearest(this, &center, &ndx, PREFER_BOTTOM);
	AddUniqueAndNeighbors(this, bl, br, tl, tr, ndx, allIndices);
	
	return (allIndices->size() > 0);
}


//=============================================================================
// CWorldBuilderView::getCellPositionFromPoint
//=============================================================================
/** Given a pixel position, returns the x/y location in the height map.  This 
will return real values, so a position can be 1.7, 2.4 or such.  If the position
is not over the height map, return -1, -1. */
//=============================================================================
Bool CWorldBuilderDoc::getCellPositionFromCoord(Coord3D cpt,  Coord3D *locP)
{
	// Set up default values.
	locP->x = -1;
	locP->y = -1;
	WorldHeightMapEdit *pMap = GetHeightMap();
	if (pMap == NULL) return(false);
//	yLocation = pMap->getYExtent() - yLocation;
	CPoint curNdx;
	if (getCellIndexFromCoord(cpt, &curNdx)) {
		locP->x = cpt.x;
		locP->y = cpt.y;
		locP->z = pMap->getHeight(curNdx.x, curNdx.y)*MAP_HEIGHT_SCALE;
		return(true);
	}
	return false;
}

//=============================================================================
// CWorldBuilderDoc::getObjArrowPoint
//=============================================================================
/** Gets the location in pixels of the arrowhead point for an object. */
//=============================================================================
void CWorldBuilderDoc::getObjArrowPoint(MapObject *pObj, Coord3D *location)
{
	// Get the center location, and the angle.
	Coord3D loc = *pObj->getLocation();
 	float angle = pObj->getAngle();
	// The arrow starts in the +x direction.
	Vector3 arrow(1.2f*MAP_XY_FACTOR, 0, 0);
	// Rotate 
	arrow.Rotate_Z(angle);
	// Rotated.
	location->x = arrow.X;
	location->y = arrow.Y;
	// Add the rotated offset to the center.
	location->x += loc.x;
	location->y += loc.y;
	//location->z += loc.z;
}

void CWorldBuilderDoc::OnEditLinkCenters() 
{
	m_linkCenters = !m_linkCenters;
}

void CWorldBuilderDoc::OnUpdateEditLinkCenters(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_linkCenters?1:0);
}

BOOL CWorldBuilderDoc::CanCloseFrame(CFrameWnd* pFrame) 
{	
	CView *pView = this->Get2DView();
	if (pView && pView->GetParentFrame() == pFrame) {
		return true; // can always close the 2d window.
	}
	return SaveModified();
}

void CWorldBuilderDoc::OnViewTimeOfDay() 
{
	WbView3d * pView = Get3DView();
	if (pView) {
		pView->stepTimeOfDay();
	}
}

void CWorldBuilderDoc::OnWindow2dwindow() 
{
/*
	CView *pView = this->Get2DView();
	if (pView) {
		CFrameWnd *pFrame = pView->GetParentFrame();
		if (pFrame->IsIconic()) {
			pFrame->ShowWindow(SW_RESTORE);
		} else {
			pFrame->DestroyWindow();
		}
	} else {
		Create2DView();
	}
*/
}

void CWorldBuilderDoc::OnUpdateWindow2dwindow(CCmdUI* pCmdUI) 
{
/*
	CView *pView = this->Get2DView();
	pCmdUI->SetCheck(pView?1:0);
*/
}

//=============================================================================
// CWorldBuilderDoc::compressWaypointIds
//=============================================================================
/** Renumbers the waypoints and the links that reference them, removing any 
unused ids. */
//=============================================================================
void CWorldBuilderDoc::compressWaypointIds(void)
{
	updateWaypointTable();
	m_curWaypointID = 0;
	MapObject *pMapObj = NULL; 
	for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext()) {
		if (pMapObj->isWaypoint()) {
			Int nwpid = getNextWaypointID();
			pMapObj->setWaypointID(nwpid);
		}
	}
	Int i, j;
	for (i=0; i<m_numWaypointLinks; i++) {
		MapObject *pWay1 = getWaypointByID(m_waypointLinks[i].waypoint1);
		MapObject *pWay2 = getWaypointByID(m_waypointLinks[i].waypoint2);
		if (pWay1 &&	pWay2) {
			m_waypointLinks[i].waypoint1 = pWay1->getWaypointID();
			m_waypointLinks[i].waypoint2 = pWay2->getWaypointID();
		} else {
			// Delete the link.
			for (j=i; j<m_numWaypointLinks-1; j++) {
				m_waypointLinks[j] = m_waypointLinks[j+1];
			}
			m_numWaypointLinks--;
			i--;
		}
	}
	m_waypointTableNeedsUpdate = true;
	updateWaypointTable();
#ifdef DEBUG_CRASHING
	for (i=0; i<m_numWaypointLinks; i++) {
		MapObject *pWay1 = getWaypointByID(m_waypointLinks[i].waypoint1);
		MapObject *pWay2 = getWaypointByID(m_waypointLinks[i].waypoint2);
		DEBUG_ASSERTCRASH(pWay1 && pWay1->getWaypointID() == m_waypointLinks[i].waypoint1, ("Bad waypoint."));
		DEBUG_ASSERTCRASH(pWay2 && pWay2->getWaypointID() == m_waypointLinks[i].waypoint2, ("Bad waypoint."));
	}
	int count = 1;
	for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext()) {
		if (pMapObj->isWaypoint()) {
			DEBUG_ASSERTCRASH(pMapObj->getWaypointID()==count, ("Bad waypoint"));
			DEBUG_ASSERTCRASH(pMapObj==getWaypointByID(count), ("Bad waypoint"));
			count++;
		}
	}
#endif
}

//=============================================================================
// CWorldBuilderDoc::updateWaypointTable
//=============================================================================
/** If any waypoints have changed (m_waypointTableNeedsUpdate) updates the waypoint
table.  The waypoint table is used to locate waypoints by id, without searching 
the objects list. (See getWaypointByID()) */
//=============================================================================
void CWorldBuilderDoc::updateWaypointTable(void) 
{
	if (m_waypointTableNeedsUpdate) {
		m_waypointTableNeedsUpdate=false;
		Int i;
		for (i=0; i<MAX_WAYPOINTS; i++) {
			m_waypointTable[i] = NULL;
		}

		MapObject *pMapObj = NULL; 
		for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext()) {
			if (pMapObj->isWaypoint()) {
				Int id = pMapObj->getWaypointID();
				DEBUG_ASSERTCRASH(id>0 && id<MAX_WAYPOINTS, ("Bad waypoint id."));
				if (id>0 && id<MAX_WAYPOINTS) {
					if (m_waypointTable[id] != NULL) DEBUG_LOG(("Duplicate waypoint id."));
					if (m_waypointTable[id] != NULL) {
						pMapObj->setWaypointID(getNextWaypointID());
						m_waypointTableNeedsUpdate=true;
					} else {
						m_waypointTable[id] = pMapObj;
					}
				}
			}
		}
	}
}

//=============================================================================
// CWorldBuilderDoc::addWaypointLink
//=============================================================================
/** Adds a waypoint link between two waypoints, referenced by waypoint id. */
//=============================================================================
void CWorldBuilderDoc::addWaypointLink(Int waypointID1, Int waypointID2) 
{
	Int i;
	for (i=0; i<m_numWaypointLinks; i++) {
		if (m_waypointLinks[i].waypoint1 == waypointID1 &&
			m_waypointLinks[i].waypoint2 == waypointID2) {
			return; // already linked.
		}
	}
	DEBUG_ASSERTCRASH(m_numWaypointLinks<MAX_WAYPOINTS-1, ("Too many links."));
	if (m_numWaypointLinks<MAX_WAYPOINTS) {
		m_waypointLinks[m_numWaypointLinks].waypoint1 = waypointID1;
		m_waypointLinks[m_numWaypointLinks].waypoint2 = waypointID2;
		m_numWaypointLinks++;
	}
}

//=============================================================================
// CWorldBuilderDoc::removeWaypointLink
//=============================================================================
/** Removes a waypoint link between two waypoints, referenced by waypoint id. */
//=============================================================================
void CWorldBuilderDoc::removeWaypointLink(Int waypointID1, Int waypointID2) 
{
	Int i;
	for (i=0; i<m_numWaypointLinks; i++) {
		if (m_waypointLinks[i].waypoint1 == waypointID1 &&
			m_waypointLinks[i].waypoint2 == waypointID2) {
			for (i; i<m_numWaypointLinks-1; i++) {
				m_waypointLinks[i] = m_waypointLinks[i+1];
			}
			m_numWaypointLinks--;
		}
	}
}


//=============================================================================
// CWorldBuilderDoc::getWaypointByID
//=============================================================================
/** Returns a pointer to the waypoint map object referenced by waypointID. */
//=============================================================================
MapObject *CWorldBuilderDoc::getWaypointByID(Int waypointID)
{
	updateWaypointTable();
	DEBUG_ASSERTCRASH(waypointID>=0 && waypointID<MAX_WAYPOINTS, ("Invalid id."));
	if (waypointID>0 && waypointID<MAX_WAYPOINTS) {
		MapObject *pObj = m_waypointTable[waypointID];
		if (pObj && pObj->isWaypoint()) {
			return pObj;
		}
		DEBUG_ASSERTCRASH(pObj==NULL, ("Waypoint links to an obj that isn't a waypoint."));
	} 
	return NULL;
}

//=============================================================================
// CWorldBuilderDoc::isWaypointLinked
//=============================================================================
/** Returns true if a waypoint is part of a linked waypoint path. */
//=============================================================================
Bool CWorldBuilderDoc::isWaypointLinked(MapObject *pWay)
{
	updateWaypointTable();
	Int i;
	for (i=0; i<m_numWaypointLinks; i++) {
		Int waypointID = m_waypointLinks[i].waypoint1;
		DEBUG_ASSERTCRASH(waypointID>=0 && waypointID<MAX_WAYPOINTS, ("Invalid id."));
		if (waypointID>0 && waypointID<MAX_WAYPOINTS) {
			MapObject *pObj = m_waypointTable[waypointID];
			if (pObj == pWay) return true;
		}
		waypointID = m_waypointLinks[i].waypoint2;
		DEBUG_ASSERTCRASH(waypointID>=0 && waypointID<MAX_WAYPOINTS, ("Invalid id."));
		if (waypointID>0 && waypointID<MAX_WAYPOINTS) {
			MapObject *pObj = m_waypointTable[waypointID];
			if (pObj == pWay) return true;
		}
	}
	return false;
}

//=============================================================================
// CWorldBuilderDoc::updateLinkedWaypointLabels
//=============================================================================
/** Updates the waypoint labels for a linked waypoint path. */
//=============================================================================
void CWorldBuilderDoc::updateLinkedWaypointLabels(MapObject *pWay)
{
	updateWaypointTable();
	Int i;
	for (i=0; i<m_numWaypointLinks; i++) {
		m_waypointLinks[i].processedFlag = false;
	}
	updateLWL(pWay, pWay);
}

//=============================================================================
// CWorldBuilderDoc::updateLWL
//=============================================================================
/** Updates the waypoint labels for a linked waypoint path. */
//=============================================================================
void CWorldBuilderDoc::updateLWL(MapObject *pWay, MapObject *pSrcWay)
{
	while (pWay) {

		Bool exists;
		AsciiString label;
		label = pSrcWay->getProperties()->getAsciiString(TheKey_waypointPathLabel1, &exists);
		if (exists) {
			pWay->getProperties()->setAsciiString(TheKey_waypointPathLabel1, label);
		} else if (pWay->getProperties()->known(TheKey_waypointPathLabel1, Dict::DICT_ASCIISTRING)) {
			pWay->getProperties()->remove(TheKey_waypointPathLabel1);
		}
		label = pSrcWay->getProperties()->getAsciiString(TheKey_waypointPathLabel2, &exists);
		if (exists) {
			pWay->getProperties()->setAsciiString(TheKey_waypointPathLabel2, label);
		} else if (pWay->getProperties()->known(TheKey_waypointPathLabel2, Dict::DICT_ASCIISTRING)){
			pWay->getProperties()->remove(TheKey_waypointPathLabel2);
		}
		label = pSrcWay->getProperties()->getAsciiString(TheKey_waypointPathLabel3, &exists);
		if (exists) {
			pWay->getProperties()->setAsciiString(TheKey_waypointPathLabel3, label);
		} else if (pWay->getProperties()->known(TheKey_waypointPathLabel3, Dict::DICT_ASCIISTRING)) {
			pWay->getProperties()->remove(TheKey_waypointPathLabel3);
		}

		Bool biDirectional;
		biDirectional = pSrcWay->getProperties()->getBool(TheKey_waypointPathBiDirectional, &exists);
		if (exists) {
			pWay->getProperties()->setBool(TheKey_waypointPathBiDirectional, biDirectional);
		}

		MapObject *pCurWay = pWay;
		pWay = NULL;
		Int i;

		for (i=0; i<m_numWaypointLinks; i++) {
			if (m_waypointLinks[i].processedFlag) continue;
			Bool process = false;
			MapObject *pNewWay = NULL;
			Int waypointID1 = m_waypointLinks[i].waypoint1;
			Int waypointID2 = m_waypointLinks[i].waypoint2;
			DEBUG_ASSERTCRASH(waypointID1>=0 && waypointID1<MAX_WAYPOINTS, ("Invalid id."));
			DEBUG_ASSERTCRASH(waypointID2>=0 && waypointID2<MAX_WAYPOINTS, ("Invalid id."));
			if (waypointID1>0 && waypointID1<MAX_WAYPOINTS && waypointID2>0 && waypointID2<MAX_WAYPOINTS ) {
				MapObject *pObj = m_waypointTable[waypointID1];
				if (pObj == pCurWay) {
					process = true;
					pNewWay = m_waypointTable[waypointID2];
				} 
				pObj = m_waypointTable[waypointID2];
				if (pObj == pCurWay) {
					process = true;
					pNewWay = m_waypointTable[waypointID1];
				} 
			}
			if (process) {
				m_waypointLinks[i].processedFlag = true;
				if (pWay == NULL) {
					pWay = pNewWay;
				} else {
					updateLWL(pNewWay, pSrcWay);
				}
			}
		}
	}
}

//=============================================================================
// CWorldBuilderDoc::getWaypointLink
//=============================================================================
/** Returns the two waypoint ID's that are linked.  Note that due to edits, one 
or both waypoints may have been deleted. */
//=============================================================================
void CWorldBuilderDoc::getWaypointLink(Int ndx, Int *waypointID1, Int *waypointID2)
{
	*waypointID1 = 0;
	*waypointID2 = 0;
	if (ndx >=0 && ndx <= m_numWaypointLinks) {
		*waypointID1 = m_waypointLinks[ndx].waypoint1;
		*waypointID2 = m_waypointLinks[ndx].waypoint2;
	}
}

//=============================================================================
// CWorldBuilderDoc::waypointLinkExists
//=============================================================================
/** Returns true if the two waypoint ID's are linked.  Note that due to edits, one 
or both waypoints may have been deleted. */
//=============================================================================
Bool CWorldBuilderDoc::waypointLinkExists(Int waypointID1, Int waypointID2) 
{
	Int i;
	for (i=0; i<m_numWaypointLinks; i++) {
		if (m_waypointLinks[i].waypoint1 == waypointID1 &&
			m_waypointLinks[i].waypoint2 == waypointID2) {
			return true; // already linked.
		}
	}
	return false;
}


void CWorldBuilderDoc::OnViewReloadtextures() 
{
	WW3D::_Invalidate_Textures();
	WorldHeightMapEdit *pMap = GetHeightMap();
	pMap->reloadTextures();
	IRegion2D range = {0,0,0,0};
	updateHeightMap(pMap, false, range);
}

void CWorldBuilderDoc::OnEditScripts() 
{
	ASSERT(CMainFrame::GetMainFrame());
	CMainFrame::GetMainFrame()->onEditScripts();
}

/* when "home" key is pressed, goes to the initial camera waypoint or if
 no such waypoint exists, goes to the center of the map */
void CWorldBuilderDoc::OnViewHome()
{
	// !!! needs to be updated if/when camera stuff for worldbuilder changes !!!
	Coord3D pos;
	AsciiString startingCamName = TheNameKeyGenerator->keyToName(TheKey_InitialCameraPosition);
	MapObject *pMapObj = MapObject::getFirstMapObject();

	// set pos to be the coordinates of the center of the map
	// pos.x = MAP_XY_FACTOR*m_heightMap->getXExtent()/2; 
	// pos.y = MAP_XY_FACTOR*m_heightMap->getYExtent()/2;

	// Actual center of the map -- centers to the middle of the cell not the corner
	pos.x = MAP_XY_FACTOR * (m_heightMap->getXExtent() * 0.5f - 0.5f);
	pos.y = MAP_XY_FACTOR * (m_heightMap->getYExtent() * 0.5f - 0.5f);

	pos.x -= MAP_XY_FACTOR*m_heightMap->getBorderSize();
	pos.y -= MAP_XY_FACTOR*m_heightMap->getBorderSize();
	
	// if waypoint "InitialCameraPosition" exists, replace pos with the appropriate coordinates
	while (pMapObj) {
		if (pMapObj->isWaypoint()) {
			if (startingCamName == pMapObj->getWaypointName()) {
				pos = *pMapObj->getLocation();
			}
		}
		pMapObj = pMapObj->getNext();
	}

	// set camera position to pos
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc) {
		WbView3d *p3View = pDoc->GetActive3DView();
		if (p3View) {
			p3View->setCenterInView(pos.x/MAP_XY_FACTOR, pos.y/MAP_XY_FACTOR);
		}
	}
}

void CWorldBuilderDoc::OnTexturesizingTile4x4() 
{
#ifdef EVAL_TILING_MODES
	WorldHeightMapEdit *pMap = GetHeightMap();
	pMap->m_tileMode = WorldHeightMap::TILE_4x4;
	IRegion2D range = {0,0,0,0};
	updateHeightMap(pMap, false, range);
#else 
	::AfxMessageBox("Feature not currently enabled.", MB_OK);
#endif
}

void CWorldBuilderDoc::OnUpdateTexturesizingTile4x4(CCmdUI* pCmdUI) 
{
#ifdef EVAL_TILING_MODES
	WorldHeightMapEdit *pMap = GetHeightMap();
	pCmdUI->SetCheck(pMap->m_tileMode == WorldHeightMap::TILE_4x4?1:0);
#endif
}

void CWorldBuilderDoc::OnTexturesizingTile6x6() 
{
#ifdef EVAL_TILING_MODES
	WorldHeightMapEdit *pMap = GetHeightMap();
	pMap->m_tileMode = WorldHeightMap::TILE_6x6;
	IRegion2D range = {0,0,0,0};
	updateHeightMap(pMap, false, range);
#else 
	::AfxMessageBox("Feature not currently enabled.", MB_OK);
#endif
}

void CWorldBuilderDoc::OnUpdateTexturesizingTile6x6(CCmdUI* pCmdUI) 
{
#ifdef EVAL_TILING_MODES
	WorldHeightMapEdit *pMap = GetHeightMap();
	pCmdUI->SetCheck(pMap->m_tileMode == WorldHeightMap::TILE_6x6?1:0);
#endif
}

void CWorldBuilderDoc::OnTexturesizingTile8x8() 
{
#ifdef EVAL_TILING_MODES
	WorldHeightMapEdit *pMap = GetHeightMap();
	pMap->m_tileMode = WorldHeightMap::TILE_8x8;
	IRegion2D range = {0,0,0,0};
	updateHeightMap(pMap, false, range);
#else 
	::AfxMessageBox("Feature not currently enabled.", MB_OK);
#endif
}

void CWorldBuilderDoc::OnUpdateTexturesizingTile8x8(CCmdUI* pCmdUI) 
{
#ifdef EVAL_TILING_MODES
	WorldHeightMapEdit *pMap = GetHeightMap();
	pCmdUI->SetCheck(pMap->m_tileMode == WorldHeightMap::TILE_8x8?1:0);
#endif
}

static AsciiString formatScriptLabel(Script *pScr) {
	AsciiString fmt;
	if (pScr->isSubroutine()) {
		fmt.concat("[S "); 
	} else {
		fmt.concat("[ns "); 
	}
	if (pScr->isActive()) {
		fmt.concat("A "); 
	} else {
		fmt.concat("na "); 
	}
	if (pScr->isOneShot()) {
		fmt.concat("D] ["); 
	} else {
		fmt.concat("nd] ["); 
	}
	if (pScr->isEasy()) {
		fmt.concat("E "); 
	} 
	if (pScr->isNormal()) {
		fmt.concat("N "); 
	} 
	if (pScr->isHard()) {
		fmt.concat("H]"); 
	} else {
		fmt.concat("]");
	}
	fmt.concat(pScr->getName().str());
	return fmt;
}


static void writeScript(FILE *theLogFile, const char * str)
{
	while (*str) {
		if (*str != '\r') {
			fputc(*str, theLogFile);
		}
		str++;
	}
}

#define DUMP_RAW_DICTS
#ifdef DUMP_RAW_DICTS
static void writeRawDict( FILE *theLogFile, const char* nm, const Dict* d ) 
{ 
	if (!d)
	{
		fprintf(theLogFile, "Dict %s is null!\n", nm);
		return;
	}

	UnsignedShort len = d->getPairCount();
	fprintf(theLogFile, "Dict %s has %d entries\n", nm, len);
	for (int i = 0; i < len; i++)
	{
		NameKeyType k = d->getNthKey(i);
		AsciiString kname = TheNameKeyGenerator->keyToName(k);

		Dict::DataType t = d->getNthType(i);

		const char* typenames[] = { "Bool", "int", "float", "ascii", "unicode" };
		fprintf(theLogFile, "Entry %d is %s: %s = ",i,typenames[t], kname.str());

		switch(t)
		{
			case Dict::DICT_BOOL:
				fprintf(theLogFile,"%s\n",d->getNthBool(i)?"true":"false");
				break;
			case Dict::DICT_INT:
				fprintf(theLogFile,"%d\n",d->getNthInt(i));
				break;
			case Dict::DICT_REAL:
				fprintf(theLogFile,"%f\n",d->getNthReal(i));
				break;
			case Dict::DICT_ASCIISTRING:
				fprintf(theLogFile,"%s\n",d->getNthAsciiString(i).str());
				break;
			case Dict::DICT_UNICODESTRING:
				fprintf(theLogFile,"%ls\n",d->getNthUnicodeString(i).str());
				break;
			default:
				DEBUG_CRASH(("impossible"));
				break;
		}
	}
}
#endif

static void fprintUnit(FILE *theLogFile, Dict *teamDict, NameKeyType keyMinUnit, NameKeyType keyMaxUnit,
									NameKeyType keyUnitType)
{
	Bool exists;
	Int minCount = teamDict->getInt(keyMinUnit, &exists);
	Int maxCount = teamDict->getInt(keyMaxUnit, &exists);
	AsciiString type = teamDict->getAsciiString(keyUnitType, &exists);
	if (type.isEmpty()) type = "<none>";
	if (minCount || maxCount) {
		fprintf(theLogFile, " %d-%d %s", minCount, maxCount, type.str());
	}

}

void CWorldBuilderDoc::OnDumpDocToText(void) 
{
	MapObject *pMapObj = NULL; 
	const char* vetStrings[] = {"Green", "Regular", "Veteran", "Elite"};
	const char* aggroStrings[] = {"Passive", "Normal", "Guard", "Hunt", "Agressive", "Sleep"};
	AsciiString noOwner = "No Owner";
	static FILE *theLogFile = NULL;
	Bool open = false;
	try {
		char dirbuf[ _MAX_PATH ];
		::GetModuleFileName( NULL, dirbuf, sizeof( dirbuf ) );
		char *pEnd = dirbuf + strlen( dirbuf );
		while( pEnd != dirbuf ) 
		{
			if( *pEnd == '\\' ) 
			{
				*(pEnd + 1) = 0;
				break;
			}
			pEnd--;
		}

		char curbuf[ _MAX_PATH ];

		strcpy(curbuf, dirbuf);
		strcat(curbuf, m_strTitle);
		strcat(curbuf, ".txt");

		theLogFile = fopen(curbuf, "w");
		if (theLogFile == NULL)
			throw;

		open = true;
		
		fprintf(theLogFile,"\n\n\nDump of Doc Contents\n");

#ifdef DUMP_RAW_DICTS
	
		writeRawDict(theLogFile, "WorldDict", MapObject::getWorldDict());

		fprintf(theLogFile,"Raw Map Object\n");
		for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext()) 
		{
			Dict *d = pMapObj->getProperties();
			TeamsInfo *teamInfo = TheSidesList->findTeamInfo(d->getAsciiString(TheKey_originalOwner));
			Dict *teamDict = (teamInfo)?teamInfo->getDict():NULL;
			writeRawDict( theLogFile, "MapObject",d );
			writeRawDict( theLogFile, "MapObjectTeam",teamDict );
		}
#endif

		// dump the Buildings
		fprintf(theLogFile,"\nBuildings\n");
		for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext()) {
			const ThingTemplate* tt = pMapObj->getThingTemplate();
			if (tt)	{
				if (tt->getEditorSorting() == ES_STRUCTURE) {
					Dict *d = pMapObj->getProperties();
					TeamsInfo *teamInfo = TheSidesList->findTeamInfo(d->getAsciiString(TheKey_originalOwner));
					Dict *teamDict = (teamInfo)?teamInfo->getDict():NULL;
					AsciiString objectOwnerName = (teamDict)?teamDict->getAsciiString(TheKey_teamOwner):noOwner;

					Bool showScript = false;
					AsciiString script = d->getAsciiString(TheKey_objectScriptAttachment, &showScript);

					Bool showName = false;
					AsciiString name = d->getAsciiString(TheKey_objectName, &showName);

					fprintf(theLogFile,"  %s", tt->getName().str());
					fprintf(theLogFile,", %s", objectOwnerName.str());
					fprintf(theLogFile,", @ (%0.0f,%0.0f)", pMapObj->getLocation()->x, pMapObj->getLocation()->y);
					fprintf(theLogFile,", Angle %0.0f", pMapObj->getAngle() * 180 / PI);
					fprintf(theLogFile,", %d%%", d->getInt(TheKey_objectInitialHealth));
					if (showName) {
						fprintf(theLogFile,", Name %s", name.str());
					} else {
						fprintf(theLogFile,", Unnamed");
					}
					if (showScript) {
					fprintf(theLogFile,", Script %s", script.str());
					} else {
					fprintf(theLogFile,", No Script");
					}

					fprintf(theLogFile,"\n");
				}
			}
		}
		fprintf(theLogFile,"End of Buildings\n");

		// dump the units
		fprintf(theLogFile,"\nUnits\n");
		for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext()) {
			const ThingTemplate* tt = pMapObj->getThingTemplate();
			if (tt)	{
				if (tt->getEditorSorting() == ES_VEHICLE || tt->getEditorSorting() == ES_INFANTRY) {
					Bool exists;
					Dict *d = pMapObj->getProperties();
					TeamsInfo *teamInfo = TheSidesList->findTeamInfo(d->getAsciiString(TheKey_originalOwner));
					Dict *teamDict = (teamInfo)?teamInfo->getDict():NULL;

					AsciiString objectOwnerName = (teamDict)?teamDict->getAsciiString(TheKey_teamOwner):noOwner;
					Int veterancy = d->getInt(TheKey_objectVeterancy, &exists);
					if (!exists) {
						veterancy = 0;
					}
					Int aggro = d->getInt(TheKey_objectAggressiveness, &exists);
					if (!exists) {
						aggro = 0;
					}
					aggro++;

					Bool showScript = false;
					AsciiString script = d->getAsciiString(TheKey_objectScriptAttachment, &showScript);

					Bool showName = false;
					AsciiString name = d->getAsciiString(TheKey_objectName, &showName);

					fprintf(theLogFile,"  %s", tt->getName().str());
					fprintf(theLogFile,", %s", objectOwnerName.str());
					fprintf(theLogFile,", @ %0.0f.%0.0f", pMapObj->getLocation()->x/10, pMapObj->getLocation()->y/10);
					fprintf(theLogFile,", Angle %0.0f", pMapObj->getAngle() * 180 / PI);
					fprintf(theLogFile,", %d%%", d->getInt(TheKey_objectInitialHealth));
					if (showName) {
						fprintf(theLogFile,", Name %s", name.str());
					} else {
						fprintf(theLogFile,", Unnamed");
					}
					if (showScript) {
					fprintf(theLogFile,", Script %s", script.str());
					} else {
					fprintf(theLogFile,", No Script");
					}
					fprintf(theLogFile,", Team %s", d->getAsciiString(TheKey_originalOwner).str());
					fprintf(theLogFile,", %s", d->getBool(TheKey_objectRecruitableAI, &exists)?"AIRecruitable":"Not AIRecruitable");
					fprintf(theLogFile,", %s", d->getBool(TheKey_objectSelectable, &exists)?"Selectable":"Not Selectable");
					fprintf(theLogFile,", %s", aggroStrings[aggro]);
					fprintf(theLogFile,", %s", vetStrings[veterancy]);

					fprintf(theLogFile,"\n");
				}
			}
		}
		fprintf(theLogFile,"End of Units\n");
		
		fprintf(theLogFile,"\nObject Types summary\n");
		{
			Int totalObjectCount = 0;
			std::map<AsciiString, Int> mapOfTemplates;
			std::map<AsciiString, Int>::iterator it;
			for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext()) {
				if (pMapObj->getThingTemplate()) {
					++totalObjectCount;

					it = mapOfTemplates.find(pMapObj->getThingTemplate()->getName());
					if (it == mapOfTemplates.end()) {
						mapOfTemplates.insert(std::make_pair(pMapObj->getThingTemplate()->getName(), 1));
					} else {
						++(it->second);
					}
				}
			}

			fprintf(theLogFile, "Total Map Objects (with ThingTemplates): %d\n", totalObjectCount);

			while (mapOfTemplates.size() > 0) {
				std::map<AsciiString, Int>::iterator storedIt = mapOfTemplates.begin();
				
				for (it = mapOfTemplates.begin(); it != mapOfTemplates.end(); ++it) {
					if (storedIt->second < it->second) {
						storedIt = it;
					}
				}

				fprintf(theLogFile, "Map Object: %s, Instances: %d\n", storedIt->first.str(), storedIt->second); 
				
				// Now, erase it. 
				mapOfTemplates.erase(storedIt);
			}
		}
		fprintf(theLogFile,"\nEnd of Object Types summary\n");
		
		// dump the waypoints
		fprintf(theLogFile,"\nWaypoints\n");
		for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext()) {
			if (pMapObj->isWaypoint()) {
				fprintf(theLogFile,"  %s, @ %0.0f.%0.0f\n", pMapObj->getWaypointName().str(), pMapObj->getLocation()->x/10, pMapObj->getLocation()->y/10);
			}
		}
		fprintf(theLogFile,"End of Waypoints\n");

		fprintf(theLogFile,"\nProps\n");
		for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext()) {
			const ThingTemplate* tt = pMapObj->getThingTemplate();
			if (tt)	{
				if (tt->getEditorSorting() == ES_MISC_MAN_MADE) {
					Dict *d = pMapObj->getProperties();
					TeamsInfo *teamInfo = TheSidesList->findTeamInfo(d->getAsciiString(TheKey_originalOwner));
					Dict *teamDict = (teamInfo)?teamInfo->getDict():NULL;

					AsciiString objectOwnerName = (teamDict)?teamDict->getAsciiString(TheKey_teamOwner):noOwner;

					Bool showName = false;
					AsciiString name = d->getAsciiString(TheKey_objectName, &showName);

					fprintf(theLogFile,"  %s", tt->getName().str());
					fprintf(theLogFile,", %s", objectOwnerName.str());
					fprintf(theLogFile,", @ %0.0f.%0.0f", pMapObj->getLocation()->x/10, pMapObj->getLocation()->y/10);
					fprintf(theLogFile,", Angle %0.0f", pMapObj->getAngle() * 180 / PI);
					fprintf(theLogFile,", %d%%", d->getInt(TheKey_objectInitialHealth));
					if (showName) {
						fprintf(theLogFile,", Name %s", name.str());
					} else {
						fprintf(theLogFile,", Unnamed");
					}

					fprintf(theLogFile,"\n");
				}
			}
		}
		fprintf(theLogFile,"End of Props\n");

		fprintf(theLogFile,"\nAudio\n");
		for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext()) {
			const ThingTemplate* tt = pMapObj->getThingTemplate();
			if (tt)	{
				if (tt->getEditorSorting() == ES_AUDIO) {
					Dict *d = pMapObj->getProperties();

					Bool showName = false;
					AsciiString name = d->getAsciiString(TheKey_objectName, &showName);

					fprintf(theLogFile,"  %s", tt->getName().str());
					fprintf(theLogFile,", @ %0.0f.%0.0f", pMapObj->getLocation()->x/10, pMapObj->getLocation()->y/10);
					if (showName) {
						fprintf(theLogFile,", Name %s", name.str());
					} else {
						fprintf(theLogFile,", Unnamed");
					}

					fprintf(theLogFile,"\n");
				}
			}
		}
		fprintf(theLogFile,"End of Audio\n");

		fprintf(theLogFile,"\nTeams\n");
		Int j;
		for (j=0; j<TheSidesList->getNumSides(); j++) {
			Dict *d = TheSidesList->getSideInfo(j)->getDict();
#ifdef DUMP_RAW_DICTS
writeRawDict( theLogFile, "TeamDict",d );
#endif
			AsciiString name = d->getAsciiString(TheKey_playerName);
			AsciiString fmt;
			if (name.isEmpty())
				fmt.format("%s", "(neutral)");
			else
				fmt.format("%s",name.str());
			fprintf(theLogFile, "PLAYER %s\n", fmt.str());
			Int numTeams = TheSidesList->getNumTeams();
			for (Int i=0; i<numTeams; i++)
			{
				TeamsInfo *ti = TheSidesList->getTeamInfo(i);
#ifdef DUMP_RAW_DICTS
writeRawDict( theLogFile, "TeamInfo",ti->getDict() );
#endif
				if (ti->getDict()->getAsciiString(TheKey_teamOwner) == name)
				{
					Bool exists;														
					AsciiString teamName = ti->getDict()->getAsciiString(TheKey_teamName);
					AsciiString waypoint = ti->getDict()->getAsciiString(TheKey_teamHome, &exists);
					CString pri;
					pri.Format(TEXT("%d"), ti->getDict()->getInt(TheKey_teamProductionPriority, &exists));
					AsciiString trigger = ti->getDict()->getAsciiString(TheKey_teamProductionCondition, &exists);

					fprintf(theLogFile, "TEAM %s home '%s', priority %s, condition '%s',\n", teamName.str(),
						waypoint.str(), static_cast<LPCSTR>(pri), trigger.str());
					fprintf(theLogFile, "  UNITS:");
					fprintUnit(theLogFile, ti->getDict(), TheKey_teamUnitMinCount1, TheKey_teamUnitMaxCount1, TheKey_teamUnitType1);
					fprintUnit(theLogFile, ti->getDict(), TheKey_teamUnitMinCount2, TheKey_teamUnitMaxCount2, TheKey_teamUnitType2);
					fprintUnit(theLogFile, ti->getDict(), TheKey_teamUnitMinCount3, TheKey_teamUnitMaxCount3, TheKey_teamUnitType3);
					fprintUnit(theLogFile, ti->getDict(), TheKey_teamUnitMinCount4, TheKey_teamUnitMaxCount4, TheKey_teamUnitType4);
					fprintUnit(theLogFile, ti->getDict(), TheKey_teamUnitMinCount5, TheKey_teamUnitMaxCount5, TheKey_teamUnitType5);
					fprintUnit(theLogFile, ti->getDict(), TheKey_teamUnitMinCount6, TheKey_teamUnitMaxCount6, TheKey_teamUnitType6);
					fprintUnit(theLogFile, ti->getDict(), TheKey_teamUnitMinCount7, TheKey_teamUnitMaxCount7, TheKey_teamUnitType7);
					fprintf(theLogFile, "\n  SCRIPTS: ");
					AsciiString script = ti->getDict()->getAsciiString(TheKey_teamOnCreateScript, &exists);
					if (script.isEmpty()) script="<none>";
					fprintf(theLogFile, "OnCreate='%s'", script.str());
					script = ti->getDict()->getAsciiString(TheKey_teamOnIdleScript, &exists);
					if (script.isEmpty()) script="<none>";
					fprintf(theLogFile, " OnIdle='%s'", script.str());
					script = ti->getDict()->getAsciiString(TheKey_teamOnDestroyedScript, &exists);
					if (script.isEmpty()) script="<none>";
					fprintf(theLogFile, " OnDestroyed='%s'", script.str());
					script = ti->getDict()->getAsciiString(TheKey_teamEnemySightedScript, &exists);
					if (script.isEmpty()) script="<none>";
					fprintf(theLogFile, " OnEnemySighted='%s'", script.str());
					script = ti->getDict()->getAsciiString(TheKey_teamAllClearScript, &exists);
					if (script.isEmpty()) script="<none>";
					fprintf(theLogFile, " OnAllClear='%s'\n", script.str());
				}
			}								
		}
		fprintf(theLogFile,"End of Teams\n");

		fprintf(theLogFile,"\nScripts\n");
		Int i, groupNdx;
		for (i=0; i<TheSidesList->getNumSides(); i++) {
			Dict *d = TheSidesList->getSideInfo(i)->getDict();
#ifdef DUMP_RAW_DICTS
writeRawDict( theLogFile, "Scripts",d );
#endif
			AsciiString name = d->getAsciiString(TheKey_playerName);
			UnicodeString uni = d->getUnicodeString(TheKey_playerDisplayName);
			AsciiString fmt;
			if (name.isEmpty())
				fmt.format("%s", "(neutral)");
			else
				fmt.format("%s",name.str());
			fprintf(theLogFile, "PLAYER %s\n", fmt.str());
			ScriptList *pSL = TheSidesList->getSideInfo(i)->getScriptList();
			if (pSL) {
				ScriptGroup *pGroup = pSL->getScriptGroup();
				for (groupNdx = 0; pGroup; groupNdx++,pGroup=pGroup->getNext()) {
					AsciiString fmt;
					if (pGroup->getName().isEmpty())
						continue;
					else
						fmt.format("%s",pGroup->getName().str());
					fprintf(theLogFile, "GROUP %s\n", fmt.str());
					Script *pScr = pGroup->getScript();
					if (pScr) {
						Int scriptNdx;
						for (scriptNdx = 0; pScr; scriptNdx++,pScr=pScr->getNext()) {
							AsciiString fmt;
							if (pScr->getName().isEmpty())
								continue;
							fmt = formatScriptLabel(pScr);
							fprintf(theLogFile, "%s:\n", fmt.str());
							AsciiString scriptComment;
							AsciiString scriptText;
							if (pScr) {
								scriptComment = pScr->getComment();
								scriptText = pScr->getUiText();
							}
							if (scriptComment.isNotEmpty()) {
								fprintf(theLogFile, "//:%s:\n", scriptComment.str());
							}
							writeScript(theLogFile, scriptText.str());
							fprintf(theLogFile, "\n");

						}
					}
				}
				Script *pScr = pSL->getScript();
				if (pScr) {
					Int scriptNdx;
					for (scriptNdx = 0; pScr; scriptNdx++,pScr=pScr->getNext()) {
						AsciiString fmt;
						if (pScr->getName().isEmpty())
							continue;
						fmt = formatScriptLabel(pScr);
						fprintf(theLogFile, "%s:\n", fmt.str());
						AsciiString scriptComment;
						AsciiString scriptText;
						if (pScr) {
							scriptComment = pScr->getComment();
							scriptText = pScr->getUiText();
						}
						if (scriptComment.isNotEmpty()) {
							fprintf(theLogFile, "//:%s:\n", scriptComment.str());
						}
						writeScript(theLogFile, scriptText.str());
						fprintf(theLogFile, "\n");
					}
				}
			}
		}
		fprintf(theLogFile,"End of Scripts\n");
		fclose(theLogFile);


		AfxMessageBox("Action completed. The file is located on your worldbuilder directory.", MB_OK | MB_ICONINFORMATION);
		CString openDir = AfxGetApp()->GetProfileString("WorldbuilderApp", "OpenDirectory", "");
		CString dumpPath;
		dumpPath.Format("%s\\%s.txt", openDir, m_strTitle);

		DEBUG_LOG(("dumpPath %s", dumpPath ));

		// Open the file with the default editor (usually Notepad)
		ShellExecute(NULL, "open", dumpPath, NULL, NULL, SW_SHOW);
		open = false;
	} catch (...) {
		if (open) {
			fclose(theLogFile);
		}
	}
}

// Find the index nearest the point in the preferred direction
void FindIndexNearest(CWorldBuilderDoc* pDoc, const Coord3D* point, CPoint* outNdx, DIRECTION pref )
{
	Coord3D testPoint = *point;
	switch(pref)
	{
		case PREFER_CENTER:
		{
			break;
		}
		case PREFER_LEFT:
		{
			testPoint.x -= MAP_XY_FACTOR / 2;
			break;
		}
		case PREFER_TOP:
		{
			testPoint.y += MAP_XY_FACTOR / 2;
			break;
		}
		case PREFER_RIGHT:
		{
			testPoint.x += MAP_XY_FACTOR / 2;
			break;
		}
		case PREFER_BOTTOM:
		{
			testPoint.y -= MAP_XY_FACTOR / 2;
			break;
		}
	};
	
	pDoc->getCellIndexFromCoord(testPoint, outNdx);
}

Bool IndexInRect(CWorldBuilderDoc* pDoc, const Coord3D* bl, const Coord3D* tl, const Coord3D* br, const Coord3D* tr, CPoint* index)
{
	Coord3D testPoint;
	pDoc->getCoordFromCellIndex(*index, &testPoint);	
	return PointInsideRect3D(bl, tl, br, tr, &testPoint);
}

Bool AddUniqueAndNeighbors(CWorldBuilderDoc* pDoc, const Coord3D* bl, const Coord3D* tl, const Coord3D* br, const Coord3D* tr, CPoint ndx, VecHeightMapIndexes* allIndices)
{
	if (!allIndices) {
		return false;
	}

	if (!IndexInRect(pDoc, bl, tl, br, tr, &ndx)) {
		return false;
	}

	if (std::find(allIndices->begin(), allIndices->end(), ndx) != allIndices->end()) {
		return false;
	}

	// we have a winner. This index is both inside the rectangle and not already in the index list.
	allIndices->push_back(ndx);

	// now attempt to add the neighbors.
	// first left
	ndx.x += 1;
	AddUniqueAndNeighbors(pDoc, bl, tl, br, tr, ndx, allIndices);
	
	// then right
	ndx.x -= 2;
	AddUniqueAndNeighbors(pDoc,bl, tl, br, tr, ndx, allIndices);

	// then top
	ndx.x += 1;
	ndx.y += 1;
	AddUniqueAndNeighbors(pDoc,bl, tl, br, tr, ndx, allIndices);

	// then bottom
	ndx.y -= 2;
	AddUniqueAndNeighbors(pDoc, bl, tl, br, tr, ndx, allIndices);
	
	return true;
}


void CWorldBuilderDoc::OnRemoveclifftexmapping() 
{
	if (::AfxMessageBox(IDS_CONFIRM_REMOVE_CLIFF_MAPPING, MB_YESNO) == IDYES) {
		if (m_heightMap) {

			WorldHeightMapEdit *htMapEditCopy = GetHeightMap()->duplicate();
			if (htMapEditCopy == NULL) return;
			if (htMapEditCopy->removeCliffMapping()) {  // does all the work.
				IRegion2D partialRange = {0,0,0,0};
				updateHeightMap(htMapEditCopy, false, partialRange);
				WBDocUndoable *pUndo = new WBDocUndoable(this, htMapEditCopy);
				this->AddAndDoUndoable(pUndo);
				REF_PTR_RELEASE(pUndo); // belongs to this now.	
			} 
			REF_PTR_RELEASE(htMapEditCopy);
		}	
	}
}

Int CWorldBuilderDoc::getNumBoundaries(void) const
{
	return m_heightMap->getNumBoundaries();
}

void CWorldBuilderDoc::getBoundary(Int ndx, ICoord2D* border) const
{
	m_heightMap->getBoundary(ndx, border);
}

void CWorldBuilderDoc::addBoundary(ICoord2D* boundaryToAdd)
{
	m_heightMap->addBoundary(boundaryToAdd);
}

void CWorldBuilderDoc::changeBoundary(Int ndx, ICoord2D *border)
{
	m_heightMap->changeBoundary(ndx, border);
}

// void CWorldBuilderDoc::removeBoundary(Int ndx, ICoord2D *border)
// {
// 	m_heightMap->removeBoundary(ndx, border);
// }

void CWorldBuilderDoc::removeLastBoundary(void)
{
	m_heightMap->removeLastBoundary();
}

void CWorldBuilderDoc::removeAllExtraBoundaries(void)
{
	m_heightMap->removeAllExtraBoundaries();
}


void CWorldBuilderDoc::findBoundaryNear(Coord3D *pt, float okDistance, Int *outNdx, Int *outHandle)
{
	m_heightMap->findBoundaryNear(pt, okDistance, outNdx, outHandle);
}

