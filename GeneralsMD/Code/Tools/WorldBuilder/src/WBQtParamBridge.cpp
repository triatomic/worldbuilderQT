// WBQtParamBridge.cpp -- MFC side of the Qt parameter-editor seam (Tier 2c). Plain MFC TU
// (no Qt include). Defines EditParameter's qt* member statics here (member statics may be
// defined in any TU) so the class's protected load* combo fillers are reused VERBATIM: the
// options are loaded into a hidden CBS_SORT CComboBox -- AddString sorts, InsertString appends,
// exactly like the real dialog's combos -- and marshalled out by index, so both the contents
// and the index-based write-back semantics (BOOLEAN/COMPARISON/KIND_OF/...) match the MFC
// dialog bit for bit. Also serves the coordinate / color / object-picker / group editors.
// Whole body guarded by RTS_HAS_QT so the OFF build compiles it to an empty object.

// These must precede every include: the name arrays are defined by headers that other
// headers below pull in transitively (include guards would swallow a later attempt).
#define DEFINE_BUILDABLE_STATUS_NAMES
#define DEFINE_OBJECT_STATUS_NAMES
#define DEFINE_SCIENCE_AVAILABILITY_NAMES
#define DEFINE_EDITOR_SORTING_NAMES

#include "StdAfx.h"
#include "resource.h"
#include "WorldBuilder.h"
#include "WorldBuilderDoc.h"

// This is used to allow sounds to be played via PlaySound
#include <mmsystem.h>

#include "EditParameter.h"

#include "Common/AudioEventInfo.h"
#include "Common/BorderColors.h"
#include "Common/GameAudio.h"
#include "Common/PlayerTemplate.h"
#include "Common/Radar.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/ThingSort.h"

#include "GameLogic/AI.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/SidesList.h"

#include "GameClient/View.h"

#include "W3DDevice/GameClient/HeightMap.h"

#include "qt/panels/WBQtParamBridge.h"

#ifdef RTS_HAS_QT

static void copyOut(const AsciiString &str, char *buf, int cap)
{
	if (buf == NULL || cap <= 0)
	{
		return;
	}
	strncpy(buf, str.str(), cap - 1);
	buf[cap - 1] = 0;
}

// ================= EditParameter::qtDescribe =================
// Port of EditParameter::OnInitDialog's type switch: fills pCombo with the options (the same
// loaders, the same InsertString/AddString calls), reports the caption, which control kind the
// dialog shows (edit / editable combo / selection list), whether the audio preview row shows,
// the initial selection, and the initial edit text.

