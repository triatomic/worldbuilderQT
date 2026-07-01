// WBQtRulerBridge.cpp -- the MFC side of the Qt Ruler-panel seam. See WBQtBrushBridge.cpp
// for the pattern. Plain MFC TU (no Qt include); reverse callbacks -> RulerTool statics,
// resolved against the exe at the final link. Whole body guarded by RTS_HAS_QT so the OFF
// build compiles it to an empty object. Lengths cross as doubles in feet; RulerTool keeps
// its length as a Real, so the double<->Real narrowing happens here at the seam.
#include "StdAfx.h"
#include "RulerTool.h"
#include "WorldBuilderDoc.h"
#include "qt/WBQtPanelBridge.h"

#ifdef RTS_HAS_QT
extern "C" {

void WBQtRuler_SetLengthFeet(double radiusFeet)  { RulerTool::setLength((Real)radiusFeet); }
double WBQtRuler_GetLengthFeet(void)             { return (double)RulerTool::getLength(); }
int WBQtRuler_SwitchType(void)                   { return RulerTool::switchType() ? 1 : 0; }
int WBQtRuler_GetType(void)                      { return RulerTool::getType(); }
void WBQtRuler_SetUseMeters(int on)              { RulerTool::setUseMeters(on != 0); }
int WBQtRuler_GetUseMeters(void)                 { return RulerTool::getUseMeters() ? 1 : 0; }
double WBQtRuler_ToDisplayUnits(double feet)     { return (double)RulerTool::toDisplayUnits((Real)feet); }
void WBQtRuler_SetShowGrid(int on)               { RulerTool::setShowGridOnActivate(on != 0); }
int WBQtRuler_GetShowGrid(void)                  { return RulerTool::getShowGridOnActivate() ? 1 : 0; }

void WBQtRuler_RepaintViews(void)
{
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc)
	{
		pDoc->updateAllViews();
	}
}

}
#endif
