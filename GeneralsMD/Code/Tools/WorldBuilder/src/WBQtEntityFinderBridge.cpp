// WBQtEntityFinderBridge.cpp -- the MFC side of the Qt Entity Finder seam (Tier 5a).
// Plain MFC TU (no Qt include); whole body guarded by RTS_HAS_QT so the OFF build
// compiles it to an empty object. Ports of CAboutDlg's data/actions (WorldBuilder.cpp):
// the hotkey table + two-column formatter, the named-object/waypoint walks, the
// center-on-name view move, the DialogFont choices, and the viewport-resolution write +
// live apply.
#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "DialogFont.h"
#include "MainFrm.h"
#include "WorldBuilderDoc.h"
#include "wbview3d.h"
#include "Common/WellKnownKeys.h"
#include "qt/panels/WBQtEntityFinderBridge.h"

#ifdef RTS_HAS_QT

#include <vector>
#include <string>

// File-local in WorldBuilder.cpp; mirrored here (same INI, same keys).
#define WBQT_ABOUT_SECTION		"AboutWindow"
#define WBQT_VIEWPORT_SECTION	"MainFrame"
#define WBQT_KEY_VIEW_WIDTH		"Width"
#define WBQT_KEY_VIEW_HEIGHT	"Height"

static void copyOut(const char *s, char *buf, int cap)
{
	if (buf == NULL || cap <= 0)
	{
		return;
	}
	strncpy(buf, (s != NULL) ? s : "", cap - 1);
	buf[cap - 1] = 0;
}

//----------------------------------------------------------------------------------------
// The hotkey list. MIRRORS the table in CWorldBuilderApp::OnAppAbout (WorldBuilder.cpp)
// -- keep the two in sync. Formatting is the same two-column pairing (short entries share
// a line, separators and long descriptions get their own).
//----------------------------------------------------------------------------------------
extern "C" int WBQtEntityFinderData_GetHotkeyText(char *bufOut, int cap)
{
	struct HotkeyEntry
	{
		const char* key;
		const char* description;
	};

	static const HotkeyEntry hotkeys[] =
	{
		{ "Tab", "Lock Horizontal" },
		{ "Q", "Brush Tool" },
		{ "W", "Add Brush" },
		{ "E", "Subtract Brush" },
		{ "R", "Feather Tool" },
		{ "T", "Mold Tool  " }, // artificially spaced it out -- theres a bug with tabs
		{ "Y", "Water Tool" },
		{ "A", "Tile Tool    " }, // artificially spaced it out -- theres a bug with tabs
		{ "S", "Big Tile Tool" },
		{ "D", "Tile Flood Fill" },
		{ "F", "Auto Edge Out Tool" },
		{ "G", "Blend Edge Tool" },
		{ "Z", "Place Object Tool" },
		{ "X", "Road Tool" },
		{ "C", "Grove Tool" },
		{ "V", "Ramp Tool" },
		{ "B", "Scorch Tool" },
		{ "N", "Fence Tool" },
		{ "M", "Build List Tool" },
		{ "F1", "Waypoint Tool" },
		{ "F2", "Polygon Tool" },
		{ "F3", "Border Tool" },
		{ "F4", "Script Editor" },
		{ "F5", "Team Editor" },

		{ "==", "===============================" },

		{ "Ctrl+M", "Select Similar" },
		{ "Ctrl+R", "Replace Selected..." },

		{ "Ctrl+W", "Show Wireframe 3D View" },
		{ "Ctrl+F", "Show From Top Down View" },
		{ "Ctrl+U", "Show Clouds" },
		{ "Ctrl+D", "Change Time Of Day" },
		{ "Ctrl+I", "Show Impassable Areas" },
		{ "Ctrl+A", "Show All of 3D Map" },
		{ "Ctrl+Shift+G", "Snap To Grid" },

		{ "Ctrl+Q", "Show Ruler Grid"},
		{ "Ctrl+E", "Show Water"},

		{ "==", "===============================" },

		{ "Alt+1", "Show Objects Icons" },
		{ "Alt+2", "Show Waypoints" },
		{ "Alt+3", "Show Polygon Triggers" },
		{ "Alt+4", "Show Labels" },
		{ "Alt+5", "Show Models" },
		{ "Alt+6", "Show Bounding Boxes" },
		{ "Alt+7", "Show Sight Ranges" },
		{ "Alt+8", "Show Weapon Ranges" },
		{ "Alt+9", "Show Map Boundaries" },
		{ "Alt+0", "Use Fixed Colored Waypoints" },

		{ "==", "===============================" },

		{ "Ctrl+0", "Select Anything" },
		{ "Ctrl+1", "Select Buildings" },
		{ "Ctrl+2", "Select Infantry" },
		{ "Ctrl+3", "Select Vehicles" },
		{ "Ctrl+4", "Select Shrubbery" },
		{ "Ctrl+5", "Select Props" },
		{ "Ctrl+6", "Select Natural" },
		{ "Ctrl+7", "Select Debris / Scorch" },
		{ "Ctrl+8", "Select Waypoints & Areas" },
		{ "Ctrl+9", "Select Roads / Bridges" },

		{ "==", "===============================" },

		{ "Ctrl+Z", "Undo Mode  " },
		{ "Ctrl+X", "Cut Mode   " }, // artificially spaced it out -- theres a bug with tabs
		{ "Ctrl+C", "Copy Mode  " },
		{ "Ctrl+V", "Paste Mode " }, // artificially spaced it out -- theres a bug with tabs
		{ "Ctrl+N", "New File   " },
		{ "Ctrl+O", "Open File" },
		{ "Ctrl+S", "Save File" },
	};

	CString text;
	int numHotkeys = sizeof(hotkeys) / sizeof(hotkeys[0]);
	const int maxFirstColumnLength = 17;

	for (int i = 0; i < numHotkeys; i++)
	{
		if (strcmp(hotkeys[i].key, "==") == 0)
		{
			CString separatorLine;
			separatorLine.Format("%s\t%s", hotkeys[i].key, hotkeys[i].description);
			text += separatorLine + "\r\n";
			continue;
		}

		CString firstEntry;
		firstEntry.Format("%s\t%s", hotkeys[i].key, hotkeys[i].description);

		bool isLast = (i + 1 >= numHotkeys);
		bool nextIsSeparator = !isLast && strcmp(hotkeys[i + 1].key, "==") == 0;

		if (strlen(hotkeys[i].description) >= maxFirstColumnLength || isLast || nextIsSeparator)
		{
			text += firstEntry + "\t" + "\r\n";
		}
		else
		{
			CString secondEntry;
			secondEntry.Format("%s\t%s", hotkeys[i + 1].key, hotkeys[i + 1].description);
			text += firstEntry + "\t" + secondEntry + "\t" + "\r\n";
			i++;
		}
	}

	copyOut((LPCTSTR)text, bufOut, cap);
	return (bufOut != NULL) ? (int)strlen(bufOut) : 0;
}

