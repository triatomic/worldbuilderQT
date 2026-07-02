// WBQtMiscModalsBridge.cpp -- MFC side of the Tier 3a workflow-modal seam. Plain MFC TU (no Qt
// include). Ports the OnInitDialog / OnOK / live-apply bodies of ShadowOptions,
// ImpassableOptions, SelectMacrotexture, MapSettings and ExportScriptsOptions, operating on the
// same global engine state as the dialogs did. Whole body guarded by RTS_HAS_QT so the OFF
// build compiles it to an empty object.

// TimeOfDayNames / WeatherNames are defined by the header that DEFINE_*_NAMES gates; the define
// must precede every include (transitive includes + include guards would swallow a later one).
#define DEFINE_TIME_OF_DAY_NAMES
#define DEFINE_WEATHER_NAMES

#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "WorldBuilder.h"
#include "WorldBuilderDoc.h"

#include "rendobj.h"
#include "Compression.h"
#include "Common/GlobalData.h"
#include "Common/FileSystem.h"
#include "Common/WellKnownKeys.h"
#include "GameLogic/SidesList.h"
#include "W3DDevice/GameClient/W3DShadow.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "wbview3d.h"			// WbView3d (Impassable preview calls updateHeightMapInView)

#include "ExportScriptsOptions.h"
#include "qt/panels/WBQtMiscModalsBridge.h"

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

// ================= Shadow Options =================
// == ShadowOptions::OnInitDialog's decompose and ::setShadowColor's compose.

extern "C" void WBQtShadow_Get(double *red, double *green, double *blue, double *intensity)
{
	UnsignedInt clr = TheW3DShadowManager->getShadowColor();
	Real r = ((clr>>16)&0x00FF)/255.0f;
	Real g = ((clr>>8)&0x00FF)/255.0f;
	Real b = ((clr)&0x00FF)/255.0f;

	Real inten = r;
	if (g < r) inten = g;
	if (b < inten) inten = b;
	inten = 1.0f - inten;
	if (inten < (1/256.0f))
	{
		inten = 0;
		r = g = b = 0;
	}
	else
	{
		r -= (1.0-inten); r /= inten;
		g -= (1.0-inten); g /= inten;
		b -= (1.0-inten); b /= inten;
	}
	*red = r;
	*green = g;
	*blue = b;
	*intensity = inten;
}

extern "C" void WBQtShadow_Apply(double red, double green, double blue, double intensity)
{
	Int r, g, b, shift;
	Real m_red = (Real)red, m_green = (Real)green, m_blue = (Real)blue, m_intensity = (Real)intensity;

	shift = (1.0-m_intensity)*255;
	if (shift>255) shift = 255;
	if (shift<0) shift = 0;

	r = m_intensity*m_red*255 + shift;
	if (r>255) r = 255;
	if (r<0) r = 0;
	g = m_intensity*m_green*255 + shift;
	if (g>255) g = 255;
	if (g<0) g = 0;
	b = m_intensity*m_blue*255 + shift;
	if (b>255) b = 255;
	if (b<0) b = 0;

	UnsignedInt clr = (255<<24) + (r<<16) + (g<<8) + b;
	TheW3DShadowManager->setShadowColor(clr);
}

// ================= Impassable Options =================

static Bool s_impassablePrevShow = FALSE;
static Real s_impassableDefaultSlope = 45.0f;

extern "C" void WBQtImpassable_Begin(void)
{
	// == ImpassableOptions ctor + the call site's SetDefaultSlopeToShow: force the overlay on
	// (remembering the prior state) and snapshot the current slope as the cancel fallback.
	s_impassablePrevShow = TheTerrainRenderObject->getShowImpassableAreas();
	s_impassableDefaultSlope = TheTerrainRenderObject->getViewImpassableAreaSlope();
	TheTerrainRenderObject->setShowImpassableAreas(TRUE);
}

extern "C" void WBQtImpassable_End(int accepted)
{
	// == the dtor (restore overlay) + the call site's OK/Cancel branch: on cancel, revert the
	// live-applied slope to the snapshot; on OK the live value stands.
	if (accepted == 0)
	{
		TheTerrainRenderObject->setViewImpassableAreaSlope(s_impassableDefaultSlope);
	}
	TheTerrainRenderObject->setShowImpassableAreas(s_impassablePrevShow);
}

extern "C" double WBQtImpassable_GetSlope(void)
{
	return TheTerrainRenderObject->getViewImpassableAreaSlope();
}

extern "C" double WBQtImpassable_SetSlope(double slope)
{
	// == ImpassableOptions::ValidateSlope + OnAngleChange's live apply.
	Real m_slopeToShow = (Real)slope;
	if (m_slopeToShow < 0.0f)
	{
		m_slopeToShow = 0.0f;
	}
	if (m_slopeToShow >= 90.0f)
	{
		m_slopeToShow = 89.9f;
	}
	TheTerrainRenderObject->setViewImpassableAreaSlope(m_slopeToShow);
	return m_slopeToShow;
}