Int EditParameter::qtDescribe(Parameter *pParm, AsciiString unitName, CComboBox *pCombo,
	AsciiString *captionOut, Int *kindOut, Int *showAudioOut, Int *initialSelOut, AsciiString *initialTextOut)
{
	m_unitName = unitName;
	pCombo->ResetContent();

	CString captionText;
	Bool showCombo = false;
	Bool showAudioButton = false;
	CString editText;
	Bool showList = false;
	Int i;
	Parameter *m_parameter = pParm;	// keep the switch below textually close to OnInitDialog
	CComboBox *pList = pCombo;		// combo and list kinds share the hidden combo

	switch (m_parameter->getParameterType()) {
		default:
			DEBUG_CRASH(("Unknown parameter type."));
			break;
		case Parameter::SCRIPT:
			captionText = "Script named:";
			showCombo = true;
			loadScripts(pCombo, false);
			break;
		case Parameter::SCRIPT_SUBROUTINE:
			captionText = "Subroutine named:";
			showCombo = true;
			loadScripts(pCombo, true);
			break;
		case Parameter::ATTACK_PRIORITY_SET:
			captionText = "Attack priority set named:";
			showCombo = true;
			loadAttackPrioritySets(pCombo);
			break;
		case Parameter::WAYPOINT:
			captionText = "Waypoint named:";
			showCombo = true;
			loadWaypoints(pCombo);
			break;
		case Parameter::LOCALIZED_TEXT:
			captionText = "Localized string:";
			showCombo = true;
			loadLocalizedText(pCombo);
			break;
		case Parameter::WAYPOINT_PATH:
			captionText = "Waypoint labeled:";
			showCombo = true;
			loadWaypointPaths(pCombo);
			break;
		case Parameter::TRIGGER_AREA:
			captionText = "Trigger area named:";
			showCombo = true;
			loadTriggerAreas(pCombo);
			break;
		case Parameter::COMMAND_BUTTON:
			captionText = "Command button named:";
			showCombo = true;
			loadCommandButtons(pCombo);
			break;
		case Parameter::FONT_NAME:
			captionText = "Text type named:";
			showCombo = true;
			loadFontNames(pCombo);
			break;
		case Parameter::TEXT_STRING:
			captionText = "Text String:";
			editText = m_parameter->getString().str();
			showCombo = false;
			break;
		case Parameter::TEAM_STATE:
			captionText = "Team State:";
			editText = m_parameter->getString().str();
			showCombo = false;
			break;
		case Parameter::BRIDGE:
			captionText = "Bridge named:";
			showCombo = true;
			loadBridges(pCombo);
			break;
		case Parameter::TEAM:
			captionText = "Team named:";
			showCombo = true;
			loadTeams(pCombo);
			break;
		case Parameter::UNIT:
			captionText = "Unit named:";
			showCombo = true;
			loadUnits(pCombo);
			break;
		case Parameter::OBJECT_TYPE:
			captionText = "Object type:";
			showList = true;
			loadObjectType(pList);
			break;
		case Parameter::SIDE:
			captionText = "Player named:";
			showCombo = true;
			loadSides(pCombo);
			break;
		case Parameter::COUNTER:
			captionText = "Counter named:";
			showCombo = true;
			loadCounters(pCombo);
			break;
		case Parameter::INT:
			captionText = "Integer:";
			editText.Format("%d", m_parameter->getInt());
			showCombo = false;
			break;
		case Parameter::COLOR:
			DEBUG_CRASH(("should never get here for this data type"));
			captionText = "Color:";
			editText.Format("%08lx", m_parameter->getInt());
			showCombo = false;
			break;
		case Parameter::BOOLEAN:
			captionText = "Boolean:";
			pList->InsertString(-1, "False");
			pList->InsertString(-1, "True");
			pList->SetCurSel(m_parameter->getInt());
			showList = true;
			break;
		case Parameter::REAL:
			captionText = "Real number:";
			editText.Format("%.2f", m_parameter->getReal());
			showCombo = false;
			break;
		case Parameter::ANGLE:
			captionText = "Angle (degrees):";
			editText.Format("%.2f", m_parameter->getReal()*180/PI);
			showCombo = false;
			break;
		case Parameter::PERCENT:
			captionText = "Percent:";
			editText.Format("%.2f", m_parameter->getReal()*100.0f);
			showCombo = false;
			break;
		case Parameter::FLAG:
			captionText = "Flag named:";
			showCombo = true;
			loadFlags(pCombo);
			break;
		case Parameter::COMPARISON:
			captionText = "Comparison:";
			pList->InsertString(-1, "LT Less Than");
			pList->InsertString(-1, "LE Less Than or Equal");
			pList->InsertString(-1, "EQ Equal To");
			pList->InsertString(-1, "GE Greater Than or Equal");
			pList->InsertString(-1, "GT Greater Than");
			pList->InsertString(-1, "NE Not Equal To");
			pList->SetCurSel(m_parameter->getInt());
			showList = true;
			break;
		case Parameter::KIND_OF_PARAM:
			captionText = "Kind of:";
			showList = true;
			for (i=KINDOF_FIRST; i<KINDOF_COUNT; i++) {
				pList->InsertString(-1, KindOfMaskType::getBitNames()[i-KINDOF_FIRST]);
			}
			pList->SetCurSel(m_parameter->getInt());
			break;
		case Parameter::OBJECT_PANEL_FLAG:
			captionText = "Object Panel Flag:";
			showCombo = true;
			loadObjectFlags(pCombo);
			break;
		case Parameter::AI_MOOD:
			captionText = "AI Mood:";
			pList->InsertString(-1, "Sleep");
			pList->InsertString(-1, "Passive");
			pList->InsertString(-1, "Normal");
			pList->InsertString(-1, "Alert");
			pList->InsertString(-1, "Agressive");
			pList->SetCurSel(m_parameter->getInt() - ATTITUDE_SLEEP);
			showList = true;
			break;
		case Parameter::SKIRMISH_WAYPOINT_PATH:
			captionText = "Skirmish Approach Path:";
			pList->InsertString(-1, "Center");
			pList->InsertString(-1, "Backdoor");
			pList->InsertString(-1, "Flank");
			pList->InsertString(-1, "Special");
			i = pList->FindStringExact(-1, m_parameter->getString().str());
			if (i!=CB_ERR)
				pList->SetCurSel(i);
			else
				pList->SetCurSel(0);
			showList = true;
			break;
		case Parameter::RADAR_EVENT_TYPE:
			captionText = "Radar Event Type:";
			pList->InsertString(-1,"Construction");
			pList->InsertString(-1,"Upgrade");
			pList->InsertString(-1,"Under Attack");
			pList->InsertString(-1,"Information");
			pList->SetCurSel(m_parameter->getInt() - RADAR_EVENT_CONSTRUCTION);
			showList = true;
			break;
		case Parameter::LEFT_OR_RIGHT:
			captionText = "Evacuate Container Side Choices:";
			pList->InsertString(-1,"Left");
			pList->InsertString(-1,"Right");
			pList->InsertString(-1,"Center (Default)");
			pList->SetCurSel(m_parameter->getInt() - 1);
			showList = true;
			break;
		case Parameter::RELATION:
			captionText = "Relation:";
			pList->InsertString(-1, "Enemy");
			pList->InsertString(-1, "Neutral");
			pList->InsertString(-1, "Friend");
			pList->SetCurSel(m_parameter->getInt());
			showList = true;
			break;
		case Parameter::DIALOG:
			captionText = "Dialog :";
		case Parameter::MUSIC:
			captionText = "Music :";
		case Parameter::SOUND:
			if (m_parameter->getParameterType() == Parameter::SOUND) {
				captionText = "Sound :";
			}
			captionText = "Locate the Audio name:";
			showCombo = true;
			// enable the preview sound button only if we are dealing with a soundfx or speech
			showAudioButton = true;
			if (m_parameter->getParameterType() == Parameter::MUSIC)
				showAudioButton = false;
			loadAudioType(m_parameter->getParameterType(), pCombo);
			break;
		case Parameter::MOVIE:
			captionText = "Locate the Audio name:";
			showCombo = true;
			loadMovies(pCombo);
			break;
		case Parameter::SPECIAL_POWER:
			captionText = "Special Power:";
			showCombo = true;
			loadSpecialPowers(pCombo);
			break;
		case Parameter::SCIENCE:
			captionText = "Science:";
			showCombo = true;
			loadSciences(pCombo);
			break;
		case Parameter::SCIENCE_AVAILABILITY:
			captionText = "Science Availability:";
			showCombo = true;
			loadScienceAvailabilities( pCombo );
			break;
		case Parameter::UPGRADE:
			captionText = "Upgrade:";
			showCombo = true;
			loadUpgrades(pCombo);
			break;
		case Parameter::COMMANDBUTTON_ABILITY:
			captionText = "Ability:";
			showCombo = true;
			loadAbilities( pCombo );
			break;
		case Parameter::COMMANDBUTTON_ALL_ABILITIES:
			captionText = "Ability:";
			showCombo = true;
			loadAllAbilities( pCombo );
			break;
		case Parameter::BOUNDARY:
		{
			captionText = "Boundary:";
			showCombo = true;
			int numBoundaries = TheTerrainRenderObject->getMap()->getAllBoundaries().size();
			for (int b = 0; b < numBoundaries; ++b) {
				pCombo->InsertString(-1, BORDER_COLORS[b % BORDER_COLORS_SIZE].m_colorName);
			}
			break;
		}
		case Parameter::BUILDABLE:
			captionText = "Buidable:";
			showList = true;
			for (i = BSTATUS_YES; i < BSTATUS_NUM_TYPES; ++i) {
				pList->InsertString(-1, BuildableStatusNames[i - BSTATUS_YES]);
			}
			pList->SetCurSel(m_parameter->getInt());
			break;
		case Parameter::SURFACES_ALLOWED:
		{
			captionText = "Surfaces allowed: ";
			showList = true;
			for (i = 0; i < 3; ++i) {
				pList->InsertString(-1, Surfaces[i]);
			}
			// 0 is invalid for surfaces, so change this to a 3 (which means AIR and GROUND)
			if (m_parameter->getInt() == 0) {
				m_parameter->friend_setInt(3);
			}
			pList->SetCurSel(m_parameter->getInt() - 1);
			break;
		}
		case Parameter::SHAKE_INTENSITY:
		{
			captionText = "Shake Intensity: ";
			showList = true;
			for (i = 0; i < View::SHAKE_COUNT; ++i) {
				pList->InsertString(-1, ShakeIntensities[i]);
			}
			pList->SetCurSel(m_parameter->getInt());
			break;
		}
		case Parameter::OBJECT_STATUS:
		{
			captionText = "Object status:";
			showList = true;
			for( i = 0; i < OBJECT_STATUS_COUNT; i++ )
			{
				pList->InsertString( -1, ObjectStatusMaskType::getBitNames()[i] );
			}
			pList->SelectString( -1, m_parameter->getString().str() );
			break;
		}
		case Parameter::FACTION_NAME:
		{
			captionText = "Faction Name: ";
			showList = true;
			AsciiStringList sideList;
			ThePlayerTemplateStore->getAllSideStrings(&sideList);
			for (AsciiStringListIterator it = sideList.begin(); it != sideList.end(); ++it) {
				pList->InsertString(-1, it->str());
			}
			pList->SetCurSel(0);
			break;
		}
		case Parameter::EMOTICON:
		{
			captionText = "Emoticons:";
			showCombo = true;
			loadEmoticons( pCombo );
			break;
		}
		case Parameter::OBJECT_TYPE_LIST:
		{
			captionText = "Object type list:";
			showCombo = true;
			loadObjectTypeList(pCombo);
			break;
		}
		case Parameter::REVEALNAME:
		{
			captionText = "Reveal Name:";
			showCombo = true;
			loadRevealNames(pCombo);
			break;
		}
	}

	Int initialSel = -1;
	AsciiString initialText;
	Int kind;
	if (showCombo)
	{
		kind = WB_QT_PARAM_KIND_COMBO;
		CString text = m_parameter->getString().str();
		initialText = (LPCTSTR)text;
		if (text.IsEmpty())
		{
			initialSel = (pCombo->GetCount() > 0) ? 0 : -1;
		}
		else
		{
			int index = pCombo->FindStringExact(-1, text);
			if (index != CB_ERR)
			{
				initialSel = index;
			}
		}
	}
	else if (showList)
	{
		kind = WB_QT_PARAM_KIND_LIST;
		initialSel = pList->GetCurSel();
	}
	else
	{
		kind = WB_QT_PARAM_KIND_EDIT;
		initialText = (LPCTSTR)editText;
	}

	*captionOut = AsciiString((LPCTSTR)captionText);
	*kindOut = kind;
	*showAudioOut = showAudioButton ? 1 : 0;
	*initialSelOut = initialSel;
	*initialTextOut = initialText;
	return pCombo->GetCount();
}

