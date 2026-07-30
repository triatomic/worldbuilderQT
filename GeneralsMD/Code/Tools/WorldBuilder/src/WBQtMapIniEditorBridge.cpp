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
#include "Common/Upgrade.h"			// TheUpgradeCenter, for the Upgrade = check
#include "Common/Science.h"			// TheScienceStore, for the Science = check
#include "GameClient/ControlBar.h"	// TheControlBar, for the CommandSet = check
#include "Common/PlayerTemplate.h"	// ThePlayerTemplateStore, for the SideInfo check
#include "EditParameter.h"			// qtCollectCommandButtons, for the Command entries
#include "Common/FileSystem.h"		// TheFileSystem: the INI-tree scan for autocomplete
#include "Common/File.h"
#include "qt/panels/WBQtMapIniEditorBridge.h"
#include <map>
#include <set>
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

//----------------------------------------------------------------------------------------
// Upgrades and command buttons.
//
// Both catalogs can be genuinely absent in WorldBuilder (TheUpgradeCenter / TheControlBar are
// engine subsystems the editor does not always bring up), so each Is* reports "known" when its
// catalog is empty. Reporting "unknown" instead would underline every name in the file the
// moment a subsystem is missing, which is worse than checking nothing.
//----------------------------------------------------------------------------------------
namespace {
	std::vector<AsciiString> s_upgrades;
	std::vector<AsciiString> s_commandButtons;
	std::vector<AsciiString> s_sciences;
	std::vector<AsciiString> s_sides;

	// WorldBuilder never brings TheControlBar up as a subsystem -- EditParameter creates it
	// lazily on first use, and until something does, every command set/button check would
	// silently pass. Do the same here so the checks are live from the first lookup.
	// ControlBar::init() reads CommandSet.ini / CommandButton.ini through TheFileSystem, so a
	// mod's archives are resolved exactly as they are for objects and upgrades.
	void ensureControlBar(void)
	{
		if (TheControlBar == NULL)
		{
			TheControlBar = new ControlBar;
			TheControlBar->init();
		}
	}

	Bool listContains(const std::vector<AsciiString> &list, const char *name)
	{
		const AsciiString wanted(name);
		for (size_t i = 0; i < list.size(); i++)
		{
			if (list[i] == wanted)
			{
				return true;
			}
		}
		return false;
	}
}

extern "C" int WBQtMapIniEditorData_BuildUpgrades(void)
{
	s_upgrades.clear();
	if (TheUpgradeCenter == NULL)
	{
		return 0;
	}
	// getUpgradeNames is the engine's own "for WorldBuilder only" enumeration.
	std::vector<AsciiString> names = TheUpgradeCenter->getUpgradeNames();
	for (size_t i = 0; i < names.size(); i++)
	{
		s_upgrades.push_back(names[i]);
	}
	return (int)s_upgrades.size();
}

extern "C" void WBQtMapIniEditorData_GetUpgrade(int i, char *bufOut, int cap)
{
	if (i < 0 || i >= (int)s_upgrades.size())
	{
		copyOut("", bufOut, cap);
		return;
	}
	copyOut(s_upgrades[i].str(), bufOut, cap);
}

extern "C" int WBQtMapIniEditorData_IsUpgrade(const char *name)
{
	if (name == NULL || name[0] == 0)
	{
		return 1;
	}
	if (TheUpgradeCenter != NULL)
	{
		return (TheUpgradeCenter->findUpgrade(AsciiString(name)) != NULL) ? 1 : 0;
	}
	// No upgrade center: fall back to the built catalog, and pass everything if that is empty
	// too (see the note above).
	if (s_upgrades.empty())
	{
		return 1;
	}
	return listContains(s_upgrades, name) ? 1 : 0;
}

extern "C" int WBQtMapIniEditorData_BuildCommandButtons(void)
{
	s_commandButtons.clear();
	// Reuse the parameter bridge's collector: it drives EditParameter::loadCommandButtons, which
	// is the one place that knows how to enumerate them safely in the Qt inversion (it borrows a
	// long-lived hidden combo rather than creating one against the hidden MFC frame).
	EditParameter::qtCollectCommandButtons(s_commandButtons);
	return (int)s_commandButtons.size();
}