//----------------------------------------------------------------------------------------
// Named-object / waypoint enumerations (== OnRefreshQueryObject / OnRefreshQueryWaypoint).
//----------------------------------------------------------------------------------------
static std::vector<std::string> s_qtEfObjects;
static std::vector<std::string> s_qtEfWaypoints;

extern "C" int WBQtEntityFinderData_BuildObjects(void)
{
	s_qtEfObjects.clear();
	MapObject *pMapObj = MapObject::getFirstMapObject();
	while (pMapObj)
	{
		Bool exists;
		AsciiString objName = pMapObj->getProperties()->getAsciiString(TheKey_objectName, &exists);
		if (exists && !objName.isEmpty() && !pMapObj->getFlag(FLAG_ROAD_FLAGS))
		{
			s_qtEfObjects.push_back(std::string(objName.str()));
		}
		pMapObj = pMapObj->getNext();
	}
	return (int)s_qtEfObjects.size();
}

extern "C" void WBQtEntityFinderData_GetObject(int i, char *bufOut, int cap)
{
	copyOut((i >= 0 && i < (int)s_qtEfObjects.size()) ? s_qtEfObjects[i].c_str() : "", bufOut, cap);
}

extern "C" int WBQtEntityFinderData_BuildWaypoints(void)
{
	s_qtEfWaypoints.clear();
	MapObject *pMapObj = MapObject::getFirstMapObject();
	while (pMapObj)
	{
		if (pMapObj->isWaypoint())
		{
			AsciiString wayName = pMapObj->getWaypointName();
			if (wayName.isNotEmpty())
			{
				// == the MFC combo's FindStringExact dedup.
				Bool dup = false;
				for (size_t j = 0; j < s_qtEfWaypoints.size(); j++)
				{
					if (s_qtEfWaypoints[j] == wayName.str())
					{
						dup = true;
						break;
					}
				}
				if (!dup)
				{
					s_qtEfWaypoints.push_back(std::string(wayName.str()));
				}
			}
		}
		pMapObj = pMapObj->getNext();
	}
	return (int)s_qtEfWaypoints.size();
}

extern "C" void WBQtEntityFinderData_GetWaypoint(int i, char *bufOut, int cap)
{
	copyOut((i >= 0 && i < (int)s_qtEfWaypoints.size()) ? s_qtEfWaypoints[i].c_str() : "", bufOut, cap);
}