// ================= EditParameter::qtStore =================
// Port of EditParameter::OnOK's write-back switch. `text` is the edit/combo text (for list
// kinds, the selected row's text); `selIndex` is the selected row (-1 = none). Returns 0 when
// the input does not parse (the Qt dialog beeps and stays open, like the MFC SetFocus+beep).

Int EditParameter::qtStore(Parameter *pParm, const char *text, Int selIndex)
{
	Parameter *m_parameter = pParm;	// keep the switch below textually close to OnOK
	AsciiString comboText(text ? text : "");
	CString txt(text ? text : "");
	switch (m_parameter->getParameterType()) {
		default:
			DEBUG_CRASH(("Unknown parameter type."));
			break;
		case Parameter::UNIT:
		case Parameter::SCRIPT:
		case Parameter::SCRIPT_SUBROUTINE:
		case Parameter::ATTACK_PRIORITY_SET:
		case Parameter::SIDE:
		case Parameter::WAYPOINT:
		case Parameter::WAYPOINT_PATH:
		case Parameter::TRIGGER_AREA:
		case Parameter::COMMAND_BUTTON:
		case Parameter::FONT_NAME:
		case Parameter::TEAM:
		case Parameter::OBJECT_TYPE:
		case Parameter::OBJECT_TYPE_LIST:
		case Parameter::BRIDGE:
		case Parameter::FLAG:
		case Parameter::COUNTER:
		case Parameter::DIALOG:
		case Parameter::MUSIC:
		case Parameter::SPECIAL_POWER:
		case Parameter::SCIENCE:
		case Parameter::SCIENCE_AVAILABILITY:
		case Parameter::UPGRADE:
		case Parameter::SOUND:
		case Parameter::MOVIE:
		case Parameter::COMMANDBUTTON_ABILITY:
		case Parameter::COMMANDBUTTON_ALL_ABILITIES:
		case Parameter::EMOTICON:
		case Parameter::REVEALNAME:
		case Parameter::OBJECT_PANEL_FLAG:
			m_parameter->friend_setString(comboText);
			break;
		case Parameter::TEAM_STATE:
		case Parameter::TEXT_STRING:
			m_parameter->friend_setString(comboText);
			break;
		case Parameter::INT:
		{
			Int theInt;
			if (1==sscanf(txt, "%d", &theInt)) {
				m_parameter->friend_setInt(theInt);
			} else {
				return 0;
			}
			break;
		}
		case Parameter::COLOR:
		{
			DEBUG_CRASH(("should never get here for this data type"));
			Int theInt;
			if (1==sscanf(txt, "%08lx", &theInt)) {
				m_parameter->friend_setInt(theInt);
			} else {
				return 0;
			}
			break;
		}
		case Parameter::REAL:
		{
			Real theReal;
			if (1==sscanf(txt, "%f", &theReal)) {
				m_parameter->friend_setReal(theReal*1.0f);
			} else {
				return 0;
			}
			break;
		}
		case Parameter::ANGLE:
		{
			Real theReal;
			if (1==sscanf(txt, "%f", &theReal)) {
				m_parameter->friend_setReal(theReal*PI/180.0f);
			} else {
				return 0;
			}
			break;
		}
		case Parameter::PERCENT:
		{
			Real theReal;
			if (1==sscanf(txt, "%f", &theReal)) {
				m_parameter->friend_setReal(theReal/100.0f);
			} else {
				return 0;
			}
			break;
		}
		case Parameter::BOOLEAN:
		case Parameter::COMPARISON:
			m_parameter->friend_setInt(selIndex);
			break;
		case Parameter::KIND_OF_PARAM:
			m_parameter->friend_setInt(selIndex+KINDOF_FIRST);
			break;
		case Parameter::RELATION:
			m_parameter->friend_setInt(selIndex + Parameter::REL_ENEMY);
			break;
		case Parameter::AI_MOOD:
			m_parameter->friend_setInt(selIndex + ATTITUDE_SLEEP);
			break;
		case Parameter::SKIRMISH_WAYPOINT_PATH:
			m_parameter->friend_setString(comboText);
			break;
		case Parameter::RADAR_EVENT_TYPE:
			m_parameter->friend_setInt(selIndex + RADAR_EVENT_CONSTRUCTION);
			break;
		case Parameter::LEFT_OR_RIGHT:
			m_parameter->friend_setInt(selIndex + 1);
			break;
		case Parameter::LOCALIZED_TEXT:
			m_parameter->friend_setString(loadLocalizedText(NULL, comboText));
			break;
		case Parameter::BOUNDARY:
			if (selIndex >= 0) {
				m_parameter->friend_setInt(selIndex);
			}
			break;
		case Parameter::BUILDABLE:
			m_parameter->friend_setInt(selIndex+BSTATUS_YES);
			break;
		case Parameter::SURFACES_ALLOWED:
			m_parameter->friend_setInt(selIndex + 1);
			break;
		case Parameter::SHAKE_INTENSITY:
			m_parameter->friend_setInt(selIndex);
			break;
		case Parameter::OBJECT_STATUS:
			if( selIndex >= 0 )
			{
				m_parameter->friend_setString( comboText );
			}
			else
			{
				m_parameter->friend_setString( AsciiString::TheEmptyString );
			}
			break;
		case Parameter::FACTION_NAME:
			if (selIndex >= 0) {
				m_parameter->friend_setString(comboText);
			}
			break;
	}
	return 1;
}

