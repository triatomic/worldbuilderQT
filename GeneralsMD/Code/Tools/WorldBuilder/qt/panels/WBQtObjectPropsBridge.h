// WBQtObjectPropsBridge.h -- self-contained opaque facade for the Qt Object Properties panel.
//
// Like the other per-panel bridge headers, this carries ONLY int/double/char* (no Qt or MFC
// types) so the MFC mapobjectprops.cpp TU can drive the Qt panel and the Qt panel can drive the
// MFC MapObjectProps dialog, without either side including the other's headers.
//
// MapObjectProps is a SELECTED-OBJECT panel: the hidden MFC dialog (TheMapObjectProps) stays the
// owner of the working Dict(s) -- m_dictToEdit for the current object, m_allSelectedDicts for a
// multi-select. The Qt panel reads state through the Get* funcs and writes through the Set* funcs,
// which set the hidden MFC control's state then call the real MFC _XToDict handler so the
// DictItemUndoable / multi-select path is reused unchanged.
//
// Phase 1: selection state + General section (object name, owning team).
#ifndef WB_QT_OBJECTPROPS_BRIDGE_H
#define WB_QT_OBJECTPROPS_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// --- Reverse: Qt widget -> MFC dialog (implemented MFC-side, MapObjectProps::qt*) ----------
// Selection. HasSelection is non-zero when a single non-waypoint/light object is selected;
// GetSelCount counts all selected editable objects (a multi-select edits every one).
int  WBQtObjectProps_HasSelection(void);
int  WBQtObjectProps_GetSelCount(void);

// Object name (General section). GetName copies the current object's name into out (cap bytes),
// returns non-zero if a Dict is being edited. SetName writes the hidden MFC edit box then runs
// _NameToDict (single-select only, like the MFC panel).
int  WBQtObjectProps_GetName(char *out, int cap);
void WBQtObjectProps_SetName(const char *name);

// Owning-team combo. GetTeamCount/GetTeamName give the team list (with the "(neutral)" relabel);
// GetCurTeam returns the index of the current object's team (-1 if none/unknown); SetTeam writes
// the hidden MFC combo then runs _TeamToDict (applies to every selected object).
int  WBQtObjectProps_GetTeamCount(void);
int  WBQtObjectProps_GetTeamName(int i, char *out, int cap);
int  WBQtObjectProps_GetCurTeam(void);
void WBQtObjectProps_SetTeam(int i);

// --- Phase 2: Logical section (flags, aggressiveness, veterancy, health, HPs, distances) ----
// Flag ids for Get/SetFlag. Selectable is tri-state (0 off / 1 on / 2 default -- the MFC panel's
// third state removes the key). Keep in sync with the enum in mapobjectprops.cpp.
enum {
	WBQT_OBJPROP_FLAG_ENABLED = 0,
	WBQT_OBJPROP_FLAG_INDESTRUCTIBLE,
	WBQT_OBJPROP_FLAG_UNSELLABLE,
	WBQT_OBJPROP_FLAG_TARGETABLE,
	WBQT_OBJPROP_FLAG_POWERED,
	WBQT_OBJPROP_FLAG_RECRUITABLEAI,
	WBQT_OBJPROP_FLAG_SELECTABLE
};
int  WBQtObjectProps_GetFlag(int which);
void WBQtObjectProps_SetFlag(int which, int state);

// Aggressiveness: value is -2 Sleep, -1 Passive, 0 Normal, 1 Alert, 2 Aggressive.
int  WBQtObjectProps_GetAggressiveness(void);
void WBQtObjectProps_SetAggressiveness(int value);

// Veterancy: combo index 0..3 (Regular / Veteran / Elite / Heroic).
int  WBQtObjectProps_GetVeterancy(void);
void WBQtObjectProps_SetVeterancy(int index);

// Starting health as a percent int (0/25/50/75/100, or any other value via the Other edit box).
int  WBQtObjectProps_GetHealthPercent(void);
void WBQtObjectProps_SetHealthPercent(int value);

// Max hit points: -1 == "Default For Unit", otherwise the explicit value.
int  WBQtObjectProps_GetMaxHPs(void);
void WBQtObjectProps_SetMaxHPs(int hps);

// Distances. Vision + shroud are ints (0 == unset/blank); stopping distance is a real.
int    WBQtObjectProps_GetVisionDistance(void);
void   WBQtObjectProps_SetVisionDistance(int dist);
int    WBQtObjectProps_GetShroudClearingDistance(void);
void   WBQtObjectProps_SetShroudClearingDistance(int dist);
double WBQtObjectProps_GetStoppingDistance(void);
void   WBQtObjectProps_SetStoppingDistance(double dist);

