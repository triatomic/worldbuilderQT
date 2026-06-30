// WBQtFeatherBridge.cpp -- the MFC side of the Qt Feather-panel seam.
//
// Plain MFC TU (no Qt include). Implements the reverse callbacks the Qt Feather panel
// (qt/panels/WBQtFeatherPanel.cpp) fires when its controls change, forwarding to the
// static FeatherTool API, and the getters the panel reads to seed itself. The Qt static
// lib resolves these against the exe at the final link (extern "C" keeps the names stable).
//
// Whole body is guarded by RTS_HAS_QT, so the default (Qt-off) build compiles this to an
// empty object and the MFC build is unchanged.
#include "StdAfx.h"
#include "FeatherTool.h"
#include "qt/WBQtPanelBridge.h"

#ifdef RTS_HAS_QT
extern "C" {

void WBQtFeather_SetFeather(int v)    { FeatherTool::setFeather(v); }
void WBQtFeather_SetRadius(int v)     { FeatherTool::setRadius(v); }
void WBQtFeather_SetRate(int v)       { FeatherTool::setRate(v); }
void WBQtFeather_ToggleMirror(void)   { FeatherTool::toggleMirror(); }
void WBQtFeather_ToggleMirrorX(void)  { FeatherTool::toggleMirrorX(); }
void WBQtFeather_ToggleMirrorY(void)  { FeatherTool::toggleMirrorY(); }
void WBQtFeather_ToggleMirrorXY(void) { FeatherTool::toggleMirrorXY(); }

int WBQtFeather_GetFeather(void)  { return FeatherTool::getFeather(); }
int WBQtFeather_GetRadius(void)   { return FeatherTool::getRadius(); }
int WBQtFeather_GetRate(void)     { return FeatherTool::getRate(); }
int WBQtFeather_GetMirror(void)   { return FeatherTool::getEnableMirror() ? 1 : 0; }
int WBQtFeather_GetMirrorX(void)  { return FeatherTool::getMirrorX() ? 1 : 0; }
int WBQtFeather_GetMirrorY(void)  { return FeatherTool::getMirrorY() ? 1 : 0; }
int WBQtFeather_GetMirrorXY(void) { return FeatherTool::getMirrorXY() ? 1 : 0; }

}
#endif
