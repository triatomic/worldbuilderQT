// WBQtPickUnitBridge.cpp -- the MFC side of the Qt Pick Unit / Replace Missing Unit seam.
// Plain MFC TU (no Qt include); whole body guarded by RTS_HAS_QT so the OFF build compiles it
// to an empty object. Supplies the template catalog filtered exactly like
// PickUnitDialog::OnInitDialog (allowable editor sortings + factionOnly==isBuildableItem) and
// the shared 128x128 preview render (ObjectPreview::qtRenderTemplatePreview).
#define DEFINE_EDITOR_SORTING_NAMES		// instantiate EditorSortingNames[] in this TU

#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "PickUnitDialog.h"
#include "ObjectPreview.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/ThingSort.h"
#include "WorldBuilderDoc.h"		// the replace report re-points map objects + recentres the view
#include "wbview3d.h"
#include "qt/panels/WBQtPickUnitBridge.h"
#include "qt/WBQtPanelBridge.h"			// script rows rewrite params via _ReplaceInParams
#include "qt/panels/WBQtTeamsBridge.h"	// team rows rewrite unit slots via _ReplaceUnitName

#ifdef RTS_HAS_QT

#include <vector>

// The MFC preview is a fixed 128x128 BGR image (see PREVIEW_WIDTH/HEIGHT in ObjectPreview.cpp).
#define WBQT_PREVIEW_W 256	// the 2x Qt render (see ObjectPreview::qtRenderTemplatePreview)
#define WBQT_PREVIEW_H 256

static void copyOut(const AsciiString &s, char *buf, int cap)
{
	if (buf == NULL || cap <= 0)
	{
		return;
	}
	strncpy(buf, s.str(), cap - 1);
	buf[cap - 1] = 0;
}

static std::vector<const ThingTemplate *> s_qtPickTemplates;

extern "C" int WBQtPickUnitData_Build(const int *allowable, int allowCount, int factionOnly)
{
	Bool allowed[ES_NUM_SORTING_TYPES];
	int i;
	// A NULL list means "every editor sorting" -- it lets Qt-side callers ask for the unfiltered
	// catalog without needing the ES_* enum, which lives on this side of the seam.
	const Bool allowEverything = (allowable == NULL);
	for (i = 0; i < ES_NUM_SORTING_TYPES; i++)
	{
		allowed[i] = allowEverything;
	}
	for (i = 0; i < allowCount; i++)
	{
		if (allowable != NULL && allowable[i] >= 0 && allowable[i] < ES_NUM_SORTING_TYPES)
		{
			allowed[allowable[i]] = true;
		}
	}
	s_qtPickTemplates.clear();
	const ThingTemplate *tTemplate;
	for( tTemplate = TheThingFactory->firstTemplate();
			 tTemplate;
			 tTemplate = tTemplate->friend_getNextTemplate() )
	{
		// == PickUnitDialog::IsAllowableType(sort, isBuildableItem).
		if (factionOnly && !tTemplate->isBuildableItem())
		{
			continue;
		}
		EditorSortingType sort = tTemplate->getEditorSorting();
		if (sort < 0 || sort >= ES_NUM_SORTING_TYPES || !allowed[sort])
		{
			continue;
		}
		s_qtPickTemplates.push_back(tTemplate);
	}
	return (int)s_qtPickTemplates.size();
}

// True when catalog entry i is a template the loaded map.ini invented, so the auto-matcher can
// leave it out of the replacement candidates (see WBMapIni_IsPhantomTemplate). The browse/pick
// dialog deliberately does NOT filter on this -- picking one by hand is a valid choice; only
// AUTOMATIC replacement must avoid resolving a broken name to another name that isn't real.
extern "C" int WBQtPickUnitData_IsPhantom(int i)
{
	if (i < 0 || i >= (int)s_qtPickTemplates.size())
	{
		return 0;
	}
	return WBMapIni_IsPhantomTemplate(s_qtPickTemplates[i]->getName()) ? 1 : 0;
}

extern "C" int WBQtPickUnitData_GetInfo(int i, char *nameOut, int nameCap, char *sideOut, int sideCap,
	char *sortingOut, int sortingCap, int *isTestOut)
{
	if (i < 0 || i >= (int)s_qtPickTemplates.size())
	{
		return 0;
	}
	const ThingTemplate *thingTemplate = s_qtPickTemplates[i];
	copyOut(thingTemplate->getName(), nameOut, nameCap);
	copyOut(thingTemplate->getDefaultOwningSide(), sideOut, sideCap);
	EditorSortingType sorting = thingTemplate->getEditorSorting();
	if (sorting >= ES_FIRST && sorting < ES_NUM_SORTING_TYPES)
	{
		copyOut(AsciiString(EditorSortingNames[sorting]), sortingOut, sortingCap);
	}
	else
	{
		// == addObject's fallthrough when no sorting matches.
		copyOut(AsciiString("UNSORTED"), sortingOut, sortingCap);
	}
	if (isTestOut != NULL)
	{
		*isTestOut = (sorting == ES_TEST) ? 1 : 0;
	}
	return 1;
}

