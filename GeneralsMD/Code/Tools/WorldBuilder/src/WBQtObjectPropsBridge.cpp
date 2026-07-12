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
#include "Common/ThingTemplate.h"
#include "Common/GameAudio.h"
#include "Common/AudioEventInfo.h"
#include "Common/AudioEventRTS.h"
#include "Common/file.h"
#include "Common/FileSystem.h"
#include "GameLogic/Module/GenerateMinefieldBehavior.h"
#include <mmsystem.h>
#include <vector>
#include <algorithm>
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
	// The engine stores angles normalized to -180..180; the panel (and the original
	// WB angle slider) shows a 0..360 compass heading, so lift negatives by a turn --
	// otherwise the 0..360 spin box clamps a negative reading to 0.
	Real deg = pObj->getAngle() * 180 / PI;
	if (deg < 0.0f) {
		deg += 360.0f;
	}
	TheMapObjectProps->m_angle = deg;
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
	// Compare in the same 0..360 space the panel edits (the engine reading is
	// -180..180), so unchanged headings over 180 degrees don't re-trigger an edit.
	Real prev = pObj->getAngle() * 180 / PI;
	if (prev < 0.0f) {
		prev += 360.0f;
	}
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


//----------------------------------------------------------------------------------------
// Sound sub-panel, de-bridged. The hidden combo/checkbox/edit controls used to hold BOTH
// the catalog (InitSound) and the derived values + enable gating (the dictTo* seeders);
// windowless, the catalog lives here and every value/gate derives from the dict + the
// AudioEventInfo defaults, mirroring the dictTo* logic exactly. Copies of the two
// file-static strings from mapobjectprops.cpp (they are file-local there).
//----------------------------------------------------------------------------------------
static const Char QTM_NO_SOUND_STRING[] = "(None)";
static const Char QTM_BASE_DEFAULT_STRING[] = "Default";

static std::vector<AsciiString> s_qtmSoundNames;	// [0] default entry, [1] (None), [2..] sorted events
static Bool s_qtmSoundNamesBuilt = false;

namespace
{
	bool qtmSoundLess(const AsciiString &a, const AsciiString &b)
	{
		return _stricmp(a.str(), b.str()) < 0;	// == the SORTED combo's case-insensitive order
	}
}

// == InitSound's catalog: all AT_SoundEffect events, combo-sorted, behind the two fixed rows.
void MapObjectProps::qtMSoundEnsureCatalog(void)
{
	if (s_qtmSoundNamesBuilt)
	{
		return;
	}
	std::vector<AsciiString> names;
	const AudioEventInfoHash & audioEventHash = TheAudio->getAllAudioEvents();
	AudioEventInfoHash::const_iterator it;
	for ( it = audioEventHash.begin(); it != audioEventHash.end(); it++ )
	{
		if ( it->second->m_soundType == AT_SoundEffect )
		{
			names.push_back( it->second->m_audioName );
		}
	}
	std::sort(names.begin(), names.end(), qtmSoundLess);
	s_qtmSoundNames.clear();
	s_qtmSoundNames.push_back(AsciiString(QTM_BASE_DEFAULT_STRING));
	s_qtmSoundNames.push_back(AsciiString(QTM_NO_SOUND_STRING));
	size_t n;
	for (n = 0; n < names.size(); n++)
	{
		s_qtmSoundNames.push_back(names[n]);
	}
	s_qtmSoundNamesBuilt = true;
}

