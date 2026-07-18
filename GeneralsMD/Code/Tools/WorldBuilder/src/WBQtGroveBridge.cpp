// WBQtGroveBridge.cpp -- the MFC side of the Qt Grove-panel seam. See WBQtFenceBridge.cpp for
// the pattern. Plain MFC TU (no Qt include); reverse callbacks resolved against the exe at the
// final link. Whole body guarded by RTS_HAS_QT so the OFF build compiles it to an empty object.
//
// The MFC GroveOptions is still Create()d as the hidden OFF fallback and owns the grove-config
// dialog controls plus the object preview; GroveTool reads its choices through the TheGroveOptions
// getters (getNumTrees / getNumType / getTypeName / getTotalTreePerc / getCanPlace*), which pull
// straight out of those hidden controls. This bridge lets the Qt Grove panel DRIVE those controls
// (through GroveOptions::qt*, which reuse the same GetDlgItem paths + On* handlers) so the tool
// keeps working unchanged. Everything routes through TheGroveOptions (the single live instance).
#include "StdAfx.h"
#include "GroveOptions.h"
#include "qt/panels/WBQtGroveBridge.h"

#ifdef RTS_HAS_QT

extern "C" {

int WBQtGrove_GetTreeTypeCount(int type)
{
	return TheGroveOptions ? TheGroveOptions->qtGetTreeTypeCount(type) : 0;
}

int WBQtGrove_GetTreeTypeName(int type, int index, char *out, int cap)
{
	return TheGroveOptions ? TheGroveOptions->qtGetTreeTypeName(type, index, out, cap) : 0;
}

int WBQtGrove_GetTreeTypeSel(int type)
{
	return TheGroveOptions ? TheGroveOptions->qtGetTreeTypeSel(type) : -1;
}

void WBQtGrove_SetTreeTypeSel(int type, int index)
{
	if (TheGroveOptions)
	{
		TheGroveOptions->qtSetTreeTypeSel(type, index);
	}
}

int WBQtGrove_GetWeight(int type)
{
	return TheGroveOptions ? TheGroveOptions->qtGetWeight(type) : 0;
}

void WBQtGrove_SetWeight(int type, int value)
{
	if (TheGroveOptions)
	{
		TheGroveOptions->qtSetWeight(type, value);
	}
}

int WBQtGrove_GetTotalPerc(void)
{
	return TheGroveOptions ? TheGroveOptions->qtGetTotalPerc() : 0;
}

int WBQtGrove_GetNumTrees(void)
{
	return TheGroveOptions ? TheGroveOptions->qtGetNumTrees() : 0;
}

void WBQtGrove_SetNumTrees(int value)
{
	if (TheGroveOptions)
	{
		TheGroveOptions->qtSetNumTrees(value);
	}
}

int WBQtGrove_GetAllowWater(void)
{
	return TheGroveOptions ? TheGroveOptions->qtGetAllowWater() : 0;
}

void WBQtGrove_SetAllowWater(int on)
{
	if (TheGroveOptions)
	{
		TheGroveOptions->qtSetAllowWater(on);
	}
}

int WBQtGrove_GetAllowCliff(void)
{
	return TheGroveOptions ? TheGroveOptions->qtGetAllowCliff() : 0;
}

void WBQtGrove_SetAllowCliff(int on)
{
	if (TheGroveOptions)
	{
		TheGroveOptions->qtSetAllowCliff(on);
	}
}

int WBQtGrove_GetUsePropsOnly(void)
{
	return TheGroveOptions ? TheGroveOptions->qtGetUsePropsOnly() : 0;
}

void WBQtGrove_SetUsePropsOnly(int on)
{
	if (TheGroveOptions)
	{
		TheGroveOptions->qtSetUsePropsOnly(on);
	}
}

void WBQtGrove_RefreshSetNames(void)
{
	if (TheGroveOptions)
	{
		TheGroveOptions->qtRefreshSetNames();
	}
}

int WBQtGrove_GetSetCount(void)
{
	return TheGroveOptions ? TheGroveOptions->qtGetSetCount() : 0;
}

int WBQtGrove_GetSetName(int index, char *out, int cap)
{
	return TheGroveOptions ? TheGroveOptions->qtGetSetName(index, out, cap) : 0;
}

int WBQtGrove_GetCurrentSet(void)
{
	return TheGroveOptions ? TheGroveOptions->qtGetCurrentSet() : 0;
}

void WBQtGrove_SelectSet(int index)
{
	if (TheGroveOptions)
	{
		TheGroveOptions->qtSelectSet(index);
	}
}

void WBQtGrove_SaveSet(void)
{
	if (TheGroveOptions)
	{
		TheGroveOptions->qtSaveSet();
	}
}

void WBQtGrove_OpenSettings(void)
{
	if (TheGroveOptions)
	{
		TheGroveOptions->qtOpenSettings();
	}
}

int WBQtGrove_GetPreviewSize(int *widthOut, int *heightOut)
{
	return TheGroveOptions ? TheGroveOptions->qtGetPreviewSize(widthOut, heightOut) : 0;
}

int WBQtGrove_RenderPreview(unsigned char *bgrOut, int cap)
{
	return TheGroveOptions ? TheGroveOptions->qtRenderPreview(bgrOut, cap) : 0;
}

}
#endif