extern "C" void WBQtMapIniEditorData_GetCommandButton(int i, char *bufOut, int cap)
{
	if (i < 0 || i >= (int)s_commandButtons.size())
	{
		copyOut("", bufOut, cap);
		return;
	}
	copyOut(s_commandButtons[i].str(), bufOut, cap);
}

extern "C" int WBQtMapIniEditorData_IsCommandButton(const char *name)
{
	if (name == NULL || name[0] == 0)
	{
		return 1;
	}
	if (s_commandButtons.empty())
	{
		WBQtMapIniEditorData_BuildCommandButtons();
	}
	if (s_commandButtons.empty())
	{
		return 1;	// nothing to check against -- do not underline everything
	}
	return listContains(s_commandButtons, name) ? 1 : 0;
}

extern "C" int WBQtMapIniEditorData_BuildSciences(void)
{
	s_sciences.clear();
	if (TheScienceStore == NULL)
	{
		return 0;
	}
	std::vector<AsciiString> names = TheScienceStore->friend_getScienceNames();
	for (size_t i = 0; i < names.size(); i++)
	{
		s_sciences.push_back(names[i]);
	}
	return (int)s_sciences.size();
}

extern "C" void WBQtMapIniEditorData_GetScience(int i, char *bufOut, int cap)
{
	if (i < 0 || i >= (int)s_sciences.size())
	{
		copyOut("", bufOut, cap);
		return;
	}
	copyOut(s_sciences[i].str(), bufOut, cap);
}

extern "C" int WBQtMapIniEditorData_IsScience(const char *name)
{
	if (name == NULL || name[0] == 0)
	{
		return 1;
	}
	if (TheScienceStore != NULL)
	{
		// getScienceFromInternalName only hashes the string into a NameKey -- it does NOT check
		// that the science exists, so on its own it calls every name valid. isValidScience is
		// the lookup that actually resolves it.
		const ScienceType st = TheScienceStore->getScienceFromInternalName(AsciiString(name));
		return (st != SCIENCE_INVALID && TheScienceStore->isValidScience(st)) ? 1 : 0;
	}
	if (s_sciences.empty())
	{
		return 1;
	}
	return listContains(s_sciences, name) ? 1 : 0;
}

// Command SET names. The engine keeps its command set list protected, so there is nothing to
// enumerate here without editing shared engine code -- validation works, suggestions do not.
extern "C" int WBQtMapIniEditorData_BuildCommandSets(void)
{
	return 0;
}

extern "C" void WBQtMapIniEditorData_GetCommandSet(int i, char *bufOut, int cap)
{
	(void)i;
	copyOut("", bufOut, cap);
}

extern "C" int WBQtMapIniEditorData_IsCommandSet(const char *name)
{
	if (name == NULL || name[0] == 0)
	{
		return 1;
	}
	ensureControlBar();
	if (TheControlBar == NULL)
	{
		return 1;	// nothing to check against -- do not underline what cannot be verified
	}
	return (TheControlBar->findCommandSet(AsciiString(name)) != NULL) ? 1 : 0;
}

// Side names, from the player templates. Distinct names only: many templates share a side
// (every GLA general is side "GLA"), which is exactly why a SideInfo block needs a comment to
// say which general it is for.
extern "C" int WBQtMapIniEditorData_BuildSides(void)
{
	s_sides.clear();
	if (ThePlayerTemplateStore == NULL)
	{
		return 0;
	}
	const Int count = ThePlayerTemplateStore->getPlayerTemplateCount();
	for (Int i = 0; i < count; i++)
	{
		const PlayerTemplate *pt = ThePlayerTemplateStore->getNthPlayerTemplate(i);
		if (pt == NULL)
		{
			continue;
		}
		AsciiString side = pt->getSide();
		if (side.isEmpty() || listContains(s_sides, side.str()))
		{
			continue;
		}
		s_sides.push_back(side);
	}
	return (int)s_sides.size();
}

extern "C" void WBQtMapIniEditorData_GetSide(int i, char *bufOut, int cap)
{
	if (i < 0 || i >= (int)s_sides.size())
	{
		copyOut("", bufOut, cap);
		return;
	}
	copyOut(s_sides[i].str(), bufOut, cap);
}