// ================= EditParameter::qtPreviewAudio =================
// Port of OnPreviewSound / OnComboSelChange's playback body.

void EditParameter::qtPreviewAudio(Parameter *pParm, const char *eventName)
{
	if (pParm->getParameterType() == Parameter::SOUND ||
		  pParm->getParameterType() == Parameter::DIALOG ||
		  pParm->getParameterType() == Parameter::MUSIC) {
		AsciiString comboText(eventName ? eventName : "");
		AudioEventRTS event;
		event.setEventName(comboText);
		event.setAudioEventInfo(TheAudio->findAudioEventInfo(comboText));
		event.generateFilename();
		if (!event.getFilename().isEmpty()) {
			PlaySound(event.getFilename().str(), NULL, SND_ASYNC | SND_FILENAME | SND_PURGE);
		}
	}
}

// ================= the C facade =================

// The hidden combo the options are loaded into. CBS_SORT + CBS_SIMPLE match the real dialog's
// IDC_COMBO/IDC_LIST styles, so AddString sorts and InsertString appends identically.
static CComboBox s_qtHiddenCombo;

static CComboBox *qtHiddenCombo(void)
{
	if (s_qtHiddenCombo.GetSafeHwnd() == NULL)
	{
		s_qtHiddenCombo.Create(WS_CHILD | CBS_SIMPLE | CBS_SORT | WS_VSCROLL,
			CRect(0, 0, 120, 200), AfxGetMainWnd(), 54321);
	}
	return &s_qtHiddenCombo;
}

