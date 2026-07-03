// WBQtShortcuts.cpp -- see WBQtShortcuts.h. The table mirrors the IDR_MAINFRAME
// ACCELERATORS block in res/WorldBuilder.rc:505-584 EXACTLY (keep the two in sync). Each
// row is (virtual key, modifier mask) -> command id. The modifier mask is the FULL chord:
// bare Z (place-object tool), Ctrl+Z (undo) and Ctrl+Shift+Z (redo) are three separate
// rows, matched against the live GetKeyState at the moment the key arrives.
//
// Qt is not actually used here (this is Win32-only), but the file lives in the Qt static
// lib so it links beside the rest of the bridge and stays out of the OFF build.
#include "WBQtShortcuts.h"

#include <qt_windows.h>

// Bridge entry points. The WBQt_* ones are declared in WBQtBridge.h with plain C++
// linkage (that facade is C++), so match that here -- NOT extern "C". WBQt_GetHostedViewWindow
// is a C++ symbol defined in WBQtBridge.cpp. WBQtChrome_PopupActive is genuinely extern "C".
int  WBQt_InversionActive(void);
void *WBQt_GetHostedViewWindow(void);
void *WBQt_MainWindowHwnd(void);
void WBQt_ToggleFullscreen(void);
int  WBQt_IsFullscreen(void);
extern "C" int  WBQtChrome_PopupActive(void);

// Modifier mask bits (our own -- not the RC's ACCEL fVirt).
enum
{
	WBK_NONE  = 0,
	WBK_CTRL  = 1,
	WBK_SHIFT = 2,
	WBK_ALT   = 4
};

// Standard MFC command ids (afxres.h; that header can't be included in this Win32-only TU,
// same #define approach as WBQtChrome.cpp). Verified against afxres.h.
#define WBID_FILE_NEW      0xE100
#define WBID_FILE_OPEN     0xE101
#define WBID_FILE_SAVE     0xE103
#define WBID_EDIT_COPY     0xE122
#define WBID_EDIT_CUT      0xE123
#define WBID_EDIT_PASTE    0xE125
#define WBID_EDIT_REPLACE  0xE129
#define WBID_EDIT_UNDO     0xE12B
#define WBID_EDIT_REDO     0xE12C

// WorldBuilder command ids (res/resource.h -- transcribed as literals so this TU stays
// free of the MFC resource header; keep in sync with resource.h + the .rc accel table).
#define WBID_EDIT_PICKANYTHING          33001
#define WBID_EDIT_PICKSTRUCTS           32994
#define WBID_EDIT_PICKINFANTRY          32995
#define WBID_EDIT_PICKVEHICLES          32996
#define WBID_EDIT_PICKSHRUBBERY         32997
#define WBID_EDIT_PICKMANMADE           32998
#define WBID_EDIT_PICKNATURAL           32999
#define WBID_EDIT_PICKDEBRIS            33000
#define WBID_EDIT_PICKWAYPOINTS         33002
#define WBID_EDIT_PICKROADS             33327
#define WBID_GROUP_PIVOT_CENTER         33019
#define WBID_GROUP_ROTATE_OBJECT        33020
#define WBID_VIEW_SHOWENTIRE3DMAP       32943
#define WBID_VIEW_SHOW_OBJECTS          32926
#define WBID_VIEW_TIME_OF_DAY           32942
#define WBID_VIEW_SHOWTOPDOWNVIEW       32944
#define WBID_ShowGrid                   32772
#define WBID_VIEW_SNAPTOGRID            32939
#define WBID_VIEW_SHOWIMPASSABLEAREAS   32981
#define WBID_FILE_JUMPTOGAME            32993
#define WBID_EDIT_LINK_CENTERS          32925
#define WBID_EDIT_SELECTSIMILAR         32988
#define WBID_VIEW_SHOWTEXTURE           32927
#define WBID_VIEW_SHOWCLOUDS            32945
#define WBID_VIEW_SHOWWATER             32967
#define WBID_VIEW_SHOWWIREFRAME         32934
#define WBID_VIEW_RULERGRID             33329
#define WBID_VIEWHOME                   33334
#define WBID_LOCK_HORIZONTAL            32962
#define WBID_BRUSH_TOOL                 32771
#define WBID_BRUSH_ADD_TOOL             32900
#define WBID_BRUSH_SUBTRACT_TOOL        32901
#define WBID_FEATHERTOOL                32791
#define WBID_MOLD_TOOL                  32955
#define WBID_WATER_TOOL                 32986
#define WBID_TILE_TOOL                  32902
#define WBID_BIG_TILE_TOOL              32792
#define WBID_TILE_FLOOD_FILL            32903
#define WBID_AUTO_EDGE_OUT_TOOL         32905
#define WBID_BLEND_EDGE_TOOL            32922
#define WBID_PLACE_OBJECT_TOOL          32918
#define WBID_ROAD_TOOL                  32937
#define WBID_GROVE_TOOL                 32924
#define WBID_RAMPTOOL                   61467
#define WBID_SCORCH_TOOL                33007
#define WBID_FENCE_TOOL                 32979
#define WBID_BUILD_LIST_TOOL            32972
#define WBID_WAYPOINT_TOOL              32964
#define WBID_POLYGON_TOOL               32968
#define WBID_BORDERTOOL                 33330
#define WBID_SCRIPT_EDIT                32959
#define WBID_TEAM_EDIT                  32960
#define WBID_VIEW_SHOWWAYPOINTS         32966
#define WBID_VIEW_SHOWPOLYGONTRIGGERS   32969
#define WBID_VIEW_LABELS                33003
#define WBID_VIEW_SHOWMODELS            33004
#define WBID_VIEW_BOUNDINGBOXES         33008
#define WBID_VIEW_SIGHTRANGES           33009
#define WBID_VIEW_WEAPONRANGES          33010
#define WBID_VIEW_SHOWMAPBOUNDARIES     33331
#define WBID_VIEW_FIXEDCOLOREDWAYPOINTS 32980

