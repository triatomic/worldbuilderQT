// WBQtObjectPropsBridge.cpp -- MFC side of the Qt Object Properties facade.
//
// These extern "C" reverse callbacks (declared in qt/panels/WBQtObjectPropsBridge.h) forward to
// the MapObjectProps::qt* statics. The whole body is behind RTS_HAS_QT; with Qt OFF this TU is
// empty, exactly like the other WBQt*Bridge.cpp files, so the MFC-only build is unchanged.

#include "StdAfx.h"

#ifdef RTS_HAS_QT

#include "mapobjectprops.h"
#include "WorldBuilderDoc.h"
#include "CUndoable.h"
#include "Common/WellKnownKeys.h"
#include "GameLogic/SidesList.h"
#include "qt/panels/WBQtObjectPropsBridge.h"

//----------------------------------------------------------------------------------------
// De-bridged (windowless) MapObjectProps members -- branch qt-debridge. Each qtM* below is
// the matching _XToDict handler's model core VERBATIM, with the value passed in instead of
// read back from a hidden control (the control write + read-back round trip only existed
// for the MFC panel). getAllSelectedDicts() still runs first so the multi-select
// DictItemUndoable path is identical; the exact undoable ctor variants (with/without the
// pDoc,true tail; explicit key for the remove-when-default cases) are preserved.
//----------------------------------------------------------------------------------------

// == the _DictTo* seed values the hidden checkboxes used to hold (incl. the Selectable
// tri-state: key absent == 2).
int MapObjectProps::qtMGetFlag(int which)
{
	Dict *d = (TheMapObjectProps != NULL) ? TheMapObjectProps->m_dictToEdit : NULL;
	Bool exists = false;
	switch (which)
	{
		case WBQT_OBJPROP_FLAG_ENABLED:
		{
			Bool v = true;
			if (d) { v = d->getBool(TheKey_objectEnabled, &exists); }
			return v ? 1 : 0;
		}
		case WBQT_OBJPROP_FLAG_INDESTRUCTIBLE:
		{
			Bool v = true;
			if (d) { v = d->getBool(TheKey_objectIndestructible, &exists); }
			return v ? 1 : 0;
		}
		case WBQT_OBJPROP_FLAG_UNSELLABLE:
		{
			Bool v = false;
			if (d) { v = d->getBool(TheKey_objectUnsellable, &exists); }
			return v ? 1 : 0;
		}
		case WBQT_OBJPROP_FLAG_TARGETABLE:
		{
			Bool v = false;
			if (d) { v = d->getBool(TheKey_objectTargetable, &exists); }
			return v ? 1 : 0;
		}
		case WBQT_OBJPROP_FLAG_POWERED:
		{
			Bool v = true;
			if (d) { v = d->getBool(TheKey_objectPowered, &exists); }
			return v ? 1 : 0;
		}
		case WBQT_OBJPROP_FLAG_RECRUITABLEAI:
		{
			Bool v = true;
			if (d) { v = d->getBool(TheKey_objectRecruitableAI, &exists); }
			return v ? 1 : 0;
		}
		case WBQT_OBJPROP_FLAG_SELECTABLE:
		{
			Int v = true;
			if (d) { v = d->getBool(TheKey_objectSelectable, &exists); }
			if (!exists) { v = 2; }
			return v;
		}
		default:
			return 0;
	}
}

// Position/Z/Angle read the LIVE object (the m_position/m_height/m_angle members are only
// refreshed by the Show* control seeders, which don't run windowless).
int MapObjectProps::qtMGetPosition(char *out, int cap)
{
	if (out == NULL || cap <= 0)
	{
		return 0;
	}
	out[0] = 0;
	if (TheMapObjectProps == NULL)
	{
		return 0;
	}
	TheMapObjectProps->getAllSelectedDicts();
	MapObject *pObj = TheMapObjectProps->m_selectedObject;
	if (pObj == NULL)
	{
		return 0;
	}
	const Coord3D *loc = pObj->getLocation();
	TheMapObjectProps->m_position = *loc;
	_snprintf(out, cap - 1, "%0.2f, %0.2f", loc->x, loc->y);
	out[cap - 1] = 0;
	return 1;
}

