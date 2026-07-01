// WBQtCameraBridge.cpp -- MFC side of the Qt Camera Options facade.
//
// Forwards the extern "C" reverse callbacks (qt/panels/WBQtCameraBridge.h) to the
// CameraOptions::qt* statics. Whole body behind RTS_HAS_QT; empty TU when Qt is OFF.

#include "StdAfx.h"

#ifdef RTS_HAS_QT

#include "resource.h"
#include "Lib/BaseType.h"
#include "Common/AsciiString.h"
#include "CameraOptions.h"
#include "qt/panels/WBQtCameraBridge.h"

extern "C" double WBQtCamera_GetPitch(void)
{
	return CameraOptions::qtGetPitch();
}

extern "C" void WBQtCamera_SetPitch(double pitch)
{
	CameraOptions::qtSetPitch(pitch);
}

extern "C" void WBQtCamera_Reset(void)
{
	CameraOptions::qtResetCamera();
}

extern "C" void WBQtCamera_DropWaypoint(void)
{
	CameraOptions::qtDropWaypoint();
}

extern "C" void WBQtCamera_CenterOnSelected(void)
{
	CameraOptions::qtCenterOnSelected();
}

extern "C" int WBQtCamera_GetInfo(float *height, float *zoom, float *posX, float *posY,
	float *tgtX, float *tgtY)
{
	return CameraOptions::qtGetInfo(height, zoom, posX, posY, tgtX, tgtY);
}

#endif // RTS_HAS_QT