// --- Phase 3a: Visual section (weather, time, XY position, Z offset, angle) ------------------
// Weather / Time are index combos (index 0 == the "Use Map ..." default). XY position is the
// "x, y" text the MFC edit uses; Z offset and angle are reals (angle in degrees). XY/Z/Angle apply
// to the single selected object (ModifyObjectUndoable), like the MFC edits.
int    WBQtObjectProps_GetWeather(void);
void   WBQtObjectProps_SetWeather(int index);
int    WBQtObjectProps_GetTime(void);
void   WBQtObjectProps_SetTime(int index);
int    WBQtObjectProps_GetPosition(char *out, int cap);
void   WBQtObjectProps_SetPosition(const char *text);
double WBQtObjectProps_GetZOffset(void);
void   WBQtObjectProps_SetZOffset(double z);
double WBQtObjectProps_GetAngle(void);
void   WBQtObjectProps_SetAngle(double deg);

// --- Phase 3b: Sound section ----------------------------------------------------------------
// The MFC dictTo* handlers gate every sound control's enable state, so the Qt panel reads the
// live control value + its enabled flag and mirrors it. Sound-flag ids and int-edit ids below.
enum {
	WBQT_SND_CUSTOMIZE = 0,
	WBQT_SND_ENABLED,
	WBQT_SND_LOOPING
};
enum {
	WBQT_SNDINT_LOOPCOUNT = 0,
	WBQT_SNDINT_VOLUME,
	WBQT_SNDINT_MINVOLUME,
	WBQT_SNDINT_MINRANGE,
	WBQT_SNDINT_MAXRANGE
};

// Attached-sound combo (holds thousands of events; enumerate once, then track selection).
int  WBQtObjectProps_GetSoundCount(void);
int  WBQtObjectProps_GetSoundItem(int i, char *out, int cap);
int  WBQtObjectProps_GetSoundCurSel(void);
void WBQtObjectProps_SetSoundCurSel(int i);

// Listen button: toggles preview; GetSoundPlaying reports whether it's currently playing (so the
// Qt button can show Listen / Stop).
int  WBQtObjectProps_GetSoundPlaying(void);
void WBQtObjectProps_ToggleSoundPreview(void);

// Customize / Enabled / Looping checkboxes (WBQT_SND_* ids). GetFlagEnabled reports the gated
// enable state so the Qt checkbox greys out exactly like the MFC one.
int  WBQtObjectProps_GetSoundFlag(int which);
int  WBQtObjectProps_GetSoundFlagEnabled(int which);
void WBQtObjectProps_SetSoundFlag(int which, int on);

// Loop count / volume / min volume / min range / max range edits (WBQT_SNDINT_* ids). The int
// getter also reports the control's enabled state via *outEnabled.
int  WBQtObjectProps_GetSoundInt(int which, int *outEnabled);
void WBQtObjectProps_SetSoundInt(int which, int value);

// Priority combo.
int  WBQtObjectProps_GetSoundPriorityCount(void);
int  WBQtObjectProps_GetSoundPriorityName(int i, char *out, int cap);
int  WBQtObjectProps_GetSoundPriority(int *outEnabled);
void WBQtObjectProps_SetSoundPriority(int i);

// --- Phase 3c: Pre-built upgrades listbox (multi-select, single-object) ----------------------
// The Qt list mirrors the MFC listbox; SetUpgradeSelected writes the MFC item's selection then
// runs _PrebuiltUpgradesToDict (single-object; a multi-select shows "Single Selection Only").
int  WBQtObjectProps_GetUpgradeCount(void);
int  WBQtObjectProps_GetUpgradeItem(int i, char *out, int cap);
int  WBQtObjectProps_GetUpgradeSelected(int i);
void WBQtObjectProps_SetUpgradeSelected(int i, int on);	// set item sel only
void WBQtObjectProps_CommitUpgrades(void);	// apply the selection set as one undoable

// --- Forward: MFC dialog -> Qt widget (implemented Qt-side, WBQtObjectPropsBridge.cpp) ------
// MapObjectProps::updateTheUI() calls this after re-seeding its controls from the new selection;
// it re-reads the panel from the Get* funcs above (no-op when the Qt panel isn't open).
void WBQtObjectProps_PushRefresh(void);

// --- Forward: open/close the Qt panel (implemented Qt-side) ---------------------------------
// The generic options-panel host in WBQtOptionsPanels.cpp creates/shows the panel; these are the
// registry hooks it uses. (Declared here so the registry TU sees them without the shared header.)

#ifdef __cplusplus
}
#endif

#endif // WB_QT_OBJECTPROPS_BRIDGE_H