// == dictToAttachedSound's default-entry refresh: "Default <X>" / "Default <None>" from the
// selected object's template ambient sound; keeps m_defaultEntryName/m_defaultIsNone current.
void MapObjectProps::qtMSoundRefreshDefault(void)
{
	if (TheMapObjectProps == NULL)
	{
		return;
	}
	qtMSoundEnsureCatalog();
	TheMapObjectProps->m_defaultIsNone = true;
	TheMapObjectProps->m_defaultEntryName = QTM_NO_SOUND_STRING;
	TheMapObjectProps->m_defaultEntryIndex = 0;

	const ThingTemplate * thingTemplate = NULL;
	if (TheMapObjectProps->m_dictSource)
	{
		thingTemplate = TheMapObjectProps->m_dictSource->getThingTemplate();
	}
	AsciiString string( QTM_BASE_DEFAULT_STRING );
	if ( thingTemplate )
	{
		const AudioEventRTS * defaultAudioEvent;
		if ( thingTemplate->hasSoundAmbient() )
		{
			defaultAudioEvent = thingTemplate->getSoundAmbient();
		}
		else
		{
			defaultAudioEvent = NULL;
		}
		if ( defaultAudioEvent == NULL || defaultAudioEvent == TheAudio->getValidSilentAudioEvent() )
		{
			string.concat( " <None>" );
		}
		else
		{
			string.concat( " <" );
			string.concat( defaultAudioEvent->getEventName() );
			string.concat( '>' );
			TheMapObjectProps->m_defaultEntryName = defaultAudioEvent->getEventName();
			TheMapObjectProps->m_defaultIsNone = false;
		}
	}
	s_qtmSoundNames[0] = string;
}

// == dictToAttachedSound's selection: key absent -> the default row; "" -> (None); else the
// event's row (fallback to default when not found, like the FindStringExact miss path).
int MapObjectProps::qtMSoundCurIndex(void)
{
	qtMSoundRefreshDefault();
	Dict *d = (TheMapObjectProps != NULL) ? TheMapObjectProps->m_dictToEdit : NULL;
	AsciiString sound;
	Bool exists = false;
	if (d)
	{
		sound = d->getAsciiString(TheKey_objectSoundAmbient, &exists);
	}
	if ( !exists )
	{
		return 0;
	}
	if ( sound.isEmpty() )
	{
		return 1;
	}
	size_t i;
	for (i = 2; i < s_qtmSoundNames.size(); i++)
	{
		if (_stricmp(s_qtmSoundNames[i].str(), sound.str()) == 0)
		{
			return (int)i;
		}
	}
	return 0;
}

// The effective current sound for defaults/gating: name + the "is none" flag.
void MapObjectProps::qtMCurSoundName(AsciiString &nameOut, Bool &isNoneOut)
{
	int idx = qtMSoundCurIndex();
	if (idx == 1 || (idx == 0 && TheMapObjectProps->m_defaultIsNone))
	{
		nameOut = AsciiString::TheEmptyString;
		isNoneOut = true;
		return;
	}
	isNoneOut = false;
	if (idx == 0)
	{
		nameOut = TheMapObjectProps->m_defaultEntryName;
	}
	else
	{
		nameOut = s_qtmSoundNames[idx];
	}
}

int MapObjectProps::qtMGetSoundCount(void)
{
	if (TheMapObjectProps == NULL)
	{
		return 0;
	}
	qtMSoundRefreshDefault();
	return (int)s_qtmSoundNames.size();
}

int MapObjectProps::qtMGetSoundItem(int i, char *out, int cap)
{
	if (out == NULL || cap <= 0 || TheMapObjectProps == NULL)
	{
		return 0;
	}
	qtMSoundRefreshDefault();
	if (i < 0 || i >= (int)s_qtmSoundNames.size())
	{
		return 0;
	}
	strncpy(out, s_qtmSoundNames[i].str(), cap - 1);
	out[cap - 1] = 0;
	return 1;
}

int MapObjectProps::qtMGetSoundCurSel(void)
{
	if (TheMapObjectProps == NULL)
	{
		return -1;
	}
	return qtMSoundCurIndex();
}

// == stopSoundPreview minus the button text / window timer (no window; the one-shot
// auto-reset timer doesn't exist windowless -- the next toggle stops a finished one-shot).
void MapObjectProps::qtMStopSoundPreview(void)
{
	if (TheMapObjectProps == NULL || !TheMapObjectProps->m_soundPreviewPlaying)
	{
		return;
	}
	::PlaySound( NULL, NULL, 0 );
	TheMapObjectProps->m_soundPreviewPlaying = false;
	TheMapObjectProps->m_soundPreviewData.clear();
}