extern "C" int WBQtPickUnit_RenderPreview(const char *templateName, unsigned char *bgrOut, int cap)
{
	if (bgrOut == NULL || cap < WBQT_PREVIEW_W * WBQT_PREVIEW_H * 3)
	{
		return 0;
	}
	if (templateName == NULL || templateName[0] == 0 || TheThingFactory == NULL)
	{
		return 0;
	}
	const ThingTemplate *tt = TheThingFactory->findTemplate(AsciiString(templateName));
	const UnsignedByte *data = ObjectPreview::qtRenderTemplatePreview(tt);
	if (data == NULL)
	{
		return 0;
	}
	memcpy(bgrOut, data, WBQT_PREVIEW_W * WBQT_PREVIEW_H * 3);
	return 1;
}

//----------------------------------------------------------------------------------------
// "Replaced Missing Units" report.
//
// validateAllObjects records one row per distinct missing template name and what it became, so
// the user can review a Replace All pass and correct anything the name matcher guessed wrong.
// Rows key on the ORIGINAL missing name: the objects have already been re-pointed at the
// replacement by then, so re-applying an edit has to find them by the replacement they are
// currently carrying, which is what m_current tracks.
//----------------------------------------------------------------------------------------
struct WBQtReplaceRow
{
	AsciiString m_missing;		///< the template name the map asked for and the data set lacks
	AsciiString m_current;		///< what its objects carry now ("" == left unreplaced)
	Int m_objectCount;
};

static std::vector<WBQtReplaceRow> s_qtReplaceRows;
static int s_qtReplaceSource = WBQT_REPLACE_SOURCE_MAPOBJECTS;

// The name this row's objects/parameters actually carry right now. An empty m_current means the
// row was never replaced, so they still carry the original missing name.
static const AsciiString &wbRowActiveName(const WBQtReplaceRow &row)
{
	return row.m_current.isEmpty() ? row.m_missing : row.m_current;
}

extern "C" void WBQtReplaceReport_Begin(int source)
{
	s_qtReplaceRows.clear();
	s_qtReplaceSource = source;
}

extern "C" int WBQtReplaceReport_GetSource(void)
{
	return s_qtReplaceSource;
}

extern "C" void WBQtReplaceReport_Add(const char *missingName, const char *replacementName,
	int objectCount)
{
	if (missingName == NULL || missingName[0] == 0)
	{
		return;
	}
	WBQtReplaceRow row;
	row.m_missing = missingName;
	row.m_current = (replacementName != NULL) ? replacementName : "";
	row.m_objectCount = objectCount;
	s_qtReplaceRows.push_back(row);
}

extern "C" int WBQtReplaceReport_HasRows(void)
{
	return s_qtReplaceRows.empty() ? 0 : 1;
}

// Count the objects each row now accounts for. Called once the validate pass has re-pointed
// everything, since rows are recorded while the swap decisions are still being made.
extern "C" void WBQtReplaceReport_CountObjects(void)
{
	if (s_qtReplaceSource != WBQT_REPLACE_SOURCE_MAPOBJECTS)
	{
		return;		// script rows already carry their own hit counts
	}
	for (size_t r = 0; r < s_qtReplaceRows.size(); r++)
	{
		const AsciiString &wanted = wbRowActiveName(s_qtReplaceRows[r]);
		Int n = 0;
		for (MapObject *obj = MapObject::getFirstMapObject(); obj; obj = obj->getNext())
		{
			if (obj->getName() == wanted)
			{
				++n;
			}
		}
		s_qtReplaceRows[r].m_objectCount = n;
	}
}

extern "C" int WBQtReplaceReport_GetCount(void)
{
	return (int)s_qtReplaceRows.size();
}

extern "C" void WBQtReplaceReport_GetRow(int i, char *missingOut, int missingCap,
	char *replacementOut, int replacementCap, int *objectCountOut)
{
	if (i < 0 || i >= (int)s_qtReplaceRows.size())
	{
		copyOut("", missingOut, missingCap);
		copyOut("", replacementOut, replacementCap);
		if (objectCountOut != NULL)
		{
			*objectCountOut = 0;
		}
		return;
	}
	copyOut(s_qtReplaceRows[i].m_missing, missingOut, missingCap);
	copyOut(s_qtReplaceRows[i].m_current, replacementOut, replacementCap);
	if (objectCountOut != NULL)
	{
		*objectCountOut = s_qtReplaceRows[i].m_objectCount;
	}
}