double MapObjectProps::qtMGetZOffset(void)
{
	if (TheMapObjectProps == NULL)
	{
		return 0.0;
	}
	TheMapObjectProps->getAllSelectedDicts();
	MapObject *pObj = TheMapObjectProps->m_selectedObject;
	if (pObj == NULL)
	{
		return 0.0;
	}
	TheMapObjectProps->m_height = pObj->getLocation()->z;
	return TheMapObjectProps->m_height;
}

double MapObjectProps::qtMGetAngle(void)
{
	if (TheMapObjectProps == NULL)
	{
		return 0.0;
	}
	TheMapObjectProps->getAllSelectedDicts();
	MapObject *pObj = TheMapObjectProps->m_selectedObject;
	if (pObj == NULL)
	{
		return 0.0;
	}
	TheMapObjectProps->m_angle = pObj->getAngle() * 180 / PI;
	return TheMapObjectProps->m_angle;
}

// == _NameToDict minus the edit-control read.
void MapObjectProps::qtMSetName(const char *name)
{
	if (TheMapObjectProps == NULL)
	{
		return;
	}
	TheMapObjectProps->getAllSelectedDicts();
	if (TheMapObjectProps->m_allSelectedDicts.size() != 1)
	{
		return;
	}
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if ( pDoc )
	{
		Dict newDict;
		newDict.setAsciiString(TheKey_objectName, AsciiString(name ? name : ""));
		DictItemUndoable *pUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, newDict.getNthKey(0), TheMapObjectProps->m_allSelectedDicts.size());
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
	}
}

// == _TeamToDict minus the combo read. The UI->internal neutral-name mapping isn't needed:
// the value path takes the sides list's team name verbatim (already the internal string).
void MapObjectProps::qtMSetTeam(int i)
{
	if (TheMapObjectProps == NULL || TheSidesList == NULL || i < 0 || i >= TheSidesList->getNumTeams())
	{
		return;
	}
	TheMapObjectProps->getAllSelectedDicts();
	AsciiString name = TheSidesList->getTeamInfo(i)->getDict()->getAsciiString(TheKey_teamName);
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if ( pDoc )
	{
		Dict newDict;
		newDict.setAsciiString(TheKey_originalOwner, name);
		DictItemUndoable *pUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, newDict.getNthKey(0), TheMapObjectProps->m_allSelectedDicts.size(), pDoc, true);
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
	}
}

// == the seven _XToDict flag handlers minus the checkbox reads. Selectable keeps its
// tri-state: state 2 removes the key (per-unit default).
void MapObjectProps::qtMSetFlag(int which, int state)
{
	if (TheMapObjectProps == NULL)
	{
		return;
	}
	TheMapObjectProps->getAllSelectedDicts();
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if ( pDoc == NULL )
	{
		return;
	}
	Dict newDict;
	switch (which)
	{
		case WBQT_OBJPROP_FLAG_ENABLED:        newDict.setBool(TheKey_objectEnabled, state != 0);        break;
		case WBQT_OBJPROP_FLAG_INDESTRUCTIBLE: newDict.setBool(TheKey_objectIndestructible, state != 0); break;
		case WBQT_OBJPROP_FLAG_UNSELLABLE:     newDict.setBool(TheKey_objectUnsellable, state != 0);     break;
		case WBQT_OBJPROP_FLAG_TARGETABLE:     newDict.setBool(TheKey_objectTargetable, state != 0);     break;
		case WBQT_OBJPROP_FLAG_POWERED:        newDict.setBool(TheKey_objectPowered, state != 0);        break;
		case WBQT_OBJPROP_FLAG_RECRUITABLEAI:  newDict.setBool(TheKey_objectRecruitableAI, state != 0);  break;
		case WBQT_OBJPROP_FLAG_SELECTABLE:
			if (state == 2)
			{
				newDict.remove(TheKey_objectSelectable);
			}
			else
			{
				newDict.setBool(TheKey_objectSelectable, state == 1);
			}
			break;
		default:
			return;
	}
	DictItemUndoable *pUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, newDict.getNthKey(0), TheMapObjectProps->m_allSelectedDicts.size());
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
}