// == attachedSoundToDict keyed by the catalog index (the hidden combo held the selection).
void MapObjectProps::qtMSetSoundCurSel(int i)
{
	if (TheMapObjectProps == NULL)
	{
		return;
	}
	qtMSoundRefreshDefault();
	if (i < 0 || i >= (int)s_qtmSoundNames.size())
	{
		return;
	}
	qtMStopSoundPreview();
	TheMapObjectProps->getAllSelectedDicts();
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if ( pDoc != NULL )
	{
		MultipleUndoable *pUndo = new MultipleUndoable;
		Dict newDict;
		if ( i != 0 )
		{
			if ( _stricmp(s_qtmSoundNames[i].str(), QTM_NO_SOUND_STRING) == 0 )
			{
				newDict.setAsciiString(TheKey_objectSoundAmbient, "");
				// Can't customize the null sound
				TheMapObjectProps->clearCustomizeFlag( pDoc, pUndo );
			}
			else
			{
				newDict.setAsciiString(TheKey_objectSoundAmbient, s_qtmSoundNames[i]);
			}
		}
		else if ( TheMapObjectProps->m_defaultIsNone )
		{
			// Can't customize the null sound
			TheMapObjectProps->clearCustomizeFlag( pDoc, pUndo );
		}
		DictItemUndoable *pDictUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, TheKey_objectSoundAmbient, TheMapObjectProps->m_allSelectedDicts.size(), pDoc, true);
		pUndo->addUndoable( pDictUndo );
		pDoc->AddAndDoUndoable( pUndo );
		REF_PTR_RELEASE( pDictUndo ); // belongs to pUndo
		REF_PTR_RELEASE( pUndo ); // belongs to pDoc now.
	}
}

// == OnPlaySound minus the button text and the one-shot auto-reset timer (window ops).
void MapObjectProps::qtMToggleSoundPreview(void)
{
	if (TheMapObjectProps == NULL)
	{
		return;
	}
	if (TheMapObjectProps->m_soundPreviewPlaying)
	{
		qtMStopSoundPreview();
		return;
	}
	AsciiString eventName;
	Bool isNone = false;
	qtMCurSoundName(eventName, isNone);
	if ( isNone || eventName.isEmpty() )
	{
		return; // nothing to play
	}
	AudioEventInfo * audioEventInfo = TheAudio->findAudioEventInfo( eventName );
	if ( audioEventInfo == NULL )
	{
		return;
	}
	AudioEventRTS event;
	event.setEventName( eventName );
	event.setAudioEventInfo( audioEventInfo );
	event.generateFilename();
	if ( event.getFilename().isEmpty() )
	{
		return;
	}
	File *file = TheFileSystem->openFile( event.getFilename().str(), File::READ | File::BINARY );
	if ( file == NULL )
	{
		return;
	}
	Int size = file->size();
	if ( size <= 0 )
	{
		file->close();
		return;
	}
	TheMapObjectProps->m_soundPreviewData.resize( size );
	Int bytesRead = file->read( &TheMapObjectProps->m_soundPreviewData[0], size );
	file->close();
	if ( bytesRead != size )
	{
		TheMapObjectProps->m_soundPreviewData.clear();
		return;
	}
	Bool looping = ( audioEventInfo->m_control & AC_LOOP ) != 0;
	DWORD flags = SND_MEMORY | SND_ASYNC | SND_NODEFAULT;
	if ( looping )
	{
		flags |= SND_LOOP;
	}
	if ( !::PlaySound( (LPCSTR)&TheMapObjectProps->m_soundPreviewData[0], NULL, flags ) )
	{
		TheMapObjectProps->m_soundPreviewData.clear();
		return;
	}
	TheMapObjectProps->m_soundPreviewPlaying = true;
}