extern "C" int WBQtMapIniEditorData_IsSide(const char *name)
{
	if (name == NULL || name[0] == 0)
	{
		return 1;
	}
	if (s_sides.empty())
	{
		WBQtMapIniEditorData_BuildSides();
	}
	if (s_sides.empty())
	{
		return 1;	// nothing to check against -- do not underline what cannot be verified
	}
	return listContains(s_sides, name) ? 1 : 0;
}

//----------------------------------------------------------------------------------------
// The whole-INI-tree scan, for autocomplete.
//
// Most engine stores keep their template maps private, so they can be checked but not listed.
// For suggestions the game's own INI files are read instead, via TheFileSystem so a mod's
// archives resolve the same way the engine resolves them. Harvests two things per line:
// a block name (a bare "Keyword Name") and a key (anything before an '=').
//----------------------------------------------------------------------------------------
namespace {
	std::vector<AsciiString> s_iniNames;
	std::vector<AsciiString> s_iniKeys;
	Bool s_iniScanned = false;

	// key (lowercased) -> the distinct values seen with it anywhere in the INI tree. This is the
	// per-key catalog; the flat s_iniNames list stays as the fallback for keys never seen.
	typedef std::map<AsciiString, std::set<AsciiString> > IniValuesByKey;
	IniValuesByKey s_iniValuesByKey;
	// The last key asked about, materialized for index-based read-back across the seam.
	std::vector<AsciiString> s_iniValuesForKey;

	AsciiString iniLowered(const char *s)
	{
		AsciiString out(s);
		out.toLower();
		return out;
	}

