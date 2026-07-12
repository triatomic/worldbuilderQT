// WBQtObjectBridge.cpp -- the MFC side of the Qt Object-panel seam. See WBQtBrushBridge.cpp
// for the pattern. Plain MFC TU (no Qt include); reverse callbacks resolved against the exe
// at the final link. Whole body guarded by RTS_HAS_QT so the OFF build compiles it to an
// empty object.
//
// The MFC ObjectOptions is still created as the hidden OFF fallback and owns m_objectsList
// (the full template list) plus the selection statics the placement tools read
// (m_currentObjectIndex / m_currentObjectName / m_curOwnerName). This bridge lets the Qt
// Object panel mirror that list by index and drive those statics, so ObjectTool / FenceTool /
// GroveTool / BuildListTool keep working unchanged.
#define DEFINE_EDITOR_SORTING_NAMES		// instantiate EditorSortingNames[] in this TU

#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "ObjectOptions.h"
#include "ObjectPreview.h"
#include "WorldBuilderDoc.h"
#include "Common/WellKnownKeys.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingSort.h"
#include "Common/PlayerTemplate.h"
#include "GameLogic/SidesList.h"
#include "qt/WBQtPanelBridge.h"

#ifdef RTS_HAS_QT

// The MFC preview is a fixed 128x128 BGR image (see PREVIEW_WIDTH/HEIGHT in ObjectPreview.cpp).
#define WBQT_PREVIEW_W 256	// the 2x Qt render (see ObjectPreview::qtRenderTemplatePreview)
#define WBQT_PREVIEW_H 256

//----------------------------------------------------------------------------------------
// ObjectOptions Qt-support statics (declared in ObjectOptions.h; defined here so they can
// reach the private selection state without churning ObjectOptions.cpp).
//----------------------------------------------------------------------------------------
MapObject *ObjectOptions::qtGetObjectListHead(void)
{
	return m_staticThis ? m_staticThis->m_objectsList : NULL;
}

void ObjectOptions::qtSetCurrentSelection(int listIndex, const char *name)
{
	m_currentObjectIndex = listIndex;
	if (name != NULL)
	{
		strncpy(m_currentObjectName, name, NAME_MAX_LEN - 1);
		m_currentObjectName[NAME_MAX_LEN - 1] = 0;
	}
}

int ObjectOptions::qtGetCurrentIndex(void)
{
	return m_currentObjectIndex;
}

void ObjectOptions::qtSetOwnerTeamName(const char *teamName)
{
	if (teamName != NULL)
	{
		m_curOwnerName = teamName;
	}
	else
	{
		m_curOwnerName.clear();
	}
}

const char *ObjectOptions::qtGetOwnerTeamName(void)
{
	return m_curOwnerName.str();
}

CWnd *ObjectOptions::qtGetMainWnd(void)
{
	return m_staticThis;	// ObjectOptions IS a CWnd; the placement code reads its edit box
}