// == _AggressivenessToDict minus the combo-text mapping (the value IS -2..2).
void MapObjectProps::qtMSetAggressiveness(int value)
{
	if (TheMapObjectProps == NULL)
	{
		return;
	}
	TheMapObjectProps->getAllSelectedDicts();
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if ( pDoc != NULL )
	{
		Dict newDict;
		newDict.setInt(TheKey_objectAggressiveness, value);
		DictItemUndoable *pUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, newDict.getNthKey(0), TheMapObjectProps->m_allSelectedDicts.size());
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
	}
}

// == _VeterancyToDict minus the combo read (index == stored value).
void MapObjectProps::qtMSetVeterancy(int index)
{
	if (TheMapObjectProps == NULL)
	{
		return;
	}
	TheMapObjectProps->getAllSelectedDicts();
	int value = (index >= 0) ? index : 0;
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if ( pDoc != NULL )
	{
		Dict newDict;
		newDict.setInt(TheKey_objectVeterancy, value);
		DictItemUndoable *pUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, newDict.getNthKey(0), TheMapObjectProps->m_allSelectedDicts.size(), pDoc, true);
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
	}
}

// == _HealthToDict minus the combo/edit reads; the Qt panel only calls this with a real
// value (selecting "Other" commits nothing until the user types -- see c963b2d30).
void MapObjectProps::qtMSetHealthPercent(int value)
{
	if (TheMapObjectProps == NULL)
	{
		return;
	}
	TheMapObjectProps->getAllSelectedDicts();
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if ( pDoc != NULL )
	{
		Dict newDict;
		newDict.setInt(TheKey_objectInitialHealth, value);
		DictItemUndoable *pUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, newDict.getNthKey(0), TheMapObjectProps->m_allSelectedDicts.size(), pDoc, true);
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
	}
}

// == _HPsToDict minus the combo read (empty/0 -> -1 == Default For Unit).
void MapObjectProps::qtMSetMaxHPs(int hps)
{
	if (TheMapObjectProps == NULL)
	{
		return;
	}
	TheMapObjectProps->getAllSelectedDicts();
	Int value = hps;
	if (value <= 0)
	{
		value = -1;
	}
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if ( pDoc != NULL )
	{
		Dict newDict;
		newDict.setInt(TheKey_objectMaxHPs, value);
		DictItemUndoable *pUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, newDict.getNthKey(0), TheMapObjectProps->m_allSelectedDicts.size());
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
	}
}

// == _VisibilityToDict minus the edit read: -1/empty means "remove the key" -- the empty
// newDict + explicit key ctor is the remove path, kept verbatim.
void MapObjectProps::qtMSetVisionDistance(int dist)
{
	if (TheMapObjectProps == NULL)
	{
		return;
	}
	TheMapObjectProps->getAllSelectedDicts();
	int value = (dist <= 0) ? -1 : dist;
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if ( pDoc != NULL )
	{
		Dict newDict;
		if (value != -1)
		{
			newDict.setInt(TheKey_objectVisualRange, value);
		}
		DictItemUndoable *pUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, TheKey_objectVisualRange, TheMapObjectProps->m_allSelectedDicts.size());
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
	}
}

// == _ShroudClearingDistanceToDict minus the edit read (same remove-when-default shape).
void MapObjectProps::qtMSetShroudClearingDistance(int dist)
{
	if (TheMapObjectProps == NULL)
	{
		return;
	}
	TheMapObjectProps->getAllSelectedDicts();
	int value = (dist <= 0) ? -1 : dist;
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if ( pDoc != NULL )
	{
		Dict newDict;
		if (value != -1)
		{
			newDict.setInt(TheKey_objectShroudClearingDistance, value);
		}
		DictItemUndoable *pUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, TheKey_objectShroudClearingDistance, TheMapObjectProps->m_allSelectedDicts.size());
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
	}
}