// The derived values the dictTo* seeders used to leave in the hidden controls.
// looping: (customize on && key set) ? key : the event's AC_LOOP default (0 when none).
namespace
{
	Bool qtmCustomizeValue(Dict *d, Bool isNone)
	{
		if (isNone)
		{
			return false;
		}
		Bool exists = false;
		Bool customized = false;
		if (d)
		{
			customized = d->getBool(TheKey_objectSoundAmbientCustomized, &exists);
		}
		return exists && customized;
	}
}

int MapObjectProps::qtMGetSoundFlag(int which)
{
	if (TheMapObjectProps == NULL)
	{
		return 0;
	}
	Dict *d = TheMapObjectProps->m_dictToEdit;
	AsciiString sound;
	Bool isNone = false;
	qtMCurSoundName(sound, isNone);
	Bool customize = qtmCustomizeValue(d, isNone);
	Bool exists = false;

	if (which == WBQT_SND_CUSTOMIZE)
	{
		return customize ? 1 : 0;
	}
	if (which == WBQT_SND_LOOPING)
	{
		// == dictToLooping: the dict value only applies while customize is on; otherwise the
		// event default (unchecked for none).
		if (customize && d)
		{
			Bool looping = d->getBool(TheKey_objectSoundAmbientLooping, &exists);
			if (exists)
			{
				return looping ? 1 : 0;
			}
		}
		if (isNone)
		{
			return 0;
		}
		AudioEventInfo * info = TheAudio->findAudioEventInfo(sound);
		return ( info && ( info->m_control & AC_LOOP ) ) ? 1 : 0;
	}
	if (which == WBQT_SND_ENABLED)
	{
		// == dictToEnabled: none -> unchecked; dict value wins; default = loops forever
		// (looping on with loop count 0).
		if (isNone)
		{
			return 0;
		}
		if (d)
		{
			Bool enabled = d->getBool(TheKey_objectSoundAmbientEnabled, &exists);
			if (exists)
			{
				return enabled ? 1 : 0;
			}
		}
		if (qtMGetSoundFlag(WBQT_SND_LOOPING) == 1)
		{
			int enabledOut = 0;
			if (qtMGetSoundInt(WBQT_SNDINT_LOOPCOUNT, &enabledOut) == 0)
			{
				return 1;
			}
		}
		return 0;
	}
	return 0;
}

int MapObjectProps::qtMGetSoundFlagEnabled(int which)
{
	if (TheMapObjectProps == NULL)
	{
		return 0;
	}
	AsciiString sound;
	Bool isNone = false;
	qtMCurSoundName(sound, isNone);
	switch (which)
	{
		case WBQT_SND_CUSTOMIZE: return isNone ? 0 : 1;	// == dictToCustomize
		case WBQT_SND_ENABLED:   return isNone ? 0 : 1;	// == dictToEnabled
		case WBQT_SND_LOOPING:   return qtmCustomizeValue(TheMapObjectProps->m_dictToEdit, isNone) ? 1 : 0;	// == dictToLooping
		default: return 0;
	}
}

// == customizeToDict / enabledToDict / loopingToDict minus the checkbox reads.
void MapObjectProps::qtMSetSoundFlag(int which, int checked)
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
	if (which == WBQT_SND_CUSTOMIZE)
	{
		Dict newDict;
		if ( checked != 0 )
		{
			newDict.setBool( TheKey_objectSoundAmbientCustomized, true );
			DictItemUndoable *pUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, TheKey_objectSoundAmbientCustomized, TheMapObjectProps->m_allSelectedDicts.size(), pDoc, true);
			pDoc->AddAndDoUndoable(pUndo);
			REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
		}
		else
		{
			MultipleUndoable *pUndo = new MultipleUndoable;
			TheMapObjectProps->clearCustomizeFlag( pDoc, pUndo );
			pDoc->AddAndDoUndoable( pUndo );
			REF_PTR_RELEASE( pUndo ); // belongs to pDoc now.
		}
		return;
	}
	Dict newDict;
	if (which == WBQT_SND_ENABLED)
	{
		newDict.setBool( TheKey_objectSoundAmbientEnabled, checked != 0 );
	}
	else if (which == WBQT_SND_LOOPING)
	{
		newDict.setBool( TheKey_objectSoundAmbientLooping, checked != 0 );
	}
	else
	{
		return;
	}
	DictItemUndoable *pUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, newDict.getNthKey(0), TheMapObjectProps->m_allSelectedDicts.size(), pDoc, true);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
}