extern "C" void WBQtImpassable_Preview(void)
{
	// == ImpassableOptions::OnPreview.
	IRegion2D range = {0,0,0,0};
	WbView3d *pView = CWorldBuilderDoc::GetActive3DView();
	if (pView != NULL)
	{
		pView->updateHeightMapInView(TheTerrainRenderObject->getMap(), false, range);
	}
}

// ================= Select Macrotexture =================
// == SelectMacrotexture::OnInitDialog's ..\TestArt\*.tga enumeration + OnNotify's live apply.

static std::vector<AsciiString> s_macroTextures;	// files, then a trailing "***Default" marker
static const char *const K_MACRO_DEFAULT = "***Default";

extern "C" int WBQtMacrotexture_GetCount(void)
{
	s_macroTextures.clear();
	char dirBuf[_MAX_PATH];
	strcpy(dirBuf, "..\\TestArt");
	int len = strlen(dirBuf);
	if (len > 0 && dirBuf[len - 1] != '\\')
	{
		dirBuf[len++] = '\\';
		dirBuf[len] = 0;
	}
	FilenameList filenameList;
	TheFileSystem->getFileListInDirectory(AsciiString(dirBuf), AsciiString("*.tga"), filenameList, FALSE);
	for (FilenameList::iterator it = filenameList.begin(); it != filenameList.end(); ++it)
	{
		if (it->getLength() >= 5)
		{
			s_macroTextures.push_back(*it);
		}
	}
	s_macroTextures.push_back(AsciiString(K_MACRO_DEFAULT));
	return (int)s_macroTextures.size();
}

extern "C" void WBQtMacrotexture_GetName(int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	if (i >= 0 && i < (int)s_macroTextures.size())
	{
		copyOut(s_macroTextures[i], buf, cap);
	}
}

extern "C" int WBQtMacrotexture_IsDefault(int i)
{
	if (i >= 0 && i < (int)s_macroTextures.size())
	{
		return (s_macroTextures[i].compareNoCase(K_MACRO_DEFAULT) == 0) ? 1 : 0;
	}
	return 0;
}

extern "C" void WBQtMacrotexture_Apply(int i)
{
	if (i < 0 || i >= (int)s_macroTextures.size())
	{
		return;
	}
	if (s_macroTextures[i].compareNoCase(K_MACRO_DEFAULT) == 0)
	{
		TheTerrainRenderObject->updateMacroTexture(AsciiString(""));
	}
	else
	{
		TheTerrainRenderObject->updateMacroTexture(s_macroTextures[i]);
	}
	TheWritableGlobalData->m_useLightMap = true;
}

// ================= Map Settings =================

extern "C" int WBQtMapSettings_GetTimeOfDayCount(void)
{
	return TIME_OF_DAY_COUNT - TIME_OF_DAY_FIRST;
}

extern "C" void WBQtMapSettings_GetTimeOfDayName(int i, char *buf, int cap)
{
	copyOut(AsciiString(TimeOfDayNames[i + TIME_OF_DAY_FIRST]), buf, cap);
}

extern "C" int WBQtMapSettings_GetTimeOfDayIndex(void)
{
	return TheGlobalData->m_timeOfDay - TIME_OF_DAY_FIRST;
}

extern "C" int WBQtMapSettings_GetWeatherCount(void)
{
	return WEATHER_COUNT;
}

extern "C" void WBQtMapSettings_GetWeatherName(int i, char *buf, int cap)
{
	copyOut(AsciiString(WeatherNames[i]), buf, cap);
}

extern "C" int WBQtMapSettings_GetWeatherIndex(void)
{
	return TheGlobalData->m_weather;
}

extern "C" int WBQtMapSettings_GetCompressionCount(void)
{
	return (COMPRESSION_MAX - COMPRESSION_MIN) + 1;
}

extern "C" void WBQtMapSettings_GetCompressionName(int i, char *buf, int cap)
{
	copyOut(AsciiString(CompressionManager::getCompressionNameByType((CompressionType)(i + COMPRESSION_MIN))), buf, cap);
}

extern "C" int WBQtMapSettings_GetCompressionIndex(void)
{
	Dict *worldDict = MapObject::getWorldDict();
	Bool exists = FALSE;
	Int index = worldDict->getInt(TheKey_compression, &exists);
	if (!exists)
	{
		index = CompressionManager::getPreferredCompression();
	}
	return index;
}