// == CAboutDlg::OnCenterOnSelected.
// Resolve an entity name to its placed MapObject: a named unit (isWaypoint 0, matched on
// TheKey_objectName) or a named waypoint (isWaypoint 1). Returns NULL if not on the map. The one
// name-match walk shared by CenterOn (center-only) and the script editor's SelectEntity
// (select+center) so their match rules can't drift. Returned as void* so it crosses the plain-C
// bridge; callers cast back to MapObject*.
extern "C" void *WBQtEntityFinder_FindByName(const char *name, int isWaypoint)
{
	if (name == NULL || name[0] == 0)
	{
		return NULL;
	}
	AsciiString wanted(name);
	for (MapObject *obj = MapObject::getFirstMapObject(); obj != NULL; obj = obj->getNext())
	{
		Bool match = false;
		if (!isWaypoint)
		{
			Bool exists = false;
			AsciiString objName = obj->getProperties()->getAsciiString(TheKey_objectName, &exists);
			match = (exists && !objName.isEmpty() && objName == wanted);
		}
		else if (obj->isWaypoint())
		{
			AsciiString wayName = obj->getWaypointName();
			match = (!wayName.isEmpty() && wayName == wanted);
		}
		if (match)
		{
			return obj;
		}
	}
	return NULL;
}

extern "C" void WBQtEntityFinder_CenterOn(const char *name, int isWaypoint)
{
	MapObject *obj = (MapObject *)WBQtEntityFinder_FindByName(name, isWaypoint);
	if (obj == NULL)
	{
		return;
	}
	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (p3View != NULL)
	{
		const Coord3D *pos = obj->getLocation();
		p3View->setCenterInView(pos->x / MAP_XY_FACTOR, pos->y / MAP_XY_FACTOR);
	}
}

//----------------------------------------------------------------------------------------
// Dialog font choices (single source of truth in DialogFont.cpp; applies on restart).
//----------------------------------------------------------------------------------------
extern "C" int WBQtEntityFinderData_GetFontCount(void)
{
	return GetDialogFontChoiceCount();
}

extern "C" void WBQtEntityFinderData_GetFontLabel(int i, char *bufOut, int cap)
{
	if (i < 0 || i >= GetDialogFontChoiceCount())
	{
		copyOut("", bufOut, cap);
		return;
	}
	const DialogFontChoice &fc = GetDialogFontChoice(i);
	CString label = fc.displayName;
	if (fc.mayClip)
	{
		label += " (may clip)";
	}
	copyOut((LPCTSTR)label, bufOut, cap);
}

extern "C" int WBQtEntityFinderData_GetFontSel(void)
{
	return LoadDialogFontChoice();
}

extern "C" void WBQtEntityFinder_SetFontSel(int i)
{
	if (i >= 0 && i < GetDialogFontChoiceCount())
	{
		SaveDialogFontChoice(i);
	}
}

//----------------------------------------------------------------------------------------
// Viewport resolution (== CAboutDlg::saveViewportResolution: write + apply live).
//----------------------------------------------------------------------------------------
extern "C" void WBQtEntityFinderData_GetSavedResolution(int *wOut, int *hOut)
{
	if (wOut != NULL)
	{
		*wOut = ::AfxGetApp()->GetProfileInt(WBQT_VIEWPORT_SECTION, WBQT_KEY_VIEW_WIDTH, 0);
	}
	if (hOut != NULL)
	{
		*hOut = ::AfxGetApp()->GetProfileInt(WBQT_VIEWPORT_SECTION, WBQT_KEY_VIEW_HEIGHT, 0);
	}
}

extern "C" void WBQtEntityFinder_SetResolution(int w, int h)
{
	if (w <= 0 || h <= 0)
	{
		return;
	}
	::AfxGetApp()->WriteProfileInt(WBQT_VIEWPORT_SECTION, WBQT_KEY_VIEW_WIDTH, w);
	::AfxGetApp()->WriteProfileInt(WBQT_VIEWPORT_SECTION, WBQT_KEY_VIEW_HEIGHT, h);
	if (CMainFrame::GetMainFrame())
	{
		CMainFrame::GetMainFrame()->adjustWindowSize(true, false);
	}
}

//----------------------------------------------------------------------------------------
// The [AboutWindow] profile section.
//----------------------------------------------------------------------------------------
extern "C" int WBQtEntityFinderData_GetProfileInt(const char *key, int def)
{
	return (key != NULL) ? ::AfxGetApp()->GetProfileInt(WBQT_ABOUT_SECTION, key, def) : def;
}

extern "C" void WBQtEntityFinder_SetProfileInt(const char *key, int value)
{
	if (key != NULL)
	{
		::AfxGetApp()->WriteProfileInt(WBQT_ABOUT_SECTION, key, value);
	}
}

//----------------------------------------------------------------------------------------
// Undo history depth ([MainFrame] MaxUndos): persisted, and applied to the open
// document immediately (new adds trim the list to the new cap).
//----------------------------------------------------------------------------------------
extern "C" int WBQtEntityFinderData_GetMaxUndos(void)
{
	return ::AfxGetApp()->GetProfileInt("MainFrame", "MaxUndos", MAX_UNDOS);
}

extern "C" void WBQtEntityFinder_SetMaxUndos(int count)
{
	::AfxGetApp()->WriteProfileInt("MainFrame", "MaxUndos", count);
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc != NULL)
	{
		pDoc->setMaxUndos(count);
	}
}

#endif // RTS_HAS_QT