extern "C" int WBQtParamData_GetEditorKind(void *parameter)
{
	Parameter *pParm = static_cast<Parameter *>(parameter);
	switch (pParm->getParameterType())
	{
		case Parameter::COORD3D:		return WB_QT_PARAM_EDITOR_COORD;
		case Parameter::OBJECT_TYPE:	return WB_QT_PARAM_EDITOR_OBJECT;
		case Parameter::COLOR:			return WB_QT_PARAM_EDITOR_COLOR;
		default:						return WB_QT_PARAM_EDITOR_GENERIC;
	}
}

extern "C" int WBQtParamData_Describe(void *parameter, const char *unitName,
	char *captionOut, int captionCap, int *kindOut, int *showAudioOut, int *initialSelOut,
	char *initialTextOut, int textCap)
{
	CComboBox *pCombo = qtHiddenCombo();
	if (pCombo->GetSafeHwnd() == NULL)
	{
		return 0;
	}
	AsciiString caption;
	AsciiString initialText;
	Int kind = WB_QT_PARAM_KIND_EDIT;
	Int showAudio = 0;
	Int initialSel = -1;
	Int count = EditParameter::qtDescribe(static_cast<Parameter *>(parameter),
		AsciiString(unitName ? unitName : ""), pCombo, &caption, &kind, &showAudio,
		&initialSel, &initialText);
	copyOut(caption, captionOut, captionCap);
	copyOut(initialText, initialTextOut, textCap);
	if (kindOut != NULL)
	{
		*kindOut = kind;
	}
	if (showAudioOut != NULL)
	{
		*showAudioOut = showAudio;
	}
	if (initialSelOut != NULL)
	{
		*initialSelOut = initialSel;
	}
	return count;
}

