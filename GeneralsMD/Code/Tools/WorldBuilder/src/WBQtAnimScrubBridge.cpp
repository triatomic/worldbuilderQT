// WBQtAnimScrubBridge.cpp -- MFC side of the Qt Animation Scrubber facade.
//
// Resolves the current selection and drives WbView3d's scrub state. Whole body behind RTS_HAS_QT;
// empty TU when Qt is OFF.

#include "StdAfx.h"

#ifdef RTS_HAS_QT

#include "resource.h"
#include "WorldBuilder.h"
#include "Lib/BaseType.h"
#include "Common/AsciiString.h"
#include "Common/ThingTemplate.h"
#include "WorldBuilderDoc.h"
#include "wbview3d.h"
#include "qt/panels/WBQtAnimScrubBridge.h"

#include <string.h>

namespace
{
	// The first selected object -- the scrubber is deliberately single-object (scrubbing a whole
	// selection at once is noise when you are inspecting one structure's motion).
	MapObject *findSelectedObject(void)
	{
		for (MapObject *obj = MapObject::getFirstMapObject(); obj; obj = obj->getNext())
		{
			if (obj->isSelected())
			{
				return obj;
			}
		}
		return NULL;
	}
}

extern "C" int WBQtAnimScrub_GetSelection(char *nameBuf, int nameBufSize, int *frameCount,
	int *moduleCount)
{
	if (nameBuf != NULL && nameBufSize > 0)
	{
		nameBuf[0] = 0;
	}
	if (frameCount != NULL)
	{
		*frameCount = 0;
	}
	if (moduleCount != NULL)
	{
		*moduleCount = 0;
	}

	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	MapObject *selected = findSelectedObject();
	if (p3View == NULL || selected == NULL)
	{
		return 0;
	}

	const ThingTemplate *tmpl = selected->getThingTemplate();
	if (nameBuf != NULL && nameBufSize > 0)
	{
		const char *name = tmpl ? tmpl->getName().str() : "(no template)";
		strncpy(nameBuf, name, nameBufSize - 1);
		nameBuf[nameBufSize - 1] = 0;
	}

	// PURE QUERY -- describing the selection must not arm it. Arming rebuilds the object, the
	// rebuild reaches the selection-changed hook, the hook calls back into the panel, and the
	// panel asks again: that recursed until the stack died. Scrubbing is armed only when the
	// user actually moves the slider (WBQtAnimScrub_SetFraction).
	Int modules = 0;
	const Int frames = p3View->getObjectAnimationInfo(selected, &modules);
	if (frameCount != NULL)
	{
		*frameCount = (int)frames;
	}
	if (moduleCount != NULL)
	{
		*moduleCount = (int)modules;
	}
	return 1;
}

extern "C" void WBQtAnimScrub_SetFraction(double fraction)
{
	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (p3View == NULL)
	{
		return;
	}
	MapObject *selected = findSelectedObject();
	if (selected == NULL)
	{
		return;
	}

	// Arming rebuilds the object, and a rebuild can run the selection-changed hook, which pushes a
	// refresh back into the panel. That is harmless as a one-shot but must not be able to re-enter
	// this call, so hold the door while the rebuild runs.
	static bool s_applying = false;
	if (s_applying)
	{
		return;
	}
	s_applying = true;
	// Moving an object that is ALREADY armed re-poses the render objects in place -- cheap enough to
	// run on every step of a drag. Only arming a different object needs the full rebuild, which is
	// why that is the fallback rather than the normal path.
	if (!p3View->repositionAnimationScrub(selected, (Real)fraction))
	{
		p3View->setAnimationScrub(selected, (Real)fraction);
	}
	s_applying = false;
}

extern "C" void WBQtAnimScrub_Clear(void)
{
	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (p3View != NULL)
	{
		p3View->setAnimationScrub(NULL, 0.0f);
	}
}

extern "C" double WBQtAnimScrub_GetFraction(void)
{
	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	return p3View ? (double)p3View->getAnimationScrubFraction() : 0.0;
}

#endif // RTS_HAS_QT