// == _StoppingDistanceToDict minus the edit read (the empty-text no-commit case can't
// happen on the value path).
void MapObjectProps::qtMSetStoppingDistance(double dist)
{
	if (TheMapObjectProps == NULL)
	{
		return;
	}
	TheMapObjectProps->getAllSelectedDicts();
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if ( pDoc != NULL )
	{
		Dict newDict;
		newDict.setReal(TheKey_objectStoppingDistance, (Real)dist);
		DictItemUndoable *pUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, newDict.getNthKey(0), TheMapObjectProps->m_allSelectedDicts.size(), pDoc, true);
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
	}
}

// == _WeatherToDict / _TimeToDict minus the combo reads.
void MapObjectProps::qtMSetWeather(int index)
{
	if (TheMapObjectProps == NULL || index < 0)
	{
		return;
	}
	TheMapObjectProps->getAllSelectedDicts();
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if ( pDoc )
	{
		Dict newDict;
		newDict.setInt(TheKey_objectWeather, index);
		DictItemUndoable *pUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, newDict.getNthKey(0), TheMapObjectProps->m_allSelectedDicts.size(), pDoc, true);
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
	}
}

void MapObjectProps::qtMSetTime(int index)
{
	if (TheMapObjectProps == NULL || index < 0)
	{
		return;
	}
	TheMapObjectProps->getAllSelectedDicts();
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if ( pDoc )
	{
		Dict newDict;
		newDict.setInt(TheKey_objectTime, index);
		DictItemUndoable *pUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, newDict.getNthKey(0), TheMapObjectProps->m_allSelectedDicts.size(), pDoc, true);
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
	}
}

// == SetPosition minus the edit read; the previous position comes from the live object
// (the m_position member is only refreshed by ShowPosition, a control seeder).
void MapObjectProps::qtMSetPosition(const char *text)
{
	if (TheMapObjectProps == NULL || text == NULL)
	{
		return;
	}
	TheMapObjectProps->getAllSelectedDicts();
	MapObject *pObj = TheMapObjectProps->m_selectedObject;
	if (pObj == NULL)
	{
		return;
	}
	Coord3D cur = *pObj->getLocation();
	Coord3D loc = cur;
	if (text[0] != 0)
	{
		if (sscanf(text, "%f, %f", &loc.x, &loc.y) != 2)
		{
			loc = cur;
		}
	}
	if (loc.x != cur.x || loc.y != cur.y)
	{
		TheMapObjectProps->m_position = loc;
		CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
		if ( pDoc )
		{
			ModifyObjectUndoable *pUndo = new ModifyObjectUndoable(pDoc);
			pDoc->AddAndDoUndoable(pUndo);
			pUndo->SetOffset(loc.x - cur.x, loc.y - cur.y);
			REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
		}
	}
}

// == SetZOffset minus the edit read (no change detection, like the original).
void MapObjectProps::qtMSetZOffset(double z)
{
	if (TheMapObjectProps == NULL)
	{
		return;
	}
	TheMapObjectProps->getAllSelectedDicts();
	TheMapObjectProps->m_height = (Real)z;
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if ( pDoc )
	{
		ModifyObjectUndoable *pUndo = new ModifyObjectUndoable(pDoc);
		pDoc->AddAndDoUndoable(pUndo);
		pUndo->SetZOffset((Real)z);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
	}
}

// == SetAngle minus the edit read; previous angle from the live object.
void MapObjectProps::qtMSetAngle(double deg)
{
	if (TheMapObjectProps == NULL)
	{
		return;
	}
	TheMapObjectProps->getAllSelectedDicts();
	MapObject *pObj = TheMapObjectProps->m_selectedObject;
	if (pObj == NULL)
	{
		return;
	}
	Real prev = pObj->getAngle() * 180 / PI;
	if (prev != (Real)deg)
	{
		TheMapObjectProps->m_angle = (Real)deg;
		CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
		if ( pDoc )
		{
			ModifyObjectUndoable *pUndo = new ModifyObjectUndoable(pDoc);
			pDoc->AddAndDoUndoable(pUndo);
			pUndo->RotateTo((Real)deg * PI / 180);
			REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
		}
	}
}