struct WBAccel
{
	int vk;
	unsigned mods;
	int commandId;
};

// == IDR_MAINFRAME ACCELERATORS. The two DEAD F6 pane-cycling rows (ID_NEXT_PANE /
// ID_PREV_PANE -- SDI, no panes) are intentionally omitted.
static const WBAccel s_accels[] =
{
	// Ctrl+digit -- pick constraints
	{ '0', WBK_CTRL, WBID_EDIT_PICKANYTHING },
	{ '1', WBK_CTRL, WBID_EDIT_PICKSTRUCTS },
	{ '2', WBK_CTRL, WBID_EDIT_PICKINFANTRY },
	{ '3', WBK_CTRL, WBID_EDIT_PICKVEHICLES },
	{ '4', WBK_CTRL, WBID_EDIT_PICKSHRUBBERY },
	{ '5', WBK_CTRL, WBID_EDIT_PICKMANMADE },
	{ '6', WBK_CTRL, WBID_EDIT_PICKNATURAL },
	{ '7', WBK_CTRL, WBID_EDIT_PICKDEBRIS },
	{ '8', WBK_CTRL, WBID_EDIT_PICKWAYPOINTS },
	{ '9', WBK_CTRL, WBID_EDIT_PICKROADS },

	// Ctrl+Shift+digit
	{ '1', WBK_CTRL | WBK_SHIFT, WBID_GROUP_PIVOT_CENTER },
	{ '2', WBK_CTRL | WBK_SHIFT, WBID_GROUP_ROTATE_OBJECT },

	// Ctrl+letter
	{ 'A', WBK_CTRL, WBID_VIEW_SHOWENTIRE3DMAP },
	{ 'B', WBK_CTRL, WBID_VIEW_SHOW_OBJECTS },
	{ 'C', WBK_CTRL, WBID_EDIT_COPY },
	{ 'D', WBK_CTRL, WBID_VIEW_TIME_OF_DAY },
	{ 'E', WBK_CTRL, WBID_VIEW_SHOWWATER },
	{ 'F', WBK_CTRL, WBID_VIEW_SHOWTOPDOWNVIEW },
	{ 'G', WBK_CTRL, WBID_ShowGrid },
	{ 'G', WBK_CTRL | WBK_SHIFT, WBID_VIEW_SNAPTOGRID },
	{ 'I', WBK_CTRL, WBID_VIEW_SHOWIMPASSABLEAREAS },
	{ 'J', WBK_CTRL, WBID_FILE_JUMPTOGAME },
	{ 'L', WBK_CTRL, WBID_EDIT_LINK_CENTERS },
	{ 'M', WBK_CTRL, WBID_EDIT_SELECTSIMILAR },
	{ 'N', WBK_CTRL, WBID_FILE_NEW },
	{ 'O', WBK_CTRL, WBID_FILE_OPEN },
	{ 'Q', WBK_CTRL, WBID_VIEW_RULERGRID },
	{ 'R', WBK_CTRL, WBID_EDIT_REPLACE },
	{ 'S', WBK_CTRL, WBID_FILE_SAVE },
	{ 'T', WBK_CTRL, WBID_VIEW_SHOWTEXTURE },
	{ 'U', WBK_CTRL, WBID_VIEW_SHOWCLOUDS },
	{ 'V', WBK_CTRL, WBID_EDIT_PASTE },
	{ 'W', WBK_CTRL, WBID_VIEW_SHOWWIREFRAME },
	{ 'X', WBK_CTRL, WBID_EDIT_CUT },
	{ 'Z', WBK_CTRL, WBID_EDIT_UNDO },
	{ 'Z', WBK_CTRL | WBK_SHIFT, WBID_EDIT_REDO },

	// Alt+Back / Shift+Del / Ctrl+Ins / Shift+Ins -- edit multibindings
	{ VK_BACK,   WBK_ALT,   WBID_EDIT_UNDO },
	{ VK_DELETE, WBK_SHIFT, WBID_EDIT_CUT },
	{ VK_INSERT, WBK_CTRL,  WBID_EDIT_COPY },
	{ VK_INSERT, WBK_SHIFT, WBID_EDIT_PASTE },

	// Nav / function keys
	{ VK_HOME, WBK_NONE, WBID_VIEWHOME },
	{ VK_F1,   WBK_NONE, WBID_WAYPOINT_TOOL },
	{ VK_F2,   WBK_NONE, WBID_POLYGON_TOOL },
	{ VK_F3,   WBK_NONE, WBID_BORDERTOOL },
	{ VK_F4,   WBK_NONE, WBID_SCRIPT_EDIT },
	{ VK_F5,   WBK_NONE, WBID_TEAM_EDIT },

	// Bare single-key tool hotkeys
	{ VK_TAB, WBK_NONE, WBID_LOCK_HORIZONTAL },
	{ 'Q', WBK_NONE, WBID_BRUSH_TOOL },
	{ 'W', WBK_NONE, WBID_BRUSH_ADD_TOOL },
	{ 'E', WBK_NONE, WBID_BRUSH_SUBTRACT_TOOL },
	{ 'R', WBK_NONE, WBID_FEATHERTOOL },
	{ 'T', WBK_NONE, WBID_MOLD_TOOL },
	{ 'Y', WBK_NONE, WBID_WATER_TOOL },
	{ 'A', WBK_NONE, WBID_TILE_TOOL },
	{ 'S', WBK_NONE, WBID_BIG_TILE_TOOL },
	{ 'D', WBK_NONE, WBID_TILE_FLOOD_FILL },
	{ 'F', WBK_NONE, WBID_AUTO_EDGE_OUT_TOOL },
	{ 'G', WBK_NONE, WBID_BLEND_EDGE_TOOL },
	{ 'Z', WBK_NONE, WBID_PLACE_OBJECT_TOOL },
	{ 'X', WBK_NONE, WBID_ROAD_TOOL },
	{ 'C', WBK_NONE, WBID_GROVE_TOOL },
	{ 'V', WBK_NONE, WBID_RAMPTOOL },
	{ 'B', WBK_NONE, WBID_SCORCH_TOOL },
	{ 'N', WBK_NONE, WBID_FENCE_TOOL },
	{ 'M', WBK_NONE, WBID_BUILD_LIST_TOOL },

	// Alt+digit -- view toggles (arrive as WM_SYSKEYDOWN)
	{ '1', WBK_ALT, WBID_VIEW_SHOW_OBJECTS },
	{ '2', WBK_ALT, WBID_VIEW_SHOWWAYPOINTS },
	{ '3', WBK_ALT, WBID_VIEW_SHOWPOLYGONTRIGGERS },
	{ '4', WBK_ALT, WBID_VIEW_LABELS },
	{ '5', WBK_ALT, WBID_VIEW_SHOWMODELS },
	{ '6', WBK_ALT, WBID_VIEW_BOUNDINGBOXES },
	{ '7', WBK_ALT, WBID_VIEW_SIGHTRANGES },
	{ '8', WBK_ALT, WBID_VIEW_WEAPONRANGES },
	{ '9', WBK_ALT, WBID_VIEW_SHOWMAPBOUNDARIES },
	{ '0', WBK_ALT, WBID_VIEW_FIXEDCOLOREDWAYPOINTS },
};

