// WBQtWaypointBridge.cpp -- the MFC side of the Qt Waypoint-panel seam. See WBQtObjectBridge.cpp
// for the pattern. Plain MFC TU (no Qt include); reverse callbacks resolved against the exe at
// the final link (extern "C" keeps the names stable). Whole body guarded by RTS_HAS_QT so the
// OFF build compiles it to an empty object and the MFC build is unchanged.
//
// The MFC WaypointOptions is still created as the hidden OFF fallback. It has no state of its
// own; the panel edits the single selected waypoint MapObject and/or the single selected
// PolygonTrigger. These callbacks reuse WaypointOptions' existing statics
// (getSingleSelectedWaypoint / getSingleSelectedPolygon / isUnique) plus the same model getters
// / setters the MFC On* handlers use, so the tools keep working unchanged. The uniqueness /
// rename path mirrors WaypointOptions::OnChangeWaypointnameEdit; label/bidirectional mirror
// changeWaypointLabel / OnWaypointBidirectional; location mirrors OnEditWaypointLocation*.
#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "WaypointOptions.h"
#include "WorldBuilder.h"
#include "WorldBuilderDoc.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/Scripts.h"
#include "Common/WellKnownKeys.h"
#include "LayersList.h"
#include "qt/panels/WBQtWaypointBridge.h"

#ifdef RTS_HAS_QT

//----------------------------------------------------------------------------------------
// Helpers: copy a string into a caller buffer, and map a 1..3 label index to its key.
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

	NameKeyType labelKeyFor(int labelIndex)
	{
		switch (labelIndex)
		{
			case 1:  return TheKey_waypointPathLabel1;
			case 2:  return TheKey_waypointPathLabel2;
			default: return TheKey_waypointPathLabel3;
		}
	}
}

//----------------------------------------------------------------------------------------
// WaypointOptions Qt-support statics (declared in WaypointOptions.h; defined here so the
// rename path can reuse isUnique() and the trigger-name duplicate check without churning
// WaypointOptions.cpp). Mirrors OnChangeWaypointnameEdit.
//----------------------------------------------------------------------------------------
void WaypointOptions::qtSetSelectionName(const char *name)
{
	if (name == NULL)
	{
		return;
	}
	AsciiString newName(name);

	MapObject *theMapObj = getSingleSelectedWaypoint();
	PolygonTrigger *theTrigger = getSingleSelectedPolygon();

	// check to see if the user-entered name is already in use.
	Bool didMatch = false;

	// check waypoint objects.
	didMatch = !isUnique(newName, theMapObj);

	// check trigger area objects
	PolygonTrigger *pTrig;
	for (pTrig = PolygonTrigger::getFirstPolygonTrigger(); !didMatch && pTrig; pTrig = pTrig->getNext())
	{
		if (pTrig == theTrigger)
		{
			continue; // don't check against yourself.
		}
		AsciiString trigName = pTrig->getTriggerName();
		if (newName == trigName)
		{
			if (pTrig->isValid())
			{
				didMatch = true;
			}
			else
			{
				PolygonTrigger::removePolygonTrigger(pTrig);
			}
			break;
		}
	}

	// if there's a match, throw up a messagebox, otherwise set the name
	if (didMatch)
	{
		::AfxMessageBox("Name already in use");
	}
	else
	{
		if (theMapObj)
		{
			AsciiString layerName = TheLayersList->removeMapObjectFromLayersList(theMapObj);
			theMapObj->setWaypointName(newName);
			theMapObj->validate();
			TheLayersList->addMapObjectToLayersList(theMapObj, layerName);
		}
		else if (theTrigger)
		{
			theTrigger->setTriggerName(newName);
		}
	}
}

