// WBQtFenceBridge.cpp -- the MFC side of the Qt Fence-panel seam. See WBQtObjectBridge.cpp for
// the pattern. Plain MFC TU (no Qt include); reverse callbacks resolved against the exe at the
// final link. Whole body guarded by RTS_HAS_QT so the OFF build compiles it to an empty object.
//
// The MFC FenceOptions is still created as the hidden OFF fallback and owns m_objectsList (its
// per-map template list) plus the fence selection statics (m_currentObjectIndex / m_fenceSpacing
// / m_fenceOffset / m_customSpacing / m_showAllObjectTypes). FenceOptions is a *front* for
// ObjectOptions -- selecting a leaf runs updateObjectOptions(), which calls
// ObjectOptions::selectObject() and recomputes spacing/offset, so FenceTool (which reads
// FenceOptions::getFenceSpacing/getFenceOffset/hasSelectedObject and ObjectOptions::getCurGdfName)
// keeps working unchanged. This bridge lets the Qt Fence panel mirror the FILTERED list by index
// and drive those statics.
#define DEFINE_EDITOR_SORTING_NAMES		// instantiate EditorSortingNames[] in this TU

#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "FenceOptions.h"
#include "ObjectPreview.h"
#include "WorldBuilderDoc.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingSort.h"
#include "qt/panels/WBQtFenceBridge.h"

#ifdef RTS_HAS_QT

// The MFC preview is a fixed 128x128 BGR image (see PREVIEW_WIDTH/HEIGHT in ObjectPreview.cpp).
#define WBQT_PREVIEW_W 128
#define WBQT_PREVIEW_H 128

//----------------------------------------------------------------------------------------
// FenceOptions Qt-support statics (declared in FenceOptions.h; defined here so they can reach
// the private selection state / call updateObjectOptions() without churning FenceOptions.cpp).
//----------------------------------------------------------------------------------------
MapObject *FenceOptions::qtGetObjectListHead(void)
{
	return m_staticThis ? m_staticThis->m_objectsList : NULL;
}

int FenceOptions::qtGetShowAll(void)
{
	return (m_staticThis && m_staticThis->m_showAllObjectTypes) ? 1 : 0;
}

void FenceOptions::qtSetShowAll(int showAll)
{
	if (m_staticThis)
	{
		m_staticThis->m_showAllObjectTypes = (showAll != 0);
	}
}

void FenceOptions::qtSelectIndex(int filteredIndex)
{
	// Mirrors the MFC TVN_SELCHANGED path: a leaf sets the index + clears the custom-spacing
	// latch; a grouping node (filteredIndex < 0) clears the index. Either way updateObjectOptions
	// runs (it drives ObjectOptions::selectObject, recomputes spacing/offset, writes the spacing
	// edit box, and refreshes the MFC preview).
	if (m_staticThis == NULL)
	{
		return;
	}
	if (filteredIndex >= 0)
	{
		m_currentObjectIndex = filteredIndex;
		m_staticThis->m_customSpacing = false;
	}
	else
	{
		m_currentObjectIndex = -1;
	}
	m_staticThis->updateObjectOptions();
}

int FenceOptions::qtGetCurrentIndex(void)
{
	return m_currentObjectIndex;
}

double FenceOptions::qtGetSpacing(void)
{
	return (double)m_fenceSpacing;
}

void FenceOptions::qtSetSpacing(double spacing)
{
	// Mirrors OnChangeFenceSpacingEdit: store the spacing + latch custom spacing so a later
	// selection won't overwrite it from the template width. Also write the hidden MFC edit box
	// so the OFF fallback panel stays consistent.
	m_fenceSpacing = (Real)spacing;
	if (m_staticThis)
	{
		m_staticThis->m_customSpacing = true;
		CWnd *pWnd = m_staticThis->GetDlgItem(IDC_FENCE_SPACING_EDIT);
		if (pWnd)
		{
			CString s;
			s.Format("%f", m_fenceSpacing);
			pWnd->SetWindowText(s);
		}
	}
}

double FenceOptions::qtGetOffset(void)
{
	return (double)m_fenceOffset;
}