extern "C" void WBQtParamData_GetOption(int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	CComboBox *pCombo = qtHiddenCombo();
	if (pCombo->GetSafeHwnd() == NULL || i < 0 || i >= pCombo->GetCount())
	{
		return;
	}
	CString text;
	pCombo->GetLBText(i, text);
	copyOut(AsciiString((LPCTSTR)text), buf, cap);
}

extern "C" int WBQtParam_Store(void *parameter, const char *text, int selIndex)
{
	return EditParameter::qtStore(static_cast<Parameter *>(parameter), text, selIndex);
}

extern "C" void WBQtParam_PreviewAudio(void *parameter, const char *eventName)
{
	EditParameter::qtPreviewAudio(static_cast<Parameter *>(parameter), eventName);
}

// --- coordinate parameter ---

extern "C" void WBQtParamData_GetCoord(void *parameter, double *x, double *y, double *z)
{
	Coord3D coord;
	static_cast<Parameter *>(parameter)->getCoord3D(&coord);
	*x = coord.x;
	*y = coord.y;
	*z = coord.z;
}

extern "C" void WBQtParamData_SetCoord(void *parameter, double x, double y, double z)
{
	Coord3D coord;
	coord.x = (Real)x;
	coord.y = (Real)y;
	coord.z = (Real)z;
	static_cast<Parameter *>(parameter)->friend_setCoord3D(&coord);
}