extern "C" {

//----------------------------------------------------------------------------------------
// Selection kind.
//----------------------------------------------------------------------------------------
int WBQtWaypoint_GetKind(void)
{
	if (WaypointOptions::getSingleSelectedWaypoint() != NULL)
	{
		return WBQT_WAYPOINT_KIND_WAYPOINT;
	}
	if (WaypointOptions::getSingleSelectedPolygon() != NULL)
	{
		return WBQT_WAYPOINT_KIND_TRIGGER;
	}
	return WBQT_WAYPOINT_KIND_NONE;
}

//----------------------------------------------------------------------------------------
// Name field (dual purpose: waypoint name or trigger area name).
//----------------------------------------------------------------------------------------
int WBQtWaypoint_GetName(char *nameOut, int cap)
{
	MapObject *theMapObj = WaypointOptions::getSingleSelectedWaypoint();
	if (theMapObj != NULL)
	{
		copyString(nameOut, cap, theMapObj->getProperties()->getAsciiString(TheKey_waypointName).str());
		return 1;
	}
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();
	if (theTrigger != NULL)
	{
		copyString(nameOut, cap, theTrigger->getTriggerName().str());
		return 1;
	}
	copyString(nameOut, cap, "");
	return 0;
}

int WBQtWaypoint_SetName(const char *name)
{
	if (name == NULL)
	{
		return 0;
	}
	if (WBQtWaypoint_GetKind() == WBQT_WAYPOINT_KIND_NONE)
	{
		return 0;
	}
	WaypointOptions::qtSetSelectionName(name);
	return 1;
}

//----------------------------------------------------------------------------------------
// Name combo preset entries: player-start keys for a waypoint (mirrors the first ResetContent
// branch), inner/outer perimeter + CombatZone for a trigger (the second branch).
//----------------------------------------------------------------------------------------
int WBQtWaypoint_GetNamePresetCount(void)
{
	int kind = WBQtWaypoint_GetKind();
	if (kind == WBQT_WAYPOINT_KIND_TRIGGER)
	{
		// 8 * (InnerPerimeter# + OuterPerimeter#) + "CombatZone"
		return 17;
	}
	if (kind == WBQT_WAYPOINT_KIND_WAYPOINT)
	{
		// InitialCameraPosition + Player_1..8_Start
		return 9;
	}
	return 0;
}

int WBQtWaypoint_GetNamePreset(int index, char *nameOut, int cap)
{
	int kind = WBQtWaypoint_GetKind();
	if (kind == WBQT_WAYPOINT_KIND_WAYPOINT)
	{
		static const NameKeyType keys[9] =
		{
			TheKey_InitialCameraPosition,
			TheKey_Player_1_Start,
			TheKey_Player_2_Start,
			TheKey_Player_3_Start,
			TheKey_Player_4_Start,
			TheKey_Player_5_Start,
			TheKey_Player_6_Start,
			TheKey_Player_7_Start,
			TheKey_Player_8_Start
		};
		if (index < 0 || index >= 9)
		{
			return 0;
		}
		copyString(nameOut, cap, TheNameKeyGenerator->keyToName(keys[index]).str());
		return 1;
	}
	if (kind == WBQT_WAYPOINT_KIND_TRIGGER)
	{
		if (index < 0 || index >= 17)
		{
			return 0;
		}
		if (index == 16)
		{
			copyString(nameOut, cap, "CombatZone");
			return 1;
		}
		// Entries alternate InnerPerimeter#, OuterPerimeter# for # = 1..8, matching the MFC loop
		// (add inner, then outer, per iteration).
		int perimeter = index / 2;	// 0..7
		char buffer[16];
		sprintf(buffer, "%d", perimeter + 1);
		AsciiString trigger = ((index % 2) == 0) ? AsciiString(INNER_PERIMETER) : AsciiString(OUTER_PERIMETER);
		trigger.concat(buffer);
		copyString(nameOut, cap, trigger.str());
		return 1;
	}
	return 0;
}

//----------------------------------------------------------------------------------------
// Waypoint location (world units). Set mirrors OnEditWaypointLocationX / Y.
//----------------------------------------------------------------------------------------
double WBQtWaypoint_GetLocationX(void)
{
	MapObject *waypt = WaypointOptions::getSingleSelectedWaypoint();
	if (waypt == NULL)
	{
		return 0.0;
	}
	return (double)waypt->getLocation()->x;
}

double WBQtWaypoint_GetLocationY(void)
{
	MapObject *waypt = WaypointOptions::getSingleSelectedWaypoint();
	if (waypt == NULL)
	{
		return 0.0;
	}
	return (double)waypt->getLocation()->y;
}

void WBQtWaypoint_SetLocationX(double x)
{
	MapObject *waypt = WaypointOptions::getSingleSelectedWaypoint();
	if (waypt == NULL)
	{
		return;
	}
	const Coord3D *loc = waypt->getLocation();
	Coord3D newLoc;
	newLoc.x = (Real)x;
	newLoc.y = loc->y;
	newLoc.z = 0;
	waypt->setLocation(&newLoc);
}

void WBQtWaypoint_SetLocationY(double y)
{
	MapObject *waypt = WaypointOptions::getSingleSelectedWaypoint();
	if (waypt == NULL)
	{
		return;
	}
	const Coord3D *loc = waypt->getLocation();
	Coord3D newLoc;
	newLoc.x = loc->x;
	newLoc.y = (Real)y;
	newLoc.z = 0;
	waypt->setLocation(&newLoc);
}

//----------------------------------------------------------------------------------------
// Path labels + bi-directional flag (only meaningful for a LINKED waypoint).
//----------------------------------------------------------------------------------------
int WBQtWaypoint_IsLinked(void)
{
	MapObject *theMapObj = WaypointOptions::getSingleSelectedWaypoint();
	if (theMapObj == NULL)
	{
		return 0;
	}
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc == NULL)
	{
		return 0;
	}
	return pDoc->isWaypointLinked(theMapObj) ? 1 : 0;
}

