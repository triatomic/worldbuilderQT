// WBQtWaveBridge.cpp -- MFC side of the Qt Wave Editor facade.
//
// These extern "C" reverse callbacks (declared in qt/panels/WBQtWaveBridge.h) forward to the
// WaveEditorTool / DrawObject statics -- the same calls the MFC WaveEditorOptions handlers make.
// The whole body is behind RTS_HAS_QT; with Qt OFF this TU is empty, like the other bridges.

#include "StdAfx.h"

#ifdef RTS_HAS_QT

#include "resource.h"
#include "Lib/BaseType.h"
#include "WaveEditorTool.h"
#include "WorldBuilderDoc.h"
#include "DrawObject.h"
#include "wbview3d.h"
#include "qt/panels/WBQtWaveBridge.h"

namespace
{
	void qtWaveCopyStr(char *out, int cap, const char *src)
	{
		if (out == NULL || cap <= 0) { return; }
		if (src == NULL) { out[0] = 0; return; }
		strncpy(out, src, cap - 1);
		out[cap - 1] = 0;
	}
}

extern "C" int WBQtWave_GetTypeName(char *out, int cap)
{
	qtWaveCopyStr(out, cap, WaveEditorTool::getWaveTypeName());
	return 1;
}

extern "C" void WBQtWave_CycleType(void)
{
	WaveEditorTool::cycleWaveType();
}

extern "C" int WBQtWave_GetTypeCount(void)
{
	return WaveEditorTool::getWaveTypeCount();
}

extern "C" int WBQtWave_GetTypeNameAt(int i, char *out, int cap)
{
	if (i < 0 || i >= WaveEditorTool::getWaveTypeCount())
	{
		return 0;
	}
	qtWaveCopyStr(out, cap, WaveEditorTool::getWaveTypeNameAt(i));
	return 1;
}

extern "C" void WBQtWave_SetSelectedWavesType(int typeIndex)
{
	WaveEditorTool::setSelectedWavesType(typeIndex);
}

extern "C" void WBQtWave_Undo(void)
{
	WaveEditorTool::undoLast();
}

extern "C" void WBQtWave_Save(void)
{
	WaveEditorTool::saveTracks(CWorldBuilderDoc::GetActiveDoc());
}

extern "C" void WBQtWave_Reload(void)
{
	WaveEditorTool::loadTracks(CWorldBuilderDoc::GetActiveDoc(), true /*announce*/);
}

extern "C" void WBQtWave_DeleteSelected(void)
{
	WaveEditorTool::deleteSelectedWave();
}

extern "C" void WBQtWave_DeleteAll(void)
{
	WaveEditorTool::deleteAllWaves();
}

extern "C" int WBQtWave_GetWaveCount(void)
{
	return WaveEditorTool::getWaveCount();
}

extern "C" int WBQtWave_GetWaveRow(int i, float *sx, float *sy, float *ex, float *ey,
	char *typeOut, int cap)
{
	float startX = 0.0f, startY = 0.0f, endX = 0.0f, endY = 0.0f;
	const char *typeName = "";
	if (!WaveEditorTool::getWaveRow(i, startX, startY, endX, endY, typeName))
	{
		return 0;
	}
	if (sx != NULL) { *sx = startX; }
	if (sy != NULL) { *sy = startY; }
	if (ex != NULL) { *ex = endX; }
	if (ey != NULL) { *ey = endY; }
	qtWaveCopyStr(typeOut, cap, typeName);
	return 1;
}

extern "C" int WBQtWave_IsWaveSelected(int i)
{
	return WaveEditorTool::isWaveSelected(i) ? 1 : 0;
}

extern "C" int WBQtWave_GetSelectedWave(void)
{
	return WaveEditorTool::getSelectedWave();
}

extern "C" int WBQtWave_GetSelectionCount(void)
{
	return WaveEditorTool::getSelectionCount();
}

extern "C" void WBQtWave_BeginListSelection(void)
{
	WaveEditorTool::beginListSelection();
}

extern "C" void WBQtWave_AddListSelection(int row)
{
	WaveEditorTool::addListSelection(row);
}

extern "C" void WBQtWave_EndListSelection(int anchorRow)
{
	WaveEditorTool::endListSelection(anchorRow);
}

extern "C" void WBQtWave_ClearSelection(void)
{
	WaveEditorTool::selectWaveNoCenter(-1);
}

extern "C" int WBQtWave_GetEditorMode(void)
{
	return (int)WaveEditorTool::getEditorMode();
}

extern "C" void WBQtWave_SetEditorMode(int mode)
{
	WaveEditorTool::setEditorMode((WaveEditorTool::EditorMode)mode);
}

extern "C" int WBQtWave_GetBrushSize(void)
{
	return WaveEditorTool::getBucketBrushSize();
}

extern "C" void WBQtWave_SetBrushSize(int worldUnits)
{
	WaveEditorTool::setBucketBrushSize(worldUnits);
}

extern "C" int WBQtWave_GetShowWaveLines(void)
{
	return DrawObject::getDoWaveFeedback() ? 1 : 0;
}

extern "C" void WBQtWave_SetShowWaveLines(int on)
{
	// Route through the 3D view so the View menu, the registry and the DrawObject overlay all
	// stay in sync (mirrors WaveEditorOptions::OnShowWaveLines).
	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (p3View != NULL)
	{
		p3View->setShowWaveLines(on != 0);
	}
	else
	{
		DrawObject::setDoWaveFeedback(on != 0);
	}
}

extern "C" int WBQtWave_GetShowShoreline(void)
{
	return DrawObject::getShowShoreline() ? 1 : 0;
}

extern "C" void WBQtWave_SetShowShoreline(int on)
{
	DrawObject::setShowShoreline(on != 0);
	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (p3View != NULL)
	{
		p3View->Invalidate();
	}
}

extern "C" void WBQtWave_InvalidateView(void)
{
	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (p3View != NULL)
	{
		p3View->Invalidate();
	}
}

#endif // RTS_HAS_QT