// Every map object that row i's missing name currently resolves to. Matched by the name the
// objects carry NOW (the replacement), not the original missing name -- validateAllObjects has
// already re-pointed them by the time the report opens.
static void wbCollectRowObjects(int i, std::vector<MapObject *> &out)
{
	out.clear();
	if (i < 0 || i >= (int)s_qtReplaceRows.size()
		|| s_qtReplaceSource != WBQT_REPLACE_SOURCE_MAPOBJECTS)
	{
		return;		// script rows name template values in parameters, not placed objects
	}
	const AsciiString &wanted = wbRowActiveName(s_qtReplaceRows[i]);
	for (MapObject *obj = MapObject::getFirstMapObject(); obj; obj = obj->getNext())
	{
		if (obj->getName() == wanted)
		{
			out.push_back(obj);
		}
	}
}

extern "C" void WBQtReplaceReport_SetReplacement(int i, const char *replacementName)
{
	if (i < 0 || i >= (int)s_qtReplaceRows.size())
	{
		return;
	}
	AsciiString newName = (replacementName != NULL) ? replacementName : "";
	if (newName == s_qtReplaceRows[i].m_current)
	{
		return;
	}

	if (s_qtReplaceSource == WBQT_REPLACE_SOURCE_BUILDLIST)
	{
		// Build-list rows: rewrite the entries' template names, leaving each entry's position,
		// angle, rebuild count and flags untouched. Empty puts the original missing name back.
		const AsciiString from = wbRowActiveName(s_qtReplaceRows[i]);
		const AsciiString to = newName.isEmpty() ? s_qtReplaceRows[i].m_missing : newName;
		if (from != to)
		{
			WBQtBuildList_ReplaceBuildingName(from.str(), to.str());
		}
		s_qtReplaceRows[i].m_current = newName;
		return;
	}
	if (s_qtReplaceSource == WBQT_REPLACE_SOURCE_TEAMS)
	{
		// Team rows: rewrite the team templates' unit-type slots. An empty replacement puts the
		// original missing name back, exactly like the script rows.
		const AsciiString from = wbRowActiveName(s_qtReplaceRows[i]);
		const AsciiString to = newName.isEmpty() ? s_qtReplaceRows[i].m_missing : newName;
		if (from != to)
		{
			WBQtTeams_ReplaceUnitName(from.str(), to.str());
		}
		s_qtReplaceRows[i].m_current = newName;
		return;
	}
	if (s_qtReplaceSource == WBQT_REPLACE_SOURCE_SCRIPTS)
	{
		// Script rows: rewrite the OBJECT_TYPE parameter values instead of re-pointing objects.
		// Whole-value, case-sensitive -- the parameter holds exactly one template name. An empty
		// replacement puts the original missing name back.
		const AsciiString from = wbRowActiveName(s_qtReplaceRows[i]);
		const AsciiString to = newName.isEmpty() ? s_qtReplaceRows[i].m_missing : newName;
		if (from != to)
		{
			WBQtScript_ReplaceInParams(from.str(), to.str(), 1, 1, 1, -1);
		}
		s_qtReplaceRows[i].m_current = newName;
		return;
	}
	// An empty replacement means "leave it missing": put the original name back so the map still
	// records what it actually wanted, and the object shows up as missing again next validate.
	const ThingTemplate *tt = NULL;
	if (!newName.isEmpty())
	{
		tt = TheThingFactory->findTemplate(newName);
		if (tt == NULL)
		{
			return;		// unknown template: leave the row alone
		}
	}

	std::vector<MapObject *> objs;
	wbCollectRowObjects(i, objs);

	const AsciiString applied = newName.isEmpty() ? s_qtReplaceRows[i].m_missing : newName;
	for (size_t n = 0; n < objs.size(); n++)
	{
		objs[n]->setName(applied);
		objs[n]->setThingTemplate(tt);		// NULL when reverting to the missing name
	}
	s_qtReplaceRows[i].m_current = newName;

	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc != NULL)
	{
		pDoc->SetModifiedFlag(TRUE);
		pDoc->invalObject(NULL);		// rebuild render objects for the changed templates
	}
}

extern "C" void WBQtReplaceReport_SelectRow(int i)
{
	std::vector<MapObject *> objs;
	wbCollectRowObjects(i, objs);
	if (objs.empty())
	{
		return;
	}
	for (MapObject *obj = MapObject::getFirstMapObject(); obj; obj = obj->getNext())
	{
		obj->setSelected(false);
	}
	for (size_t n = 0; n < objs.size(); n++)
	{
		objs[n]->setSelected(true);
	}
	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (p3View != NULL)
	{
		const Coord3D *pos = objs[0]->getLocation();
		p3View->setCenterInView(pos->x / MAP_XY_FACTOR, pos->y / MAP_XY_FACTOR);
		p3View->Invalidate(false);
	}
}

extern "C" void WBQtPickUnit_SavePos(int top, int left)
{
	// == PickUnitDialog::OnMove: keep the shared PickUnitWindow profile position in sync so
	// BuildListTool's pick panel (still MFC) opens where the user last left a pick dialog.
	::AfxGetApp()->WriteProfileInt(BUILD_PICK_PANEL_SECTION, "Top", top);
	::AfxGetApp()->WriteProfileInt(BUILD_PICK_PANEL_SECTION, "Left", left);
}

#endif // RTS_HAS_QT