// == the dictToLoopCount/MinVolume/Volume/MinRange/MaxRange value+gate derivations.
int MapObjectProps::qtMGetSoundInt(int which, int *outEnabled)
{
	if (outEnabled != NULL)
	{
		*outEnabled = 0;
	}
	if (TheMapObjectProps == NULL)
	{
		return 0;
	}
	Dict *d = TheMapObjectProps->m_dictToEdit;
	AsciiString sound;
	Bool isNone = false;
	qtMCurSoundName(sound, isNone);
	Bool customize = qtmCustomizeValue(d, isNone);
	Bool exists = false;
	AudioEventInfo * info = (!isNone) ? TheAudio->findAudioEventInfo(sound) : NULL;

	switch (which)
	{
		case WBQT_SNDINT_LOOPCOUNT:
		{
			Bool gate = customize && (qtMGetSoundFlag(WBQT_SND_LOOPING) == 1);
			if (outEnabled != NULL)
			{
				*outEnabled = gate ? 1 : 0;
			}
			if (gate && d)
			{
				Int loopCount = d->getInt(TheKey_objectSoundAmbientLoopCount, &exists);
				if (exists)
				{
					return loopCount;
				}
			}
			if (isNone || info == NULL)
			{
				return 0;
			}
			return info->m_loopCount;
		}
		case WBQT_SNDINT_VOLUME:
		{
			if (outEnabled != NULL)
			{
				*outEnabled = customize ? 1 : 0;
			}
			if (customize && d)
			{
				Real volume = d->getReal(TheKey_objectSoundAmbientVolume, &exists);
				if (exists)
				{
					return REAL_TO_INT( ( volume * 100.0f ) + 0.5 );
				}
			}
			if (isNone || info == NULL)
			{
				return 100;
			}
			return REAL_TO_INT( ( info->m_volume * 100.0f ) + 0.5 );
		}
		case WBQT_SNDINT_MINVOLUME:
		{
			if (outEnabled != NULL)
			{
				*outEnabled = customize ? 1 : 0;
			}
			if (customize && d)
			{
				Real minVolume = d->getReal(TheKey_objectSoundAmbientMinVolume, &exists);
				if (exists)
				{
					return REAL_TO_INT( ( minVolume * 100.0f ) + 0.5 );
				}
			}
			if (isNone || info == NULL)
			{
				return 40;
			}
			return REAL_TO_INT( ( info->m_minVolume * 100.0f ) + 0.5 );
		}
		case WBQT_SNDINT_MINRANGE:
		{
			if (outEnabled != NULL)
			{
				*outEnabled = customize ? 1 : 0;
			}
			if (customize && d)
			{
				Real minRange = d->getReal(TheKey_objectSoundAmbientMinRange, &exists);
				if (exists)
				{
					return REAL_TO_INT( minRange );
				}
			}
			if (isNone || info == NULL)
			{
				return 175;
			}
			return REAL_TO_INT( info->m_minDistance );
		}
		case WBQT_SNDINT_MAXRANGE:
		{
			if (outEnabled != NULL)
			{
				*outEnabled = customize ? 1 : 0;
			}
			if (customize && d)
			{
				Real maxRange = d->getReal(TheKey_objectSoundAmbientMaxRange, &exists);
				if (exists)
				{
					return REAL_TO_INT( maxRange );
				}
			}
			if (isNone || info == NULL)
			{
				return 600;
			}
			return REAL_TO_INT( info->m_maxDistance );
		}
		default:
			return 0;
	}
}

