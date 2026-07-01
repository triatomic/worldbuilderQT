// WBQtObjectPropsBridge.cpp -- MFC side of the Qt Object Properties facade.
//
// These extern "C" reverse callbacks (declared in qt/panels/WBQtObjectPropsBridge.h) forward to
// the MapObjectProps::qt* statics. The whole body is behind RTS_HAS_QT; with Qt OFF this TU is
// empty, exactly like the other WBQt*Bridge.cpp files, so the MFC-only build is unchanged.

#include "StdAfx.h"

#ifdef RTS_HAS_QT

#include "mapobjectprops.h"
#include "qt/panels/WBQtObjectPropsBridge.h"

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
	MapObjectProps::qtSetName(name);
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
	MapObjectProps::qtSetTeam(i);
}

// --- Phase 2: Logical section ---------------------------------------------------------------

extern "C" int WBQtObjectProps_GetFlag(int which)
{
	return MapObjectProps::qtGetFlag(which);
}

extern "C" void WBQtObjectProps_SetFlag(int which, int state)
{
	MapObjectProps::qtSetFlag(which, state);
}

extern "C" int WBQtObjectProps_GetAggressiveness(void)
{
	return MapObjectProps::qtGetAggressiveness();
}

extern "C" void WBQtObjectProps_SetAggressiveness(int value)
{
	MapObjectProps::qtSetAggressiveness(value);
}

extern "C" int WBQtObjectProps_GetVeterancy(void)
{
	return MapObjectProps::qtGetVeterancy();
}

extern "C" void WBQtObjectProps_SetVeterancy(int index)
{
	MapObjectProps::qtSetVeterancy(index);
}

extern "C" int WBQtObjectProps_GetHealthPercent(void)
{
	return MapObjectProps::qtGetHealthPercent();
}

extern "C" void WBQtObjectProps_SetHealthPercent(int value)
{
	MapObjectProps::qtSetHealthPercent(value);
}

extern "C" int WBQtObjectProps_GetMaxHPs(void)
{
	return MapObjectProps::qtGetMaxHPs();
}

extern "C" void WBQtObjectProps_SetMaxHPs(int hps)
{
	MapObjectProps::qtSetMaxHPs(hps);
}

extern "C" int WBQtObjectProps_GetVisionDistance(void)
{
	return MapObjectProps::qtGetVisionDistance();
}

extern "C" void WBQtObjectProps_SetVisionDistance(int dist)
{
	MapObjectProps::qtSetVisionDistance(dist);
}

extern "C" int WBQtObjectProps_GetShroudClearingDistance(void)
{
	return MapObjectProps::qtGetShroudClearingDistance();
}

extern "C" void WBQtObjectProps_SetShroudClearingDistance(int dist)
{
	MapObjectProps::qtSetShroudClearingDistance(dist);
}

extern "C" double WBQtObjectProps_GetStoppingDistance(void)
{
	return MapObjectProps::qtGetStoppingDistance();
}

extern "C" void WBQtObjectProps_SetStoppingDistance(double dist)
{
	MapObjectProps::qtSetStoppingDistance(dist);
}

// --- Phase 3a: Visual section ---------------------------------------------------------------

extern "C" int WBQtObjectProps_GetWeather(void)
{
	return MapObjectProps::qtGetWeather();
}

extern "C" void WBQtObjectProps_SetWeather(int index)
{
	MapObjectProps::qtSetWeather(index);
}

extern "C" int WBQtObjectProps_GetTime(void)
{
	return MapObjectProps::qtGetTime();
}

extern "C" void WBQtObjectProps_SetTime(int index)
{
	MapObjectProps::qtSetTime(index);
}

extern "C" int WBQtObjectProps_GetPosition(char *out, int cap)
{
	return MapObjectProps::qtGetPosition(out, cap);
}

extern "C" void WBQtObjectProps_SetPosition(const char *text)
{
	MapObjectProps::qtSetPosition(text);
}

extern "C" double WBQtObjectProps_GetZOffset(void)
{
	return MapObjectProps::qtGetZOffset();
}

extern "C" void WBQtObjectProps_SetZOffset(double z)
{
	MapObjectProps::qtSetZOffset(z);
}

extern "C" double WBQtObjectProps_GetAngle(void)
{
	return MapObjectProps::qtGetAngle();
}

extern "C" void WBQtObjectProps_SetAngle(double deg)
{
	MapObjectProps::qtSetAngle(deg);
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