//----------------------------------------------------------------------------------------
// Helpers: walk the (index-ordered) template list and derive the tree path for an entry,
// mirroring ObjectOptions::addObject().
//----------------------------------------------------------------------------------------
namespace
{
	MapObject *objectAtIndex(int listIndex)
	{
		MapObject *pObj = ObjectOptions::qtGetObjectListHead();
		int count = 0;
		while (pObj != NULL)
		{
			if (count == listIndex)
			{
				return pObj;
			}
			count++;
			pObj = pObj->getNext();
		}
		return NULL;
	}

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

int WBQtObject_GetCount(void)
{
	int count = 0;
	for (MapObject *pObj = ObjectOptions::qtGetObjectListHead(); pObj != NULL; pObj = pObj->getNext())
	{
		count++;
	}
	return count;
}

int WBQtObject_GetEntry(int listIndex, char *sideOut, char *sortingOut, char *leafOut, int cap)
{
	MapObject *pObj = objectAtIndex(listIndex);
	if (pObj == NULL)
	{
		return 0;
	}

	const ThingTemplate *tt = pObj->getThingTemplate();
	if (tt != NULL)
	{
		// side / editor-sorting category / leaf, exactly like addObject().
		copyString(sideOut, cap, tt->getDefaultOwningSide().str());

		EditorSortingType es = tt->getEditorSorting();
		if (es == ES_TEST)
		{
			copyString(sortingOut, cap, "TEST");
		}
		else if (es >= ES_FIRST && es < ES_NUM_SORTING_TYPES)
		{
			copyString(sortingOut, cap, EditorSortingNames[es]);
		}
		else
		{
			copyString(sortingOut, cap, "UNSORTED");
		}

		copyString(leafOut, cap, tt->getName().str());
	}
	else
	{
		// Legacy / test-model entries go under a single bucket, leaf = last path element.
		copyString(sideOut, cap, "**TEST MODELS");
		copyString(sortingOut, cap, "");
		const char *full = pObj->getName().str();
		const char *leaf = full;
		for (const char *p = full; *p; ++p)
		{
			if (*p == '/')
			{
				leaf = p + 1;
			}
		}
		copyString(leafOut, cap, leaf);
	}
	return 1;
}

int WBQtObject_GetFullName(int listIndex, char *nameOut, int cap)
{
	MapObject *pObj = objectAtIndex(listIndex);
	if (pObj == NULL)
	{
		return 0;
	}
	copyString(nameOut, cap, pObj->getName().str());
	return 1;
}

void WBQtObject_SelectIndex(int listIndex)
{
	MapObject *pObj = objectAtIndex(listIndex);
	if (pObj != NULL)
	{
		ObjectOptions::qtSetCurrentSelection(listIndex, pObj->getName().str());
	}
}

int WBQtObject_GetSelectedIndex(void)
{
	return ObjectOptions::qtGetCurrentIndex();
}

//----------------------------------------------------------------------------------------
// Owning-team combo. The team list + neutral relabel + default-for-current mirror the MFC
// ObjectOptions::updateLabel(); SetTeam mirrors OnEditchangeOwningteam.
//----------------------------------------------------------------------------------------
static int findSideListEntryWithPlayerOfSide(AsciiString side)
{
	for (int i = 0; i < TheSidesList->getNumSides(); i++)
	{
		AsciiString ptname = TheSidesList->getSideInfo(i)->getDict()->getAsciiString(TheKey_playerFaction);
		const PlayerTemplate *pt = ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY(ptname));
		if (pt && pt->getSide() == side)
		{
			return i;
		}
	}
	return -1;
}

int WBQtObject_GetTeamCount(void)
{
	return TheSidesList->getNumTeams();
}

int WBQtObject_GetTeamName(int teamIndex, char *nameOut, int cap)
{
	if (teamIndex < 0 || teamIndex >= TheSidesList->getNumTeams())
	{
		return 0;
	}
	Dict *d = TheSidesList->getTeamInfo(teamIndex)->getDict();
	AsciiString name = d->getAsciiString(TheKey_teamName);
	if (name == "team")
	{
		name = "(neutral)";
	}
	copyString(nameOut, cap, name.str());
	return 1;
}

int WBQtObject_GetDefaultTeamForCurrent(void)
{
	MapObject *pCur = objectAtIndex(ObjectOptions::qtGetCurrentIndex());
	AsciiString defTeamName;
	if (pCur != NULL)
	{
		const ThingTemplate *tt = pCur->getThingTemplate();
		if (tt != NULL)
		{
			Int i = findSideListEntryWithPlayerOfSide(tt->getDefaultOwningSide());
			if (i >= 0)
			{
				defTeamName.set("team");
				defTeamName.concat(TheSidesList->getSideInfo(i)->getDict()->getAsciiString(TheKey_playerName));
			}
		}
		else
		{
			defTeamName.set("team");	// neutral
		}
	}

	int neutral = -1;
	for (int i = 0; i < TheSidesList->getNumTeams(); i++)
	{
		AsciiString name = TheSidesList->getTeamInfo(i)->getDict()->getAsciiString(TheKey_teamName);
		if (name == defTeamName)
		{
			return i;
		}
		if (name == "team")
		{
			neutral = i;
		}
	}
	return neutral;
}

void WBQtObject_SetTeam(int teamIndex)
{
	if (teamIndex < 0 || teamIndex >= TheSidesList->getNumTeams())
	{
		ObjectOptions::qtSetOwnerTeamName(NULL);
		return;
	}
	Dict *d = TheSidesList->getTeamInfo(teamIndex)->getDict();
	ObjectOptions::qtSetOwnerTeamName(d->getAsciiString(TheKey_teamName).str());
}