// == loopCountToDict / volumeToDict / minVolumeToDict / minRangeToDict / maxRangeToDict
// minus the edit reads (volumes arrive as 0-100, stored 0.0-1.0 like the handlers).
void MapObjectProps::qtMSetSoundInt(int which, int value)
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
		case WBQT_SNDINT_LOOPCOUNT: newDict.setInt( TheKey_objectSoundAmbientLoopCount, value ); break;
		case WBQT_SNDINT_VOLUME:    newDict.setReal( TheKey_objectSoundAmbientVolume, INT_TO_REAL( value ) / 100.0f ); break;
		case WBQT_SNDINT_MINVOLUME: newDict.setReal( TheKey_objectSoundAmbientMinVolume, INT_TO_REAL( value ) / 100.0f ); break;
		case WBQT_SNDINT_MINRANGE:  newDict.setReal( TheKey_objectSoundAmbientMinRange, INT_TO_REAL( value ) ); break;
		case WBQT_SNDINT_MAXRANGE:  newDict.setReal( TheKey_objectSoundAmbientMaxRange, INT_TO_REAL( value ) ); break;
		default: return;
	}
	DictItemUndoable *pUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, newDict.getNthKey(0), TheMapObjectProps->m_allSelectedDicts.size(), pDoc, true);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
}

// == dictToPriority's value+gate.
int MapObjectProps::qtMGetSoundPriority(int *outEnabled)
{
	if (outEnabled != NULL)
	{
		*outEnabled = 0;
	}
	if (TheMapObjectProps == NULL)
	{
		return AP_LOWEST;
	}
	Dict *d = TheMapObjectProps->m_dictToEdit;
	AsciiString sound;
	Bool isNone = false;
	qtMCurSoundName(sound, isNone);
	Bool customize = qtmCustomizeValue(d, isNone);
	if (outEnabled != NULL)
	{
		*outEnabled = customize ? 1 : 0;
	}
	Bool exists = false;
	if (customize && d)
	{
		Int priorityEnum = d->getInt(TheKey_objectSoundAmbientPriority, &exists);
		if (exists && priorityEnum >= 0 && priorityEnum <= AP_CRITICAL)
		{
			return priorityEnum;
		}
	}
	if (isNone)
	{
		return AP_LOWEST;
	}
	AudioEventInfo * info = TheAudio->findAudioEventInfo(sound);
	if (info == NULL)
	{
		return AP_LOWEST;
	}
	return info->m_priority;
}

// == priorityToDict minus the combo read.
void MapObjectProps::qtMSetSoundPriority(int i)
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
		newDict.setInt( TheKey_objectSoundAmbientPriority, i );
		DictItemUndoable *pUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, newDict.getNthKey(0), TheMapObjectProps->m_allSelectedDicts.size(), pDoc, true);
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
	}
}

//----------------------------------------------------------------------------------------
// Pre-built upgrades, de-bridged. The hidden listbox used to hold both the per-template
// catalog (_DictToPrebuiltUpgrades) and the selection state; windowless they live here,
// rebuilt on every count query (== every Qt refresh).
//----------------------------------------------------------------------------------------
static std::vector<AsciiString> s_qtmUpgradeNames;
static std::vector<char> s_qtmUpgradeSel;
static Bool s_qtmUpgradeSingle = false;	// false when the list is the multi-select placeholder