extern "C" int WBQtObjectProps_HasSelection(void)
{
	return MapObjectProps::qtHasSelection();
}

extern "C" int WBQtObjectProps_GetSelCount(void)
{
	return MapObjectProps::qtGetSelCount();
}

extern "C" int WBQtObjectProps_GetName(char *out, int cap)
{
	return MapObjectProps::qtGetName(out, cap);
}

extern "C" void WBQtObjectProps_SetName(const char *name)
{
	MapObjectProps::qtMSetName(name);	// windowless model core (qt-debridge)
}

extern "C" int WBQtObjectProps_GetTeamCount(void)
{
	return MapObjectProps::qtGetTeamCount();
}

extern "C" int WBQtObjectProps_GetTeamName(int i, char *out, int cap)
{
	return MapObjectProps::qtGetTeamName(i, out, cap);
}

extern "C" int WBQtObjectProps_GetCurTeam(void)
{
	return MapObjectProps::qtGetCurTeam();
}

extern "C" void WBQtObjectProps_SetTeam(int i)
{
	MapObjectProps::qtMSetTeam(i);
}

// --- Phase 2: Logical section ---------------------------------------------------------------

extern "C" int WBQtObjectProps_GetFlag(int which)
{
	return MapObjectProps::qtMGetFlag(which);
}

extern "C" void WBQtObjectProps_SetFlag(int which, int state)
{
	MapObjectProps::qtMSetFlag(which, state);
}

extern "C" int WBQtObjectProps_GetAggressiveness(void)
{
	return MapObjectProps::qtGetAggressiveness();
}

extern "C" void WBQtObjectProps_SetAggressiveness(int value)
{
	MapObjectProps::qtMSetAggressiveness(value);
}

extern "C" int WBQtObjectProps_GetVeterancy(void)
{
	return MapObjectProps::qtGetVeterancy();
}

extern "C" void WBQtObjectProps_SetVeterancy(int index)
{
	MapObjectProps::qtMSetVeterancy(index);
}

extern "C" int WBQtObjectProps_GetHealthPercent(void)
{
	return MapObjectProps::qtGetHealthPercent();
}

extern "C" void WBQtObjectProps_SetHealthPercent(int value)
{
	MapObjectProps::qtMSetHealthPercent(value);
}

extern "C" int WBQtObjectProps_GetMaxHPs(void)
{
	return MapObjectProps::qtGetMaxHPs();
}

extern "C" void WBQtObjectProps_SetMaxHPs(int hps)
{
	MapObjectProps::qtMSetMaxHPs(hps);
}

extern "C" int WBQtObjectProps_GetVisionDistance(void)
{
	return MapObjectProps::qtGetVisionDistance();
}

extern "C" void WBQtObjectProps_SetVisionDistance(int dist)
{
	MapObjectProps::qtMSetVisionDistance(dist);
}

extern "C" int WBQtObjectProps_GetShroudClearingDistance(void)
{
	return MapObjectProps::qtGetShroudClearingDistance();
}

extern "C" void WBQtObjectProps_SetShroudClearingDistance(int dist)
{
	MapObjectProps::qtMSetShroudClearingDistance(dist);
}

extern "C" double WBQtObjectProps_GetStoppingDistance(void)
{
	return MapObjectProps::qtGetStoppingDistance();
}

extern "C" void WBQtObjectProps_SetStoppingDistance(double dist)
{
	MapObjectProps::qtMSetStoppingDistance(dist);
}

// --- Phase 3a: Visual section ---------------------------------------------------------------

extern "C" int WBQtObjectProps_GetWeather(void)
{
	return MapObjectProps::qtGetWeather();
}

extern "C" void WBQtObjectProps_SetWeather(int index)
{
	MapObjectProps::qtMSetWeather(index);
}

extern "C" int WBQtObjectProps_GetTime(void)
{
	return MapObjectProps::qtGetTime();
}

extern "C" void WBQtObjectProps_SetTime(int index)
{
	MapObjectProps::qtMSetTime(index);
}

