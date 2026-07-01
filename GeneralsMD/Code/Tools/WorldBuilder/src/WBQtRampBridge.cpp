// WBQtRampBridge.cpp -- the MFC side of the Qt Ramp-panel seam. See WBQtBrushBridge.cpp for
// the pattern. Plain MFC TU (no Qt include); reverse callbacks resolved against the exe at
// the final link. Whole body guarded by RTS_HAS_QT so the OFF build compiles it to an empty
// object. Ramp is unusual: width + the "apply" latch live on the MFC RampOptions object
// (TheRampOptions), not on the tool, so those calls drive that object; mirror state is on
// RampTool statics like the other tools.
#include "StdAfx.h"
#include "RampOptions.h"
#include "RampTool.h"
#include "qt/WBQtPanelBridge.h"

#ifdef RTS_HAS_QT
extern "C" {

void WBQtRamp_SetWidth(double width)
{
	if (TheRampOptions != NULL)
	{
		TheRampOptions->setRampWidthExternal((Real)width);
	}
}

double WBQtRamp_GetWidth(void)
{
	if (TheRampOptions != NULL)
	{
		return (double)TheRampOptions->getRampWidth();
	}
	return 0.0;
}

void WBQtRamp_Apply(void)
{
	if (TheRampOptions != NULL)
	{
		TheRampOptions->requestApply();
	}
}

void WBQtRamp_ToggleMirror(void)     { RampTool::toggleMirror(); }
void WBQtRamp_ToggleMirrorX(void)    { RampTool::toggleMirrorX(); }
void WBQtRamp_ToggleMirrorY(void)    { RampTool::toggleMirrorY(); }
void WBQtRamp_ToggleMirrorXY(void)   { RampTool::toggleMirrorXY(); }

int WBQtRamp_GetMirror(void)   { return RampTool::getEnableMirror() ? 1 : 0; }
int WBQtRamp_GetMirrorX(void)  { return RampTool::getMirrorX() ? 1 : 0; }
int WBQtRamp_GetMirrorY(void)  { return RampTool::getMirrorY() ? 1 : 0; }
int WBQtRamp_GetMirrorXY(void) { return RampTool::getMirrorXY() ? 1 : 0; }

}
#endif