extern "C" void WBQtMapSettings_GetMapName(char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	Dict *worldDict = MapObject::getWorldDict();
	Bool exists = false;
	AsciiString mapName = worldDict->getAsciiString(TheKey_mapName, &exists);
	if (exists)
	{
		copyOut(mapName, buf, cap);
	}
}

extern "C" void WBQtMapSettings_Store(int timeOfDayIndex, int weatherIndex, int compressionIndex, const char *mapName)
{
	// == MapSettings::OnOK.
	TimeOfDay tod = (TimeOfDay)(timeOfDayIndex + TIME_OF_DAY_FIRST);
	Weather theWeather = (Weather)weatherIndex;
	TheWritableGlobalData->setTimeOfDay(tod);
	TheWritableGlobalData->m_weather = theWeather;

	Dict *worldDict = MapObject::getWorldDict();
	worldDict->setAsciiString(TheKey_mapName, AsciiString(mapName ? mapName : ""));
	CompressionType compType = (CompressionType)(compressionIndex + COMPRESSION_MIN);
	worldDict->setInt(TheKey_compression, compType);
}

// ================= Export Scripts Options =================
// The dialog persists six static bools. Read/write them directly (they are the same statics
// ScriptDialog::OnSave reads via the getters after DoModal).

extern "C" void WBQtExportScripts_Get(int *waypoints, int *triggers, int *units, int *teams, int *sides, int *allScripts)
{
	// Values seeded from the statics; a temporary object exposes them via the getters.
	ExportScriptsOptions probe;
	*waypoints = probe.getDoWaypoints() ? 1 : 0;
	*triggers = probe.getDoTriggers() ? 1 : 0;
	*units = probe.getDoUnits() ? 1 : 0;
	*teams = probe.getDoTeams() ? 1 : 0;
	*sides = probe.getDoSides() ? 1 : 0;
	*allScripts = probe.getDoAllScripts() ? 1 : 0;
}

extern "C" void WBQtExportScripts_Store(int waypoints, int triggers, int units, int teams, int sides, int allScripts)
{
	ExportScriptsOptions::qtStore(units != 0, teams != 0, waypoints != 0, triggers != 0, sides != 0, allScripts != 0);
}

// ================= Fix Team Owner =================
// == CFixTeamOwnerDialog::OnInitDialog's list build + prompt; rows are side indices.

static const char *const K_NEUTRAL_NAME = "(neutral)";

extern "C" void WBQtFixOwnerData_GetPrompt(void *teamsInfo, char *buf, int cap)
{
	TeamsInfo *ti = static_cast<TeamsInfo *>(teamsInfo);
	AsciiString teamName = "No Name";
	Bool exists;
	AsciiString temp = ti->getDict()->getAsciiString(TheKey_teamName, &exists);
	if (exists) {
		teamName = temp;
	}
	CString loadStr;
	loadStr.Format(IDS_REPLACEOWNER, teamName.str());
	copyOut(AsciiString((LPCTSTR)loadStr), buf, cap);
}

extern "C" int WBQtFixOwnerData_GetCount(void *sidesList)
{
	return static_cast<SidesList *>(sidesList)->getNumSides();
}

extern "C" void WBQtFixOwnerData_GetDisplay(void *sidesList, int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	SidesList *sl = static_cast<SidesList *>(sidesList);
	SidesInfo *si = sl->getSideInfo(i);
	if (si == NULL)
	{
		return;
	}
	Bool displayExists;
	AsciiString displayName = si->getDict()->getAsciiString(TheKey_playerDisplayName, &displayExists);
	if (displayExists) {
		if (displayName.isEmpty()) {
			displayName = K_NEUTRAL_NAME;
		}
		copyOut(displayName, buf, cap);
	} else {
		AsciiString internalName = si->getDict()->getAsciiString(TheKey_playerName, &displayExists);
		if (internalName.isEmpty()) {
			internalName = K_NEUTRAL_NAME;
		}
		copyOut(internalName, buf, cap);
	}
}

extern "C" void WBQtFixOwnerData_GetInternal(void *sidesList, int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	SidesList *sl = static_cast<SidesList *>(sidesList);
	SidesInfo *si = sl->getSideInfo(i);
	if (si != NULL)
	{
		Bool exists;
		copyOut(si->getDict()->getAsciiString(TheKey_playerName, &exists), buf, cap);
	}
}

// qtStore is a member static (declared in ExportScriptsOptions.h), so it can write the private
// flag statics. == ExportScriptsOptions::OnOK's assignments.
void ExportScriptsOptions::qtStore(Bool units, Bool teams, Bool waypoints, Bool triggers, Bool sides, Bool allScripts)
{
	m_units = units;
	m_teams = teams;
	m_waypoints = waypoints;
	m_triggers = triggers;
	m_sides = sides;
	m_allScripts = allScripts;
}

#endif // RTS_HAS_QT