// --- color parameter (raw aarrggbb int; the Qt side converts to/from QColor) ---

extern "C" int WBQtParamData_GetColor(void *parameter)
{
	return static_cast<Parameter *>(parameter)->getInt();
}

extern "C" void WBQtParamData_SetColor(void *parameter, int argb)
{
	static_cast<Parameter *>(parameter)->friend_setInt(argb);
}

extern "C" void WBQtParamData_GetString(void *parameter, char *buf, int cap)
{
	copyOut(static_cast<Parameter *>(parameter)->getString(), buf, cap);
}

// --- object-type picker: template catalog + script object lists ---
// == EditObjectParameter::OnInitDialog/addObject: [TEST/]side/editor-sorting/leaf.

static std::vector<const ThingTemplate *> s_qtTemplates;

extern "C" int WBQtParamData_GetTemplateCount(void)
{
	s_qtTemplates.clear();
	const ThingTemplate *tTemplate;
	for( tTemplate = TheThingFactory->firstTemplate();
			 tTemplate;
			 tTemplate = tTemplate->friend_getNextTemplate() )
	{
		s_qtTemplates.push_back(tTemplate);
	}
	return (int)s_qtTemplates.size();
}

extern "C" int WBQtParamData_GetTemplateInfo(int i, char *nameOut, int nameCap,
	char *sideOut, int sideCap, char *sortingOut, int sortingCap, int *isTestOut)
{
	if (i < 0 || i >= (int)s_qtTemplates.size())
	{
		return 0;
	}
	const ThingTemplate *thingTemplate = s_qtTemplates[i];
	copyOut(thingTemplate->getName(), nameOut, nameCap);
	copyOut(thingTemplate->getDefaultOwningSide(), sideOut, sideCap);
	EditorSortingType sorting = thingTemplate->getEditorSorting();
	if (sorting >= ES_FIRST && sorting < ES_NUM_SORTING_TYPES)
	{
		copyOut(AsciiString(EditorSortingNames[sorting]), sortingOut, sortingCap);
	}
	else
	{
		copyOut(AsciiString("UNSORTED"), sortingOut, sortingCap);
	}
	if (isTestOut != NULL)
	{
		*isTestOut = (sorting == ES_TEST) ? 1 : 0;
	}
	return 1;
}

static std::vector<AsciiString> s_qtObjectLists;

extern "C" int WBQtParamData_GetObjectListCount(void)
{
	s_qtObjectLists.clear();
	EditParameter::loadObjectTypeList(NULL, &s_qtObjectLists);
	return (int)s_qtObjectLists.size();
}

extern "C" void WBQtParamData_GetObjectList(int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	if (i >= 0 && i < (int)s_qtObjectLists.size())
	{
		copyOut(s_qtObjectLists[i], buf, cap);
	}
}

// --- group editor (== EditGroup's OnInitDialog/OnOK over the ScriptGroup) ---

extern "C" void WBQtGroupData_Get(void *scriptGroup, char *nameOut, int cap, int *activeOut, int *subroutineOut)
{
	ScriptGroup *pGroup = static_cast<ScriptGroup *>(scriptGroup);
	copyOut(pGroup->getName(), nameOut, cap);
	if (activeOut != NULL)
	{
		*activeOut = pGroup->isActive() ? 1 : 0;
	}
	if (subroutineOut != NULL)
	{
		*subroutineOut = pGroup->isSubroutine() ? 1 : 0;
	}
}

extern "C" void WBQtGroupData_Set(void *scriptGroup, const char *name, int active, int subroutine)
{
	ScriptGroup *pGroup = static_cast<ScriptGroup *>(scriptGroup);
	pGroup->setName(AsciiString(name ? name : ""));
	pGroup->setActive(active != 0);
	pGroup->setSubroutine(subroutine != 0);
}

#endif // RTS_HAS_QT