// == _DictToPrebuiltUpgrades minus the listbox: template upgrade catalog + dict selection.
int MapObjectProps::qtMGetUpgradeCount(void)
{
	s_qtmUpgradeNames.clear();
	s_qtmUpgradeSel.clear();
	s_qtmUpgradeSingle = false;
	if (TheMapObjectProps == NULL)
	{
		return 0;
	}
	TheMapObjectProps->getAllSelectedDicts();
	if (TheMapObjectProps->m_allSelectedDicts.size() > 1)
	{
		CString cstr;
		cstr.LoadString(IDS_SINGLE_SELECTION_ONLY);
		s_qtmUpgradeNames.push_back(AsciiString((const char *)cstr));
		s_qtmUpgradeSel.push_back(0);
		return 1;
	}
	if (TheMapObjectProps->m_selectedObject == NULL)
	{
		return 0;
	}
	const ThingTemplate *tt = TheMapObjectProps->m_selectedObject->getThingTemplate();
	if (tt == NULL)
	{
		return 0;
	}
	s_qtmUpgradeSingle = true;

	const ModuleInfo& mi = tt->getBehaviorModuleInfo();
	Int numBehaviorModules = mi.getCount();
	for (int i = 0; i < numBehaviorModules; ++i)
	{
		if (mi.getNthName(i).compareNoCase("GenerateMinefieldBehavior") == 0)
		{
			const GenerateMinefieldBehaviorModuleData *gmbmd = (const GenerateMinefieldBehaviorModuleData *)mi.getNthData(i);
			if (!gmbmd)
			{
				continue;
			}
			if (gmbmd->m_upgradeMuxData.m_activationUpgradeNames.size() > 0)
			{
				AsciiString name = gmbmd->m_upgradeMuxData.m_activationUpgradeNames[0];
				Bool found = false;
				size_t k;
				for (k = 0; k < s_qtmUpgradeNames.size(); k++)
				{
					if (_stricmp(s_qtmUpgradeNames[k].str(), name.str()) == 0)
					{
						found = true;
						break;
					}
				}
				if (!found)
				{
					s_qtmUpgradeNames.push_back(name);
					s_qtmUpgradeSel.push_back(0);
				}
			}
		}
	}

	// Select the entries the dict already grants (TheKey_objectGrantUpgradeN chain).
	Dict *d = TheMapObjectProps->m_dictToEdit;
	if (d)
	{
		Bool exists;
		int upgradeNum = 0;
		AsciiString upgradeString;
		do {
			AsciiString keyName;
			keyName.format("%s%d", TheNameKeyGenerator->keyToName(TheKey_objectGrantUpgrade).str(), upgradeNum);
			upgradeString = d->getAsciiString(NAMEKEY(keyName), &exists);
			if (exists)
			{
				size_t k;
				for (k = 0; k < s_qtmUpgradeNames.size(); k++)
				{
					if (_stricmp(s_qtmUpgradeNames[k].str(), upgradeString.str()) == 0)
					{
						s_qtmUpgradeSel[k] = 1;
						break;
					}
				}
			}
			else
			{
				upgradeString.clear();
			}
			++upgradeNum;
		} while (!upgradeString.isEmpty());
	}
	return (int)s_qtmUpgradeNames.size();
}

int MapObjectProps::qtMGetUpgradeItem(int i, char *out, int cap)
{
	if (out == NULL || cap <= 0 || i < 0 || i >= (int)s_qtmUpgradeNames.size())
	{
		return 0;
	}
	strncpy(out, s_qtmUpgradeNames[i].str(), cap - 1);
	out[cap - 1] = 0;
	return 1;
}

int MapObjectProps::qtMGetUpgradeSelected(int i)
{
	if (i < 0 || i >= (int)s_qtmUpgradeSel.size())
	{
		return 0;
	}
	return s_qtmUpgradeSel[i] ? 1 : 0;
}

void MapObjectProps::qtMSetUpgradeSelected(int i, int on)
{
	if (i < 0 || i >= (int)s_qtmUpgradeSel.size())
	{
		return;
	}
	s_qtmUpgradeSel[i] = on ? 1 : 0;
}

