/*
**	Command & Conquer Generals(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "StdAfx.h"

#include "WorldBuilderMcpBridge.h"

#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <wchar.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "Common/GlobalData.h"
#include "Common/MapObject.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Science.h"
#include "Common/ThingFactory.h"
#include "Common/ThingSort.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/WellKnownKeys.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

#include "CUndoable.h"
#include "resource.h"
#ifdef RTS_ZEROHOUR
#include "LayersList.h"
#endif
#include "MapPreview.h"
#include "mapobjectprops.h"
#include "ObjectOptions.h"
#include "PointerTool.h"
#include "TerrainMaterial.h"
#include "WHeightMapEdit.h"
#include "WorldBuilder.h"
#include "WorldBuilderDoc.h"
#include "WorldBuilderView.h"
#include "wbview3d.h"

namespace
{

// This fork has no WWCommon.h, so define the helper locally rather than pulling in WWLib.
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) int(sizeof(x) / sizeof(x[0]))
#endif

const DWORD MAX_REQUEST_BYTES = 1024 * 1024;
const Int MAX_LIST_RESULTS = 500;
const Int MAX_TERRAIN_SAMPLES = 4096;
const Int MAX_AREA_POINTS = 256;
const Int MAX_SCRIPT_ITEMS = 512;

// TheSuperHackers @feature Eugene 30/08/2026 Identify the active editor edition through the shared MCP bridge.
#ifdef RTS_ZEROHOUR
const char BRIDGE_EDITION[] = "zero_hour";
#else
const char BRIDGE_EDITION[] = "generals";
#endif

typedef std::map<std::string, std::string> RequestFields;

struct CommandResult
{
	Bool ok;
	std::string value;
	std::string errorCode;
	std::string errorMessage;

	CommandResult() : ok(false) {}
};

class McpCommandUI : public CCmdUI
{
public:
	McpCommandUI() : enabled(false), updated(false) {}

	virtual void Enable(BOOL value) override
	{
		enabled = value != FALSE;
		updated = true;
	}

	Bool enabled;
	Bool updated;
};

class McpModifyObjectUndoable : public Undoable
{
public:
	McpModifyObjectUndoable(
		CWorldBuilderDoc *document,
		MapObject *object,
		const Coord3D &new_location,
		Real new_angle,
		const ThingTemplate *new_template,
		Bool change_template,
		const AsciiString &new_owner,
		Bool change_owner,
		Int new_veterancy = 0,
		Bool change_veterancy = false,
		const AsciiString &new_script_name = AsciiString::TheEmptyString,
		Bool change_script_name = false) :
		m_document(document),
		m_object(object),
		m_oldLocation(*object->getLocation()),
		m_newLocation(new_location),
		m_oldAngle(object->getAngle()),
		m_newAngle(new_angle),
		m_oldTemplate(object->getThingTemplate()),
		m_newTemplate(new_template),
		m_oldName(object->getName()),
		m_newName(change_template ? new_template->getName() : object->getName()),
		m_newOwner(new_owner),
		m_changeTemplate(change_template),
		m_changeOwner(change_owner),
		m_newVeterancy(new_veterancy),
		m_changeVeterancy(change_veterancy),
			m_newScriptName(new_script_name),
			m_changeScriptName(change_script_name)
	{
		m_oldOwner = object->getProperties()->getAsciiString(TheKey_originalOwner, &m_oldOwnerExists);
		m_oldVeterancy = object->getProperties()->getInt(TheKey_objectVeterancy, &m_oldVeterancyExists);
			m_oldScriptName = object->getProperties()->getAsciiString(TheKey_objectName, &m_oldScriptNameExists);
	}

	virtual void Do() override
	{
		Apply(m_newLocation, m_newAngle, m_newTemplate, m_newName, m_newOwner, true,
			m_newVeterancy, true, m_newScriptName, true);
	}

	virtual void Undo() override
	{
		Apply(m_oldLocation, m_oldAngle, m_oldTemplate, m_oldName, m_oldOwner, m_oldOwnerExists,
			m_oldVeterancy, m_oldVeterancyExists,
				m_oldScriptName, m_oldScriptNameExists);
	}

	virtual void Redo() override
	{
		Do();
	}

private:
	void Apply(
		const Coord3D &location,
		Real angle,
		const ThingTemplate *thing,
		const AsciiString &name,
		const AsciiString &owner,
		Bool owner_exists,
		Int veterancy,
		Bool veterancy_exists,
			const AsciiString &script_name,
			Bool script_name_exists)
	{
		m_document->invalObject(m_object);
		Coord3D mutable_location = location;
		m_object->setLocation(&mutable_location);
		m_object->setAngle(angle);
		if (m_changeTemplate) {
			m_object->setThingTemplate(thing);
			m_object->setName(name);
		}
		if (m_changeOwner) {
			Dict *properties = m_object->getProperties();
			if (owner_exists) {
				properties->setAsciiString(TheKey_originalOwner, owner);
			} else if (properties->getType(TheKey_originalOwner) != Dict::DICT_NONE) {
				properties->remove(TheKey_originalOwner);
			}
		}
		if (m_changeVeterancy) {
			Dict *properties = m_object->getProperties();
			if (veterancy_exists) {
				properties->setInt(TheKey_objectVeterancy, veterancy);
			} else if (properties->getType(TheKey_objectVeterancy) != Dict::DICT_NONE) {
				properties->remove(TheKey_objectVeterancy);
			}
		}
		if (m_changeScriptName) {
			Dict *properties = m_object->getProperties();
			if (script_name_exists) {
				properties->setAsciiString(TheKey_objectName, script_name);
			} else if (properties->getType(TheKey_objectName) != Dict::DICT_NONE) {
				properties->remove(TheKey_objectName);
			}
		}
		m_document->invalObject(m_object);

		if (m_changeTemplate || m_changeOwner || m_changeVeterancy || m_changeScriptName) {
			WbView3d *view = m_document->Get3DView();
			if (view != nullptr) {
				view->resetRenderObjects();
				view->invalObjectInView(nullptr);
			}
		}
	}

	CWorldBuilderDoc *m_document;
	MapObject *m_object;
	Coord3D m_oldLocation;
	Coord3D m_newLocation;
	Real m_oldAngle;
	Real m_newAngle;
	const ThingTemplate *m_oldTemplate;
	const ThingTemplate *m_newTemplate;
	AsciiString m_oldName;
	AsciiString m_newName;
	AsciiString m_oldOwner;
	AsciiString m_newOwner;
	Bool m_oldOwnerExists;
	Bool m_changeTemplate;
	Bool m_changeOwner;
	Int m_oldVeterancy;
	Int m_newVeterancy;
	Bool m_oldVeterancyExists;
	Bool m_changeVeterancy;
		AsciiString m_oldScriptName;
		AsciiString m_newScriptName;
		Bool m_oldScriptNameExists;
		Bool m_changeScriptName;
};

class McpCompositeUndoable : public Undoable
{
public:
	virtual ~McpCompositeUndoable() override
	{
		for (size_t i = 0; i < m_children.size(); ++i) {
			m_children[i]->Release_Ref();
		}
	}

	void Add(Undoable *child)
	{
		child->Add_Ref();
		m_children.push_back(child);
	}

	virtual void Do() override
	{
		for (size_t i = 0; i < m_children.size(); ++i) {
			m_children[i]->Do();
		}
	}

	virtual void Undo() override
	{
		for (size_t i = m_children.size(); i > 0; --i) {
			m_children[i - 1]->Undo();
		}
	}

	virtual void Redo() override
	{
		for (size_t i = 0; i < m_children.size(); ++i) {
			m_children[i]->Redo();
		}
	}

	Bool IsEmpty() const { return m_children.empty(); }

private:
	std::vector<Undoable *> m_children;
};

class McpWaypointLinkUndoable : public Undoable
{
public:
	McpWaypointLinkUndoable(CWorldBuilderDoc *document, Int from, Int to, Bool add) :
		m_document(document), m_from(from), m_to(to), m_add(add) {}

	virtual void Do() override { Apply(m_add); }
	virtual void Undo() override { Apply(!m_add); }

private:
	void Apply(Bool add)
	{
		if (add) {
			m_document->addWaypointLink(m_from, m_to);
		} else {
			m_document->removeWaypointLink(m_from, m_to);
		}
		MapObject *from = m_document->getWaypointByID(m_from);
		if (from != nullptr) {
			m_document->updateLinkedWaypointLabels(from);
		}
		m_document->invalObject(nullptr);
	}

	CWorldBuilderDoc *m_document;
	Int m_from;
	Int m_to;
	Bool m_add;
};

class McpModifyWaypointUndoable : public Undoable
{
public:
	McpModifyWaypointUndoable(
		CWorldBuilderDoc *document, MapObject *waypoint,
		const Coord3D &location, const AsciiString &name) :
		m_document(document), m_waypoint(waypoint),
		m_oldLocation(*waypoint->getLocation()), m_newLocation(location),
		m_oldName(waypoint->getWaypointName()), m_newName(name) {}

	virtual void Do() override { Apply(m_newLocation, m_newName); }
	virtual void Undo() override { Apply(m_oldLocation, m_oldName); }

private:
	void Apply(const Coord3D &location, const AsciiString &name)
	{
		m_document->invalObject(m_waypoint);
		Coord3D mutable_location = location;
		m_waypoint->setLocation(&mutable_location);
		m_waypoint->setWaypointName(name);
		m_waypoint->setName(name);
		m_document->updateLinkedWaypointLabels(m_waypoint);
		m_document->invalObject(m_waypoint);
	}

	CWorldBuilderDoc *m_document;
	MapObject *m_waypoint;
	Coord3D m_oldLocation;
	Coord3D m_newLocation;
	AsciiString m_oldName;
	AsciiString m_newName;
};

struct McpPolygonState
{
	AsciiString name;
#ifdef RTS_ZEROHOUR
	AsciiString layerName;
#endif
	std::vector<ICoord3D> points;
	Bool exportWithScripts;
	Bool water;
	Bool river;
	Int riverStart;
};

McpPolygonState CapturePolygon(PolygonTrigger *polygon)
{
	McpPolygonState state;
	state.name = polygon->getTriggerName();
#ifdef RTS_ZEROHOUR
	state.layerName = polygon->getLayerName();
#endif
	state.exportWithScripts = polygon->doExportWithScripts();
	state.water = polygon->isWaterArea();
	state.river = polygon->isRiver();
	state.riverStart = polygon->getRiverStart();
	for (Int i = 0; i < polygon->getNumPoints(); ++i) {
		state.points.push_back(*polygon->getPoint(i));
	}
	return state;
}

void ApplyPolygon(PolygonTrigger *polygon, const McpPolygonState &state)
{
	while (polygon->getNumPoints() > 0) {
		polygon->deletePoint(0);
	}
	for (size_t i = 0; i < state.points.size(); ++i) {
		polygon->addPoint(state.points[i]);
	}
	polygon->setTriggerName(state.name);
#ifdef RTS_ZEROHOUR
	polygon->setLayerName(state.layerName);
#endif
	polygon->setDoExportWithScripts(state.exportWithScripts);
	polygon->setWaterArea(state.water);
	polygon->setRiver(state.river);
	polygon->setRiverStart(state.riverStart);
}

class McpModifyPolygonUndoable : public Undoable
{
public:
	McpModifyPolygonUndoable(
		CWorldBuilderDoc *document, PolygonTrigger *polygon, const McpPolygonState &state) :
		m_document(document), m_polygon(polygon), m_old(CapturePolygon(polygon)), m_new(state) {}

	virtual void Do() override { Apply(m_new); }
	virtual void Undo() override { Apply(m_old); }

private:
	void Apply(const McpPolygonState &state)
	{
#ifdef RTS_ZEROHOUR
		AsciiString old_layer_name = m_polygon->getLayerName();
#endif
		ApplyPolygon(m_polygon, state);
#ifdef RTS_ZEROHOUR
		// TheSuperHackers @feature Eugene 30/08/2026 Keep Zero Hour's Layers List synchronized across area edits and undo/redo.
		if (old_layer_name.compareNoCase(state.layerName) != 0) {
			TheLayersList->changePolygonTriggerLayer(m_polygon, state.layerName);
		}
#endif
		m_document->invalObject(nullptr);
	}

	CWorldBuilderDoc *m_document;
	PolygonTrigger *m_polygon;
	McpPolygonState m_old;
	McpPolygonState m_new;
};

struct McpMapSettingsState
{
	Dict world;
	TimeOfDay timeOfDay;
	Weather weather;
	Real waterHeight;
	GlobalData::TerrainLighting terrain[TIME_OF_DAY_COUNT][MAX_GLOBAL_LIGHTS];
	GlobalData::TerrainLighting objects[TIME_OF_DAY_COUNT][MAX_GLOBAL_LIGHTS];
};

McpMapSettingsState CaptureMapSettings()
{
	McpMapSettingsState state;
	state.world = *MapObject::getWorldDict();
	state.timeOfDay = TheGlobalData->m_timeOfDay;
	state.weather = TheGlobalData->m_weather;
	state.waterHeight = TheGlobalData->m_waterPositionZ;
	for (Int tod = 0; tod < TIME_OF_DAY_COUNT; ++tod) {
		for (Int light = 0; light < MAX_GLOBAL_LIGHTS; ++light) {
			state.terrain[tod][light] = TheGlobalData->m_terrainLighting[tod][light];
			state.objects[tod][light] = TheGlobalData->m_terrainObjectsLighting[tod][light];
		}
	}
	return state;
}

class McpMapSettingsUndoable : public Undoable
{
public:
	McpMapSettingsUndoable(CWorldBuilderDoc *document, const McpMapSettingsState &next) :
		m_document(document), m_old(CaptureMapSettings()), m_new(next) {}

	virtual void Do() override { Apply(m_new); }
	virtual void Undo() override { Apply(m_old); }

private:
	void Apply(const McpMapSettingsState &state)
	{
		*MapObject::getWorldDict() = state.world;
		TheWritableGlobalData->m_weather = state.weather;
		TheWritableGlobalData->m_waterPositionZ = state.waterHeight;
		for (Int tod = 0; tod < TIME_OF_DAY_COUNT; ++tod) {
			for (Int light = 0; light < MAX_GLOBAL_LIGHTS; ++light) {
				TheWritableGlobalData->m_terrainLighting[tod][light] = state.terrain[tod][light];
				TheWritableGlobalData->m_terrainObjectsLighting[tod][light] = state.objects[tod][light];
			}
		}
		TheWritableGlobalData->setTimeOfDay(state.timeOfDay);
		m_document->updateAllViews();
		WbView3d *view = m_document->Get3DView();
		if (view != nullptr) {
			view->resetRenderObjects();
		}
	}

	CWorldBuilderDoc *m_document;
	McpMapSettingsState m_old;
	McpMapSettingsState m_new;
};

std::string WideToUtf8(const wchar_t *value)
{
	if (value == nullptr || *value == L'\0') {
		return std::string();
	}

	int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
	if (size <= 1) {
		return std::string();
	}

	std::vector<char> buffer(size);
	if (WideCharToMultiByte(CP_UTF8, 0, value, -1, &buffer[0], size, nullptr, nullptr) == 0) {
		return std::string();
	}
	return std::string(&buffer[0]);
}

std::string AnsiToUtf8(const char *value)
{
	if (value == nullptr || *value == '\0') {
		return std::string();
	}

	int size = MultiByteToWideChar(CP_ACP, 0, value, -1, nullptr, 0);
	if (size <= 1) {
		return std::string();
	}

	std::vector<wchar_t> buffer(size);
	if (MultiByteToWideChar(CP_ACP, 0, value, -1, &buffer[0], size) == 0) {
		return std::string();
	}
	return WideToUtf8(&buffer[0]);
}

Bool AnsiToWide(const char *value, std::wstring *wide)
{
	wide->clear();
	if (value == nullptr || *value == '\0') {
		return true;
	}
	int size = MultiByteToWideChar(CP_ACP, 0, value, -1, nullptr, 0);
	if (size <= 0) {
		return false;
	}
	std::vector<wchar_t> buffer(size);
	if (MultiByteToWideChar(CP_ACP, 0, value, -1, &buffer[0], size) == 0) {
		return false;
	}
	wide->assign(&buffer[0]);
	return true;
}

Bool WideToAnsi(const std::wstring &value, std::string *ansi)
{
	ansi->clear();
	if (value.empty()) {
		return true;
	}
	int size = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (size <= 0) {
		return false;
	}
	std::vector<char> buffer(size);
	BOOL used_default = FALSE;
	if (WideCharToMultiByte(
			CP_ACP, WC_NO_BEST_FIT_CHARS, value.c_str(), -1, &buffer[0], size, nullptr, &used_default) == 0
		|| used_default) {
		return false;
	}
	ansi->assign(&buffer[0]);
	return true;
}

Bool WideToCString(const std::wstring &value, CString *result)
{
#ifdef _UNICODE
	*result = value.c_str();
	return true;
#else
	std::string ansi;
	if (!WideToAnsi(value, &ansi)) {
		return false;
	}
	*result = ansi.c_str();
	return true;
#endif
}

std::string CStringToUtf8(const CString &value)
{
#ifdef _UNICODE
	return WideToUtf8((LPCTSTR)value);
#else
	return AnsiToUtf8((LPCTSTR)value);
#endif
}

Bool Utf8ToWide(const std::string &value, std::wstring *wide)
{
	wide->clear();
	if (value.empty()) {
		return true;
	}

	int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, nullptr, 0);
	if (size <= 0) {
		return false;
	}

	std::vector<wchar_t> buffer(size);
	if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, &buffer[0], size) == 0) {
		return false;
	}
	wide->assign(&buffer[0]);
	return true;
}

std::string JsonString(const std::string &value)
{
	static const char hex[] = "0123456789abcdef";
	std::string result;
	result.reserve(value.size() + 2);
	result += '"';
	for (std::string::const_iterator it = value.begin(); it != value.end(); ++it) {
		unsigned char c = static_cast<unsigned char>(*it);
		switch (c) {
			case '"': result += "\\\""; break;
			case '\\': result += "\\\\"; break;
			case '\b': result += "\\b"; break;
			case '\f': result += "\\f"; break;
			case '\n': result += "\\n"; break;
			case '\r': result += "\\r"; break;
			case '\t': result += "\\t"; break;
			default:
				if (c < 0x20) {
					result += "\\u00";
					result += hex[(c >> 4) & 0x0f];
					result += hex[c & 0x0f];
				} else {
					result += static_cast<char>(c);
				}
				break;
		}
	}
	result += '"';
	return result;
}

std::string FormatInt(Int value)
{
	char buffer[32];
	_snprintf(buffer, sizeof(buffer), "%d", value);
	buffer[sizeof(buffer) - 1] = '\0';
	return std::string(buffer);
}

std::string FormatUnsigned(UnsignedInt value)
{
	char buffer[32];
	_snprintf(buffer, sizeof(buffer), "%u", value);
	buffer[sizeof(buffer) - 1] = '\0';
	return std::string(buffer);
}

std::string FormatUInt64(unsigned __int64 value)
{
	char buffer[32];
	_snprintf(buffer, sizeof(buffer), "%I64u", value);
	buffer[sizeof(buffer) - 1] = '\0';
	return std::string(buffer);
}

std::string FormatReal(Real value)
{
	char buffer[64];
	_snprintf(buffer, sizeof(buffer), "%.9g", static_cast<double>(value));
	buffer[sizeof(buffer) - 1] = '\0';
	return std::string(buffer);
}

CommandResult Success(const std::string &json_value)
{
	CommandResult result;
	result.ok = true;
	result.value = json_value;
	return result;
}

CommandResult Error(const char *code, const char *message)
{
	CommandResult result;
	result.errorCode = code;
	result.errorMessage = message;
	return result;
}

std::string MakeEnvelope(const CommandResult &result)
{
	if (result.ok) {
		return "{\"ok\":true,\"result\":" + result.value + "}";
	}
	return "{\"ok\":false,\"error\":{\"code\":" + JsonString(result.errorCode)
		+ ",\"message\":" + JsonString(result.errorMessage) + "}}";
}

Bool ReadFileBytes(const std::wstring &path, std::string *contents)
{
	contents->clear();
	HANDLE file = CreateFileW(
		path.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
		nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		return false;
	}

	BY_HANDLE_FILE_INFORMATION file_information;
	if (GetFileInformationByHandle(file, &file_information) == FALSE
		|| (file_information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0
		|| (file_information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
		CloseHandle(file);
		return false;
	}

	DWORD high_size = 0;
	DWORD low_size = GetFileSize(file, &high_size);
	if (low_size == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
		CloseHandle(file);
		return false;
	}
	if (high_size != 0 || low_size > MAX_REQUEST_BYTES) {
		CloseHandle(file);
		return false;
	}

	std::vector<char> buffer(low_size);
	DWORD bytes_read = 0;
	Bool read_ok = low_size == 0
		|| ReadFile(file, &buffer[0], low_size, &bytes_read, nullptr) != FALSE;
	CloseHandle(file);
	if (!read_ok || bytes_read != low_size) {
		return false;
	}

	if (!buffer.empty()) {
		contents->assign(&buffer[0], buffer.size());
	}
	return true;
}

Bool WriteFileBytes(const std::wstring &path, const std::string &contents)
{
	HANDLE file = CreateFileW(
		path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		return false;
	}

	DWORD bytes_written = 0;
	Bool write_ok = contents.empty()
		|| WriteFile(file, contents.data(), static_cast<DWORD>(contents.size()), &bytes_written, nullptr) != FALSE;
	CloseHandle(file);
	return write_ok && bytes_written == contents.size();
}

Bool GetFullPath(const wchar_t *path, std::wstring *full_path)
{
	if (path == nullptr || *path == L'\0') {
		return false;
	}

	DWORD required = GetFullPathNameW(path, 0, nullptr, nullptr);
	if (required == 0 || required > 32768) {
		return false;
	}

	std::vector<wchar_t> buffer(required + 1);
	DWORD length = GetFullPathNameW(path, static_cast<DWORD>(buffer.size()), &buffer[0], nullptr);
	if (length == 0 || length >= buffer.size()) {
		return false;
	}
	full_path->assign(&buffer[0], length);
	return true;
}

Bool ParseBool(const std::string &text, Bool *value)
{
	if (text == "true" || text == "1") {
		*value = true;
		return true;
	}
	if (text == "false" || text == "0") {
		*value = false;
		return true;
	}
	return false;
}

Bool GetUserMapsRoot(std::wstring *root)
{
	std::wstring user_data;
	if (TheGlobalData == nullptr
		|| !AnsiToWide(TheGlobalData->getPath_UserData().str(), &user_data)
		|| user_data.empty()) {
		return false;
	}
	if (user_data[user_data.size() - 1] != L'\\') {
		user_data += L'\\';
	}
	user_data += L"Maps\\";
	return GetFullPath(user_data.c_str(), root);
}

Bool HasMapExtension(const std::wstring &path)
{
	return path.size() >= 4 && _wcsicmp(path.c_str() + path.size() - 4, L".map") == 0;
}

std::wstring PathLeaf(const std::wstring &path)
{
	size_t end = path.size();
	while (end > 0 && (path[end - 1] == L'\\' || path[end - 1] == L'/')) {
		--end;
	}
	size_t separator = path.find_last_of(L"\\/", end == 0 ? 0 : end - 1);
	return path.substr(separator == std::wstring::npos ? 0 : separator + 1, end - (separator == std::wstring::npos ? 0 : separator + 1));
}

Bool IsAbsoluteWindowsPath(const std::wstring &path)
{
	return path.size() >= 3
		&& ((path[1] == L':' && (path[2] == L'\\' || path[2] == L'/'))
			|| (path[0] == L'\\' && path[1] == L'\\'));
}

Bool IsPathInsideRoot(const std::wstring &path, const std::wstring &root)
{
	return path.size() > root.size()
		&& _wcsnicmp(path.c_str(), root.c_str(), root.size()) == 0;
}

Bool ExistingPathComponentsAreSafe(const std::wstring &path, const std::wstring &root)
{
	if (!IsPathInsideRoot(path, root)) {
		return false;
	}
	std::wstring current(root);
	size_t offset = root.size();
	while (offset < path.size()) {
		size_t separator = path.find(L'\\', offset);
		std::wstring component = path.substr(offset, separator == std::wstring::npos ? std::wstring::npos : separator - offset);
		if (!component.empty()) {
			current += component;
			DWORD attributes = GetFileAttributesW(current.c_str());
			if (attributes != INVALID_FILE_ATTRIBUTES
				&& (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
				return false;
			}
			current += L'\\';
		}
		if (separator == std::wstring::npos) {
			break;
		}
		offset = separator + 1;
	}
	return true;
}

CommandResult ResolveUserMapPath(const std::string &input, Bool for_save, std::wstring *resolved)
{
	if (input.empty()) {
		return Error("invalid_arguments", "path must not be empty.");
	}
	std::wstring root;
	if (!GetUserMapsRoot(&root)) {
		return Error("user_maps_unavailable", "WorldBuilder could not resolve the current user's Maps directory.");
	}
	if (root.empty() || root[root.size() - 1] != L'\\') {
		root += L'\\';
	}

	std::wstring requested;
	if (!Utf8ToWide(input, &requested) || requested.empty()) {
		return Error("invalid_arguments", "path must be valid UTF-8.");
	}
	for (size_t i = 0; i < requested.size(); ++i) {
		if (requested[i] == L'/') {
			requested[i] = L'\\';
		}
	}

	std::wstring candidate;
	if (!IsAbsoluteWindowsPath(requested)) {
		if (requested.find(L'\\') == std::wstring::npos) {
			std::wstring map_name(requested);
			if (HasMapExtension(map_name)) {
				map_name.erase(map_name.size() - 4);
			}
			if (map_name.empty() || map_name == L"." || map_name == L"..") {
				return Error("invalid_arguments", "Map name is invalid.");
			}
			candidate = root + map_name + L"\\" + map_name + L".map";
		} else {
			candidate = root + requested;
		}
	} else {
		candidate = requested;
	}

	DWORD candidate_attributes = GetFileAttributesW(candidate.c_str());
	if (candidate_attributes != INVALID_FILE_ATTRIBUTES
		&& (candidate_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
		std::wstring leaf = PathLeaf(candidate);
		candidate += L"\\" + leaf + L".map";
	} else if (!HasMapExtension(candidate)) {
		std::wstring leaf = PathLeaf(candidate);
		candidate += L"\\" + leaf + L".map";
	}

	if (!GetFullPath(candidate.c_str(), resolved)
		|| !IsPathInsideRoot(*resolved, root)
		|| !ExistingPathComponentsAreSafe(*resolved, root)) {
		return Error("path_outside_user_maps", "The map path must stay inside the current user's Generals Maps directory.");
	}
	if (!HasMapExtension(*resolved)) {
		return Error("invalid_map_path", "The resolved map file must have a .map extension.");
	}

	DWORD attributes = GetFileAttributesW(resolved->c_str());
	if (!for_save) {
		if (attributes == INVALID_FILE_ATTRIBUTES
			|| (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0
			|| (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
			return Error("map_not_found", "The requested user map does not exist.");
		}
	} else if (attributes != INVALID_FILE_ATTRIBUTES
		&& ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0
			|| (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)) {
		return Error("invalid_map_path", "The map target is not a regular file.");
	}
	return Success("null");
}

Bool EnsureParentDirectories(const std::wstring &path, const std::wstring &root)
{
	size_t last_separator = path.find_last_of(L'\\');
	if (last_separator == std::wstring::npos || last_separator < root.size()) {
		return false;
	}
	std::wstring directory = path.substr(0, last_separator);
	std::wstring current(root);
	size_t offset = root.size();
	while (offset < directory.size()) {
		size_t separator = directory.find(L'\\', offset);
		std::wstring component = directory.substr(offset, separator == std::wstring::npos ? std::wstring::npos : separator - offset);
		if (!component.empty()) {
			current += component;
			DWORD attributes = GetFileAttributesW(current.c_str());
			if (attributes == INVALID_FILE_ATTRIBUTES) {
				if (CreateDirectoryW(current.c_str(), nullptr) == FALSE && GetLastError() != ERROR_ALREADY_EXISTS) {
					return false;
				}
				attributes = GetFileAttributesW(current.c_str());
			}
			if (attributes == INVALID_FILE_ATTRIBUTES
				|| (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0
				|| (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
				return false;
			}
			current += L'\\';
		}
		if (separator == std::wstring::npos) {
			break;
		}
		offset = separator + 1;
	}
	return true;
}

Bool IsAllowedBridgePath(const std::wstring &path)
{
	wchar_t temp_path[MAX_PATH + 1];
	DWORD temp_length = GetTempPathW(ARRAY_SIZE(temp_path), temp_path);
	if (temp_length == 0 || temp_length >= ARRAY_SIZE(temp_path)) {
		return false;
	}

	std::wstring allowed_root(temp_path);
	if (!allowed_root.empty() && allowed_root[allowed_root.size() - 1] != L'\\') {
		allowed_root += L'\\';
	}
	allowed_root += L"GeneralsWorldBuilderMcp\\";

	std::wstring full_path;
	std::wstring full_root;
	if (!GetFullPath(path.c_str(), &full_path) || !GetFullPath(allowed_root.c_str(), &full_root)) {
		return false;
	}
	std::wstring root_directory(full_root);
	while (!root_directory.empty()
		&& (root_directory[root_directory.size() - 1] == L'\\'
			|| root_directory[root_directory.size() - 1] == L'/')) {
		root_directory.erase(root_directory.size() - 1);
	}
	DWORD root_attributes = GetFileAttributesW(root_directory.c_str());
	if (root_attributes == INVALID_FILE_ATTRIBUTES
		|| (root_attributes & FILE_ATTRIBUTE_DIRECTORY) == 0
		|| (root_attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
		return false;
	}
	if (full_path.size() <= full_root.size()) {
		return false;
	}
	if (_wcsnicmp(full_path.c_str(), full_root.c_str(), full_root.size()) != 0) {
		return false;
	}
	std::wstring filename = full_path.substr(full_root.size());
	return filename.find_first_of(L"\\/:") == std::wstring::npos;
}

int HexValue(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

Bool PercentDecode(const std::string &encoded, std::string *decoded)
{
	decoded->clear();
	decoded->reserve(encoded.size());
	for (size_t i = 0; i < encoded.size(); ++i) {
		if (encoded[i] != '%') {
			decoded->push_back(encoded[i]);
			continue;
		}
		if (i + 2 >= encoded.size()) {
			return false;
		}
		int high = HexValue(encoded[i + 1]);
		int low = HexValue(encoded[i + 2]);
		if (high < 0 || low < 0) {
			return false;
		}
		decoded->push_back(static_cast<char>((high << 4) | low));
		i += 2;
	}
	return true;
}

Bool ParseRequest(const std::string &contents, RequestFields *fields)
{
	fields->clear();
	size_t start = 0;
	while (start <= contents.size()) {
		size_t end = contents.find('\n', start);
		if (end == std::string::npos) {
			end = contents.size();
		}
		std::string line = contents.substr(start, end - start);
		if (!line.empty() && line[line.size() - 1] == '\r') {
			line.erase(line.size() - 1);
		}
		if (!line.empty()) {
			size_t separator = line.find('\t');
			if (separator == std::string::npos || separator == 0) {
				return false;
			}
			std::string key = line.substr(0, separator);
			std::string value;
			if (!PercentDecode(line.substr(separator + 1), &value)) {
				return false;
			}
			if (value.find('\0') != std::string::npos || fields->find(key) != fields->end()) {
				return false;
			}
			(*fields)[key] = value;
		}
		if (end == contents.size()) {
			break;
		}
		start = end + 1;
	}
	return fields->find("command") != fields->end() && fields->find("response_path") != fields->end();
}

Bool ParseInt(const std::string &text, Int *value)
{
	if (text.empty()) {
		return false;
	}
	char *end = nullptr;
	long parsed = strtol(text.c_str(), &end, 10);
	if (end == text.c_str() || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX) {
		return false;
	}
	*value = static_cast<Int>(parsed);
	return true;
}

Bool ParseReal(const std::string &text, Real *value)
{
	if (text.empty()) {
		return false;
	}
	char *end = nullptr;
	double parsed = strtod(text.c_str(), &end);
	if (end == text.c_str() || *end != '\0' || !_finite(parsed)) {
		return false;
	}
	*value = static_cast<Real>(parsed);
	return _finite(*value) != 0;
}

Bool GetOptionalInt(
	const RequestFields &fields, const char *name, Int default_value, Int minimum, Int maximum, Int *value)
{
	RequestFields::const_iterator it = fields.find(name);
	if (it == fields.end()) {
		*value = default_value;
		return true;
	}
	if (!ParseInt(it->second, value)) {
		return false;
	}
	return *value >= minimum && *value <= maximum;
}

Bool GetRequiredInt(const RequestFields &fields, const char *name, Int minimum, Int maximum, Int *value)
{
	RequestFields::const_iterator it = fields.find(name);
	return it != fields.end() && ParseInt(it->second, value) && *value >= minimum && *value <= maximum;
}

Bool GetRequiredReal(const RequestFields &fields, const char *name, Real *value)
{
	RequestFields::const_iterator it = fields.find(name);
	return it != fields.end() && ParseReal(it->second, value);
}

Bool GetOptionalReal(const RequestFields &fields, const char *name, Real *value, Bool *exists)
{
	RequestFields::const_iterator it = fields.find(name);
	*exists = it != fields.end();
	return !*exists || ParseReal(it->second, value);
}

std::string LowerAscii(const std::string &value)
{
	std::string result(value);
	for (size_t i = 0; i < result.size(); ++i) {
		if (result[i] >= 'A' && result[i] <= 'Z') {
			result[i] = static_cast<char>(result[i] - 'A' + 'a');
		}
	}
	return result;
}

Bool ContainsFilter(const std::string &value, const std::string &lower_filter)
{
	return lower_filter.empty() || LowerAscii(value).find(lower_filter) != std::string::npos;
}

// TheSuperHackers @feature Eugene 01/09/2026 Expose unique map object names for deterministic script actions.
Bool IsUniqueScriptName(const AsciiString &script_name, const MapObject *excluded)
{
	std::string lower_name = LowerAscii(AnsiToUtf8(script_name.str()));
	for (MapObject *object = MapObject::getFirstMapObject(); object != nullptr; object = object->getNext()) {
		if (object == excluded) {
			continue;
		}
		Bool exists = false;
		AsciiString existing_name = object->getProperties()->getAsciiString(TheKey_objectName, &exists);
		if (exists && LowerAscii(AnsiToUtf8(existing_name.str())) == lower_name) {
			return false;
		}
	}
	return true;
}

std::string GetPersistentObjectId(MapObject *object)
{
	Bool exists = false;
	AsciiString id = object->getProperties()->getAsciiString(TheKey_uniqueID, &exists);
	return exists ? AnsiToUtf8(id.str()) : std::string();
}

std::string GetObjectId(MapObject *object)
{
	std::string persistent = GetPersistentObjectId(object);
	if (!persistent.empty()) {
		return persistent;
	}
	char buffer[32];
	_snprintf(buffer, sizeof(buffer), "@%p", object);
	buffer[sizeof(buffer) - 1] = '\0';
	return std::string(buffer);
}

std::string GetObjectTemplateName(MapObject *object)
{
	const ThingTemplate *thing = object->getThingTemplate();
	return thing != nullptr ? AnsiToUtf8(thing->getName().str()) : AnsiToUtf8(object->getName().str());
}

MapObject *FindObject(const std::string &object_id)
{
	for (MapObject *object = MapObject::getFirstMapObject(); object != nullptr; object = object->getNext()) {
		if (GetObjectId(object) == object_id || GetPersistentObjectId(object) == object_id) {
			return object;
		}
	}
	return nullptr;
}

Bool IsRuntimePlayerTeam(const AsciiString &owner)
{
	const char *value = owner.str();
	const char prefix[] = "teamplayer";
	const size_t prefix_length = sizeof(prefix) - 1;
	return strncmp(value, prefix, prefix_length) == 0
		&& value[prefix_length] >= '0'
		&& value[prefix_length] <= '7'
		&& value[prefix_length + 1] == '\0';
}

Bool IsValidOwner(const AsciiString &owner)
{
	return IsRuntimePlayerTeam(owner)
		|| (TheSidesList != nullptr && TheSidesList->findTeamInfo(owner) != nullptr);
}

std::string ObjectToJson(MapObject *object, CWorldBuilderDoc *document = nullptr)
{
	const Coord3D *location = object->getLocation();
	const ThingTemplate *thing = object->getThingTemplate();
	EditorSortingType sorting = thing != nullptr ? thing->getEditorSorting() : ES_NONE;
	Bool owner_exists = false;
	Dict *properties = object->getProperties();
	AsciiString owner = properties->getAsciiString(TheKey_originalOwner, &owner_exists);
	Bool veterancy_exists = false;
	Int veterancy = properties->getInt(TheKey_objectVeterancy, &veterancy_exists);
	Bool script_name_exists = false;
	AsciiString script_name = properties->getAsciiString(TheKey_objectName, &script_name_exists);

	std::string result = "{";
	result += "\"id\":" + JsonString(GetObjectId(object));
	std::string persistent_id = GetPersistentObjectId(object);
	result += ",\"persistent_id\":";
	result += persistent_id.empty() ? "null" : JsonString(persistent_id);
	result += ",\"id_stable\":";
	result += persistent_id.empty() ? "false" : "true";
	result += ",\"name\":" + JsonString(AnsiToUtf8(object->getName().str()));
	result += ",\"template\":" + JsonString(GetObjectTemplateName(object));
	result += ",\"location\":{\"x\":" + FormatReal(location->x)
		+ ",\"y\":" + FormatReal(location->y) + ",\"z\":" + FormatReal(location->z) + "}";
	result += ",\"angle\":" + FormatReal(object->getAngle() * 180.0f / PI);
	result += ",\"flags\":" + FormatInt(object->getFlags());
	result += ",\"selected\":";
	result += object->isSelected() ? "true" : "false";
	result += ",\"waypoint\":";
	result += object->isWaypoint() ? "true" : "false";
	result += ",\"editor_sorting\":" + FormatInt(static_cast<Int>(sorting));
	result += ",\"is_unit\":";
	result += sorting == ES_INFANTRY || sorting == ES_VEHICLE ? "true" : "false";
	result += ",\"weapon_salvager\":";
	result += thing != nullptr && thing->isKindOf(KINDOF_WEAPON_SALVAGER) ? "true" : "false";
	result += ",\"veterancy\":";
	result += veterancy_exists ? FormatInt(veterancy) : "null";
	result += ",\"owner\":";
	result += owner_exists ? JsonString(AnsiToUtf8(owner.str())) : "null";
	result += ",\"script_name\":";
	result += script_name_exists ? JsonString(AnsiToUtf8(script_name.str())) : "null";
	// TheSuperHackers @bugfix Eugene 30/08/2026 Return revisions with single-object mutation results for safe chaining.
	if (document != nullptr) {
		result += ",\"revision\":" + FormatUnsigned(document->getChangeSerial());
	}
	result += "}";
	return result;
}

CWorldBuilderDoc *GetDocument(CommandResult *error)
{
	CWorldBuilderDoc *document = CWorldBuilderDoc::GetActiveDoc();
	if (document == nullptr || document->GetHeightMap() == nullptr) {
		*error = Error("no_map", "WorldBuilder has no active map.");
		return nullptr;
	}
	return document;
}

Bool CheckExpectedRevision(
	const RequestFields &fields, UnsignedInt current_revision, CommandResult *error)
{
	RequestFields::const_iterator revision_it = fields.find("expected_revision");
	if (revision_it == fields.end()) {
		return true;
	}
	Int revision = 0;
	if (!ParseInt(revision_it->second, &revision) || revision < 0) {
		*error = Error("invalid_arguments", "expected_revision must be a non-negative integer.");
		return false;
	}
	if (static_cast<UnsignedInt>(revision) != current_revision) {
		*error = Error("revision_conflict", "The map changed after the client last read it.");
		return false;
	}
	return true;
}

Bool CheckExpectedRevision(const RequestFields &fields, CWorldBuilderDoc *document, CommandResult *error)
{
	return CheckExpectedRevision(fields, document->getChangeSerial(), error);
}

Bool CommandAvailable(CWorldBuilderDoc *document, UINT command)
{
	McpCommandUI command_ui;
	command_ui.m_nID = command;
	return document->OnCmdMsg(command, CN_UPDATE_COMMAND_UI, &command_ui, nullptr)
		&& command_ui.updated
		&& command_ui.enabled;
}

CommandResult GetBridgeInfo()
{
	std::string result = "{\"bridge_version\":";
	result += FormatUnsigned(static_cast<UnsignedInt>(WorldBuilderMcpBridge::BRIDGE_VERSION));
	result += ",\"edition\":" + JsonString(BRIDGE_EDITION) + ",\"commands\":[";
	const char *commands[] = {
		"get_bridge_info", "list_user_maps", "open_map", "save_map", "save_map_as",
		"get_state", "list_objects", "get_object", "list_templates",
		"add_object", "add_objects", "update_object", "update_objects",
		"delete_object", "delete_objects", "select_object",
		"get_terrain_heights", "set_terrain_heights", "focus_view",
		"list_players", "list_teams", "create_team", "update_team", "delete_team",
		"validate_map", "undo", "redo", "new_map", "close_map",
		"list_waypoints", "create_waypoint", "update_waypoint", "delete_waypoint",
		"connect_waypoints", "disconnect_waypoints",
		"list_areas", "create_area", "update_area", "delete_area",
		"list_script_types", "list_sciences", "list_upgrades",
		"list_scripts", "get_script", "upsert_script", "delete_script",
		"list_terrain_textures", "get_terrain_cells", "set_terrain_cells",
		"list_linear_features", "create_linear_feature", "delete_linear_feature",
		"get_playable_areas", "set_playable_areas", "get_map_settings", "update_map_settings",
		"generate_preview", "capture_view"
	};
	for (size_t i = 0; i < ARRAY_SIZE(commands); ++i) {
		if (i != 0) result += ",";
		result += JsonString(commands[i]);
	}
	result += "]}";
	return Success(result);
}

CommandResult ListUserMaps()
{
	std::wstring root;
	if (!GetUserMapsRoot(&root)) {
		return Error("user_maps_unavailable", "WorldBuilder could not resolve the current user's Maps directory.");
	}
	CreateDirectoryW(root.c_str(), nullptr);

	std::wstring pattern(root + L"*");
	WIN32_FIND_DATAW directory_data;
	HANDLE find = FindFirstFileW(pattern.c_str(), &directory_data);
	std::string maps = "[";
	Int count = 0;
	if (find != INVALID_HANDLE_VALUE) {
		do {
			if ((directory_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0
				|| (directory_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0
				|| wcscmp(directory_data.cFileName, L".") == 0
				|| wcscmp(directory_data.cFileName, L"..") == 0) {
				continue;
			}
			std::wstring map_path = root + directory_data.cFileName + L"\\" + directory_data.cFileName + L".map";
			WIN32_FILE_ATTRIBUTE_DATA map_data;
			if (GetFileAttributesExW(map_path.c_str(), GetFileExInfoStandard, &map_data) == FALSE
				|| (map_data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
				continue;
			}
			std::wstring relative_path = std::wstring(directory_data.cFileName) + L"\\" + directory_data.cFileName + L".map";
			std::wstring preview_path = root + directory_data.cFileName + L"\\" + directory_data.cFileName + L".tga";
			std::wstring string_path = root + directory_data.cFileName + L"\\map.str";
			ULARGE_INTEGER file_size;
			file_size.HighPart = map_data.nFileSizeHigh;
			file_size.LowPart = map_data.nFileSizeLow;
			ULARGE_INTEGER write_time;
			write_time.HighPart = map_data.ftLastWriteTime.dwHighDateTime;
			write_time.LowPart = map_data.ftLastWriteTime.dwLowDateTime;
			if (count != 0) maps += ",";
			maps += "{\"name\":" + JsonString(WideToUtf8(directory_data.cFileName));
			maps += ",\"relative_path\":" + JsonString(WideToUtf8(relative_path.c_str()));
			maps += ",\"path\":" + JsonString(WideToUtf8(map_path.c_str()));
			maps += ",\"size\":" + FormatUInt64(file_size.QuadPart);
			maps += ",\"last_write_time\":" + FormatUInt64(write_time.QuadPart);
			maps += ",\"has_preview\":";
			maps += GetFileAttributesW(preview_path.c_str()) != INVALID_FILE_ATTRIBUTES ? "true" : "false";
			maps += ",\"has_map_strings\":";
			maps += GetFileAttributesW(string_path.c_str()) != INVALID_FILE_ATTRIBUTES ? "true" : "false";
			maps += "}";
			++count;
		} while (FindNextFileW(find, &directory_data));
		FindClose(find);
	}
	maps += "]";
	return Success("{\"root\":" + JsonString(WideToUtf8(root.c_str()))
		+ ",\"maps\":" + maps + ",\"total\":" + FormatInt(count) + "}");
}

CommandResult OpenMap(const RequestFields &fields)
{
	RequestFields::const_iterator path_it = fields.find("path");
	if (path_it == fields.end()) {
		return Error("invalid_arguments", "path is required.");
	}
	std::wstring resolved;
	CommandResult resolution = ResolveUserMapPath(path_it->second, false, &resolved);
	if (!resolution.ok) {
		return resolution;
	}

	std::string on_unsaved = "error";
	RequestFields::const_iterator policy_it = fields.find("on_unsaved");
	if (policy_it != fields.end()) {
		on_unsaved = policy_it->second;
	}
	if (on_unsaved != "error" && on_unsaved != "save" && on_unsaved != "discard") {
		return Error("invalid_arguments", "on_unsaved must be error, save, or discard.");
	}

	CString editor_path;
	if (!WideToCString(resolved, &editor_path)) {
		return Error("path_encoding_unsupported", "The map path cannot be represented by this WorldBuilder build.");
	}

	CWorldBuilderDoc *current = CWorldBuilderDoc::GetActiveDoc();
	CString original_path;
	Bool same_path = false;
	if (current != nullptr) {
		original_path = current->GetPathName();
		same_path = !original_path.IsEmpty() && original_path.CompareNoCase(editor_path) == 0;
	}
	Bool restore_modified = false;
	Bool force_reload = false;
	if (current != nullptr && current->IsModified()) {
		if (on_unsaved == "error") {
			return Error("unsaved_changes", "The active map has unsaved changes.");
		}
		if (on_unsaved == "save") {
			CString current_path = current->GetPathName();
			if (current_path.IsEmpty()) {
				return Error("map_has_no_path", "The active map must be saved with save_map_as before switching maps.");
			}
			if (!current->DoSave(current_path, TRUE)) {
				return Error("save_failed", "WorldBuilder failed to save the active map before opening another map.");
			}
		} else {
			current->SetModifiedFlag(FALSE);
			restore_modified = true;
			force_reload = same_path;
		}
	}

	if (same_path && !force_reload) {
		return Success("{\"opened\":true,\"already_open\":true,\"path\":"
			+ JsonString(CStringToUtf8(current->GetPathName()))
			+ ",\"revision\":" + FormatUnsigned(current->getChangeSerial()) + "}");
	}
	// MFC returns an already-open document without reading it again. Invoke the
	// document's normal loader directly when discard targets that same file.
	CDocument *opened = force_reload
		? (current->OnOpenDocument(editor_path) ? current : nullptr)
		: AfxGetApp()->OpenDocumentFile(editor_path);
	if (opened == nullptr) {
		if (restore_modified && current != nullptr) {
			if (force_reload && current->GetPathName().IsEmpty()) {
				current->SetPathName(original_path, FALSE);
			}
			current->SetModifiedFlag(TRUE);
		}
		return Error("open_failed", "WorldBuilder failed to open the requested map.");
	}
	CWorldBuilderDoc *document = CWorldBuilderDoc::GetActiveDoc();
	if (document == nullptr || document->GetHeightMap() == nullptr) {
		return Error("open_failed", "WorldBuilder opened no usable map document.");
	}
	return Success("{\"opened\":true,\"path\":" + JsonString(CStringToUtf8(document->GetPathName()))
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult SaveMapAs(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) {
		return error;
	}
	RequestFields::const_iterator path_it = fields.find("path");
	if (path_it == fields.end()) {
		return Error("invalid_arguments", "path is required.");
	}
	Bool overwrite = false;
	RequestFields::const_iterator overwrite_it = fields.find("overwrite");
	if (overwrite_it != fields.end() && !ParseBool(overwrite_it->second, &overwrite)) {
		return Error("invalid_arguments", "overwrite must be a boolean.");
	}

	std::wstring resolved;
	CommandResult resolution = ResolveUserMapPath(path_it->second, true, &resolved);
	if (!resolution.ok) {
		return resolution;
	}
	DWORD attributes = GetFileAttributesW(resolved.c_str());
	if (attributes != INVALID_FILE_ATTRIBUTES && !overwrite) {
		return Error("map_already_exists", "The destination map already exists; pass overwrite=true to replace it.");
	}
	std::wstring root;
	if (!GetUserMapsRoot(&root) || !EnsureParentDirectories(resolved, root)) {
		return Error("directory_create_failed", "WorldBuilder could not create the map directory.");
	}
	CString editor_path;
	if (!WideToCString(resolved, &editor_path)) {
		return Error("path_encoding_unsupported", "The map path cannot be represented by this WorldBuilder build.");
	}
	if (!document->DoSave(editor_path, TRUE)) {
		return Error("save_failed", "WorldBuilder failed to save the map to the requested path.");
	}
	return Success("{\"path\":" + JsonString(CStringToUtf8(document->GetPathName()))
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult GetState()
{
	CWorldBuilderDoc *document = CWorldBuilderDoc::GetActiveDoc();
	if (document == nullptr || document->GetHeightMap() == nullptr) {
		return Success("{\"map_loaded\":false}");
	}

	WorldHeightMapEdit *height_map = document->GetHeightMap();
	Int object_count = 0;
	Int selected_count = 0;
	for (MapObject *object = MapObject::getFirstMapObject(); object != nullptr; object = object->getNext()) {
		++object_count;
		if (object->isSelected()) {
			++selected_count;
		}
	}

	std::string result = "{\"map_loaded\":true";
	result += ",\"path\":" + JsonString(CStringToUtf8(document->GetPathName()));
	result += ",\"title\":" + JsonString(CStringToUtf8(document->GetTitle()));
	std::wstring user_maps_root;
	result += ",\"user_maps_root\":";
	result += GetUserMapsRoot(&user_maps_root) ? JsonString(WideToUtf8(user_maps_root.c_str())) : "null";
	result += ",\"edition\":" + JsonString(BRIDGE_EDITION) + ",\"bridge_version\":";
	result += FormatUnsigned(static_cast<UnsignedInt>(WorldBuilderMcpBridge::BRIDGE_VERSION));
	result += ",\"revision\":" + FormatUnsigned(document->getChangeSerial());
	result += ",\"modified\":";
	result += document->IsModified() ? "true" : "false";
	result += ",\"can_undo\":";
	result += CommandAvailable(document, ID_EDIT_UNDO) ? "true" : "false";
	result += ",\"can_redo\":";
	result += CommandAvailable(document, ID_EDIT_REDO) ? "true" : "false";
	DWORD path_attributes = GetFileAttributes(document->GetPathName());
	result += ",\"read_only\":";
	result += path_attributes != INVALID_FILE_ATTRIBUTES && (path_attributes & FILE_ATTRIBUTE_READONLY) != 0 ? "true" : "false";
	result += ",\"width\":" + FormatInt(height_map->getXExtent());
	result += ",\"height\":" + FormatInt(height_map->getYExtent());
	result += ",\"border\":" + FormatInt(height_map->getBorderSize());
	result += ",\"cell_size\":" + FormatReal(MAP_XY_FACTOR);
	result += ",\"height_scale\":" + FormatReal(MAP_HEIGHT_SCALE);
	result += ",\"object_count\":" + FormatInt(object_count);
	result += ",\"selected_count\":" + FormatInt(selected_count);
	result += "}";
	return Success(result);
}

CommandResult ListObjects(const RequestFields &fields)
{
	CommandResult error;
	if (GetDocument(&error) == nullptr) {
		return error;
	}

	Int offset = 0;
	Int limit = 100;
	if (!GetOptionalInt(fields, "offset", 0, 0, INT_MAX, &offset)
		|| !GetOptionalInt(fields, "limit", 100, 1, MAX_LIST_RESULTS, &limit)) {
		return Error("invalid_arguments", "offset or limit is outside the supported range.");
	}

	RequestFields::const_iterator filter_it = fields.find("filter");
	std::string lower_filter = filter_it == fields.end() ? std::string() : LowerAscii(filter_it->second);
	RequestFields::const_iterator template_filter = fields.find("template");
	RequestFields::const_iterator owner_filter = fields.find("owner");
	Bool selected_filter = false;
	Bool waypoint_filter = false;
	Bool has_selected_filter = fields.find("selected") != fields.end();
	Bool has_waypoint_filter = fields.find("waypoint") != fields.end();
	if ((has_selected_filter && !ParseBool(fields.find("selected")->second, &selected_filter))
		|| (has_waypoint_filter && !ParseBool(fields.find("waypoint")->second, &waypoint_filter))) {
		return Error("invalid_arguments", "selected and waypoint filters must be booleans.");
	}
	Real min_x = 0.0f;
	Real max_x = 0.0f;
	Real min_y = 0.0f;
	Real max_y = 0.0f;
	Bool has_min_x = false;
	Bool has_max_x = false;
	Bool has_min_y = false;
	Bool has_max_y = false;
	if (!GetOptionalReal(fields, "min_x", &min_x, &has_min_x)
		|| !GetOptionalReal(fields, "max_x", &max_x, &has_max_x)
		|| !GetOptionalReal(fields, "min_y", &min_y, &has_min_y)
		|| !GetOptionalReal(fields, "max_y", &max_y, &has_max_y)) {
		return Error("invalid_arguments", "Bounding-box filters must be finite numbers.");
	}
	Int matched_count = 0;
	Int emitted_count = 0;
	std::string items = "[";
	for (MapObject *object = MapObject::getFirstMapObject(); object != nullptr; object = object->getNext()) {
		std::string id = GetObjectId(object);
		std::string name = AnsiToUtf8(object->getName().str());
		std::string template_name = GetObjectTemplateName(object);
		Bool owner_exists = false;
		std::string owner = AnsiToUtf8(
			object->getProperties()->getAsciiString(TheKey_originalOwner, &owner_exists).str());
		const Coord3D *location = object->getLocation();
		if (!ContainsFilter(id, lower_filter)
			&& !ContainsFilter(name, lower_filter)
			&& !ContainsFilter(template_name, lower_filter)) {
			continue;
		}
		if (template_filter != fields.end() && LowerAscii(template_name) != LowerAscii(template_filter->second)) continue;
		if (owner_filter != fields.end() && (!owner_exists || LowerAscii(owner) != LowerAscii(owner_filter->second))) continue;
		if (has_selected_filter && object->isSelected() != selected_filter) continue;
		if (has_waypoint_filter && object->isWaypoint() != waypoint_filter) continue;
		if ((has_min_x && location->x < min_x) || (has_max_x && location->x > max_x)
			|| (has_min_y && location->y < min_y) || (has_max_y && location->y > max_y)) continue;
		if (matched_count >= offset && emitted_count < limit) {
			if (emitted_count != 0) {
				items += ",";
			}
			items += ObjectToJson(object);
			++emitted_count;
		}
		++matched_count;
	}
	items += "]";

	std::string result = "{\"objects\":" + items;
	result += ",\"total\":" + FormatInt(matched_count);
	result += ",\"offset\":" + FormatInt(offset);
	result += ",\"limit\":" + FormatInt(limit);
	result += ",\"next_offset\":";
	result += offset + emitted_count < matched_count ? FormatInt(offset + emitted_count) : "null";
	result += "}";
	return Success(result);
}

CommandResult GetObject(const RequestFields &fields)
{
	CommandResult error;
	if (GetDocument(&error) == nullptr) {
		return error;
	}
	RequestFields::const_iterator id_it = fields.find("object_id");
	if (id_it == fields.end() || id_it->second.empty()) {
		return Error("invalid_arguments", "object_id is required.");
	}
	MapObject *object = FindObject(id_it->second);
	return object == nullptr
		? Error("object_not_found", "No map object has the requested id.")
		: Success(ObjectToJson(object));
}

// TheSuperHackers @feature Eugene 31/08/2026 Expose native template geometry so placement audits can use
// the editor's actual footprint instead of an independent collision approximation.
std::string GeometryToJson(const GeometryInfo &geometry)
{
	std::string result = "{\"type\":" + FormatInt(static_cast<Int>(geometry.getGeomType()))
		+ ",\"is_small\":" + (geometry.getIsSmall() ? "true" : "false")
		+ ",\"major_radius\":" + FormatReal(geometry.getMajorRadius())
		+ ",\"minor_radius\":" + FormatReal(geometry.getMinorRadius())
		+ ",\"bounding_circle_radius\":" + FormatReal(geometry.getBoundingCircleRadius())
		+ ",\"bounding_sphere_radius\":" + FormatReal(geometry.getBoundingSphereRadius())
		+ ",\"footprint_area\":" + FormatReal(geometry.getFootprintArea())
		+ ",\"max_height_above_position\":" + FormatReal(geometry.getMaxHeightAbovePosition())
		+ ",\"max_height_below_position\":" + FormatReal(geometry.getMaxHeightBelowPosition()) + "}";
	return result;
}

CommandResult ListTemplates(const RequestFields &fields)
{
	if (TheThingFactory == nullptr) {
		return Error("templates_unavailable", "WorldBuilder has not initialized the ThingTemplate catalog.");
	}

	Int offset = 0;
	Int limit = 100;
	if (!GetOptionalInt(fields, "offset", 0, 0, INT_MAX, &offset)
		|| !GetOptionalInt(fields, "limit", 100, 1, MAX_LIST_RESULTS, &limit)) {
		return Error("invalid_arguments", "offset or limit is outside the supported range.");
	}

	RequestFields::const_iterator filter_it = fields.find("filter");
	std::string lower_filter = filter_it == fields.end() ? std::string() : LowerAscii(filter_it->second);
	Int matched_count = 0;
	Int emitted_count = 0;
	std::string items = "[";
	for (const ThingTemplate *thing = TheThingFactory->firstTemplate();
			 thing != nullptr;
			 thing = thing->friend_getNextTemplate()) {
		std::string name = AnsiToUtf8(thing->getName().str());
		if (!ContainsFilter(name, lower_filter)) {
			continue;
		}
		if (matched_count >= offset && emitted_count < limit) {
			if (emitted_count != 0) {
				items += ",";
			}
			items += "{\"name\":" + JsonString(name)
				+ ",\"default_side\":" + JsonString(AnsiToUtf8(thing->getDefaultOwningSide().str()))
				+ ",\"geometry\":" + GeometryToJson(thing->getTemplateGeometryInfo()) + "}";
			++emitted_count;
		}
		++matched_count;
	}
	items += "]";

	std::string result = "{\"templates\":" + items;
	result += ",\"total\":" + FormatInt(matched_count);
	result += ",\"offset\":" + FormatInt(offset);
	result += ",\"limit\":" + FormatInt(limit);
	result += ",\"next_offset\":";
	result += offset + emitted_count < matched_count ? FormatInt(offset + emitted_count) : "null";
	result += "}";
	return Success(result);
}

CommandResult AddObject(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) {
		return error;
	}

	RequestFields::const_iterator template_it = fields.find("template");
	Real x = 0.0f;
	Real y = 0.0f;
	if (template_it == fields.end() || template_it->second.empty()
		|| !GetRequiredReal(fields, "x", &x) || !GetRequiredReal(fields, "y", &y)) {
		return Error("invalid_arguments", "template, x, and y are required.");
	}

	WorldHeightMapEdit *height_map = document->GetHeightMap();
	Real maximum_x = (height_map->getXExtent() - 1) * MAP_XY_FACTOR;
	Real maximum_y = (height_map->getYExtent() - 1) * MAP_XY_FACTOR;
	if (x < 0.0f || y < 0.0f || x > maximum_x || y > maximum_y) {
		return Error("out_of_bounds", "The object location is outside the map.");
	}

	Int object_count = 0;
	for (MapObject *current = MapObject::getFirstMapObject(); current != nullptr; current = current->getNext()) {
		if (!current->isWaypoint()
			&& (current->getFlags() & (FLAG_ROAD_FLAGS | FLAG_BRIDGE_FLAGS)) == 0) {
			++object_count;
		}
	}
	if (object_count >= MAX_OBJECTS_IN_MAP) {
		return Error("object_limit_reached", "The map already contains the maximum number of objects.");
	}

	Real z = MAGIC_GROUND_Z;
	Real angle = 0.0f;
	Bool has_z = false;
	Bool has_angle = false;
	if (!GetOptionalReal(fields, "z", &z, &has_z)
		|| !GetOptionalReal(fields, "angle", &angle, &has_angle)) {
		return Error("invalid_arguments", "z or angle is not a finite number.");
	}

	AsciiString template_name(template_it->second.c_str());
	const ThingTemplate *thing = TheThingFactory->findTemplate(template_name, false);
	if (thing == nullptr) {
		return Error("template_not_found", "No ThingTemplate has the requested name.");
	}

	AsciiString owner_name("team");
	RequestFields::const_iterator owner_it = fields.find("owner");
	if (owner_it != fields.end()) {
		owner_name.set(owner_it->second.c_str());
	}
	if (!IsValidOwner(owner_name)) {
		return Error("owner_not_found", "No map or runtime player team has the requested owner name.");
	}

	Coord3D location = { x, y, z };
	MapObject *prototype = ObjectOptions::getObjectNamed(template_name);
	MapObject *object = newInstance(MapObject)(
		location,
		template_name,
		angle * PI / 180.0f,
		prototype != nullptr ? prototype->getFlags() : 0,
		prototype != nullptr ? prototype->getProperties() : nullptr,
		thing);
	object->getProperties()->setAsciiString(TheKey_originalOwner, owner_name);
	object->getProperties()->setBool(TheKey_objectTargetable, true);
	if (prototype != nullptr) object->setColor(prototype->getColor());

	AddObjectUndoable *undoable = new AddObjectUndoable(document, object);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	document->updateAllViews();
	return Success(ObjectToJson(object, document));
}

std::string IndexedField(const char *prefix, Int index, const char *name)
{
	char buffer[96];
	_snprintf(buffer, sizeof(buffer), "%s%d_%s", prefix, index, name);
	buffer[sizeof(buffer) - 1] = '\0';
	return std::string(buffer);
}

CommandResult AddObjects(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) {
		return error;
	}
	Int count = 0;
	if (!GetRequiredInt(fields, "item_count", 1, 500, &count)) {
		return Error("invalid_arguments", "items must contain between 1 and 500 objects.");
	}

	Int existing_count = 0;
	for (MapObject *current = MapObject::getFirstMapObject(); current != nullptr; current = current->getNext()) {
		if (!current->isWaypoint() && (current->getFlags() & (FLAG_ROAD_FLAGS | FLAG_BRIDGE_FLAGS)) == 0) {
			++existing_count;
		}
	}
	if (existing_count + count > MAX_OBJECTS_IN_MAP) {
		return Error("object_limit_reached", "The requested objects would exceed the map object limit.");
	}

	WorldHeightMapEdit *height_map = document->GetHeightMap();
	MapObject *first = nullptr;
	MapObject *last = nullptr;
	for (Int i = 0; i < count; ++i) {
		RequestFields::const_iterator template_it = fields.find(IndexedField("item", i, "template"));
		Real x = 0.0f;
		Real y = 0.0f;
		RequestFields::const_iterator x_it = fields.find(IndexedField("item", i, "x"));
		RequestFields::const_iterator y_it = fields.find(IndexedField("item", i, "y"));
		if (template_it == fields.end() || template_it->second.empty()
			|| x_it == fields.end() || y_it == fields.end()
			|| !ParseReal(x_it->second, &x) || !ParseReal(y_it->second, &y)) {
			if (first != nullptr) first->deleteInstance();
			return Error("invalid_arguments", "Every object needs template, x, and y.");
		}
		if (x < 0.0f || y < 0.0f
			|| x > (height_map->getXExtent() - 1) * MAP_XY_FACTOR
			|| y > (height_map->getYExtent() - 1) * MAP_XY_FACTOR) {
			if (first != nullptr) first->deleteInstance();
			return Error("out_of_bounds", "An object location is outside the map.");
		}
		const ThingTemplate *thing =
			TheThingFactory->findTemplate(AsciiString(template_it->second.c_str()), false);
		if (thing == nullptr) {
			if (first != nullptr) first->deleteInstance();
			return Error("template_not_found", "An object references an unknown ThingTemplate.");
		}
		Real z = MAGIC_GROUND_Z;
		Real angle = 0.0f;
		RequestFields::const_iterator z_it = fields.find(IndexedField("item", i, "z"));
		RequestFields::const_iterator angle_it = fields.find(IndexedField("item", i, "angle"));
		if ((z_it != fields.end() && !ParseReal(z_it->second, &z))
			|| (angle_it != fields.end() && !ParseReal(angle_it->second, &angle))) {
			if (first != nullptr) first->deleteInstance();
			return Error("invalid_arguments", "Object z and angle values must be finite numbers.");
		}
		AsciiString owner("team");
		RequestFields::const_iterator owner_it = fields.find(IndexedField("item", i, "owner"));
		if (owner_it != fields.end()) owner.set(owner_it->second.c_str());
		if (!IsValidOwner(owner)) {
			if (first != nullptr) first->deleteInstance();
			return Error("owner_not_found", "An object references an unknown map or runtime player team.");
		}
		Coord3D location = { x, y, z };
		AsciiString template_name(template_it->second.c_str());
		MapObject *prototype = ObjectOptions::getObjectNamed(template_name);
		MapObject *object = newInstance(MapObject)(
			location,
			template_name,
			angle * PI / 180.0f,
			prototype != nullptr ? prototype->getFlags() : 0,
			prototype != nullptr ? prototype->getProperties() : nullptr,
			thing);
		object->getProperties()->setAsciiString(TheKey_originalOwner, owner);
		object->getProperties()->setBool(TheKey_objectTargetable, true);
		if (prototype != nullptr) object->setColor(prototype->getColor());
		if (first == nullptr) first = object;
		else last->setNextMap(object);
		last = object;
	}

	AddObjectUndoable *undoable = new AddObjectUndoable(document, first);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	std::string objects = "[";
	MapObject *current = first;
	for (Int i = 0; i < count && current != nullptr; ++i, current = current->getNext()) {
		if (i != 0) objects += ",";
		objects += ObjectToJson(current);
	}
	objects += "]";
	document->updateAllViews();
	return Success("{\"objects\":" + objects + ",\"added\":" + FormatInt(count)
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult UpdateObject(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) {
		return error;
	}

	RequestFields::const_iterator id_it = fields.find("object_id");
	if (id_it == fields.end() || id_it->second.empty()) {
		return Error("invalid_arguments", "object_id is required.");
	}
	MapObject *object = FindObject(id_it->second);
	if (object == nullptr) {
		return Error("object_not_found", "No map object has the requested id.");
	}

	Real x = 0.0f;
	Real y = 0.0f;
	Real z = 0.0f;
	Real angle = 0.0f;
	Bool has_x = false;
	Bool has_y = false;
	Bool has_z = false;
	Bool has_angle = false;
	RequestFields::const_iterator template_it = fields.find("template");
	RequestFields::const_iterator owner_it = fields.find("owner");
	RequestFields::const_iterator veterancy_it = fields.find("veterancy");
		RequestFields::const_iterator script_name_it = fields.find("script_name");
	Bool has_template = template_it != fields.end();
	Bool has_owner = owner_it != fields.end();
	Bool has_veterancy = veterancy_it != fields.end();
		Bool has_script_name = script_name_it != fields.end();
	Int veterancy = 0;
	std::set<std::string> script_names;
		if (!GetOptionalReal(fields, "x", &x, &has_x)
		|| !GetOptionalReal(fields, "y", &y, &has_y)
		|| !GetOptionalReal(fields, "z", &z, &has_z)
		|| !GetOptionalReal(fields, "angle", &angle, &has_angle)) {
		return Error("invalid_arguments", "Transform values must be finite numbers.");
	}
	if ((has_veterancy && (!ParseInt(veterancy_it->second, &veterancy) || veterancy < 0 || veterancy > 3))) {
		return Error("invalid_arguments", "veterancy must be 0..3.");
	}
	AsciiString new_script_name;
	if (has_script_name) {
		if (script_name_it->second.empty() || script_name_it->second.size() > 128) {
			return Error("invalid_arguments", "script_name must contain 1 to 128 characters.");
		}
		new_script_name.set(script_name_it->second.c_str());
		if (!IsUniqueScriptName(new_script_name, object)
				|| !script_names.insert(LowerAscii(AnsiToUtf8(new_script_name.str()))).second) {
			return Error("duplicate_script_name", "script_name is already assigned to another map object.");
		}
	}
	if (!has_x && !has_y && !has_z && !has_angle && !has_template && !has_owner
		&& !has_veterancy && !has_script_name) {
		return Error("invalid_arguments", "At least one object field is required.");
	}

	const ThingTemplate *new_template = nullptr;
	if (has_template) {
		if (template_it->second.empty()) {
			return Error("invalid_arguments", "template must not be empty.");
		}
		if (object->getThingTemplate() == nullptr) {
			return Error("unsupported_object_type", "This map object cannot change its template.");
		}
		new_template = TheThingFactory->findTemplate(AsciiString(template_it->second.c_str()), false);
		if (new_template == nullptr) {
			return Error("template_not_found", "No ThingTemplate has the requested name.");
		}
	}

	const Coord3D *old_location = object->getLocation();
	Real target_x = has_x ? x : old_location->x;
	Real target_y = has_y ? y : old_location->y;
	AsciiString new_owner;
	if (has_owner) {
		new_owner.set(owner_it->second.c_str());
		if (!IsValidOwner(new_owner)) {
			return Error("owner_not_found", "No map or runtime player team has the requested owner name.");
		}
	}
	WorldHeightMapEdit *height_map = document->GetHeightMap();
	if (target_x < 0.0f || target_y < 0.0f
		|| target_x > (height_map->getXExtent() - 1) * MAP_XY_FACTOR
		|| target_y > (height_map->getYExtent() - 1) * MAP_XY_FACTOR) {
		return Error("out_of_bounds", "The object location is outside the map.");
	}

	Coord3D new_location = *old_location;
	new_location.x = target_x;
	new_location.y = target_y;
	if (has_z) {
		new_location.z = z;
	}
	Real new_angle = has_angle ? angle * PI / 180.0f : object->getAngle();
	McpModifyObjectUndoable *undoable = new McpModifyObjectUndoable(
		document,
		object,
		new_location,
		new_angle,
		has_template ? new_template : object->getThingTemplate(),
		has_template,
		new_owner,
		has_owner,
		veterancy,
		has_veterancy,
		new_script_name, has_script_name);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	document->updateAllViews();
	return Success(ObjectToJson(object, document));
}

CommandResult UpdateObjects(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) {
		return error;
	}
	Int count = 0;
	if (!GetRequiredInt(fields, "item_count", 1, 500, &count)) {
		return Error("invalid_arguments", "items must contain between 1 and 500 updates.");
	}
	McpCompositeUndoable *composite = new McpCompositeUndoable();
	std::vector<MapObject *> objects;
	std::set<MapObject *> seen;
	std::set<std::string> script_names;
	WorldHeightMapEdit *height_map = document->GetHeightMap();
	for (Int i = 0; i < count; ++i) {
		RequestFields::const_iterator id_it = fields.find(IndexedField("item", i, "object_id"));
		if (id_it == fields.end() || id_it->second.empty()) {
			REF_PTR_RELEASE(composite);
			return Error("invalid_arguments", "Every update needs object_id.");
		}
		MapObject *object = FindObject(id_it->second);
		if (object == nullptr) {
			REF_PTR_RELEASE(composite);
			return Error("object_not_found", "An update references an unknown object.");
		}
		if (!seen.insert(object).second) {
			REF_PTR_RELEASE(composite);
			return Error("invalid_arguments", "The same object cannot appear twice in one update batch.");
		}
		Real x = 0.0f;
		Real y = 0.0f;
		Real z = 0.0f;
		Real angle = 0.0f;
		RequestFields::const_iterator x_it = fields.find(IndexedField("item", i, "x"));
		RequestFields::const_iterator y_it = fields.find(IndexedField("item", i, "y"));
		RequestFields::const_iterator z_it = fields.find(IndexedField("item", i, "z"));
		RequestFields::const_iterator angle_it = fields.find(IndexedField("item", i, "angle"));
		RequestFields::const_iterator template_it = fields.find(IndexedField("item", i, "template"));
		RequestFields::const_iterator owner_it = fields.find(IndexedField("item", i, "owner"));
		RequestFields::const_iterator veterancy_it = fields.find(IndexedField("item", i, "veterancy"));
		RequestFields::const_iterator script_name_it = fields.find(IndexedField("item", i, "script_name"));
		Bool has_x = x_it != fields.end();
		Bool has_y = y_it != fields.end();
		Bool has_z = z_it != fields.end();
		Bool has_angle = angle_it != fields.end();
		Bool has_template = template_it != fields.end();
		Bool has_owner = owner_it != fields.end();
		Bool has_veterancy = veterancy_it != fields.end();
		Bool has_script_name = script_name_it != fields.end();
		Int veterancy = 0;
		AsciiString new_script_name;
		if (has_script_name) {
			if (script_name_it->second.empty() || script_name_it->second.size() > 128) {
				return Error("invalid_arguments", "script_name must contain 1 to 128 characters.");
			}
			new_script_name.set(script_name_it->second.c_str());
			if (!IsUniqueScriptName(new_script_name, object)
				|| !script_names.insert(LowerAscii(AnsiToUtf8(new_script_name.str()))).second) {
				return Error("duplicate_script_name", "script_name is already assigned to another map object.");
			}
		}
		if (!has_x && !has_y && !has_z && !has_angle && !has_template && !has_owner
			&& !has_veterancy && !has_script_name) {
			REF_PTR_RELEASE(composite);
			return Error("invalid_arguments", "Every update needs at least one changed field.");
		}
		if ((has_x && !ParseReal(x_it->second, &x))
			|| (has_y && !ParseReal(y_it->second, &y))
			|| (has_z && !ParseReal(z_it->second, &z))
			|| (has_angle && !ParseReal(angle_it->second, &angle))) {
			REF_PTR_RELEASE(composite);
			return Error("invalid_arguments", "Object transform values must be finite numbers.");
		}
		if ((has_veterancy && (!ParseInt(veterancy_it->second, &veterancy) || veterancy < 0 || veterancy > 3))) {
			REF_PTR_RELEASE(composite);
			return Error("invalid_arguments", "veterancy must be 0..3.");
		}
		const Coord3D *old_location = object->getLocation();
		Coord3D new_location = *old_location;
		if (has_x) new_location.x = x;
		if (has_y) new_location.y = y;
		if (has_z) new_location.z = z;
		if (new_location.x < 0.0f || new_location.y < 0.0f
			|| new_location.x > (height_map->getXExtent() - 1) * MAP_XY_FACTOR
			|| new_location.y > (height_map->getYExtent() - 1) * MAP_XY_FACTOR) {
			REF_PTR_RELEASE(composite);
			return Error("out_of_bounds", "An updated object location is outside the map.");
		}
		const ThingTemplate *new_template = object->getThingTemplate();
		if (has_template) {
			if (template_it->second.empty() || object->getThingTemplate() == nullptr) {
				REF_PTR_RELEASE(composite);
				return Error("unsupported_object_type", "An object cannot change to the requested template.");
			}
			new_template = TheThingFactory->findTemplate(AsciiString(template_it->second.c_str()), false);
			if (new_template == nullptr) {
				REF_PTR_RELEASE(composite);
				return Error("template_not_found", "An update references an unknown ThingTemplate.");
			}
		}
		AsciiString new_owner;
		if (has_owner) {
			new_owner.set(owner_it->second.c_str());
			if (!IsValidOwner(new_owner)) {
				REF_PTR_RELEASE(composite);
				return Error("owner_not_found", "An update references an unknown map or runtime player team.");
			}
		}
		McpModifyObjectUndoable *child = new McpModifyObjectUndoable(
			document, object, new_location,
			has_angle ? angle * PI / 180.0f : object->getAngle(),
			new_template, has_template, new_owner, has_owner,
			veterancy, has_veterancy, new_script_name, has_script_name);
		composite->Add(child);
		REF_PTR_RELEASE(child);
		objects.push_back(object);
	}
	document->AddAndDoUndoable(composite);
	REF_PTR_RELEASE(composite);
	document->updateAllViews();
	std::string result = "{\"objects\":[";
	for (size_t i = 0; i < objects.size(); ++i) {
		if (i != 0) result += ",";
		result += ObjectToJson(objects[i]);
	}
	result += "],\"updated\":" + FormatInt(count)
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}";
	return Success(result);
}

CommandResult DeleteObject(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) {
		return error;
	}

	RequestFields::const_iterator id_it = fields.find("object_id");
	if (id_it == fields.end() || id_it->second.empty()) {
		return Error("invalid_arguments", "object_id is required.");
	}
	MapObject *object = FindObject(id_it->second);
	if (object == nullptr) {
		return Error("object_not_found", "No map object has the requested id.");
	}

	std::string deleted_id = GetObjectId(object);
	PointerTool::clearSelection();
	object->setSelected(true);
	DeleteObjectUndoable *undoable = new DeleteObjectUndoable(document);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	document->updateAllViews();
	// TheSuperHackers @bugfix Eugene 30/08/2026 Return the delete revision for expected_revision chaining.
	return Success("{\"deleted_id\":" + JsonString(deleted_id)
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult DeleteObjects(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) {
		return error;
	}
	Int count = 0;
	if (!GetRequiredInt(fields, "item_count", 1, 500, &count)) {
		return Error("invalid_arguments", "object_ids must contain between 1 and 500 ids.");
	}
	std::vector<MapObject *> objects;
	std::vector<MapObject *> previously_selected;
	std::set<MapObject *> seen;
	for (MapObject *object = MapObject::getFirstMapObject(); object != nullptr; object = object->getNext()) {
		if (object->isSelected()) previously_selected.push_back(object);
	}
	for (Int i = 0; i < count; ++i) {
		RequestFields::const_iterator id_it = fields.find(IndexedField("item", i, "object_id"));
		MapObject *object = id_it == fields.end() ? nullptr : FindObject(id_it->second);
		if (object == nullptr) {
			return Error("object_not_found", "A delete batch references an unknown object.");
		}
		if (!seen.insert(object).second) {
			return Error("invalid_arguments", "The same object cannot appear twice in one delete batch.");
		}
		objects.push_back(object);
	}
	PointerTool::clearSelection();
	for (size_t i = 0; i < objects.size(); ++i) objects[i]->setSelected(true);
	DeleteObjectUndoable *undoable = new DeleteObjectUndoable(document);
	PointerTool::clearSelection();
	for (size_t i = 0; i < previously_selected.size(); ++i) previously_selected[i]->setSelected(true);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	document->updateAllViews();
	return Success("{\"deleted\":" + FormatInt(count)
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult SelectObject(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) {
		return error;
	}

	RequestFields::const_iterator id_it = fields.find("object_id");
	if (id_it == fields.end() || id_it->second.empty()) {
		return Error("invalid_arguments", "object_id is required.");
	}
	MapObject *object = FindObject(id_it->second);
	if (object == nullptr) {
		return Error("object_not_found", "No map object has the requested id.");
	}

	PointerTool::clearSelection();
	object->setSelected(true);
	document->invalObject(object);
	const Coord3D *location = object->getLocation();
	if (document->Get2DView() != nullptr) {
		document->Get2DView()->setCenterInView(location->x / MAP_XY_FACTOR, location->y / MAP_XY_FACTOR);
	}
	if (document->Get3DView() != nullptr) {
		document->Get3DView()->setCenterInView(location->x / MAP_XY_FACTOR, location->y / MAP_XY_FACTOR);
	}
	return Success(ObjectToJson(object));
}

CommandResult ListPlayers()
{
	CommandResult error;
	if (GetDocument(&error) == nullptr) return error;
	if (TheSidesList == nullptr) return Error("players_unavailable", "The map player list is unavailable.");
	std::string players = "[";
	for (Int i = 0; i < TheSidesList->getNumSides(); ++i) {
		Dict *dict = TheSidesList->getSideInfo(i)->getDict();
		if (i != 0) players += ",";
		players += "{\"index\":" + FormatInt(i);
		players += ",\"name\":" + JsonString(AnsiToUtf8(dict->getAsciiString(TheKey_playerName).str()));
		players += ",\"faction\":" + JsonString(AnsiToUtf8(dict->getAsciiString(TheKey_playerFaction).str()));
		players += ",\"human\":";
		players += dict->getBool(TheKey_playerIsHuman) ? "true" : "false";
		players += ",\"allies\":" + JsonString(AnsiToUtf8(dict->getAsciiString(TheKey_playerAllies).str()));
		players += ",\"enemies\":" + JsonString(AnsiToUtf8(dict->getAsciiString(TheKey_playerEnemies).str()));
		players += "}";
	}
	players += "]";
	return Success("{\"players\":" + players + ",\"total\":" + FormatInt(TheSidesList->getNumSides()) + "}");
}

CommandResult ListTeams()
{
	CommandResult error;
	if (GetDocument(&error) == nullptr) return error;
	if (TheSidesList == nullptr) return Error("teams_unavailable", "The map team list is unavailable.");
	std::string teams = "[";
	for (Int i = 0; i < TheSidesList->getNumTeams(); ++i) {
		Dict *dict = TheSidesList->getTeamInfo(i)->getDict();
		if (i != 0) teams += ",";
		teams += "{\"index\":" + FormatInt(i);
		teams += ",\"name\":" + JsonString(AnsiToUtf8(dict->getAsciiString(TheKey_teamName).str()));
		teams += ",\"owner\":" + JsonString(AnsiToUtf8(dict->getAsciiString(TheKey_teamOwner).str()));
		teams += ",\"singleton\":";
		teams += dict->getBool(TheKey_teamIsSingleton) ? "true" : "false";
		teams += ",\"object_count\":"
			+ FormatInt(MapObject::countMapObjectsWithOwner(dict->getAsciiString(TheKey_teamName)));
		teams += "}";
	}
	teams += "]";
	return Success("{\"teams\":" + teams + ",\"total\":" + FormatInt(TheSidesList->getNumTeams()) + "}");
}

CommandResult CreateTeam(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	RequestFields::const_iterator name_it = fields.find("name");
	RequestFields::const_iterator owner_it = fields.find("owner");
	if (name_it == fields.end() || name_it->second.empty() || owner_it == fields.end()) {
		return Error("invalid_arguments", "name and owner are required.");
	}
	AsciiString name(name_it->second.c_str());
	AsciiString owner(owner_it->second.c_str());
	if (TheSidesList->findTeamInfo(name) != nullptr) {
		return Error("team_already_exists", "A team with this name already exists.");
	}
	if (TheSidesList->findSideInfo(owner) == nullptr) {
		return Error("player_not_found", "No map player has the requested owner name.");
	}
	Bool singleton = false;
	RequestFields::const_iterator singleton_it = fields.find("singleton");
	if (singleton_it != fields.end() && !ParseBool(singleton_it->second, &singleton)) {
		return Error("invalid_arguments", "singleton must be a boolean.");
	}
	Dict dict;
	dict.setAsciiString(TheKey_teamName, name);
	dict.setAsciiString(TheKey_teamOwner, owner);
	dict.setBool(TheKey_teamIsSingleton, singleton);
	SidesList updated = *TheSidesList;
	updated.addTeam(&dict);
	updated.validateSides();
	SidesListUndoable *undoable = new SidesListUndoable(updated, document);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	return Success("{\"name\":" + JsonString(name_it->second)
		+ ",\"owner\":" + JsonString(owner_it->second)
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult UpdateTeam(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	RequestFields::const_iterator name_it = fields.find("name");
	if (name_it == fields.end() || name_it->second.empty()) {
		return Error("invalid_arguments", "name is required.");
	}
	SidesList updated = *TheSidesList;
	TeamsInfo *team = updated.findTeamInfo(AsciiString(name_it->second.c_str()));
	if (team == nullptr) return Error("team_not_found", "No map team has the requested name.");
	Dict *dict = team->getDict();
	RequestFields::const_iterator owner_it = fields.find("owner");
	RequestFields::const_iterator singleton_it = fields.find("singleton");
	if (owner_it == fields.end() && singleton_it == fields.end()) {
		return Error("invalid_arguments", "update_team needs owner or singleton.");
	}
	if (owner_it != fields.end()) {
		AsciiString owner(owner_it->second.c_str());
		if (updated.findSideInfo(owner) == nullptr) {
			return Error("player_not_found", "No map player has the requested owner name.");
		}
		dict->setAsciiString(TheKey_teamOwner, owner);
	}
	if (singleton_it != fields.end()) {
		Bool singleton = false;
		if (!ParseBool(singleton_it->second, &singleton)) {
			return Error("invalid_arguments", "singleton must be a boolean.");
		}
		dict->setBool(TheKey_teamIsSingleton, singleton);
	}
	updated.validateSides();
	SidesListUndoable *undoable = new SidesListUndoable(updated, document);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	return Success("{\"name\":" + JsonString(name_it->second)
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult DeleteTeam(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	RequestFields::const_iterator name_it = fields.find("name");
	if (name_it == fields.end() || name_it->second.empty()) {
		return Error("invalid_arguments", "name is required.");
	}
	AsciiString name(name_it->second.c_str());
	Int index = -1;
	TeamsInfo *team = TheSidesList->findTeamInfo(name, &index);
	if (team == nullptr) return Error("team_not_found", "No map team has the requested name.");
	if (team->getDict()->getBool(TheKey_teamIsSingleton)) {
		return Error("default_team_protected", "Player default teams cannot be deleted.");
	}
	if (MapObject::countMapObjectsWithOwner(name) != 0) {
		return Error("team_in_use", "Move or delete the team's map objects before deleting the team.");
	}
	SidesList updated = *TheSidesList;
	updated.removeTeam(index);
	updated.validateSides();
	SidesListUndoable *undoable = new SidesListUndoable(updated, document);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	return Success("{\"deleted\":" + JsonString(name_it->second)
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

void AddValidationIssue(
	std::string *issues, Int *count, const char *severity, const char *code,
	const std::string &message, const std::string &object_id)
{
	if (*count != 0) *issues += ",";
	*issues += "{\"severity\":" + JsonString(severity)
		+ ",\"code\":" + JsonString(code)
		+ ",\"message\":" + JsonString(message);
	if (!object_id.empty()) *issues += ",\"object_id\":" + JsonString(object_id);
	*issues += "}";
	++*count;
}

CommandResult ValidateMap()
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr) return error;
	WorldHeightMapEdit *height_map = document->GetHeightMap();
	std::map<std::string, Int> id_counts;
	for (MapObject *object = MapObject::getFirstMapObject(); object != nullptr; object = object->getNext()) {
		std::string persistent = GetPersistentObjectId(object);
		if (!persistent.empty()) ++id_counts[persistent];
	}
	std::string issues = "[";
	Int issue_count = 0;
	Int error_count = 0;
	Int warning_count = 0;
	std::set<Int> start_positions;
	for (MapObject *object = MapObject::getFirstMapObject(); object != nullptr; object = object->getNext()) {
		std::string persistent = GetPersistentObjectId(object);
		std::string object_id = GetObjectId(object);
		if (persistent.empty() && !object->isWaypoint()) {
			AddValidationIssue(&issues, &issue_count, "warning", "missing_unique_id",
				"Object has no persistent uniqueID.", object_id);
			++warning_count;
		} else if (!persistent.empty() && id_counts[persistent] > 1) {
			AddValidationIssue(&issues, &issue_count, "error", "duplicate_unique_id",
				"Multiple objects share the same uniqueID.", object_id);
			++error_count;
		}
		const Coord3D *location = object->getLocation();
		if (location->x < 0.0f || location->y < 0.0f
			|| location->x > (height_map->getXExtent() - 1) * MAP_XY_FACTOR
			|| location->y > (height_map->getYExtent() - 1) * MAP_XY_FACTOR) {
			AddValidationIssue(&issues, &issue_count, "error", "object_out_of_bounds",
				"Object is outside the height map.", object_id);
			++error_count;
		}
		Bool owner_exists = false;
		AsciiString owner = object->getProperties()->getAsciiString(TheKey_originalOwner, &owner_exists);
		if (owner_exists && !IsValidOwner(owner)) {
			AddValidationIssue(&issues, &issue_count, "error", "unknown_owner",
				"Object references neither a map team nor a runtime player team.", object_id);
			++error_count;
		}
		if (object->getThingTemplate() == nullptr && !object->isWaypoint()
			&& (object->getFlags() & (FLAG_ROAD_FLAGS | FLAG_BRIDGE_FLAGS)) == 0) {
			AddValidationIssue(&issues, &issue_count, "warning", "unknown_template",
				"Object has no resolved ThingTemplate.", object_id);
			++warning_count;
		}
		if (object->isWaypoint()) {
			std::string waypoint_name = AnsiToUtf8(object->getWaypointName().str());
			const char *prefix = "Player_";
			const char *suffix = "_Start";
			if (waypoint_name.find(prefix) == 0
				&& waypoint_name.size() > strlen(prefix) + strlen(suffix)
				&& waypoint_name.rfind(suffix) == waypoint_name.size() - strlen(suffix)) {
				Int number = atoi(waypoint_name.substr(strlen(prefix), waypoint_name.size() - strlen(prefix) - strlen(suffix)).c_str());
				if (number > 0) start_positions.insert(number);
			}
		}
	}
	Int human_players = 0;
	for (Int i = 0; i < TheSidesList->getNumSides(); ++i) {
		Dict *dict = TheSidesList->getSideInfo(i)->getDict();
		if (dict->getBool(TheKey_playerIsHuman)) ++human_players;
	}
	for (Int i = 1; i <= human_players; ++i) {
		if (start_positions.find(i) == start_positions.end()) {
			AddValidationIssue(&issues, &issue_count, "warning", "missing_start_position",
				"Human player has no matching Player_N_Start waypoint.", std::string());
			++warning_count;
		}
	}
	issues += "]";
	return Success("{\"valid\":" + std::string(error_count == 0 ? "true" : "false")
		+ ",\"errors\":" + FormatInt(error_count)
		+ ",\"warnings\":" + FormatInt(warning_count)
		+ ",\"issues\":" + issues
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult GetTerrainHeights(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr) {
		return error;
	}

	Int x = 0;
	Int y = 0;
	Int width = 0;
	Int height = 0;
	if (!GetRequiredInt(fields, "x", 0, INT_MAX, &x)
		|| !GetRequiredInt(fields, "y", 0, INT_MAX, &y)
		|| !GetRequiredInt(fields, "width", 1, MAX_TERRAIN_SAMPLES, &width)
		|| !GetRequiredInt(fields, "height", 1, MAX_TERRAIN_SAMPLES, &height)
		|| width > MAX_TERRAIN_SAMPLES / height) {
		return Error("invalid_arguments", "x, y, width, and height must describe at most 4096 cells.");
	}

	WorldHeightMapEdit *height_map = document->GetHeightMap();
	if (x > height_map->getXExtent() - width || y > height_map->getYExtent() - height) {
		return Error("out_of_bounds", "The requested terrain rectangle is outside the height map.");
	}

	std::string rows = "[";
	for (Int row = 0; row < height; ++row) {
		if (row != 0) {
			rows += ",";
		}
		rows += "[";
		for (Int column = 0; column < width; ++column) {
			if (column != 0) {
				rows += ",";
			}
			rows += FormatUnsigned(height_map->getHeight(x + column, y + row));
		}
		rows += "]";
	}
	rows += "]";

	std::string result = "{\"x\":" + FormatInt(x) + ",\"y\":" + FormatInt(y)
		+ ",\"width\":" + FormatInt(width) + ",\"height\":" + FormatInt(height)
		+ ",\"heights\":" + rows + "}";
	return Success(result);
}

Bool ParseTerrainPoints(const std::string &text, std::vector<ICoord3D> *points)
{
	points->clear();
	if (text.empty()) {
		return false;
	}

	size_t start = 0;
	while (start <= text.size()) {
		size_t end = text.find(';', start);
		if (end == std::string::npos) {
			end = text.size();
		}
		std::string point = text.substr(start, end - start);
		size_t first_comma = point.find(',');
		size_t second_comma = first_comma == std::string::npos
			? std::string::npos
			: point.find(',', first_comma + 1);
		if (first_comma == std::string::npos || second_comma == std::string::npos
			|| point.find(',', second_comma + 1) != std::string::npos) {
			return false;
		}

		ICoord3D parsed;
		if (!ParseInt(point.substr(0, first_comma), &parsed.x)
			|| !ParseInt(point.substr(first_comma + 1, second_comma - first_comma - 1), &parsed.y)
			|| !ParseInt(point.substr(second_comma + 1), &parsed.z)) {
			return false;
		}
		points->push_back(parsed);
		if (points->size() > MAX_TERRAIN_SAMPLES) {
			return false;
		}
		if (end == text.size()) {
			break;
		}
		start = end + 1;
	}
	return !points->empty();
}

CommandResult SetTerrainHeights(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) {
		return error;
	}

	RequestFields::const_iterator points_it = fields.find("points");
	std::vector<ICoord3D> points;
	if (points_it == fields.end() || !ParseTerrainPoints(points_it->second, &points)) {
		return Error("invalid_arguments", "points must contain 1 to 4096 x,y,height triples.");
	}

	WorldHeightMapEdit *current_map = document->GetHeightMap();
	for (size_t i = 0; i < points.size(); ++i) {
		if (points[i].x < 0 || points[i].x >= current_map->getXExtent()
			|| points[i].y < 0 || points[i].y >= current_map->getYExtent()
			|| points[i].z < WorldHeightMapEdit::getMinHeightValue()
			|| points[i].z > WorldHeightMapEdit::getMaxHeightValue()) {
			return Error("out_of_bounds", "A terrain point is outside the map or the 0..255 height range.");
		}
	}

	WorldHeightMapEdit *edited_map = current_map->duplicate();
	for (size_t i = 0; i < points.size(); ++i) {
		edited_map->setHeight(points[i].x, points[i].y, static_cast<UnsignedByte>(points[i].z));
	}

	WBDocUndoable *undoable = new WBDocUndoable(document, edited_map);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	IRegion2D full_range = { 0, 0, edited_map->getXExtent() - 1, edited_map->getYExtent() - 1 };
	document->updateHeightMap(edited_map, false, full_range);
	REF_PTR_RELEASE(edited_map);
	// TheSuperHackers @bugfix Eugene 30/08/2026 Return the terrain revision for expected_revision chaining.
	return Success("{\"updated\":" + FormatInt(static_cast<Int>(points.size()))
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult FocusView(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr) {
		return error;
	}

	Real x = 0.0f;
	Real y = 0.0f;
	if (!GetRequiredReal(fields, "x", &x) || !GetRequiredReal(fields, "y", &y)) {
		return Error("invalid_arguments", "x and y are required finite world coordinates.");
	}
	WorldHeightMapEdit *height_map = document->GetHeightMap();
	if (x < 0.0f || y < 0.0f
		|| x > (height_map->getXExtent() - 1) * MAP_XY_FACTOR
		|| y > (height_map->getYExtent() - 1) * MAP_XY_FACTOR) {
		return Error("out_of_bounds", "The focus point is outside the map.");
	}

	if (document->Get2DView() != nullptr) {
		document->Get2DView()->setCenterInView(x / MAP_XY_FACTOR, y / MAP_XY_FACTOR);
	}
	if (document->Get3DView() != nullptr) {
		document->Get3DView()->setCenterInView(x / MAP_XY_FACTOR, y / MAP_XY_FACTOR);
	}
	return Success("{\"x\":" + FormatReal(x) + ",\"y\":" + FormatReal(y) + "}");
}

CommandResult SaveMap(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) {
		return error;
	}
	CString path = document->GetPathName();
	if (path.IsEmpty()) {
		return Error("map_has_no_path", "Save the new map once in WorldBuilder before using MCP save.");
	}
	if (!document->DoSave(path, TRUE)) {
		return Error("save_failed", "WorldBuilder failed to save the active map.");
	}
	return Success("{\"path\":" + JsonString(CStringToUtf8(path))
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult UndoOrRedo(const RequestFields &fields, Bool redo)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) {
		return error;
	}

	UINT command = redo ? ID_EDIT_REDO : ID_EDIT_UNDO;
	McpCommandUI command_ui;
	command_ui.m_nID = command;
	if (!document->OnCmdMsg(command, CN_UPDATE_COMMAND_UI, &command_ui, nullptr)
		|| !command_ui.updated
		|| !command_ui.enabled) {
		return Error("command_unavailable", redo ? "Redo is unavailable." : "Undo is unavailable.");
	}
	if (!document->OnCmdMsg(command, CN_COMMAND, nullptr, nullptr)) {
		return Error("command_unavailable", redo ? "Redo is unavailable." : "Undo is unavailable.");
	}
	document->updateAllViews();
	return Success(std::string(redo ? "{\"redone\":true" : "{\"undone\":true")
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

Bool Utf8ToAsciiString(const std::string &value, AsciiString *result)
{
	std::wstring wide;
	std::string ansi;
	if (!Utf8ToWide(value, &wide) || !WideToAnsi(wide, &ansi)) {
		return false;
	}
	*result = ansi.c_str();
	return true;
}

Bool GetOptionalBool(const RequestFields &fields, const char *name, Bool current, Bool *value)
{
	RequestFields::const_iterator it = fields.find(name);
	if (it == fields.end()) {
		*value = current;
		return true;
	}
	return ParseBool(it->second, value);
}

CommandResult HandleUnsaved(CWorldBuilderDoc *document, const RequestFields &fields)
{
	std::string policy = "error";
	RequestFields::const_iterator it = fields.find("on_unsaved");
	if (it != fields.end()) policy = it->second;
	if (policy != "error" && policy != "save" && policy != "discard") {
		return Error("invalid_arguments", "on_unsaved must be error, save, or discard.");
	}
	if (document == nullptr || !document->IsModified()) {
		return Success("null");
	}
	if (policy == "error") {
		return Error("unsaved_changes", "The active map has unsaved changes.");
	}
	if (policy == "save") {
		CString path = document->GetPathName();
		if (path.IsEmpty()) {
			return Error("map_has_no_path", "The active map must be saved with save_map_as first.");
		}
		if (!document->DoSave(path, TRUE)) {
			return Error("save_failed", "WorldBuilder failed to save the active map.");
		}
	} else {
		document->SetModifiedFlag(FALSE);
	}
	return Success("null");
}

CommandResult NewMap(const RequestFields &fields)
{
	Int width = 0;
	Int height = 0;
	Int initial_height = 16;
	Int border = 30;
	if (!GetRequiredInt(fields, "width", 2, 4096, &width)
		|| !GetRequiredInt(fields, "height", 2, 4096, &height)
		|| !GetOptionalInt(fields, "default_height", 16, 0, 255, &initial_height)
		|| !GetOptionalInt(fields, "border_size", 30, 0, 2047, &border)
		|| border * 2 >= width || border * 2 >= height) {
		return Error("invalid_arguments", "Invalid dimensions, default_height, or border_size.");
	}
	CWorldBuilderDoc *document = CWorldBuilderDoc::GetActiveDoc();
	CommandResult error;
	// TheSuperHackers @bugfix Eugene 30/08/2026 Reject stale new-map requests before applying their destructive unsaved policy.
	UnsignedInt current_revision = document != nullptr ? document->getChangeSerial() : 0;
	if (!CheckExpectedRevision(fields, current_revision, &error)) return error;
	if (document == nullptr) {
		CWorldBuilderDoc::setAutomationNewDocument(true);
		CDocument *created = nullptr;
		try {
			created = AfxGetApp()->OpenDocumentFile(nullptr);
		} catch (...) {
			// TheSuperHackers @bugfix Eugene 30/08/2026 Never leak dialog-free automation mode after document creation fails.
			CWorldBuilderDoc::setAutomationNewDocument(false);
			throw;
		}
		CWorldBuilderDoc::setAutomationNewDocument(false);
		document = DYNAMIC_DOWNCAST(CWorldBuilderDoc, created);
	}
	CommandResult unsaved = HandleUnsaved(document, fields);
	if (!unsaved.ok) return unsaved;
	if (document == nullptr
		|| !document->createMapForAutomation(width, height, static_cast<UnsignedByte>(initial_height), border)) {
		return Error("new_map_failed", "WorldBuilder failed to create the requested map.");
	}
	return Success("{\"created\":true,\"width\":" + FormatInt(width)
		+ ",\"height\":" + FormatInt(height)
		+ ",\"default_height\":" + FormatInt(initial_height)
		+ ",\"border_size\":" + FormatInt(border)
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult CloseMap(const RequestFields &fields)
{
	CWorldBuilderDoc *document = CWorldBuilderDoc::GetActiveDoc();
	if (document == nullptr) {
		return Success("{\"closed\":true,\"already_closed\":true}");
	}
	Bool restore_modified = document->IsModified();
	RequestFields::const_iterator policy_it = fields.find("on_unsaved");
	restore_modified = restore_modified && policy_it != fields.end() && policy_it->second == "discard";
	CommandResult unsaved = HandleUnsaved(document, fields);
	if (!unsaved.ok) return unsaved;
	CWnd *main_window = AfxGetMainWnd();
	if (main_window == nullptr || !main_window->PostMessage(WM_COMMAND, ID_FILE_CLOSE, 0)) {
		if (restore_modified) document->SetModifiedFlag(TRUE);
		return Error("close_failed", "WorldBuilder could not queue the map close command.");
	}
	return Success("{\"closed\":true,\"queued\":true}");
}

Bool WaypointNameExists(const AsciiString &name, MapObject *except = nullptr)
{
	for (MapObject *object = MapObject::getFirstMapObject(); object != nullptr; object = object->getNext()) {
		if (object != except && object->isWaypoint() && object->getWaypointName() == name) {
			return true;
		}
	}
	return false;
}

CommandResult ListWaypoints()
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr) return error;
	std::string result = "{\"waypoints\":[";
	Int count = 0;
	for (MapObject *object = MapObject::getFirstMapObject(); object != nullptr; object = object->getNext()) {
		if (!object->isWaypoint()) continue;
		if (count++ != 0) result += ",";
		const Coord3D *location = object->getLocation();
		result += "{\"id\":" + FormatInt(object->getWaypointID())
			+ ",\"object_id\":" + JsonString(GetObjectId(object))
			+ ",\"name\":" + JsonString(AnsiToUtf8(object->getWaypointName().str()))
			+ ",\"location\":{\"x\":" + FormatReal(location->x)
			+ ",\"y\":" + FormatReal(location->y) + ",\"z\":" + FormatReal(location->z) + "}}";
	}
	result += "],\"links\":[";
	for (Int i = 0; i < document->getNumWaypointLinks(); ++i) {
		Int from = 0;
		Int to = 0;
		document->getWaypointLink(i, &from, &to);
		if (i != 0) result += ",";
		result += "{\"from_id\":" + FormatInt(from) + ",\"to_id\":" + FormatInt(to) + "}";
	}
	result += "],\"total\":" + FormatInt(count) + "}";
	return Success(result);
}

CommandResult CreateWaypoint(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	RequestFields::const_iterator name_it = fields.find("name");
	Real x = 0;
	Real y = 0;
	Real z = 0;
	AsciiString name;
	if (name_it == fields.end() || name_it->second.empty()
		|| !Utf8ToAsciiString(name_it->second, &name)
		|| !GetRequiredReal(fields, "x", &x) || !GetRequiredReal(fields, "y", &y)) {
		return Error("invalid_arguments", "name, x and y are required.");
	}
	RequestFields::const_iterator z_it = fields.find("z");
	if (z_it != fields.end() && !ParseReal(z_it->second, &z)) {
		return Error("invalid_arguments", "z must be a finite number.");
	}
	if (WaypointNameExists(name)) {
		return Error("waypoint_name_exists", "A waypoint with this name already exists.");
	}
	Coord3D location = {x, y, z};
	MapObject *waypoint = newInstance(MapObject)(location, "*Waypoints/Waypoint", 0, 0, nullptr, nullptr);
	waypoint->setIsWaypoint();
	waypoint->setWaypointID(document->getNextWaypointID());
	waypoint->setWaypointName(name);
	waypoint->setName(name);
	AddObjectUndoable *undoable = new AddObjectUndoable(document, waypoint);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	return Success("{\"created\":true,\"waypoint_id\":" + FormatInt(waypoint->getWaypointID())
		+ ",\"object_id\":" + JsonString(GetObjectId(waypoint))
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult UpdateWaypoint(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	Int id = 0;
	if (!GetRequiredInt(fields, "waypoint_id", 1, INT_MAX, &id)) {
		return Error("invalid_arguments", "waypoint_id is required.");
	}
	MapObject *waypoint = document->getWaypointByID(id);
	if (waypoint == nullptr) return Error("waypoint_not_found", "Waypoint was not found.");
	Coord3D location = *waypoint->getLocation();
	Bool exists = false;
	if (!GetOptionalReal(fields, "x", &location.x, &exists)) return Error("invalid_arguments", "x must be numeric.");
	if (!GetOptionalReal(fields, "y", &location.y, &exists)) return Error("invalid_arguments", "y must be numeric.");
	if (!GetOptionalReal(fields, "z", &location.z, &exists)) return Error("invalid_arguments", "z must be numeric.");
	AsciiString name = waypoint->getWaypointName();
	RequestFields::const_iterator name_it = fields.find("name");
	if (name_it != fields.end() && (!Utf8ToAsciiString(name_it->second, &name) || name.isEmpty())) {
		return Error("invalid_arguments", "name must be representable by WorldBuilder.");
	}
	if (WaypointNameExists(name, waypoint)) return Error("waypoint_name_exists", "A waypoint with this name already exists.");
	McpModifyWaypointUndoable *undoable = new McpModifyWaypointUndoable(document, waypoint, location, name);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	return Success("{\"updated\":true,\"waypoint_id\":" + FormatInt(id)
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult ChangeWaypointLink(const RequestFields &fields, Bool add)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	Int from = 0;
	Int to = 0;
	if (!GetRequiredInt(fields, "from_id", 1, INT_MAX, &from)
		|| !GetRequiredInt(fields, "to_id", 1, INT_MAX, &to) || from == to) {
		return Error("invalid_arguments", "from_id and to_id must name two different waypoints.");
	}
	if (document->getWaypointByID(from) == nullptr || document->getWaypointByID(to) == nullptr) {
		return Error("waypoint_not_found", "One or both waypoints were not found.");
	}
	Bool exists = document->waypointLinkExists(from, to);
	if (exists == add) {
		return Success(std::string("{\"changed\":false,\"already_") + (add ? "connected" : "disconnected")
			+ "\":true,\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
	}
	McpWaypointLinkUndoable *undoable = new McpWaypointLinkUndoable(document, from, to, add);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	return Success("{\"changed\":true,\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult DeleteWaypoint(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	Int id = 0;
	if (!GetRequiredInt(fields, "waypoint_id", 1, INT_MAX, &id)) return Error("invalid_arguments", "waypoint_id is required.");
	MapObject *waypoint = document->getWaypointByID(id);
	if (waypoint == nullptr) return Error("waypoint_not_found", "Waypoint was not found.");
	McpCompositeUndoable *composite = new McpCompositeUndoable;
	for (Int i = document->getNumWaypointLinks() - 1; i >= 0; --i) {
		Int from = 0;
		Int to = 0;
		document->getWaypointLink(i, &from, &to);
		if (from == id || to == id) {
			McpWaypointLinkUndoable *link = new McpWaypointLinkUndoable(document, from, to, false);
			composite->Add(link);
			REF_PTR_RELEASE(link);
		}
	}
	PointerTool::clearSelection();
	waypoint->setSelected(true);
	DeleteObjectUndoable *deletion = new DeleteObjectUndoable(document);
	composite->Add(deletion);
	REF_PTR_RELEASE(deletion);
	document->AddAndDoUndoable(composite);
	REF_PTR_RELEASE(composite);
	return Success("{\"deleted\":true,\"waypoint_id\":" + FormatInt(id)
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

PolygonTrigger *FindArea(Int id)
{
	return PolygonTrigger::getPolygonTriggerByID(id);
}

std::string PolygonToJson(PolygonTrigger *polygon)
{
	std::string result = "{\"id\":" + FormatInt(polygon->getID())
		+ ",\"name\":" + JsonString(AnsiToUtf8(polygon->getTriggerName().str()))
		+ ",\"kind\":" + JsonString(polygon->isWaterArea() ? "water" : "scripting")
		+ ",\"export_with_scripts\":" + (polygon->doExportWithScripts() ? "true" : "false")
		+ ",\"river\":" + (polygon->isRiver() ? "true" : "false")
		+ ",\"river_start\":" + FormatInt(polygon->getRiverStart());
#ifdef RTS_ZEROHOUR
	result += ",\"layer\":" + JsonString(AnsiToUtf8(polygon->getLayerName().str()));
#endif
	result += ",\"points\":[";
	for (Int i = 0; i < polygon->getNumPoints(); ++i) {
		if (i != 0) result += ",";
		const ICoord3D *point = polygon->getPoint(i);
		result += "{\"x\":" + FormatInt(point->x) + ",\"y\":" + FormatInt(point->y)
			+ ",\"z\":" + FormatInt(point->z) + "}";
	}
	result += "]}";
	return result;
}

CommandResult ListAreas(const RequestFields &fields)
{
	CommandResult error;
	if (GetDocument(&error) == nullptr) return error;
	std::string kind = "all";
	RequestFields::const_iterator kind_it = fields.find("kind");
	if (kind_it != fields.end()) kind = kind_it->second;
	if (kind != "all" && kind != "scripting" && kind != "water") return Error("invalid_arguments", "kind is invalid.");
	std::string result = "{\"areas\":[";
	Int count = 0;
	for (PolygonTrigger *polygon = PolygonTrigger::getFirstPolygonTrigger(); polygon != nullptr; polygon = polygon->getNext()) {
		if ((kind == "water") != polygon->isWaterArea() && kind != "all") continue;
		if (count++ != 0) result += ",";
		result += PolygonToJson(polygon);
	}
	result += "],\"total\":" + FormatInt(count) + "}";
	return Success(result);
}

Bool HasUnsupportedAreaLayer(const RequestFields &fields)
{
#ifdef RTS_ZEROHOUR
	return false;
#else
	RequestFields::const_iterator layer_it = fields.find("layer");
	return layer_it != fields.end() && !layer_it->second.empty();
#endif
}

Bool ReadPolygonState(const RequestFields &fields, McpPolygonState *state, Bool require_points)
{
	RequestFields::const_iterator name_it = fields.find("name");
	if (name_it != fields.end() && !Utf8ToAsciiString(name_it->second, &state->name)) return false;
#ifdef RTS_ZEROHOUR
	RequestFields::const_iterator layer_it = fields.find("layer");
	if (layer_it != fields.end() && !Utf8ToAsciiString(layer_it->second, &state->layerName)) return false;
#endif
	RequestFields::const_iterator kind_it = fields.find("kind");
	if (kind_it != fields.end()) {
		if (kind_it->second != "water" && kind_it->second != "scripting") return false;
		state->water = kind_it->second == "water";
	}
	if (!GetOptionalBool(fields, "export_with_scripts", state->exportWithScripts, &state->exportWithScripts)
		|| !GetOptionalBool(fields, "river", state->river, &state->river)) return false;
	RequestFields::const_iterator river_start = fields.find("river_start");
	if (river_start != fields.end() && (!ParseInt(river_start->second, &state->riverStart) || state->riverStart < 0)) return false;
	RequestFields::const_iterator count_it = fields.find("point_count");
	if (count_it == fields.end()) return !require_points;
	Int count = 0;
	if (!ParseInt(count_it->second, &count) || count < 3 || count > MAX_AREA_POINTS) return false;
	state->points.clear();
	for (Int i = 0; i < count; ++i) {
		Real x = 0;
		Real y = 0;
		Real z = 0;
		RequestFields::const_iterator x_it = fields.find(IndexedField("point", i, "x"));
		RequestFields::const_iterator y_it = fields.find(IndexedField("point", i, "y"));
		RequestFields::const_iterator z_it = fields.find(IndexedField("point", i, "z"));
		if (x_it == fields.end() || y_it == fields.end() || !ParseReal(x_it->second, &x) || !ParseReal(y_it->second, &y)
			|| (z_it != fields.end() && !ParseReal(z_it->second, &z))) return false;
		ICoord3D point = {REAL_TO_INT(x), REAL_TO_INT(y), REAL_TO_INT(z)};
		state->points.push_back(point);
	}
	return true;
}

CommandResult CreateArea(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	// TheSuperHackers @bugfix Eugene 30/08/2026 Reject unsupported Generals area layers instead of silently discarding them.
	if (HasUnsupportedAreaLayer(fields)) return Error("invalid_arguments", "layer is supported only by Zero Hour WorldBuilder.");
	McpPolygonState state;
	state.exportWithScripts = true;
	state.water = false;
	state.river = false;
	state.riverStart = 0;
	if (!ReadPolygonState(fields, &state, true) || state.name.isEmpty() || fields.find("kind") == fields.end()) {
		return Error("invalid_arguments", "name, kind and at least three valid points are required.");
	}
	PolygonTrigger *polygon = newInstance(PolygonTrigger)(static_cast<Int>(state.points.size()));
	ApplyPolygon(polygon, state);
	AddPolygonUndoable *undoable = new AddPolygonUndoable(polygon);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	document->invalObject(nullptr);
	return Success("{\"created\":true,\"area_id\":" + FormatInt(polygon->getID())
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult UpdateArea(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	// TheSuperHackers @bugfix Eugene 30/08/2026 Reject unsupported Generals area layer updates instead of dropping them.
	if (HasUnsupportedAreaLayer(fields)) return Error("invalid_arguments", "layer is supported only by Zero Hour WorldBuilder.");
	Int id = 0;
	if (!GetRequiredInt(fields, "area_id", 0, INT_MAX, &id)) return Error("invalid_arguments", "area_id is required.");
	PolygonTrigger *polygon = FindArea(id);
	if (polygon == nullptr) return Error("area_not_found", "Area was not found.");
	McpPolygonState state = CapturePolygon(polygon);
	if (!ReadPolygonState(fields, &state, false) || state.name.isEmpty()) return Error("invalid_arguments", "Area update is invalid.");
	McpModifyPolygonUndoable *undoable = new McpModifyPolygonUndoable(document, polygon, state);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	return Success("{\"updated\":true,\"area_id\":" + FormatInt(id)
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult DeleteArea(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	Int id = 0;
	if (!GetRequiredInt(fields, "area_id", 0, INT_MAX, &id)) return Error("invalid_arguments", "area_id is required.");
	PolygonTrigger *polygon = FindArea(id);
	if (polygon == nullptr) return Error("area_not_found", "Area was not found.");
	DeletePolygonUndoable *undoable = new DeletePolygonUndoable(polygon);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	document->invalObject(nullptr);
	return Success("{\"deleted\":true,\"area_id\":" + FormatInt(id)
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

Bool ParameterIsInteger(Parameter::ParameterType type)
{
	switch (type) {
		case Parameter::INT:
		case Parameter::COMPARISON:
		case Parameter::BOOLEAN:
		case Parameter::RELATION:
		case Parameter::AI_MOOD:
		case Parameter::KIND_OF_PARAM:
		case Parameter::RADAR_EVENT_TYPE:
		case Parameter::COMMANDBUTTON_ABILITY:
		case Parameter::BOUNDARY:
		case Parameter::BUILDABLE:
		case Parameter::SURFACES_ALLOWED:
		case Parameter::SHAKE_INTENSITY:
		case Parameter::COLOR:
		case Parameter::EMOTICON:
			return true;
		default:
			return false;
	}
}

std::string ParameterToJson(Parameter *parameter)
{
	if (parameter == nullptr) return "null";
	Parameter::ParameterType type = parameter->getParameterType();
	std::string value;
	if (ParameterIsInteger(type)) {
		value = FormatInt(parameter->getInt());
	} else if (type == Parameter::REAL || type == Parameter::ANGLE) {
		value = FormatReal(parameter->getReal());
	} else if (type == Parameter::COORD3D) {
		Coord3D point;
		parameter->getCoord3D(&point);
		value = "{\"x\":" + FormatReal(point.x) + ",\"y\":" + FormatReal(point.y)
			+ ",\"z\":" + FormatReal(point.z) + "}";
	} else {
		value = JsonString(AnsiToUtf8(parameter->getString().str()));
	}
	return "{\"type\":" + FormatInt(static_cast<Int>(type)) + ",\"value\":" + value
		+ ",\"ui_text\":" + JsonString(AnsiToUtf8(parameter->getUiText().str())) + "}";
}

std::string ActionListToJson(ScriptAction *action)
{
	std::string result = "[";
	Int count = 0;
	for (; action != nullptr && count < MAX_SCRIPT_ITEMS; action = action->getNext(), ++count) {
		if (count != 0) result += ",";
		result += "{\"type\":" + FormatInt(static_cast<Int>(action->getActionType()))
			+ ",\"ui_text\":" + JsonString(AnsiToUtf8(action->getUiText().str())) + ",\"parameters\":[";
		for (Int i = 0; i < action->getNumParameters(); ++i) {
			if (i != 0) result += ",";
			result += ParameterToJson(action->getParameter(i));
		}
		result += "]}";
	}
	result += "]";
	return result;
}

std::string ConditionsToJson(OrCondition *condition)
{
	std::string result = "[";
	Int count = 0;
	Int or_group = 0;
	for (OrCondition *or_condition = condition; or_condition != nullptr && count < MAX_SCRIPT_ITEMS;
		or_condition = or_condition->getNextOrCondition(), ++or_group) {
		for (Condition *and_condition = or_condition->getFirstAndCondition(); and_condition != nullptr && count < MAX_SCRIPT_ITEMS;
			and_condition = and_condition->getNext(), ++count) {
			if (count != 0) result += ",";
			result += "{\"or_group\":" + FormatInt(or_group)
				+ ",\"type\":" + FormatInt(static_cast<Int>(and_condition->getConditionType()))
				+ ",\"custom_data\":" + FormatInt(and_condition->getCustomData())
				+ ",\"ui_text\":" + JsonString(AnsiToUtf8(and_condition->getUiText().str()))
				+ ",\"parameters\":[";
			for (Int i = 0; i < and_condition->getNumParameters(); ++i) {
				if (i != 0) result += ",";
				result += ParameterToJson(and_condition->getParameter(i));
			}
			result += "]}";
		}
	}
	result += "]";
	return result;
}

std::string ScriptToJson(Script *script, Int player_index, const AsciiString &group)
{
	std::string result = "{\"player_index\":" + FormatInt(player_index)
		+ ",\"group\":" + (group.isEmpty() ? "null" : JsonString(AnsiToUtf8(group.str())))
		+ ",\"name\":" + JsonString(AnsiToUtf8(script->getName().str()))
		+ ",\"comment\":" + JsonString(AnsiToUtf8(script->getComment().str()))
		+ ",\"condition_comment\":" + JsonString(AnsiToUtf8(script->getConditionComment().str()))
		+ ",\"action_comment\":" + JsonString(AnsiToUtf8(script->getActionComment().str()))
		+ ",\"active\":" + (script->isActive() ? "true" : "false")
		+ ",\"one_shot\":" + (script->isOneShot() ? "true" : "false")
		+ ",\"subroutine\":" + (script->isSubroutine() ? "true" : "false")
		+ ",\"easy\":" + (script->isEasy() ? "true" : "false")
		+ ",\"normal\":" + (script->isNormal() ? "true" : "false")
		+ ",\"hard\":" + (script->isHard() ? "true" : "false")
		+ ",\"delay_seconds\":" + FormatInt(script->getDelayEvalSeconds())
		+ ",\"conditions\":" + ConditionsToJson(script->getOrCondition())
		+ ",\"actions\":" + ActionListToJson(script->getAction())
		+ ",\"false_actions\":" + ActionListToJson(script->getFalseAction()) + "}";
	return result;
}

Script *FindScriptInList(ScriptList *list, const AsciiString &name, AsciiString *group_name = nullptr)
{
	if (list == nullptr) return nullptr;
	for (Script *script = list->getScript(); script != nullptr; script = script->getNext()) {
		if (script->getName() == name) {
			if (group_name != nullptr) group_name->clear();
			return script;
		}
	}
	for (ScriptGroup *group = list->getScriptGroup(); group != nullptr; group = group->getNext()) {
		for (Script *script = group->getScript(); script != nullptr; script = script->getNext()) {
			if (script->getName() == name) {
				if (group_name != nullptr) *group_name = group->getName();
				return script;
			}
		}
	}
	return nullptr;
}

ScriptGroup *FindScriptGroup(ScriptList *list, const AsciiString &name)
{
	if (list == nullptr) return nullptr;
	for (ScriptGroup *group = list->getScriptGroup(); group != nullptr; group = group->getNext()) {
		if (group->getName() == name) return group;
	}
	return nullptr;
}

Script *FindScriptScoped(ScriptList *list, const AsciiString &name, const AsciiString *group_name)
{
	if (list == nullptr) return nullptr;
	if (group_name == nullptr) {
		for (Script *script = list->getScript(); script != nullptr; script = script->getNext()) {
			if (script->getName() == name) return script;
		}
		return nullptr;
	}
	ScriptGroup *group = FindScriptGroup(list, *group_name);
	if (group == nullptr) return nullptr;
	for (Script *script = group->getScript(); script != nullptr; script = script->getNext()) {
		if (script->getName() == name) return script;
	}
	return nullptr;
}

CommandResult ListScriptTypes()
{
	CommandResult error;
	if (GetDocument(&error) == nullptr) return error;
	std::string result = "{\"conditions\":[";
	// TheSuperHackers @feature Eugene 30/08/2026 Expose Zero Hour internal script names for edition-neutral discovery.
	for (Int type = 0; type < Condition::NUM_ITEMS; ++type) {
		if (type != 0) result += ",";
		const ConditionTemplate *item = TheScriptEngine->getConditionTemplate(type);
		result += "{\"id\":" + FormatInt(type) + ",\"name\":" + JsonString(AnsiToUtf8(item->getName().str()));
#ifdef RTS_ZEROHOUR
		result += ",\"internal_name\":" + JsonString(AnsiToUtf8(item->m_internalName.str()));
#endif
		result += ",\"parameter_types\":[";
		for (Int parameter = 0; parameter < item->getNumParameters(); ++parameter) {
			if (parameter != 0) result += ",";
			result += FormatInt(static_cast<Int>(item->getParameterType(parameter)));
		}
		result += "]}";
	}
	result += "],\"actions\":[";
	for (Int type = 0; type < ScriptAction::NUM_ITEMS; ++type) {
		if (type != 0) result += ",";
		const ActionTemplate *item = TheScriptEngine->getActionTemplate(type);
		result += "{\"id\":" + FormatInt(type) + ",\"name\":" + JsonString(AnsiToUtf8(item->getName().str()));
#ifdef RTS_ZEROHOUR
		result += ",\"internal_name\":" + JsonString(AnsiToUtf8(item->m_internalName.str()));
#endif
		result += ",\"parameter_types\":[";
		for (Int parameter = 0; parameter < item->getNumParameters(); ++parameter) {
			if (parameter != 0) result += ",";
			result += FormatInt(static_cast<Int>(item->getParameterType(parameter)));
		}
		result += "]}";
	}
	result += "]}";
	return Success(result);
}

CommandResult ListSciences()
{
	CommandResult error;
	if (GetDocument(&error) == nullptr) return error;
	if (TheScienceStore == nullptr) {
		return Error("sciences_unavailable", "The science catalog is unavailable.");
	}

	std::vector<AsciiString> names = TheScienceStore->friend_getScienceNames();
	std::string result = "{\"sciences\":[";
	for (size_t i = 0; i < names.size(); ++i) {
		if (i != 0) result += ",";
		ScienceType science = TheScienceStore->getScienceFromInternalName(names[i]);
		result += "{\"name\":" + JsonString(AnsiToUtf8(names[i].str()));
		result += ",\"grantable\":";
		result += science != SCIENCE_INVALID && TheScienceStore->isScienceGrantable(science)
			? "true" : "false";
		result += ",\"purchase_cost\":"
			+ FormatInt(science != SCIENCE_INVALID ? TheScienceStore->getSciencePurchaseCost(science) : 0);
		result += "}";
	}
	result += "],\"total\":" + FormatInt(static_cast<Int>(names.size())) + "}";
	return Success(result);
}

CommandResult ListUpgrades()
{
	CommandResult error;
	if (GetDocument(&error) == nullptr) return error;
	if (TheUpgradeCenter == nullptr) {
		return Error("upgrades_unavailable", "The upgrade catalog is unavailable.");
	}

	std::string result = "{\"upgrades\":[";
	Int count = 0;
	for (UpgradeTemplate *upgrade = TheUpgradeCenter->firstUpgradeTemplate();
		upgrade != nullptr;
		upgrade = upgrade->friend_getNext()) {
		if (count++ != 0) result += ",";
		Bool player_scoped = upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER;
		result += "{\"name\":" + JsonString(AnsiToUtf8(upgrade->getUpgradeName().str()));
		result += ",\"scope\":" + JsonString(player_scoped ? "player" : "object");
		result += ",\"player_scoped\":";
		result += player_scoped ? "true" : "false";
		result += "}";
	}
	result += "],\"total\":" + FormatInt(count) + "}";
	return Success(result);
}

CommandResult ListScripts(const RequestFields &fields)
{
	CommandResult error;
	if (GetDocument(&error) == nullptr) return error;
	Int requested = -1;
	RequestFields::const_iterator player_it = fields.find("player_index");
	if (player_it != fields.end() && (!ParseInt(player_it->second, &requested) || requested < 0)) {
		return Error("invalid_arguments", "player_index must be non-negative.");
	}
	if (TheSidesList == nullptr || (requested >= TheSidesList->getNumSides())) {
		return Error("player_not_found", "Map player index was not found.");
	}
	std::string result = "{\"scripts\":[";
	Int count = 0;
	for (Int player = 0; player < TheSidesList->getNumSides(); ++player) {
		if (requested >= 0 && requested != player) continue;
		ScriptList *list = TheSidesList->getSideInfo(player)->getScriptList();
		if (list == nullptr) continue;
		for (Script *script = list->getScript(); script != nullptr; script = script->getNext()) {
			if (count++ != 0) result += ",";
			result += "{\"player_index\":" + FormatInt(player) + ",\"group\":null,\"name\":"
				+ JsonString(AnsiToUtf8(script->getName().str())) + ",\"active\":"
				+ (script->isActive() ? "true" : "false") + "}";
		}
		for (ScriptGroup *group = list->getScriptGroup(); group != nullptr; group = group->getNext()) {
			for (Script *script = group->getScript(); script != nullptr; script = script->getNext()) {
				if (count++ != 0) result += ",";
				result += "{\"player_index\":" + FormatInt(player) + ",\"group\":"
					+ JsonString(AnsiToUtf8(group->getName().str())) + ",\"name\":"
					+ JsonString(AnsiToUtf8(script->getName().str())) + ",\"active\":"
					+ (script->isActive() ? "true" : "false") + "}";
			}
		}
	}
	result += "],\"total\":" + FormatInt(count) + "}";
	return Success(result);
}

CommandResult GetScript(const RequestFields &fields)
{
	CommandResult error;
	if (GetDocument(&error) == nullptr) return error;
	Int player = 0;
	RequestFields::const_iterator name_it = fields.find("name");
	AsciiString name;
	if (!GetRequiredInt(fields, "player_index", 0, INT_MAX, &player)
		|| name_it == fields.end() || !Utf8ToAsciiString(name_it->second, &name)
		|| TheSidesList == nullptr || player >= TheSidesList->getNumSides()) {
		return Error("invalid_arguments", "A valid player_index and name are required.");
	}
	AsciiString requested_group;
	AsciiString group;
	RequestFields::const_iterator group_it = fields.find("group");
	if (group_it != fields.end() && !Utf8ToAsciiString(group_it->second, &requested_group)) {
		return Error("invalid_arguments", "group is not representable by WorldBuilder.");
	}
	Script *script = group_it == fields.end()
		? FindScriptInList(TheSidesList->getSideInfo(player)->getScriptList(), name, &group)
		: FindScriptScoped(TheSidesList->getSideInfo(player)->getScriptList(), name, &requested_group);
	if (group_it != fields.end()) group = requested_group;
	if (script == nullptr) return Error("script_not_found", "Script was not found.");
	return Success(ScriptToJson(script, player, group));
}

Bool ApplyParameterFields(const RequestFields &fields, const std::string &prefix, Parameter *parameter)
{
	if (parameter == nullptr) return false;
	Parameter::ParameterType type = parameter->getParameterType();
	if (type == Parameter::COORD3D) {
		RequestFields::const_iterator x_it = fields.find(prefix + "_x");
		RequestFields::const_iterator y_it = fields.find(prefix + "_y");
		RequestFields::const_iterator z_it = fields.find(prefix + "_z");
		Coord3D point;
		if (x_it == fields.end() || y_it == fields.end() || !ParseReal(x_it->second, &point.x)
			|| !ParseReal(y_it->second, &point.y) || (z_it != fields.end() && !ParseReal(z_it->second, &point.z))) return false;
		if (z_it == fields.end()) point.z = 0;
		parameter->friend_setCoord3D(&point);
		return true;
	}
	RequestFields::const_iterator value_it = fields.find(prefix);
	if (value_it == fields.end()) return false;
	if (ParameterIsInteger(type)) {
		Int value = 0;
		if (!ParseInt(value_it->second, &value)) return false;
		parameter->friend_setInt(value);
		return true;
	}
	if (type == Parameter::REAL || type == Parameter::ANGLE) {
		Real value = 0;
		if (!ParseReal(value_it->second, &value)) return false;
		parameter->friend_setReal(value);
		return true;
	}
	AsciiString value;
	if (!Utf8ToAsciiString(value_it->second, &value)) return false;
	parameter->friend_setString(value);
	return true;
}

Bool ApplyParameters(const RequestFields &fields, const std::string &prefix, ScriptAction *action)
{
	Int supplied = 0;
	RequestFields::const_iterator count_it = fields.find(prefix + "_param_count");
	if (count_it == fields.end() || !ParseInt(count_it->second, &supplied)
		|| supplied < 0 || supplied > action->getNumParameters()) return false;
	for (Int i = 0; i < supplied; ++i) {
		if (!ApplyParameterFields(fields, prefix + "_param" + FormatInt(i), action->getParameter(i))) return false;
	}
	return true;
}

Bool ApplyParameters(const RequestFields &fields, const std::string &prefix, Condition *condition)
{
	Int supplied = 0;
	RequestFields::const_iterator count_it = fields.find(prefix + "_param_count");
	if (count_it == fields.end() || !ParseInt(count_it->second, &supplied)
		|| supplied < 0 || supplied > condition->getNumParameters()) return false;
	for (Int i = 0; i < supplied; ++i) {
		if (!ApplyParameterFields(fields, prefix + "_param" + FormatInt(i), condition->getParameter(i))) return false;
	}
	return true;
}

ScriptAction *BuildActions(const RequestFields &fields, const char *collection, Bool *ok)
{
	*ok = false;
	Int count = 0;
	std::string count_key = std::string(collection) + "_count";
	RequestFields::const_iterator count_it = fields.find(count_key);
	if (count_it == fields.end() || !ParseInt(count_it->second, &count) || count < 0 || count > MAX_SCRIPT_ITEMS) return nullptr;
	ScriptAction *head = nullptr;
	ScriptAction *tail = nullptr;
	for (Int i = 0; i < count; ++i) {
		std::string prefix = std::string(collection) + FormatInt(i);
		RequestFields::const_iterator type_it = fields.find(prefix + "_type");
		Int type = 0;
		if (type_it == fields.end() || !ParseInt(type_it->second, &type)
			|| type < 0 || type >= ScriptAction::NUM_ITEMS) {
			head->deleteInstance();
			return nullptr;
		}
		ScriptAction *action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(type));
		if (!ApplyParameters(fields, prefix, action)) {
			action->deleteInstance();
			head->deleteInstance();
			return nullptr;
		}
		if (tail != nullptr) tail->setNextAction(action); else head = action;
		tail = action;
	}
	*ok = true;
	return head;
}

OrCondition *BuildConditions(const RequestFields &fields, Bool *ok)
{
	*ok = false;
	Int count = 0;
	RequestFields::const_iterator count_it = fields.find("condition_count");
	if (count_it == fields.end() || !ParseInt(count_it->second, &count) || count < 0 || count > MAX_SCRIPT_ITEMS) return nullptr;
	OrCondition *head = nullptr;
	OrCondition *or_tail = nullptr;
	Condition *and_tail = nullptr;
	Int current_group = -1;
	for (Int i = 0; i < count; ++i) {
		std::string prefix = "condition" + FormatInt(i);
		Int type = 0;
		Int group = 0;
		Int custom = 0;
		RequestFields::const_iterator type_it = fields.find(prefix + "_type");
		RequestFields::const_iterator group_it = fields.find(prefix + "_or_group");
		RequestFields::const_iterator custom_it = fields.find(prefix + "_custom_data");
		if (type_it == fields.end() || group_it == fields.end() || custom_it == fields.end()
			|| !ParseInt(type_it->second, &type) || type < 0 || type >= Condition::NUM_ITEMS
			|| !ParseInt(group_it->second, &group) || group < current_group
			|| !ParseInt(custom_it->second, &custom)) {
			head->deleteInstance();
			return nullptr;
		}
		if (head == nullptr || group != current_group) {
			OrCondition *next = newInstance(OrCondition);
			if (or_tail != nullptr) or_tail->setNextOrCondition(next); else head = next;
			or_tail = next;
			and_tail = nullptr;
			current_group = group;
		}
		Condition *condition = newInstance(Condition)(static_cast<Condition::ConditionType>(type));
		condition->setCustomData(custom);
		if (!ApplyParameters(fields, prefix, condition)) {
			condition->deleteInstance();
			head->deleteInstance();
			return nullptr;
		}
		if (and_tail != nullptr) and_tail->setNextCondition(condition); else or_tail->setFirstAndCondition(condition);
		and_tail = condition;
	}
	*ok = true;
	return head;
}

CommandResult UpsertScript(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	Int player = 0;
	RequestFields::const_iterator name_it = fields.find("name");
	AsciiString name;
	if (!GetRequiredInt(fields, "player_index", 0, INT_MAX, &player) || name_it == fields.end()
		|| !Utf8ToAsciiString(name_it->second, &name) || name.isEmpty()
		|| TheSidesList == nullptr || player >= TheSidesList->getNumSides()) {
		return Error("invalid_arguments", "A valid player_index and name are required.");
	}
	Bool conditions_ok = false;
	Bool actions_ok = false;
	Bool false_actions_ok = false;
	OrCondition *conditions = BuildConditions(fields, &conditions_ok);
	ScriptAction *actions = BuildActions(fields, "action", &actions_ok);
	ScriptAction *false_actions = BuildActions(fields, "false_action", &false_actions_ok);
	if (!conditions_ok || !actions_ok || !false_actions_ok) {
		conditions->deleteInstance();
		actions->deleteInstance();
		false_actions->deleteInstance();
		return Error("invalid_script", "A condition/action type or parameter is invalid for its native template.");
	}
	Script *replacement = newInstance(Script);
	replacement->setName(name);
	replacement->setOrCondition(conditions);
	replacement->setAction(actions);
	replacement->setFalseAction(false_actions);
	RequestFields::const_iterator text_it;
	AsciiString text;
	text_it = fields.find("comment"); if (text_it != fields.end() && Utf8ToAsciiString(text_it->second, &text)) replacement->setComment(text);
	text_it = fields.find("condition_comment"); if (text_it != fields.end() && Utf8ToAsciiString(text_it->second, &text)) replacement->setConditionComment(text);
	text_it = fields.find("action_comment"); if (text_it != fields.end() && Utf8ToAsciiString(text_it->second, &text)) replacement->setActionComment(text);
	Bool flag = false;
	if (!GetOptionalBool(fields, "active", true, &flag)) { replacement->deleteInstance(); return Error("invalid_arguments", "active must be boolean."); } replacement->setActive(flag);
	if (!GetOptionalBool(fields, "one_shot", true, &flag)) { replacement->deleteInstance(); return Error("invalid_arguments", "one_shot must be boolean."); } replacement->setOneShot(flag);
	if (!GetOptionalBool(fields, "subroutine", false, &flag)) { replacement->deleteInstance(); return Error("invalid_arguments", "subroutine must be boolean."); } replacement->setSubroutine(flag);
	if (!GetOptionalBool(fields, "easy", true, &flag)) { replacement->deleteInstance(); return Error("invalid_arguments", "easy must be boolean."); } replacement->setEasy(flag);
	if (!GetOptionalBool(fields, "normal", true, &flag)) { replacement->deleteInstance(); return Error("invalid_arguments", "normal must be boolean."); } replacement->setNormal(flag);
	if (!GetOptionalBool(fields, "hard", true, &flag)) { replacement->deleteInstance(); return Error("invalid_arguments", "hard must be boolean."); } replacement->setHard(flag);
	Int delay = 0;
	if (!GetOptionalInt(fields, "delay_seconds", 0, 0, INT_MAX, &delay)) { replacement->deleteInstance(); return Error("invalid_arguments", "delay_seconds is invalid."); }
	replacement->setDelayEvalSeconds(delay);

	SidesList updated(*TheSidesList);
	SidesInfo *side = updated.getSideInfo(player);
	ScriptList *list = side->getScriptList();
	if (list == nullptr) {
		list = newInstance(ScriptList);
		side->setScriptList(list);
	}
	AsciiString requested_group;
	RequestFields::const_iterator group_it = fields.find("group");
	if (group_it != fields.end() && !Utf8ToAsciiString(group_it->second, &requested_group)) {
		replacement->deleteInstance();
		return Error("invalid_arguments", "group is not representable by WorldBuilder.");
	}
	ScriptGroup *group = group_it == fields.end() ? nullptr : FindScriptGroup(list, requested_group);
	if (group_it != fields.end() && group == nullptr) {
		replacement->deleteInstance();
		return Error("script_group_not_found", "Script group was not found.");
	}
	Script *existing = FindScriptScoped(list, name, group_it == fields.end() ? nullptr : &requested_group);
	Bool created = existing == nullptr;
	if (existing != nullptr) {
		if (group != nullptr) group->deleteScript(existing); else list->deleteScript(existing);
	}
	if (group != nullptr) group->addScript(replacement, 0); else list->addScript(replacement, 0);
	SidesListUndoable *undoable = new SidesListUndoable(updated, document);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	return Success(std::string("{\"") + (created ? "created" : "updated") + "\":true,\"player_index\":"
		+ FormatInt(player) + ",\"name\":" + JsonString(name_it->second)
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult DeleteScript(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	Int player = 0;
	RequestFields::const_iterator name_it = fields.find("name");
	AsciiString name;
	if (!GetRequiredInt(fields, "player_index", 0, INT_MAX, &player) || name_it == fields.end()
		|| !Utf8ToAsciiString(name_it->second, &name) || TheSidesList == nullptr || player >= TheSidesList->getNumSides()) {
		return Error("invalid_arguments", "A valid player_index and name are required.");
	}
	SidesList updated(*TheSidesList);
	ScriptList *list = updated.getSideInfo(player)->getScriptList();
	AsciiString requested_group;
	RequestFields::const_iterator group_it = fields.find("group");
	if (group_it != fields.end() && !Utf8ToAsciiString(group_it->second, &requested_group)) return Error("invalid_arguments", "group is invalid.");
	ScriptGroup *group = group_it == fields.end() ? nullptr : FindScriptGroup(list, requested_group);
	if (group_it != fields.end() && group == nullptr) return Error("script_group_not_found", "Script group was not found.");
	Script *script = list == nullptr ? nullptr : FindScriptScoped(list, name, group_it == fields.end() ? nullptr : &requested_group);
	if (script == nullptr) return Error("script_not_found", "Script was not found.");
	if (group != nullptr) group->deleteScript(script); else list->deleteScript(script);
	SidesListUndoable *undoable = new SidesListUndoable(updated, document);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	return Success("{\"deleted\":true,\"player_index\":" + FormatInt(player)
		+ ",\"name\":" + JsonString(name_it->second)
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult ListTerrainTextures()
{
	CommandResult error;
	if (GetDocument(&error) == nullptr) return error;
	std::string result = "{\"textures\":[";
	Int count = WorldHeightMapEdit::getNumTexClasses();
	for (Int i = 0; i < count; ++i) {
		if (i != 0) result += ",";
		result += "{\"class\":" + FormatInt(i)
			+ ",\"name\":" + JsonString(AnsiToUtf8(WorldHeightMapEdit::getTexClassName(i).str()))
			+ ",\"ui_name\":" + JsonString(AnsiToUtf8(WorldHeightMapEdit::getTexClassUiName(i).str()))
			+ ",\"tiles\":" + FormatInt(WorldHeightMapEdit::getTexClassNumTiles(i))
			+ ",\"blend_edge\":" + (WorldHeightMapEdit::getTexClassIsBlendEdge(i) ? "true" : "false") + "}";
	}
	result += "],\"total\":" + FormatInt(count) + "}";
	return Success(result);
}

CommandResult GetTerrainCells(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr) return error;
	Int x = 0;
	Int y = 0;
	Int width = 0;
	Int height = 0;
	WorldHeightMapEdit *map = document->GetHeightMap();
	if (!GetRequiredInt(fields, "x", 0, INT_MAX, &x)
		|| !GetRequiredInt(fields, "y", 0, INT_MAX, &y)
		|| !GetRequiredInt(fields, "width", 1, MAX_TERRAIN_SAMPLES, &width)
		|| !GetRequiredInt(fields, "height", 1, MAX_TERRAIN_SAMPLES, &height)
		|| width > MAX_TERRAIN_SAMPLES / height
		|| x + width > map->getXExtent() - 1 || y + height > map->getYExtent() - 1) {
		return Error("terrain_region_out_of_bounds", "Terrain-cell rectangle is invalid or exceeds 4096 cells.");
	}
	std::string result = "{\"x\":" + FormatInt(x) + ",\"y\":" + FormatInt(y)
		+ ",\"width\":" + FormatInt(width) + ",\"height\":" + FormatInt(height) + ",\"cells\":[";
	Int count = 0;
	for (Int row = 0; row < height; ++row) {
		for (Int column = 0; column < width; ++column) {
			Int cell_x = x + column;
			Int cell_y = y + row;
			Int base_class = map->getTextureClass(cell_x, cell_y, true);
			Int resolved_class = map->getTextureClass(cell_x, cell_y, false);
			if (count++ != 0) result += ",";
			result += "{\"x\":" + FormatInt(cell_x) + ",\"y\":" + FormatInt(cell_y)
				+ ",\"texture_class\":" + FormatInt(resolved_class)
				+ ",\"base_texture_class\":" + FormatInt(base_class)
				+ ",\"blended\":" + (resolved_class != base_class ? "true" : "false")
				+ ",\"cliff\":" + (map->getCliffState(cell_x, cell_y) ? "true" : "false")
				+ ",\"passable\":" + (map->getCliffState(cell_x, cell_y) ? "false" : "true")
				+ ",\"flipped\":" + (map->getFlipState(cell_x, cell_y) ? "true" : "false") + "}";
		}
	}
	result += "]}";
	return Success(result);
}

CommandResult SetTerrainCells(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	Int count = 0;
	if (!GetRequiredInt(fields, "cell_count", 1, MAX_TERRAIN_SAMPLES, &count)) {
		return Error("invalid_arguments", "cell_count is invalid.");
	}
	WorldHeightMapEdit *edited = document->GetHeightMap()->duplicate();
	Bool changed = false;
	for (Int i = 0; i < count; ++i) {
		Int x = 0;
		Int y = 0;
		RequestFields::const_iterator x_it = fields.find(IndexedField("cell", i, "x"));
		RequestFields::const_iterator y_it = fields.find(IndexedField("cell", i, "y"));
		if (x_it == fields.end() || y_it == fields.end() || !ParseInt(x_it->second, &x) || !ParseInt(y_it->second, &y)
			|| x < 0 || y < 0 || x >= edited->getXExtent() - 1 || y >= edited->getYExtent() - 1) {
			REF_PTR_RELEASE(edited);
			return Error("terrain_cell_out_of_bounds", "A terrain cell is outside the map.");
		}
		RequestFields::const_iterator texture_it = fields.find(IndexedField("cell", i, "texture_class"));
		if (texture_it != fields.end()) {
			Int texture = 0;
			if (!ParseInt(texture_it->second, &texture) || texture < 0 || texture >= WorldHeightMapEdit::getNumTexClasses()
				|| !edited->setTileNdx(x, y, texture, true)) {
				REF_PTR_RELEASE(edited);
				return Error("invalid_texture_class", "A terrain texture class is invalid or cannot fit in this map.");
			}
			Bool auto_blend = true;
			RequestFields::const_iterator blend_it = fields.find(IndexedField("cell", i, "auto_blend"));
			if (blend_it != fields.end() && !ParseBool(blend_it->second, &auto_blend)) {
				REF_PTR_RELEASE(edited);
				return Error("invalid_arguments", "auto_blend must be boolean.");
			}
			if (auto_blend) edited->autoBlendOut(x, y);
			changed = true;
		}
		RequestFields::const_iterator passable_it = fields.find(IndexedField("cell", i, "passable"));
		if (passable_it != fields.end()) {
			Bool passable = false;
			if (!ParseBool(passable_it->second, &passable)) {
				REF_PTR_RELEASE(edited);
				return Error("invalid_arguments", "passable must be boolean.");
			}
			edited->setCliff(x, y, !passable);
			changed = true;
		}
	}
	if (!changed) {
		REF_PTR_RELEASE(edited);
		return Error("invalid_arguments", "Each cell must provide texture_class and/or passable.");
	}
	WBDocUndoable *undoable = new WBDocUndoable(document, edited);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	REF_PTR_RELEASE(edited);
	return Success("{\"updated\":true,\"cell_count\":" + FormatInt(count)
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

Bool IsLandmarkBridge(MapObject *object)
{
	const ThingTemplate *thing = object == nullptr ? nullptr : object->getThingTemplate();
	return thing != nullptr && thing->isBridge();
}

CommandResult ListLinearFeatures(const RequestFields &fields)
{
	CommandResult error;
	if (GetDocument(&error) == nullptr) return error;
	std::string kind = "all";
	RequestFields::const_iterator kind_it = fields.find("kind");
	if (kind_it != fields.end()) kind = kind_it->second;
	if (kind != "all" && kind != "road" && kind != "bridge") return Error("invalid_arguments", "kind is invalid.");
	std::string result = "{\"features\":[";
	Int count = 0;
	for (MapObject *object = MapObject::getFirstMapObject(); object != nullptr; object = object->getNext()) {
		Bool road = object->getFlag(FLAG_ROAD_POINT1) != 0;
		Bool bridge = object->getFlag(FLAG_BRIDGE_POINT1) != 0 || IsLandmarkBridge(object);
		if (!road && !bridge) continue;
		if ((kind == "road" && !road) || (kind == "bridge" && !bridge)) continue;
		MapObject *end = (road || object->getFlag(FLAG_BRIDGE_POINT1)) ? object->getNext() : nullptr;
		if ((road || object->getFlag(FLAG_BRIDGE_POINT1)) && end == nullptr) continue;
		if (count++ != 0) result += ",";
		result += "{\"id\":" + JsonString(GetObjectId(object))
			+ ",\"kind\":" + JsonString(bridge ? "bridge" : "road")
			+ ",\"template\":" + JsonString(GetObjectTemplateName(object))
			+ ",\"start\":" + ObjectToJson(object);
		result += ",\"end\":" + (end == nullptr ? "null" : ObjectToJson(end)) + "}";
	}
	result += "],\"total\":" + FormatInt(count) + "}";
	return Success(result);
}

CommandResult CreateLinearFeature(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	RequestFields::const_iterator kind_it = fields.find("kind");
	RequestFields::const_iterator template_it = fields.find("template");
	if (kind_it == fields.end() || template_it == fields.end()
		|| (kind_it->second != "road" && kind_it->second != "bridge") || template_it->second.empty()) {
		return Error("invalid_arguments", "kind and template are required.");
	}
	AsciiString template_name;
	if (!Utf8ToAsciiString(template_it->second, &template_name)) return Error("path_encoding_unsupported", "Template name is not representable.");
	Coord3D start;
	Coord3D end;
	if (!GetRequiredReal(fields, "start_x", &start.x) || !GetRequiredReal(fields, "start_y", &start.y)
		|| !GetRequiredReal(fields, "start_z", &start.z) || !GetRequiredReal(fields, "end_x", &end.x)
		|| !GetRequiredReal(fields, "end_y", &end.y) || !GetRequiredReal(fields, "end_z", &end.z)) {
		return Error("invalid_arguments", "start and end coordinates are invalid.");
	}
	const ThingTemplate *thing = TheThingFactory->findTemplate(template_name);
	if (kind_it->second == "bridge" && thing != nullptr && thing->isBridge()) {
		MapObject *landmark = newInstance(MapObject)(start, template_name, 0, 0, nullptr, thing);
		landmark->getProperties()->setAsciiString(TheKey_originalOwner, NEUTRAL_TEAM_INTERNAL_STR);
		AddObjectUndoable *undoable = new AddObjectUndoable(document, landmark);
		document->AddAndDoUndoable(undoable);
		REF_PTR_RELEASE(undoable);
		return Success("{\"created\":true,\"feature_id\":" + JsonString(GetObjectId(landmark))
			+ ",\"landmark\":true,\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
	}
	start.z = MAGIC_GROUND_Z;
	end.z = MAGIC_GROUND_Z;
	MapObject *first = newInstance(MapObject)(start, template_name, 0, 0, nullptr, thing);
	MapObject *second = newInstance(MapObject)(end, template_name, 0, 0, nullptr, thing);
	first->setColor(RGB(255, 255, 0));
	second->setColor(RGB(255, 255, 0));
	if (kind_it->second == "bridge") {
		first->setFlag(FLAG_BRIDGE_POINT1);
		second->setFlag(FLAG_BRIDGE_POINT2);
	} else {
		first->setFlag(FLAG_ROAD_POINT1);
		second->setFlag(FLAG_ROAD_POINT2);
		RequestFields::const_iterator corner_it = fields.find("corner");
		if (corner_it != fields.end() && corner_it->second == "angled") {
			first->setFlag(FLAG_ROAD_CORNER_ANGLED); second->setFlag(FLAG_ROAD_CORNER_ANGLED);
		} else if (corner_it != fields.end() && corner_it->second == "tight") {
			first->setFlag(FLAG_ROAD_CORNER_TIGHT); second->setFlag(FLAG_ROAD_CORNER_TIGHT);
		}
	}
	first->getProperties()->setAsciiString(TheKey_originalOwner, NEUTRAL_TEAM_INTERNAL_STR);
	second->getProperties()->setAsciiString(TheKey_originalOwner, NEUTRAL_TEAM_INTERNAL_STR);
	first->setNextMap(second);
	AddObjectUndoable *undoable = new AddObjectUndoable(document, first);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	return Success("{\"created\":true,\"feature_id\":" + JsonString(GetObjectId(first))
		+ ",\"end_object_id\":" + JsonString(GetObjectId(second))
		+ ",\"landmark\":false,\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult DeleteLinearFeature(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	RequestFields::const_iterator id_it = fields.find("feature_id");
	MapObject *object = id_it == fields.end() ? nullptr : FindObject(id_it->second);
	if (object == nullptr) return Error("feature_not_found", "Road or bridge feature was not found.");
	Bool endpoint = object->getFlag(FLAG_ROAD_POINT1) || object->getFlag(FLAG_BRIDGE_POINT1);
	if (!endpoint && !IsLandmarkBridge(object)) return Error("feature_not_found", "Object is not a first road/bridge endpoint.");
	PointerTool::clearSelection();
	object->setSelected(true);
	if (endpoint && object->getNext() != nullptr) object->getNext()->setSelected(true);
	std::string id = GetObjectId(object);
	DeleteObjectUndoable *undoable = new DeleteObjectUndoable(document);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	return Success("{\"deleted\":true,\"feature_id\":" + JsonString(id)
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult GetPlayableAreas()
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr) return error;
	std::string result = "{\"boundaries\":[";
	for (Int i = 0; i < document->getNumBoundaries(); ++i) {
		ICoord2D boundary;
		document->getBoundary(i, &boundary);
		if (i != 0) result += ",";
		result += "{\"index\":" + FormatInt(i) + ",\"x\":" + FormatInt(boundary.x)
			+ ",\"y\":" + FormatInt(boundary.y) + "}";
	}
	result += "],\"total\":" + FormatInt(document->getNumBoundaries()) + "}";
	return Success(result);
}

CommandResult SetPlayableAreas(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	Int count = 0;
	if (!GetRequiredInt(fields, "boundary_count", 1, 64, &count)) return Error("invalid_arguments", "boundary_count is invalid.");
	WorldHeightMapEdit *edited = document->GetHeightMap()->duplicate();
	while (edited->getNumBoundaries() > 0) edited->removeLastBoundary();
	for (Int i = 0; i < count; ++i) {
		RequestFields::const_iterator x_it = fields.find(IndexedField("boundary", i, "x"));
		RequestFields::const_iterator y_it = fields.find(IndexedField("boundary", i, "y"));
		ICoord2D boundary;
		if (x_it == fields.end() || y_it == fields.end() || !ParseInt(x_it->second, &boundary.x)
			|| !ParseInt(y_it->second, &boundary.y) || boundary.x <= 0 || boundary.y <= 0
			|| boundary.x > edited->getXExtent() || boundary.y > edited->getYExtent()) {
			REF_PTR_RELEASE(edited);
			return Error("invalid_boundary", "A playable-area boundary is outside the map.");
		}
		edited->addBoundary(&boundary);
	}
	WBDocUndoable *undoable = new WBDocUndoable(document, edited);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	REF_PTR_RELEASE(edited);
	return Success("{\"updated\":true,\"boundary_count\":" + FormatInt(count)
		+ ",\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

NameKeyType MapDescriptionKey()
{
	return TheNameKeyGenerator->nameToKey("mapDescription");
}

NameKeyType MapPlayerCountKey()
{
	return TheNameKeyGenerator->nameToKey("mapPlayerCount");
}

std::string LightingToJson(const GlobalData::TerrainLighting &lighting)
{
	return "{\"ambient\":{\"red\":" + FormatReal(lighting.ambient.red)
		+ ",\"green\":" + FormatReal(lighting.ambient.green) + ",\"blue\":" + FormatReal(lighting.ambient.blue)
		+ "},\"diffuse\":{\"red\":" + FormatReal(lighting.diffuse.red)
		+ ",\"green\":" + FormatReal(lighting.diffuse.green) + ",\"blue\":" + FormatReal(lighting.diffuse.blue)
		+ "},\"direction\":{\"x\":" + FormatReal(lighting.lightPos.x)
		+ ",\"y\":" + FormatReal(lighting.lightPos.y) + ",\"z\":" + FormatReal(lighting.lightPos.z) + "}}";
}

CommandResult GetMapSettings()
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr) return error;
	Dict *world = MapObject::getWorldDict();
	Bool exists = false;
	AsciiString name = world->getAsciiString(TheKey_mapName, &exists);
	if (!exists) name.clear();
	AsciiString description = world->getAsciiString(MapDescriptionKey(), &exists);
	if (!exists) description.clear();
	Int players = world->getInt(MapPlayerCountKey(), &exists);
	if (!exists) players = 0;
	Int tod = static_cast<Int>(TheGlobalData->m_timeOfDay);
	std::string result = "{\"name\":" + JsonString(AnsiToUtf8(name.str()))
		+ ",\"description\":" + JsonString(AnsiToUtf8(description.str()))
		+ ",\"player_count\":" + (players == 0 ? "null" : FormatInt(players))
		+ ",\"time_of_day\":" + FormatInt(tod)
		+ ",\"weather\":" + FormatInt(static_cast<Int>(TheGlobalData->m_weather))
		+ ",\"water_height\":" + FormatReal(TheGlobalData->m_waterPositionZ)
		+ ",\"lighting\":{\"terrain\":[";
	for (Int i = 0; i < MAX_GLOBAL_LIGHTS; ++i) {
		if (i != 0) result += ",";
		result += LightingToJson(TheGlobalData->m_terrainLighting[tod][i]);
	}
	result += "],\"objects\":[";
	for (Int i = 0; i < MAX_GLOBAL_LIGHTS; ++i) {
		if (i != 0) result += ",";
		result += LightingToJson(TheGlobalData->m_terrainObjectsLighting[tod][i]);
	}
	result += "]},\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}";
	return Success(result);
}

Bool ReadColorOrDirection(
	const RequestFields &fields, const char *prefix, GlobalData::TerrainLighting *lighting)
{
	RequestFields::const_iterator x_it = fields.find(std::string(prefix) + "_x");
	if (x_it == fields.end()) return true;
	RequestFields::const_iterator y_it = fields.find(std::string(prefix) + "_y");
	RequestFields::const_iterator z_it = fields.find(std::string(prefix) + "_z");
	Real x = 0;
	Real y = 0;
	Real z = 0;
	if (y_it == fields.end() || z_it == fields.end() || !ParseReal(x_it->second, &x)
		|| !ParseReal(y_it->second, &y) || !ParseReal(z_it->second, &z)) return false;
	if (strcmp(prefix, "ambient") == 0) {
		if (x < 0 || x > 1 || y < 0 || y > 1 || z < 0 || z > 1) return false;
		lighting->ambient.red = x; lighting->ambient.green = y; lighting->ambient.blue = z;
	} else if (strcmp(prefix, "diffuse") == 0) {
		if (x < 0 || x > 1 || y < 0 || y > 1 || z < 0 || z > 1) return false;
		lighting->diffuse.red = x; lighting->diffuse.green = y; lighting->diffuse.blue = z;
	} else {
		lighting->lightPos.x = x; lighting->lightPos.y = y; lighting->lightPos.z = z;
	}
	return true;
}

CommandResult UpdateMapSettings(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	McpMapSettingsState next = CaptureMapSettings();
	RequestFields::const_iterator it = fields.find("name");
	AsciiString text;
	if (it != fields.end()) {
		if (!Utf8ToAsciiString(it->second, &text)) return Error("path_encoding_unsupported", "Map name is not representable.");
		next.world.setAsciiString(TheKey_mapName, text);
	}
	it = fields.find("description");
	if (it != fields.end()) {
		if (!Utf8ToAsciiString(it->second, &text)) return Error("path_encoding_unsupported", "Map description is not representable.");
		next.world.setAsciiString(MapDescriptionKey(), text);
	}
	Int integer = 0;
	it = fields.find("player_count");
	if (it != fields.end()) {
		if (!ParseInt(it->second, &integer) || integer < 1 || integer > 8) return Error("invalid_arguments", "player_count must be 1 through 8.");
		next.world.setInt(MapPlayerCountKey(), integer);
	}
	it = fields.find("time_of_day");
	if (it != fields.end()) {
		if (!ParseInt(it->second, &integer) || integer < TIME_OF_DAY_FIRST || integer >= TIME_OF_DAY_COUNT) return Error("invalid_arguments", "time_of_day is invalid.");
		next.timeOfDay = static_cast<TimeOfDay>(integer);
	}
	it = fields.find("weather");
	if (it != fields.end()) {
		if (!ParseInt(it->second, &integer) || integer < 0 || integer >= WEATHER_COUNT) return Error("invalid_arguments", "weather is invalid.");
		next.weather = static_cast<Weather>(integer);
	}
	it = fields.find("water_height");
	if (it != fields.end() && !ParseReal(it->second, &next.waterHeight)) return Error("invalid_arguments", "water_height must be numeric.");

	Bool has_lighting = fields.find("ambient_x") != fields.end() || fields.find("diffuse_x") != fields.end()
		|| fields.find("direction_x") != fields.end();
	if (has_lighting) {
		std::string target = "terrain";
		it = fields.find("lighting_target");
		if (it != fields.end()) target = it->second;
		Int light = 0;
		if ((target != "terrain" && target != "objects")
			|| !GetOptionalInt(fields, "light_index", 0, 0, MAX_GLOBAL_LIGHTS - 1, &light)) {
			return Error("invalid_arguments", "lighting_target or light_index is invalid.");
		}
		GlobalData::TerrainLighting *lighting = target == "terrain"
			? &next.terrain[static_cast<Int>(next.timeOfDay)][light]
			: &next.objects[static_cast<Int>(next.timeOfDay)][light];
		if (!ReadColorOrDirection(fields, "ambient", lighting)
			|| !ReadColorOrDirection(fields, "diffuse", lighting)
			|| !ReadColorOrDirection(fields, "direction", lighting)) {
			return Error("invalid_arguments", "Lighting colors must be 0..1 and direction must be numeric.");
		}
	}
	McpMapSettingsUndoable *undoable = new McpMapSettingsUndoable(document, next);
	document->AddAndDoUndoable(undoable);
	REF_PTR_RELEASE(undoable);
	return Success("{\"updated\":true,\"revision\":" + FormatUnsigned(document->getChangeSerial()) + "}");
}

CommandResult GeneratePreview(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr || !CheckExpectedRevision(fields, document, &error)) return error;
	CString map_path = document->GetPathName();
	if (map_path.IsEmpty()) return Error("map_has_no_path", "Save the map with save_map_as before generating a preview.");
	MapPreview preview;
	preview.save(map_path);
	CString preview_path = map_path;
	preview_path.Replace(_T(".map"), _T(".tga"));
	if (GetFileAttributes(preview_path) == INVALID_FILE_ATTRIBUTES) {
		return Error("preview_failed", "WorldBuilder did not create the preview file.");
	}
	return Success("{\"generated\":true,\"path\":" + JsonString(CStringToUtf8(preview_path)) + "}");
}

Bool SaveWindowBitmap(HWND window, const std::wstring &path, Int *saved_width, Int *saved_height)
{
	RECT client;
	if (window == nullptr || GetClientRect(window, &client) == FALSE) return false;
	Int width = client.right - client.left;
	Int height = client.bottom - client.top;
	if (width <= 0 || height <= 0) return false;
	HDC source = GetDC(window);
	HDC memory = CreateCompatibleDC(source);
	HBITMAP bitmap = CreateCompatibleBitmap(source, width, height);
	HGDIOBJ old = SelectObject(memory, bitmap);
	Bool copied = BitBlt(memory, 0, 0, width, height, source, 0, 0, SRCCOPY | CAPTUREBLT) != FALSE;
	BITMAPINFO info;
	memset(&info, 0, sizeof(info));
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = width;
	info.bmiHeader.biHeight = -height;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	std::vector<UnsignedByte> pixels(static_cast<size_t>(width) * height * 4);
	Bool read = copied && GetDIBits(memory, bitmap, 0, height, &pixels[0], &info, DIB_RGB_COLORS) != 0;
	SelectObject(memory, old);
	DeleteObject(bitmap);
	DeleteDC(memory);
	ReleaseDC(window, source);
	if (!read) return false;
	BITMAPFILEHEADER file_header;
	memset(&file_header, 0, sizeof(file_header));
	file_header.bfType = 0x4d42;
	file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	file_header.bfSize = file_header.bfOffBits + static_cast<DWORD>(pixels.size());
	HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;
	DWORD written = 0;
	Bool ok = WriteFile(file, &file_header, sizeof(file_header), &written, nullptr) != FALSE && written == sizeof(file_header)
		&& WriteFile(file, &info.bmiHeader, sizeof(info.bmiHeader), &written, nullptr) != FALSE && written == sizeof(info.bmiHeader)
		&& WriteFile(file, &pixels[0], static_cast<DWORD>(pixels.size()), &written, nullptr) != FALSE && written == pixels.size();
	CloseHandle(file);
	if (ok) { *saved_width = width; *saved_height = height; }
	return ok;
}

CommandResult CaptureView(const RequestFields &fields)
{
	CommandResult error;
	CWorldBuilderDoc *document = GetDocument(&error);
	if (document == nullptr) return error;
	wchar_t temp[MAX_PATH + 1];
	DWORD length = GetTempPathW(ARRAY_SIZE(temp), temp);
	if (length == 0 || length >= ARRAY_SIZE(temp)) return Error("capture_failed", "System temporary directory is unavailable.");
	std::wstring directory(temp);
	if (directory[directory.size() - 1] != L'\\') directory += L'\\';
	directory += L"GeneralsWorldBuilderMcp";
	CreateDirectoryW(directory.c_str(), nullptr);
	std::wstring path;
	RequestFields::const_iterator path_it = fields.find("path");
	if (path_it != fields.end()) {
		if (!Utf8ToWide(path_it->second, &path) || !IsAllowedBridgePath(path)) {
			return Error("path_outside_temp", "capture_view path must be a direct child of the MCP temporary directory.");
		}
	} else {
		wchar_t filename[96];
		_snwprintf(filename, ARRAY_SIZE(filename), L"\\view-%lu-%lu.bmp", GetCurrentProcessId(), GetTickCount());
		filename[ARRAY_SIZE(filename) - 1] = L'\0';
		path = directory + filename;
	}
	if (path.size() < 4 || _wcsicmp(path.c_str() + path.size() - 4, L".bmp") != 0) {
		return Error("invalid_capture_path", "capture_view output must use the .bmp extension.");
	}
	WbView3d *view = document->Get3DView();
	HWND window = view != nullptr ? view->GetSafeHwnd() : (AfxGetMainWnd() != nullptr ? AfxGetMainWnd()->GetSafeHwnd() : nullptr);
	Int width = 0;
	Int height = 0;
	if (!SaveWindowBitmap(window, path, &width, &height)) return Error("capture_failed", "WorldBuilder could not capture the current view.");
	return Success("{\"captured\":true,\"path\":" + JsonString(WideToUtf8(path.c_str()))
		+ ",\"width\":" + FormatInt(width) + ",\"height\":" + FormatInt(height) + ",\"format\":\"bmp\"}");
}

CommandResult Dispatch(const RequestFields &fields)
{
	std::string command = fields.find("command")->second;
	if (command == "get_bridge_info") return GetBridgeInfo();
	if (command == "list_user_maps") return ListUserMaps();
	if (command == "open_map") return OpenMap(fields);
	if (command == "get_state") return GetState();
	if (command == "list_objects") return ListObjects(fields);
	if (command == "get_object") return GetObject(fields);
	if (command == "list_templates") return ListTemplates(fields);
	if (command == "add_object") return AddObject(fields);
	if (command == "add_objects") return AddObjects(fields);
	if (command == "update_object") return UpdateObject(fields);
	if (command == "update_objects") return UpdateObjects(fields);
	if (command == "delete_object") return DeleteObject(fields);
	if (command == "delete_objects") return DeleteObjects(fields);
	if (command == "select_object") return SelectObject(fields);
	if (command == "list_players") return ListPlayers();
	if (command == "list_teams") return ListTeams();
	if (command == "create_team") return CreateTeam(fields);
	if (command == "update_team") return UpdateTeam(fields);
	if (command == "delete_team") return DeleteTeam(fields);
	if (command == "validate_map") return ValidateMap();
	if (command == "get_terrain_heights") return GetTerrainHeights(fields);
	if (command == "set_terrain_heights") return SetTerrainHeights(fields);
	if (command == "focus_view") return FocusView(fields);
	if (command == "save_map") return SaveMap(fields);
	if (command == "save_map_as") return SaveMapAs(fields);
	if (command == "undo") return UndoOrRedo(fields, false);
	if (command == "redo") return UndoOrRedo(fields, true);
	if (command == "new_map") return NewMap(fields);
	if (command == "close_map") return CloseMap(fields);
	if (command == "list_waypoints") return ListWaypoints();
	if (command == "create_waypoint") return CreateWaypoint(fields);
	if (command == "update_waypoint") return UpdateWaypoint(fields);
	if (command == "delete_waypoint") return DeleteWaypoint(fields);
	if (command == "connect_waypoints") return ChangeWaypointLink(fields, true);
	if (command == "disconnect_waypoints") return ChangeWaypointLink(fields, false);
	if (command == "list_areas") return ListAreas(fields);
	if (command == "create_area") return CreateArea(fields);
	if (command == "update_area") return UpdateArea(fields);
	if (command == "delete_area") return DeleteArea(fields);
	if (command == "list_script_types") return ListScriptTypes();
	if (command == "list_sciences") return ListSciences();
	if (command == "list_upgrades") return ListUpgrades();
	if (command == "list_scripts") return ListScripts(fields);
	if (command == "get_script") return GetScript(fields);
	if (command == "upsert_script") return UpsertScript(fields);
	if (command == "delete_script") return DeleteScript(fields);
	if (command == "list_terrain_textures") return ListTerrainTextures();
	if (command == "get_terrain_cells") return GetTerrainCells(fields);
	if (command == "set_terrain_cells") return SetTerrainCells(fields);
	if (command == "list_linear_features") return ListLinearFeatures(fields);
	if (command == "create_linear_feature") return CreateLinearFeature(fields);
	if (command == "delete_linear_feature") return DeleteLinearFeature(fields);
	if (command == "get_playable_areas") return GetPlayableAreas();
	if (command == "set_playable_areas") return SetPlayableAreas(fields);
	if (command == "get_map_settings") return GetMapSettings();
	if (command == "update_map_settings") return UpdateMapSettings(fields);
	if (command == "generate_preview") return GeneratePreview(fields);
	if (command == "capture_view") return CaptureView(fields);
	return Error("unknown_command", "The WorldBuilder MCP command is not supported.");
}

void CancelQueuedRequest(std::wstring *request_path)
{
	if (request_path == nullptr) return;
	std::string request_contents;
	RequestFields fields;
	std::wstring response_path;
	if (IsAllowedBridgePath(*request_path)
		&& ReadFileBytes(*request_path, &request_contents)
		&& ParseRequest(request_contents, &fields)
		&& Utf8ToWide(fields["response_path"], &response_path)
		&& IsAllowedBridgePath(response_path)) {
		WriteFileBytes(response_path, MakeEnvelope(Error(
			"request_cancelled", "WorldBuilder stopped the MCP bridge before dispatching the request.")));
	}
	delete request_path;
}

} // namespace

namespace WorldBuilderMcpBridge
{

void Attach(HWND window)
{
	if (window != nullptr) {
		SetPropW(window, WINDOW_PROPERTY, reinterpret_cast<HANDLE>(BRIDGE_VERSION));
	}
}

void Detach(HWND window)
{
	if (window != nullptr) {
		RemovePropW(window, WINDOW_PROPERTY);
		// TheSuperHackers @bugfix Eugene 30/08/2026 Cancel queued MCP payloads when the endpoint closes or is disabled.
		MSG message;
		while (PeekMessageW(
			&message, window, PROCESS_REQUEST_MESSAGE, PROCESS_REQUEST_MESSAGE, PM_REMOVE) != FALSE) {
			CancelQueuedRequest(reinterpret_cast<std::wstring *>(message.lParam));
		}
	}
}

BOOL IsEnabled(HWND window)
{
	return window != nullptr && GetPropW(window, WINDOW_PROPERTY) != nullptr;
}

BOOL IsRequest(const COPYDATASTRUCT *copy_data)
{
	return copy_data != nullptr
		&& copy_data->dwData == REQUEST_MAGIC
		&& copy_data->lpData != nullptr
		&& copy_data->cbData >= sizeof(wchar_t)
		&& copy_data->cbData <= 65536
		&& copy_data->cbData % sizeof(wchar_t) == 0;
}

BOOL HandleRequest(const COPYDATASTRUCT *copy_data)
{
	if (!IsRequest(copy_data)) {
		return FALSE;
	}

	const wchar_t *request_path_value = static_cast<const wchar_t *>(copy_data->lpData);
	size_t character_count = copy_data->cbData / sizeof(wchar_t);
	if (request_path_value[character_count - 1] != L'\0') {
		return FALSE;
	}

	std::wstring request_path(request_path_value);
	if (!IsAllowedBridgePath(request_path)) {
		return FALSE;
	}

	std::string request_contents;
	RequestFields fields;
	if (!ReadFileBytes(request_path, &request_contents) || !ParseRequest(request_contents, &fields)) {
		return FALSE;
	}

	std::wstring response_path;
	if (!Utf8ToWide(fields["response_path"], &response_path) || !IsAllowedBridgePath(response_path)) {
		return FALSE;
	}

	CommandResult result;
	try {
		result = Dispatch(fields);
	} catch (...) {
		result = Error("internal_error", "WorldBuilder failed while executing the MCP command.");
	}
	return WriteFileBytes(response_path, MakeEnvelope(result)) ? TRUE : FALSE;
}

BOOL QueueRequest(HWND window, const COPYDATASTRUCT *copy_data)
{
	if (window == nullptr || !IsRequest(copy_data)) {
		return FALSE;
	}
	const wchar_t *request_path_value = static_cast<const wchar_t *>(copy_data->lpData);
	size_t character_count = copy_data->cbData / sizeof(wchar_t);
	if (request_path_value[character_count - 1] != L'\0') {
		return FALSE;
	}
	std::wstring *request_path = new std::wstring(request_path_value);
	if (!IsAllowedBridgePath(*request_path)
		|| PostMessageW(window, PROCESS_REQUEST_MESSAGE, 0, reinterpret_cast<LPARAM>(request_path)) == FALSE) {
		delete request_path;
		return FALSE;
	}
	return TRUE;
}

LRESULT ProcessQueuedRequest(LPARAM parameter)
{
	std::wstring *request_path = reinterpret_cast<std::wstring *>(parameter);
	if (request_path == nullptr) {
		return FALSE;
	}
	COPYDATASTRUCT copy_data;
	copy_data.dwData = REQUEST_MAGIC;
	copy_data.cbData = static_cast<DWORD>((request_path->size() + 1) * sizeof(wchar_t));
	copy_data.lpData = const_cast<wchar_t *>(request_path->c_str());
	BOOL result = HandleRequest(&copy_data);
	delete request_path;
	return result;
}

} // namespace WorldBuilderMcpBridge