int WBQtWaypoint_GetLabel(int labelIndex, char *nameOut, int cap)
{
	MapObject *theMapObj = WaypointOptions::getSingleSelectedWaypoint();
	if (theMapObj == NULL)
	{
		copyString(nameOut, cap, "");
		return 0;
	}
	Bool exists;
	AsciiString name = theMapObj->getProperties()->getAsciiString(labelKeyFor(labelIndex), &exists);
	copyString(nameOut, cap, name.str());
	return 1;
}

void WBQtWaypoint_SetLabel(int labelIndex, const char *name)
{
	MapObject *theMapObj = WaypointOptions::getSingleSelectedWaypoint();
	if (theMapObj == NULL)
	{
		return;
	}
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc == NULL || !pDoc->isWaypointLinked(theMapObj))
	{
		return;
	}
	AsciiString value(name != NULL ? name : "");
	theMapObj->getProperties()->setAsciiString(labelKeyFor(labelIndex), value);
	pDoc->updateLinkedWaypointLabels(theMapObj);
}

int WBQtWaypoint_GetBiDirectional(void)
{
	MapObject *theMapObj = WaypointOptions::getSingleSelectedWaypoint();
	if (theMapObj == NULL)
	{
		return 0;
	}
	Bool exists;
	return theMapObj->getProperties()->getBool(TheKey_waypointPathBiDirectional, &exists) ? 1 : 0;
}

void WBQtWaypoint_SetBiDirectional(int on)
{
	MapObject *theMapObj = WaypointOptions::getSingleSelectedWaypoint();
	if (theMapObj == NULL)
	{
		return;
	}
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc == NULL || !pDoc->isWaypointLinked(theMapObj))
	{
		return;
	}
	theMapObj->getProperties()->setBool(TheKey_waypointPathBiDirectional, on ? true : false);
	pDoc->updateLinkedWaypointLabels(theMapObj);
}

}
#endif