//----------------------------------------------------------------------------------------
// Placement height. getCurObjectHeight() reads the MFC edit box (IDC_OBJECT_HEIGHT_EDIT),
// so keep writing that so the value the tools read stays correct.
//----------------------------------------------------------------------------------------
void WBQtObject_SetHeight(int height)
{
	if (ObjectOptions::qtGetMainWnd() == NULL)
	{
		return;
	}
	CWnd *pWnd = ObjectOptions::qtGetMainWnd()->GetDlgItem(IDC_OBJECT_HEIGHT_EDIT);
	if (pWnd != NULL)
	{
		CString s;
		s.Format("%d", height);
		pWnd->SetWindowText(s);
	}
}

int WBQtObject_GetHeight(void)
{
	// getCurObjectHeight() returns feet; the panel edits the raw integer, so read the box.
	if (ObjectOptions::qtGetMainWnd() == NULL)
	{
		return 0;
	}
	CWnd *pWnd = ObjectOptions::qtGetMainWnd()->GetDlgItem(IDC_OBJECT_HEIGHT_EDIT);
	if (pWnd != NULL)
	{
		CString val;
		pWnd->GetWindowText(val);
		return atoi(val);
	}
	return 0;
}

//----------------------------------------------------------------------------------------
// Preview: reuse the exact MFC render path (ObjectPreview::qtRenderTemplatePreview ->
// generatePreview) and hand the BGR bytes to the Qt panel.
//----------------------------------------------------------------------------------------
int WBQtObject_GetPreviewSize(int *widthOut, int *heightOut)
{
	if (widthOut != NULL)
	{
		*widthOut = WBQT_PREVIEW_W;
	}
	if (heightOut != NULL)
	{
		*heightOut = WBQT_PREVIEW_H;
	}
	return 1;
}

int WBQtObject_RenderPreview(unsigned char *bgrOut, int cap)
{
	if (bgrOut == NULL || cap < WBQT_PREVIEW_W * WBQT_PREVIEW_H * 3)
	{
		return 0;
	}
	MapObject *pCur = objectAtIndex(ObjectOptions::qtGetCurrentIndex());
	const ThingTemplate *tt = (pCur != NULL) ? pCur->getThingTemplate() : NULL;
	const UnsignedByte *data = ObjectPreview::qtRenderTemplatePreview(tt);
	if (data == NULL)
	{
		return 0;
	}
	memcpy(bgrOut, data, WBQT_PREVIEW_W * WBQT_PREVIEW_H * 3);
	return 1;
}

//----------------------------------------------------------------------------------------
// Preview toggles, persisted in the registry under the same section/keys the MFC panel uses.
//----------------------------------------------------------------------------------------
void WBQtObject_SetPreviewSound(int on)
{
	::AfxGetApp()->WriteProfileInt("ObjectOptionPanel", "PreviewSound", on ? 1 : 0);
}
int WBQtObject_GetPreviewSound(void)
{
	return ::AfxGetApp()->GetProfileInt("ObjectOptionPanel", "PreviewSound", 1);
}
void WBQtObject_SetPreviewBuildZone(int on)
{
	::AfxGetApp()->WriteProfileInt("ObjectOptionPanel", "PreviewBuildZone", on ? 1 : 0);
}
int WBQtObject_GetPreviewBuildZone(void)
{
	return ::AfxGetApp()->GetProfileInt("ObjectOptionPanel", "PreviewBuildZone", 1);
}
void WBQtObject_SetUseWaterHeight(int on)
{
	::AfxGetApp()->WriteProfileInt("ObjectOptionPanel", "UseWaterHeight", on ? 1 : 0);
}
int WBQtObject_GetUseWaterHeight(void)
{
	return ::AfxGetApp()->GetProfileInt("ObjectOptionPanel", "UseWaterHeight", 1);
}

// Place-all-in-category: the checkbox drives the ObjectOptions static that ObjectTool
// reads on mouse-up; the setter persists it (same profile section as the other toggles).
void WBQtObject_SetPlaceAll(int on)
{
	ObjectOptions::setPlaceAllInCategory(on != 0);
}
int WBQtObject_GetPlaceAll(void)
{
	return ObjectOptions::isPlaceAllInCategory() ? 1 : 0;
}

}
#endif
