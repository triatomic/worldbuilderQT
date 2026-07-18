// WBQtWaterBridge.cpp -- the MFC side of the Qt Water-panel seam. See WBQtWaypointBridge.cpp for
// the pattern. Plain MFC TU (no Qt include); reverse callbacks resolved against the exe at the
// final link (extern "C" keeps the names stable). Whole body guarded by RTS_HAS_QT so the OFF
// build compiles it to an empty object and the MFC build is unchanged.
//
// The MFC WaterOptions is still created as the hidden OFF fallback. Its global statics (water
// height, point spacing, "creating water areas") plus the selected water-area PolygonTrigger are
// the model the Qt panel edits. These callbacks forward to WaterOptions::qt* statics (declared in
// WaterOptions.h, defined in WaterOptions.cpp) that reuse the same helpers the MFC On* handlers
// use -- startUpdateHeight / updateHeight / endUpdateHeight for the height MovePolygonUndoable,
// the OnChangeWaterEdit uniqueness path for the rename, and the OnMakeRiver flow-direction
// reorder -- so the tools keep working unchanged.
#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "WaterOptions.h"
#include "WaypointOptions.h"
#include "GameLogic/PolygonTrigger.h"
#include "Common/MapObject.h"
#include "qt/panels/WBQtWaterBridge.h"

#ifdef RTS_HAS_QT

//----------------------------------------------------------------------------------------
// Helper: copy a string into a caller buffer (no ownership crosses the seam).
//----------------------------------------------------------------------------------------
namespace
{
	void copyString(char *out, int cap, const char *src)
	{
		if (out == NULL || cap <= 0)
		{
			return;
		}
		if (src == NULL)
		{
			out[0] = 0;
			return;
		}
		strncpy(out, src, cap - 1);
		out[cap - 1] = 0;
	}
}

extern "C" {

//----------------------------------------------------------------------------------------
// Water height (global static + selected-trigger apply). GetHeight reads the selected trigger's
// current height; Set drives the MovePolygonUndoable sequence. Min/Max mirror GetPopSliderInfo.
//----------------------------------------------------------------------------------------
int WBQtWater_GetHeight(void)
{
	return WaterOptions::qtGetSelectionHeight();
}

void WBQtWater_SetHeight(int height)
{
	WaterOptions::qtSetHeight(height);
}

void WBQtWater_SetHeightDragStep(int height)
{
	WaterOptions::qtSetHeightDragStep(height);
}

void WBQtWater_EndHeightScrub(void)
{
	WaterOptions::qtEndHeightScrub();
}

int WBQtWater_GetHeightMin(void)
{
	return 0;
}

int WBQtWater_GetHeightMax(void)
{
	// Matches GetPopSliderInfo for IDC_HEIGHT_POPUP: 255 * MAP_HEIGHT_SCALE.
	return (int)(255 * MAP_HEIGHT_SCALE);
}

//----------------------------------------------------------------------------------------
// Point spacing (global static).
//----------------------------------------------------------------------------------------
int WBQtWater_GetSpacing(void)
{
	return WaterOptions::getSpacing();
}

void WBQtWater_SetSpacing(int spacing)
{
	WaterOptions::qtSetSpacing(spacing);
}

//----------------------------------------------------------------------------------------
// "Water Polygon" toggle (global static).
//----------------------------------------------------------------------------------------
int WBQtWater_GetCreatingWaterAreas(void)
{
	return WaterOptions::getCreatingWaterAreas() ? 1 : 0;
}

void WBQtWater_SetCreatingWaterAreas(int on)
{
	WaterOptions::qtSetCreatingWaterAreas(on != 0);
}

//----------------------------------------------------------------------------------------
// Selection state + name + Make River (selected water-area trigger).
//----------------------------------------------------------------------------------------
int WBQtWater_HasSelection(void)
{
	return WaterOptions::qtHasSelection() ? 1 : 0;
}

int WBQtWater_IsWaterArea(void)
{
	return WaterOptions::qtIsWaterArea() ? 1 : 0;
}

int WBQtWater_GetName(char *nameOut, int cap)
{
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();
	if (theTrigger != NULL)
	{
		copyString(nameOut, cap, theTrigger->getTriggerName().str());
		return 1;
	}
	copyString(nameOut, cap, "");
	return 0;
}

int WBQtWater_SetName(const char *name)
{
	return WaterOptions::qtSetName(name) ? 1 : 0;
}

int WBQtWater_GetRiver(void)
{
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();
	if (theTrigger != NULL)
	{
		return theTrigger->isRiver() ? 1 : 0;
	}
	return 0;
}

void WBQtWater_SetRiver(int on)
{
	WaterOptions::qtSetRiver(on != 0);
}

}
#endif
