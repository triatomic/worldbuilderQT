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

#if !defined(AFX_MAPOBJECTPROPS_H__44517B9E_12AB_4E2C_B49B_D6BB65C59649__INCLUDED_)
#define AFX_MAPOBJECTPROPS_H__44517B9E_12AB_4E2C_B49B_D6BB65C59649__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// mapobjectprops.h : header file
//

#include "OptionsPanel.h"
#include "Common/Dict.h"
#include "WBPopupSlider.h"

class MapObject;
class ModifyObjectUndoable;
class MultipleUndoable;
class CWorldBuilderDoc;
class MapObject;

/////////////////////////////////////////////////////////////////////////////
// External Defines
extern const char* NEUTRAL_TEAM_UI_STR;
extern const char* NEUTRAL_TEAM_INTERNAL_STR;


/////////////////////////////////////////////////////////////////////////////
// MapObjectProps dialog

class MapObjectProps : public COptionsPanel, public PopupSliderOwner
{
// Construction
public:
	MapObjectProps(Dict* dictToEdit = NULL, const char* title = NULL, CWnd* pParent = NULL);   // standard constructor
	~MapObjectProps();
	void makeMain();

// Dialog Data
	//{{AFX_DATA(MapObjectProps)
	enum { IDD = IDD_MAPOBJECT_PROPS };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(MapObjectProps)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	void getAllSelectedDicts(void);

	static MapObjectProps *TheMapObjectProps; 

	// Data common to all property pages
	Dict* m_dictToEdit;
	std::vector<Dict*> m_allSelectedDicts;
	const char* m_title;
	MapObject *m_selectedObject;
	MapObject *m_dictSource; // Source object for m_dictToEdit. m_selectedObject is not always the current source
	                         // of m_dictToEdit, and I don't understand why, so I'm making another MapObject pointer
	                         // which is always kept in sync.

	Real m_angle;
	Real m_height;
	Real m_scale;

	WBPopupSliderButton m_heightSlider;
	WBPopupSliderButton m_angleSlider;
	WBPopupSliderButton m_scaleSlider;

	Int              m_defaultEntryIndex; //< Index in the sound combobox of the entry labelled "default"
	Bool             m_defaultIsNone; //< The default for this object is no sound
	AsciiString      m_defaultEntryName; //< The original name of the default entry

	Bool             m_soundPreviewPlaying; //< The Play button is currently in its "Stop" state
	std::vector<unsigned char> m_soundPreviewData; //< In-memory WAV the preview plays from (must outlive playback)
	Int              m_soundComboTextWidth; //< Widest sound-combo entry, measured once in InitSound (px)
	void stopSoundPreview(void);

	ModifyObjectUndoable *m_posUndoable;
	Coord3D m_position;

	void deletePages();
	void updateTheUI(void);
	void enableButtons();
	int getSel();


	// Generated message map functions
	//{{AFX_MSG(MapObjectProps)
	virtual BOOL OnInitDialog();
	afx_msg void OnMove(int x, int y);
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnSelchangeProperties();
	afx_msg void OnEditprop();
	afx_msg void OnNewprop();
	afx_msg void OnRemoveprop();
	afx_msg void OnDblclkProperties();

	afx_msg void _TeamToDict(void);
	afx_msg void _NameToDict(void);
	// afx_msg void _ScriptToDict(void);
	afx_msg void _WeatherToDict(void);
	afx_msg void _TimeToDict(void);
	afx_msg void _ScaleToDict(void);
	afx_msg void SetZOffset(void);
	afx_msg void SetAngle(void);
	afx_msg void SetPosition(void);
	afx_msg void OnScaleOn();
	afx_msg void OnScaleOff();
	afx_msg void OnKillfocusMAPOBJECTXYPosition();
	afx_msg void _PrebuiltUpgradesToDict(void);
	afx_msg void _HealthToDict(void);
	afx_msg void _EnabledToDict(void);
	afx_msg void _IndestructibleToDict(void);
	afx_msg void _UnsellableToDict(void);
	afx_msg void _TargetableToDict();
	afx_msg void _PoweredToDict(void);
	afx_msg void _AggressivenessToDict(void);
	afx_msg void _VisibilityToDict(void);
	afx_msg void _VeterancyToDict(void);
	afx_msg void _ShroudClearingDistanceToDict(void);
	afx_msg void _RecruitableAIToDict(void);
	afx_msg void _SelectableToDict(void);
	afx_msg void _HPsToDict();
	afx_msg void _StoppingDistanceToDict(void);
	afx_msg void attachedSoundToDict(void);
	afx_msg void customizeToDict(void);
	afx_msg void enabledToDict(void);
	afx_msg void loopingToDict(void);
	afx_msg void loopCountToDict(void);
	afx_msg void minVolumeToDict(void);
	afx_msg void volumeToDict(void);
	afx_msg void minRangeToDict(void);
	afx_msg void maxRangeToDict(void);
	afx_msg void priorityToDict(void);
	afx_msg void OnPlaySound(void);
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnDestroy();
		//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

	void _DictToName(void);
	void _DictToTeam(void);
	// void _DictToScript(void);
	void _DictToScale(void);
	void _DictToWeather(void);
	void _DictToTime(void);
	void _DictToPrebuiltUpgrades(void);
	void _DictToHealth(void);
	void _DictToHPs(void);
	void _DictToEnabled(void);
	void _DictToDestructible(void);
	void _DictToUnsellable(void);
	void _DictToTargetable(void);