// == _PrebuiltUpgradesToDict minus the listbox walk (full-dict replacement undoable).
void MapObjectProps::qtMCommitUpgrades(void)
{
	if (TheMapObjectProps == NULL || !s_qtmUpgradeSingle)
	{
		return;
	}
	TheMapObjectProps->getAllSelectedDicts();
	if (TheMapObjectProps->m_allSelectedDicts.size() != 1)
	{
		return;
	}
	if (TheMapObjectProps->m_selectedObject)
	{
		if ( !TheMapObjectProps->m_selectedObject->getThingTemplate() )
		{
			return;
		}
	}

	Bool exists;
	int upgradeNum = 0;
	AsciiString upgradeString;

	// We're going to sub this entire dict for the existing entire dict.
	Dict newDict = *TheMapObjectProps->m_allSelectedDicts[0];

	// First, clear out any existing notions of what we should upgrade.
	do {
		AsciiString keyName;
		keyName.format("%s%d", TheNameKeyGenerator->keyToName(TheKey_objectGrantUpgrade).str(), upgradeNum);
		upgradeString = newDict.getAsciiString(NAMEKEY(keyName), &exists);
		if (exists)
		{
			newDict.remove(NAMEKEY(keyName));
		}
		++upgradeNum;
	} while (!upgradeString.isEmpty());

	upgradeNum = 0;
	size_t i;
	for (i = 0; i < s_qtmUpgradeNames.size(); ++i)
	{
		if (s_qtmUpgradeSel[i])
		{
			AsciiString keyName;
			keyName.format("%s%d", TheNameKeyGenerator->keyToName(TheKey_objectGrantUpgrade).str(), upgradeNum);
			newDict.setAsciiString(NAMEKEY(keyName), s_qtmUpgradeNames[i]);
			++upgradeNum;
		}
	}

	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if ( pDoc != NULL )
	{
		DictItemUndoable *pUndo = new DictItemUndoable(&TheMapObjectProps->m_allSelectedDicts.front(), newDict, NAMEKEY_INVALID, TheMapObjectProps->m_allSelectedDicts.size(), pDoc, true);
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
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
	return MapObjectProps::qtMGetSoundCount();
}

extern "C" int WBQtObjectProps_GetSoundItem(int i, char *out, int cap)
{
	return MapObjectProps::qtMGetSoundItem(i, out, cap);
}

extern "C" int WBQtObjectProps_GetSoundCurSel(void)
{
	return MapObjectProps::qtMGetSoundCurSel();
}

extern "C" void WBQtObjectProps_SetSoundCurSel(int i)
{
	MapObjectProps::qtMSetSoundCurSel(i);
}

extern "C" int WBQtObjectProps_GetSoundPlaying(void)
{
	return MapObjectProps::qtGetSoundPlaying();
}

extern "C" void WBQtObjectProps_ToggleSoundPreview(void)
{
	MapObjectProps::qtMToggleSoundPreview();
}

extern "C" int WBQtObjectProps_GetSoundFlag(int which)
{
	return MapObjectProps::qtMGetSoundFlag(which);
}

extern "C" int WBQtObjectProps_GetSoundFlagEnabled(int which)
{
	return MapObjectProps::qtMGetSoundFlagEnabled(which);
}

extern "C" void WBQtObjectProps_SetSoundFlag(int which, int on)
{
	MapObjectProps::qtMSetSoundFlag(which, on);
}

extern "C" int WBQtObjectProps_GetSoundInt(int which, int *outEnabled)
{
	return MapObjectProps::qtMGetSoundInt(which, outEnabled);
}

extern "C" void WBQtObjectProps_SetSoundInt(int which, int value)
{
	MapObjectProps::qtMSetSoundInt(which, value);
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
	return MapObjectProps::qtMGetSoundPriority(outEnabled);
}

extern "C" void WBQtObjectProps_SetSoundPriority(int i)
{
	MapObjectProps::qtMSetSoundPriority(i);
}

// --- Phase 3c: Pre-built upgrades ----------------------------------------------------------

extern "C" int WBQtObjectProps_GetUpgradeCount(void)
{
	return MapObjectProps::qtMGetUpgradeCount();
}

extern "C" int WBQtObjectProps_GetUpgradeItem(int i, char *out, int cap)
{
	return MapObjectProps::qtMGetUpgradeItem(i, out, cap);
}

extern "C" int WBQtObjectProps_GetUpgradeSelected(int i)
{
	return MapObjectProps::qtMGetUpgradeSelected(i);
}

extern "C" void WBQtObjectProps_SetUpgradeSelected(int i, int on)
{
	MapObjectProps::qtMSetUpgradeSelected(i, on);
}

extern "C" void WBQtObjectProps_CommitUpgrades(void)
{
	MapObjectProps::qtMCommitUpgrades();
}

#endif // RTS_HAS_QT
