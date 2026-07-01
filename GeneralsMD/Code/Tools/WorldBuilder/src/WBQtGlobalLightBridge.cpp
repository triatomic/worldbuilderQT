// WBQtGlobalLightBridge.cpp -- MFC side of the Qt Global Light Options facade.
//
// Forwards the extern "C" reverse callbacks (qt/panels/WBQtGlobalLightBridge.h) to the
// GlobalLightOptions::qt* statics. Whole body behind RTS_HAS_QT; empty TU when Qt is OFF.

#include "StdAfx.h"

#ifdef RTS_HAS_QT

#include "Lib/BaseType.h"
#include "GlobalLightOptions.h"
#include "qt/panels/WBQtGlobalLightBridge.h"

extern "C" int WBQtGlobalLight_GetLighting(void)
{
	return GlobalLightOptions::qtGetLighting();
}

extern "C" void WBQtGlobalLight_SetLighting(int mode)
{
	GlobalLightOptions::qtSetLighting(mode);
}

extern "C" void WBQtGlobalLight_GetAngles(int light, int *azimuth, int *elevation)
{
	GlobalLightOptions::qtGetAngles(light, azimuth, elevation);
}

extern "C" void WBQtGlobalLight_SetAngles(int light, int azimuth, int elevation)
{
	GlobalLightOptions::qtSetAngles(light, azimuth, elevation);
}

extern "C" void WBQtGlobalLight_GetAmbient(int *r, int *g, int *b)
{
	GlobalLightOptions::qtGetAmbient(r, g, b);
}

extern "C" void WBQtGlobalLight_SetAmbient(int r, int g, int b)
{
	GlobalLightOptions::qtSetAmbient(r, g, b);
}

extern "C" void WBQtGlobalLight_GetDiffuse(int light, int *r, int *g, int *b)
{
	GlobalLightOptions::qtGetDiffuse(light, r, g, b);
}

extern "C" void WBQtGlobalLight_SetDiffuse(int light, int r, int g, int b)
{
	GlobalLightOptions::qtSetDiffuse(light, r, g, b);
}

extern "C" void WBQtGlobalLight_GetLightPos(int light, float *x, float *y, float *z)
{
	GlobalLightOptions::qtGetLightPos(light, x, y, z);
}

extern "C" void WBQtGlobalLight_GetTimeOfDayName(char *out, int cap)
{
	GlobalLightOptions::qtGetTimeOfDayName(out, cap);
}

extern "C" void WBQtGlobalLight_ResetLights(void)
{
	GlobalLightOptions::qtResetLights();
}

extern "C" void WBQtGlobalLight_SyncFromGlobals(void)
{
	GlobalLightOptions::qtSyncFromGlobals();
}

extern "C" void WBQtGlobalLight_FeedbackOff(void)
{
	GlobalLightOptions::qtFeedbackOff();
}

#endif // RTS_HAS_QT