//----------------------------------------------------------------------------------------
// Helpers: walk the FILTERED (index-ordered) template list. The filter + running index mirror
// FenceOptions::addObject() / updateObjectOptions(): when "fence only" is on (showAll == 0)
// only templates with a non-zero fence width are counted, and the index is the position within
// that filtered list -- exactly the lParam the MFC tree stores.
//----------------------------------------------------------------------------------------
namespace
{
	MapObject *fenceObjectAtIndex(int filteredIndex)
	{
		int showAll = FenceOptions::qtGetShowAll();
		int count = 0;
		for (MapObject *pObj = FenceOptions::qtGetObjectListHead(); pObj != NULL; pObj = pObj->getNext())
		{
			const ThingTemplate *tt = pObj->getThingTemplate();
			if (!showAll && (tt == NULL || tt->getFenceWidth() == 0))
			{
				continue;
			}
			if (count == filteredIndex)
			{
				return pObj;
			}
			count++;
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

int WBQtFence_GetCount(void)
{
	int showAll = FenceOptions::qtGetShowAll();
	int count = 0;
	for (MapObject *pObj = FenceOptions::qtGetObjectListHead(); pObj != NULL; pObj = pObj->getNext())
	{
		const ThingTemplate *tt = pObj->getThingTemplate();
		if (!showAll && (tt == NULL || tt->getFenceWidth() == 0))
		{
			continue;
		}
		count++;
	}
	return count;
}

int WBQtFence_GetEntry(int filteredIndex, char *sideOut, char *sortingOut, char *leafOut, int cap)
{
	MapObject *pObj = fenceObjectAtIndex(filteredIndex);
	if (pObj == NULL)
	{
		return 0;
	}

	const ThingTemplate *tt = pObj->getThingTemplate();
	if (tt != NULL)
	{
		// side / editor-sorting category / leaf, exactly like FenceOptions::addObject().
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
		// FenceOptions::addObject only emits a leaf when there IS a template; a template-less
		// entry produces no tree node. Bucket it defensively so the index stays aligned.
		copyString(sideOut, cap, "UNSORTED");
		copyString(sortingOut, cap, "");
		copyString(leafOut, cap, pObj->getName().str());
	}
	return 1;
}

int WBQtFence_GetFullName(int filteredIndex, char *nameOut, int cap)
{
	MapObject *pObj = fenceObjectAtIndex(filteredIndex);
	if (pObj == NULL)
	{
		return 0;
	}
	copyString(nameOut, cap, pObj->getName().str());
	return 1;
}

void WBQtFence_SelectIndex(int filteredIndex)
{
	FenceOptions::qtSelectIndex(filteredIndex);
}

int WBQtFence_GetSelectedIndex(void)
{
	return FenceOptions::qtGetCurrentIndex();
}

int WBQtFence_GetShowAll(void)
{
	return FenceOptions::qtGetShowAll();
}

void WBQtFence_SetShowAll(int showAll)
{
	FenceOptions::qtSetShowAll(showAll);
}

double WBQtFence_GetSpacing(void)
{
	return FenceOptions::qtGetSpacing();
}

void WBQtFence_SetSpacing(double spacing)
{
	FenceOptions::qtSetSpacing(spacing);
}

double WBQtFence_GetOffset(void)
{
	return FenceOptions::qtGetOffset();
}

//----------------------------------------------------------------------------------------
// Preview: reuse the exact MFC render path (ObjectPreview::qtRenderTemplatePreview ->
// generatePreview) and hand the BGR bytes to the Qt panel.
//----------------------------------------------------------------------------------------
int WBQtFence_GetPreviewSize(int *widthOut, int *heightOut)
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

int WBQtFence_RenderPreview(unsigned char *bgrOut, int cap)
{
	if (bgrOut == NULL || cap < WBQT_PREVIEW_W * WBQT_PREVIEW_H * 3)
	{
		return 0;
	}
	MapObject *pCur = fenceObjectAtIndex(FenceOptions::qtGetCurrentIndex());
	const ThingTemplate *tt = (pCur != NULL) ? pCur->getThingTemplate() : NULL;
	const UnsignedByte *data = ObjectPreview::qtRenderTemplatePreview(tt);
	if (data == NULL)
	{
		return 0;
	}
	memcpy(bgrOut, data, WBQT_PREVIEW_W * WBQT_PREVIEW_H * 3);
	return 1;
}

}
#endif
