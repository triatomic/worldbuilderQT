// WBQtBrushBridge.cpp -- the MFC side of the Qt Brush-panel seam. See WBQtFeatherBridge.cpp
// for the pattern. Plain MFC TU (no Qt include); reverse callbacks -> BrushTool statics,
// resolved against the exe at the final link. Whole body guarded by RTS_HAS_QT so the OFF
// build compiles it to an empty object.
#include "StdAfx.h"
#include "BrushTool.h"
#include "qt/WBQtPanelBridge.h"

#ifdef RTS_HAS_QT
extern "C" {

void WBQtBrush_SetWidth(int v)        { BrushTool::setWidth(v); }
void WBQtBrush_SetFeather(int v)      { BrushTool::setFeather(v); }
void WBQtBrush_SetHeight(int v)       { BrushTool::setHeight(v); }
void WBQtBrush_ToggleMirror(void)     { BrushTool::toggleMirror(); }
void WBQtBrush_ToggleMirrorX(void)    { BrushTool::toggleMirrorX(); }
void WBQtBrush_ToggleMirrorY(void)    { BrushTool::toggleMirrorY(); }
void WBQtBrush_ToggleMirrorXY(void)   { BrushTool::toggleMirrorXY(); }

int WBQtBrush_GetWidth(void)    { return BrushTool::getWidth(); }
int WBQtBrush_GetFeather(void)  { return BrushTool::getFeather(); }
int WBQtBrush_GetHeight(void)   { return BrushTool::getHeight(); }
int WBQtBrush_GetMirror(void)   { return BrushTool::getEnableMirror() ? 1 : 0; }
int WBQtBrush_GetMirrorX(void)  { return BrushTool::getMirrorX() ? 1 : 0; }
int WBQtBrush_GetMirrorY(void)  { return BrushTool::getMirrorY() ? 1 : 0; }
int WBQtBrush_GetMirrorXY(void) { return BrushTool::getMirrorXY() ? 1 : 0; }

}
#endif
