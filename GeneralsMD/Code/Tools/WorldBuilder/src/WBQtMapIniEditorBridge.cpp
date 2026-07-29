// WBQtMapIniEditorBridge.cpp -- MFC side of the Qt map.ini editor seam. Plain MFC TU (no Qt
// include). The editor is otherwise self-contained Qt; this supplies only what it cannot see
// for itself: the current map's map.ini path and the template catalog its name checking
// validates against. Whole body guarded by RTS_HAS_QT so the OFF build compiles it to an
// empty object.
#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "WorldBuilder.h"
#include "WorldBuilderDoc.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "qt/panels/WBQtMapIniEditorBridge.h"
#include <vector>

#ifdef RTS_HAS_QT

#define MAPINI_EDITOR_SECTION "MapIniEditor"

namespace {
	// The catalog is rebuilt on demand (BuildTemplates) and read back by index, so the Qt side
	// never holds engine types. Same shape as the entity finder's object list.
	std::vector<AsciiString> s_templates;

	void copyOut(const char *src, char *bufOut, int cap)
	{
		if (bufOut == NULL || cap <= 0)
		{
			return;
		}
		if (src == NULL)
		{
			bufOut[0] = 0;
			return;
		}
		strncpy(bufOut, src, cap - 1);
		bufOut[cap - 1] = 0;
	}
}

extern "C" void WBQtMapIniEditorData_GetPath(char *bufOut, int cap)
{
	copyOut("", bufOut, cap);
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc == NULL)
	{
		return;
	}
	CString mapPathName = pDoc->GetPathName();
	if (mapPathName.IsEmpty())
	{
		return;		// no map open/saved yet -- there is no map.ini to point at
	}
	// == currentMapIniPath in WorldBuilderDoc.cpp: the map's folder plus "map.ini".
	AsciiString iniPath = (LPCTSTR)mapPathName;
	while (iniPath.getLength() && iniPath.getCharAt(iniPath.getLength() - 1) != '\\')
	{
		iniPath.removeLastChar();
	}
	iniPath.concat("map.ini");
	copyOut(iniPath.str(), bufOut, cap);
}

extern "C" int WBQtMapIniEditorData_BuildTemplates(void)
{
	s_templates.clear();
	if (TheThingFactory == NULL)
	{
		return 0;
	}
	for (const ThingTemplate *tt = TheThingFactory->firstTemplate();
			tt != NULL;
			tt = tt->friend_getNextTemplate())
	{
		s_templates.push_back(tt->getName());
	}
	return (int)s_templates.size();
}

extern "C" void WBQtMapIniEditorData_GetTemplate(int i, char *bufOut, int cap)
{
	if (i < 0 || i >= (int)s_templates.size())
	{
		copyOut("", bufOut, cap);
		return;
	}
	copyOut(s_templates[i].str(), bufOut, cap);
}

extern "C" int WBQtMapIniEditorData_IsTemplate(const char *name)
{
	if (name == NULL || name[0] == 0 || TheThingFactory == NULL)
	{
		return 0;
	}
	return (TheThingFactory->findTemplate(AsciiString(name)) != NULL) ? 1 : 0;
}

extern "C" int WBQtMapIniEditorData_GetProfileInt(const char *key, int def)
{
	if (key == NULL)
	{
		return def;
	}
	return ::AfxGetApp()->GetProfileInt(MAPINI_EDITOR_SECTION, key, def);
}

extern "C" void WBQtMapIniEditor_SetProfileInt(const char *key, int value)
{
	if (key == NULL)
	{
		return;
	}
	::AfxGetApp()->WriteProfileInt(MAPINI_EDITOR_SECTION, key, value);
}

#endif // RTS_HAS_QT