	void _DictToPowered(void);
	void _DictToAggressiveness(void);
	void _DictToVisibilityRange(void);
	void _DictToVeterancy(void);
	void _DictToShroudClearingDistance(void);
	void _DictToRecruitableAI();
	void _DictToSelectable(void);
	void _DictToStoppingDistance(void);
	void ShowZOffset(MapObject* pMapObj);
	void ShowAngle(MapObject* pMapObj);
	void ShowPosition(MapObject* pMapObj);
	void dictToAttachedSound(void);
	void dictToCustomize(void);
	void dictToEnabled(void);
	void dictToLooping(void);
	void dictToLoopCount(void);
	void dictToMinVolume(void);
	void dictToVolume(void);
	void dictToMinRange(void);
	void dictToMaxRange(void);
	void dictToPriority(void);

	void clearCustomizeFlag( CWorldBuilderDoc* pDoc, MultipleUndoable * ownerUndoable );

	// Implementation of PopupSliderOwner callbacks
	virtual void GetPopSliderInfo(const long sliderID, long *pMin, long *pMax, long *pLineSize, long *pInitial);
	virtual void PopSliderChanged(const long sliderID, long theVal);
	virtual void PopSliderFinished(const long sliderID, long theVal);

public:
	static MapObject *getSingleSelectedObject(void);
	static void update(void);

#ifdef RTS_HAS_QT
	// Qt front-end support (WBQtObjectPropsBridge). The MFC panel stays created + hidden
	// (TheMapObjectProps intact) so update()/getSingleSelectedObject keep working; the Qt
	// panel reads the selected object's props + drives the same _XToDict handlers. Defined
	// in mapobjectprops.cpp. Phase 1: selection + General (name/team).
	static int  qtHasSelection(void);
	static int  qtGetSelCount(void);
	static int  qtGetName(char *out, int cap);
	static void qtSetName(const char *name);
	static int  qtGetTeamCount(void);
	static int  qtGetTeamName(int i, char *out, int cap);
	static int  qtGetCurTeam(void);
	static void qtSetTeam(int i);
	// Phase 2: Logical section. Flags use the WBQT_OBJPROP_FLAG_* ids; the getters return
	// the current state and the setters write the hidden MFC control then run the real
	// _XToDict handler (so the DictItemUndoable / multi-select path is reused).
	static int  qtGetFlag(int which);
	static void qtSetFlag(int which, int state);
	static int  qtGetAggressiveness(void);
	static void qtSetAggressiveness(int value);
	static int  qtGetVeterancy(void);
	static void qtSetVeterancy(int index);
	static int  qtGetHealthPercent(void);
	static void qtSetHealthPercent(int value);
	static int  qtGetMaxHPs(void);
	static void qtSetMaxHPs(int hps);
	static int  qtGetVisionDistance(void);
	static void qtSetVisionDistance(int dist);
	static int  qtGetShroudClearingDistance(void);
	static void qtSetShroudClearingDistance(int dist);
	static double qtGetStoppingDistance(void);
	static void qtSetStoppingDistance(double dist);
	// Phase 3a: Visual section. Weather/Time are index combos; XY/Z/Angle drive the same
	// ModifyObjectUndoable path as the MFC edits (single-object).
	static int  qtGetWeather(void);
	static void qtSetWeather(int index);
	static int  qtGetTime(void);
	static void qtSetTime(int index);
	static int  qtGetPosition(char *out, int cap);
	static void qtSetPosition(const char *text);
	static double qtGetZOffset(void);
	static void qtSetZOffset(double z);
	static double qtGetAngle(void);
	static void qtSetAngle(double deg);
	// Phase 3b: Sound section. The MFC dictTo* handlers already encode the enable-state gating
	// (customize off disables the rest; looping off disables loop count; sound == none disables
	// customize/enabled), so the Qt getters read the LIVE MFC control (value + IsWindowEnabled)
	// after updateTheUI has run, and the setters write the control then call the *ToDict handler.
	static int  qtGetSoundCount(void);
	static int  qtGetSoundItem(int i, char *out, int cap);
	static int  qtGetSoundCurSel(void);
	static void qtSetSoundCurSel(int i);
	static int  qtGetSoundPlaying(void);
	static void qtToggleSoundPreview(void);
	// which = WBQT_SND_* id; Get returns the checkbox state, GetEnabled its enable state.
	static int  qtGetSoundFlag(int which);
	static int  qtGetSoundFlagEnabled(int which);
	static void qtSetSoundFlag(int which, int on);
	// Numeric sound edits (loop count int; volumes/ranges are the 0-100 / raw ints the edits show).
	static int  qtGetSoundInt(int which, int *outEnabled);
	static void qtSetSoundInt(int which, int value);
	static int  qtGetSoundPriorityCount(void);
	static int  qtGetSoundPriorityName(int i, char *out, int cap);
	static int  qtGetSoundPriority(int *outEnabled);
	static void qtSetSoundPriority(int i);
	// Phase 3c: Pre-built upgrades listbox (multi-select, single-object). The Qt list mirrors
	// the MFC listbox; the setter writes the MFC item selection then runs _PrebuiltUpgradesToDict.
	static int  qtGetUpgradeCount(void);
	static int  qtGetUpgradeItem(int i, char *out, int cap);
	static int  qtGetUpgradeSelected(int i);
	static void qtSetUpgradeSelected(int i, int on);
	static void qtCommitUpgrades(void);
#endif
  
private:
  /// Disallow copying: Object is not set up to be copied
  MapObjectProps( const MapObjectProps & other ); // Deliberately undefined
  MapObjectProps & operator=( const MapObjectProps & other ); // Deliberately undefined
	void updateTheUI(MapObject *pMapObj);
	void InitSound(void);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAPOBJECTPROPS_H__44517B9E_12AB_4E2C_B49B_D6BB65C59649__INCLUDED_)