static const int s_accelCount = (int)(sizeof(s_accels) / sizeof(s_accels[0]));

static int wbLookup(int vk, unsigned mods)
{
	for (int i = 0; i < s_accelCount; i++)
	{
		if (s_accels[i].vk == vk && s_accels[i].mods == mods)
		{
			return s_accels[i].commandId;
		}
	}
	return 0;
}

extern "C" int WBQtShortcuts_TranslateKey(void *pMsgVoid)
{
	if (!WBQt_InversionActive())
	{
		return 0;		// pre-inversion / fallback path keeps the MFC accelerator table
	}

	MSG *pMsg = reinterpret_cast<MSG *>(pMsgVoid);
	const UINT msg = pMsg->message;
	if (msg != WM_KEYDOWN && msg != WM_SYSKEYDOWN)
	{
		return 0;		// only key-down drives hotkeys; KEYUP/CHAR/mouse untouched (polling ok)
	}
	const int vk = (int)pMsg->wParam;

	// F11 / Esc fullscreen: owned here regardless of focus (the frame's old handler is dead
	// for viewport-focused keys under the inversion). Esc only exits when in fullscreen so a
	// tool's own Esc-cancel still reaches it otherwise.
	if (msg == WM_KEYDOWN)
	{
		if (vk == VK_F11)
		{
			WBQt_ToggleFullscreen();
			return 1;
		}
		if (vk == VK_ESCAPE && WBQt_IsFullscreen())
		{
			WBQt_ToggleFullscreen();
			return 1;
		}
	}

	// While a Qt chrome menu popup is open every key must reach Qt (menu navigation).
	if (WBQtChrome_PopupActive())
	{
		return 0;
	}

	// Focus gate (replaces the 7 *_OwnsFocus guards): act only when the hosted viewport --
	// or the Qt main window itself -- owns Win32 focus. A line-edit in any floating Qt tool
	// window is a different HWND, so its typing is never intercepted.
	HWND focus = ::GetFocus();
	HWND view = reinterpret_cast<HWND>(WBQt_GetHostedViewWindow());
	HWND mainWin = reinterpret_cast<HWND>(WBQt_MainWindowHwnd());
	if (focus != view && focus != mainWin)
	{
		return 0;
	}

	unsigned mods = 0;
	if (::GetKeyState(VK_CONTROL) & 0x8000)
	{
		mods |= WBK_CTRL;
	}
	if (::GetKeyState(VK_SHIFT) & 0x8000)
	{
		mods |= WBK_SHIFT;
	}
	if (msg == WM_SYSKEYDOWN)
	{
		// Alt is down (that's what makes it a syskey). Consume ONLY Alt+digit view toggles;
		// let Alt+letter fall through so the Qt menubar mnemonics still open menus.
		mods |= WBK_ALT;
		if (vk < '0' || vk > '9')
		{
			return 0;
		}
	}

	// Bare keys the MFC view must keep owning -- never translate these (WbView::OnKeyDown
	// handles Delete/Backspace object-delete and [ ] brush resize; arrows nudge). Only bare
	// (no modifier) -- Shift+Del / Alt+Back above are real table rows and already matched.
	if (mods == WBK_NONE)
	{
		if (vk == VK_DELETE || vk == VK_BACK || vk == VK_OEM_4 || vk == VK_OEM_6
			|| vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN)
		{
			return 0;
		}
	}

	int commandId = wbLookup(vk, mods);
	if (commandId == 0)
	{
		return 0;
	}
	WBQtShortcuts_PostCommand(commandId);
	return 1;
}