	void iniHarvestLine(const char *line, std::set<AsciiString> &names,
		std::set<AsciiString> &keys)
	{
		// Copy, then cut the comment in place, then tokenize on the same separators the engine's
		// own parser uses (" \n\r\t=").
		char buf[1024];
		strncpy(buf, line, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = 0;
		char *semi = strchr(buf, ';');
		if (semi != NULL)
		{
			*semi = 0;
		}

		const Bool hasEquals = (strchr(buf, '=') != NULL);
		char *first = strtok(buf, " \t\r\n=");
		if (first == NULL || first[0] == 0)
		{
			return;
		}
		if (hasEquals)
		{
			keys.insert(AsciiString(first));		// "Key = ..." -> the key
			// Remember which values go with THIS key, so completion can offer just those.
			std::set<AsciiString> &forKey = s_iniValuesByKey[iniLowered(first)];
			// Also harvest the VALUES. Many keys take neither a block name nor a number but an
			// enum or bit-flag name from a compile-time table -- Surfaces = GROUND WATER,
			// KindOf = STRUCTURE SELECTABLE, Locomotor = SET_NORMAL. Those tables are not
			// enumerable from here, but every name in them is used somewhere in the game's own
			// INI files, so reading the values picks them all up.
			for (char *value = strtok(NULL, " \t\r\n="); value != NULL;
					value = strtok(NULL, " \t\r\n="))
			{
				// Skip numbers and percentages -- completing "50" or "30%" is noise.
				const char *c = value;
				Bool looksNumeric = true;
				for (; *c != 0; ++c)
				{
					if (!isdigit((unsigned char)*c) && *c != '.' && *c != '-'
						&& *c != '+' && *c != '%')
					{
						looksNumeric = false;
						break;
					}
				}
				if (looksNumeric || value[0] == 0)
				{
					continue;
				}
				names.insert(AsciiString(value));
				forKey.insert(AsciiString(value));
			}
			return;
		}
		char *second = strtok(NULL, " \t\r\n=");
		if (second == NULL || second[0] == 0)
		{
			return;		// a bare word (End, or a block with no name) -- nothing to harvest
		}
		if (strtok(NULL, " \t\r\n=") != NULL)
		{
			return;		// three or more tokens: not a "Keyword Name" header
		}
		names.insert(AsciiString(second));		// "Locomotor BasicCarLocomotor" -> the name
		keys.insert(AsciiString(first));		// the keyword is worth completing too
	}

	void iniScanTree(void)
	{
		if (s_iniScanned)
		{
			return;
		}
		s_iniScanned = true;		// set first: a failed scan must not be retried per keystroke
		if (TheFileSystem == NULL)
		{
			return;
		}

		FilenameList files;
		// The directory MUST end in a backslash: Win32LocalFileSystem builds its search pattern
		// as directory + searchName with no separator inserted, so "Data\INI" + "*.ini" would
		// look for "Data\INI*.ini" and match nothing. (== the callers in ObjectOptions.cpp,
		// which all append the slash before calling.)
		TheFileSystem->getFileListInDirectory(AsciiString("Data\\INI\\"), AsciiString("*.ini"),
			files, TRUE);

		std::set<AsciiString> names;
		std::set<AsciiString> keys;
		for (FilenameList::const_iterator it = files.begin(); it != files.end(); ++it)
		{
			File *fp = TheFileSystem->openFile(it->str(), File::READ | File::TEXT);
			if (fp == NULL)
			{
				continue;
			}
			// nextLine returns void, so eof() is the loop condition; it also guarantees
			// termination on a file whose last line has no newline.
			char line[1024];
			while (!fp->eof())
			{
				line[0] = 0;
				fp->nextLine(line, sizeof(line));
				if (line[0] == 0 && fp->eof())
				{
					break;
				}
				iniHarvestLine(line, names, keys);
			}
			fp->close();
		}

		for (std::set<AsciiString>::const_iterator n = names.begin(); n != names.end(); ++n)
		{
			s_iniNames.push_back(*n);
		}
		for (std::set<AsciiString>::const_iterator k = keys.begin(); k != keys.end(); ++k)
		{
			s_iniKeys.push_back(*k);
		}
	}
}

extern "C" int WBQtMapIniEditorData_BuildIniNames(void)
{
	iniScanTree();
	return (int)s_iniNames.size();
}

extern "C" void WBQtMapIniEditorData_GetIniName(int i, char *bufOut, int cap)
{
	if (i < 0 || i >= (int)s_iniNames.size())
	{
		copyOut("", bufOut, cap);
		return;
	}
	copyOut(s_iniNames[i].str(), bufOut, cap);
}

extern "C" int WBQtMapIniEditorData_BuildIniKeys(void)
{
	iniScanTree();
	return (int)s_iniKeys.size();
}

extern "C" void WBQtMapIniEditorData_GetIniKey(int i, char *bufOut, int cap)
{
	if (i < 0 || i >= (int)s_iniKeys.size())
	{
		copyOut("", bufOut, cap);
		return;
	}
	copyOut(s_iniKeys[i].str(), bufOut, cap);
}

extern "C" int WBQtMapIniEditorData_BuildValuesForKey(const char *key)
{
	s_iniValuesForKey.clear();
	if (key == NULL || key[0] == 0)
	{
		return 0;
	}
	iniScanTree();
	IniValuesByKey::const_iterator it = s_iniValuesByKey.find(iniLowered(key));
	if (it == s_iniValuesByKey.end())
	{
		return 0;		// key never seen -- the caller falls back to the whole-tree catalog
	}
	for (std::set<AsciiString>::const_iterator v = it->second.begin();
			v != it->second.end(); ++v)
	{
		s_iniValuesForKey.push_back(*v);
	}
	return (int)s_iniValuesForKey.size();
}

extern "C" void WBQtMapIniEditorData_GetValueForKey(int i, char *bufOut, int cap)
{
	if (i < 0 || i >= (int)s_iniValuesForKey.size())
	{
		copyOut("", bufOut, cap);
		return;
	}
	copyOut(s_iniValuesForKey[i].str(), bufOut, cap);
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

extern "C" void WBQtMapIniEditorData_GetProfileString(const char *key, char *bufOut, int cap)
{
	copyOut("", bufOut, cap);
	if (key == NULL)
	{
		return;
	}
	CString value = ::AfxGetApp()->GetProfileString(MAPINI_EDITOR_SECTION, key, "");
	copyOut((LPCTSTR)value, bufOut, cap);
}

extern "C" void WBQtMapIniEditor_SetProfileString(const char *key, const char *value)
{
	if (key == NULL)
	{
		return;
	}
	::AfxGetApp()->WriteProfileString(MAPINI_EDITOR_SECTION, key, (value != NULL) ? value : "");
}

#endif // RTS_HAS_QT