extern "C" int WBQtObjectProps_GetPosition(char *out, int cap)
{
	return MapObjectProps::qtMGetPosition(out, cap);
}

extern "C" void WBQtObjectProps_SetPosition(const char *text)
{
	MapObjectProps::qtMSetPosition(text);
}

extern "C" double WBQtObjectProps_GetZOffset(void)
{
	return MapObjectProps::qtMGetZOffset();
}

extern "C" void WBQtObjectProps_SetZOffset(double z)
{
	MapObjectProps::qtMSetZOffset(z);
}

extern "C" double WBQtObjectProps_GetAngle(void)
{
	return MapObjectProps::qtMGetAngle();
}

extern "C" void WBQtObjectProps_SetAngle(double deg)
{
	MapObjectProps::qtMSetAngle(deg);
}

// --- Phase 3b: Sound section ----------------------------------------------------------------

extern "C" int WBQtObjectProps_GetSoundCount(void)
{
	return MapObjectProps::qtGetSoundCount();
}

extern "C" int WBQtObjectProps_GetSoundItem(int i, char *out, int cap)
{
	return MapObjectProps::qtGetSoundItem(i, out, cap);
}

extern "C" int WBQtObjectProps_GetSoundCurSel(void)
{
	return MapObjectProps::qtGetSoundCurSel();
}

extern "C" void WBQtObjectProps_SetSoundCurSel(int i)
{
	MapObjectProps::qtSetSoundCurSel(i);
}

extern "C" int WBQtObjectProps_GetSoundPlaying(void)
{
	return MapObjectProps::qtGetSoundPlaying();
}

extern "C" void WBQtObjectProps_ToggleSoundPreview(void)
{
	MapObjectProps::qtToggleSoundPreview();
}

extern "C" int WBQtObjectProps_GetSoundFlag(int which)
{
	return MapObjectProps::qtGetSoundFlag(which);
}

extern "C" int WBQtObjectProps_GetSoundFlagEnabled(int which)
{
	return MapObjectProps::qtGetSoundFlagEnabled(which);
}

extern "C" void WBQtObjectProps_SetSoundFlag(int which, int on)
{
	MapObjectProps::qtSetSoundFlag(which, on);
}

extern "C" int WBQtObjectProps_GetSoundInt(int which, int *outEnabled)
{
	return MapObjectProps::qtGetSoundInt(which, outEnabled);
}

extern "C" void WBQtObjectProps_SetSoundInt(int which, int value)
{
	MapObjectProps::qtSetSoundInt(which, value);
}

extern "C" int WBQtObjectProps_GetSoundPriorityCount(void)
{
	return MapObjectProps::qtGetSoundPriorityCount();
}

extern "C" int WBQtObjectProps_GetSoundPriorityName(int i, char *out, int cap)
{
	return MapObjectProps::qtGetSoundPriorityName(i, out, cap);
}

extern "C" int WBQtObjectProps_GetSoundPriority(int *outEnabled)
{
	return MapObjectProps::qtGetSoundPriority(outEnabled);
}

extern "C" void WBQtObjectProps_SetSoundPriority(int i)
{
	MapObjectProps::qtSetSoundPriority(i);
}

// --- Phase 3c: Pre-built upgrades ----------------------------------------------------------

extern "C" int WBQtObjectProps_GetUpgradeCount(void)
{
	return MapObjectProps::qtGetUpgradeCount();
}

extern "C" int WBQtObjectProps_GetUpgradeItem(int i, char *out, int cap)
{
	return MapObjectProps::qtGetUpgradeItem(i, out, cap);
}

extern "C" int WBQtObjectProps_GetUpgradeSelected(int i)
{
	return MapObjectProps::qtGetUpgradeSelected(i);
}

extern "C" void WBQtObjectProps_SetUpgradeSelected(int i, int on)
{
	MapObjectProps::qtSetUpgradeSelected(i, on);
}

extern "C" void WBQtObjectProps_CommitUpgrades(void)
{
	MapObjectProps::qtCommitUpgrades();
}

#endif // RTS_HAS_QT
