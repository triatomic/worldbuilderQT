// WBQtMapFileBridge.cpp -- MFC side of the Qt Open/Save Map seam (Tier 3d). Plain MFC TU (no
// Qt include). Open Map: OpenMap's qt* member functions are defined here (member functions may
// be defined in any TU); the dialog is created HIDDEN so its enumeration/search/packed-.big
// logic runs verbatim, with a TOpenMapInfo sentinel distinguishing "completed" picks from
// packed-mode drills. Save Map: faithful free-function ports of the small populate/overwrite
// logic. Whole body guarded by RTS_HAS_QT so the OFF build compiles it to an empty object.
#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "WorldBuilder.h"
#include "OpenMap.h"
#include "SaveMap.h"
#include "Common/GlobalData.h"
#include "Common/ArchiveFile.h"
#include "Common/ArchiveFileSystem.h"
#include "Common/File.h"
#include "Common/FileSystem.h"
#include "qt/panels/WBQtMapFileBridge.h"
#include <vector>

#ifdef RTS_HAS_QT

static void copyOut(const char *str, char *buf, int cap)
{
	if (buf == NULL || cap <= 0)
	{
		return;
	}
	strncpy(buf, str ? str : "", cap - 1);
	buf[cap - 1] = 0;
}

extern "C" int WBQtMapFileData_RadiosVisible(void)
{
	// == the MFC pages hiding System/User outside debug/internal builds.
#if defined(_DEBUG) || defined(_INTERNAL)
	return 1;
#else
	return 0;
#endif
}

// ================= Open Map (hidden-dialog driven) =================

static OpenMap *s_qtOpenMap = NULL;
static TOpenMapInfo s_qtOpenInfo;

// De-bridged view model (qt-debridge): the rows the hidden listbox used to hold, plus the
// selection and the OK-enable state the MFC populate*/EnableWindow calls used to encode.
//
// MUST be a CStringArray, NOT std::vector<CString>: WB globally overrides operator new/delete
// (the game MemoryPool), so a std::vector's element storage comes from the game pool while each
// CString's own buffer comes from MFC's CRT heap. When the vector reallocates/clears/copies its
// CString elements (which the packed-.big drill does every populate cycle), the two allocators
// cross and the heap gets corrupted -- detonating later on a free() with a __debugbreak from the
// heap-corruption detector (seen live: CMemFile::Free -> CStringData::Release). CStringArray
// keeps every allocation on the CRT heap, so there is no crossing. See the MemoryPool/Qt
// allocator-collision note in the migration memory.
static CStringArray s_qtView;
static int s_qtViewSel = -1;
static Bool s_qtOkEnabled = TRUE;

OpenMap *OpenMap::qtInstance(void)
{
	return s_qtOpenMap;
}

OpenMap *OpenMap::qtOpen(void)
{
	if (s_qtOpenMap == NULL)
	{
		s_qtOpenInfo.filename = "";
		s_qtOpenInfo.browse = false;
		s_qtOpenMap = new OpenMap(&s_qtOpenInfo);
		// De-bridged (qt-debridge): the dialog window is never Create()d -- the object only
		// carries the mode/list/sentinel state and the qtM* fills own the view model (the
		// MFC populate* handlers early-return without their controls).
		s_qtView.RemoveAll();
		s_qtViewSel = -1;
		s_qtOkEnabled = TRUE;
		s_qtOpenMap->qtMPopulateMain(s_qtOpenMap->m_usingSystemDir);
	}
	return s_qtOpenMap;
}

void OpenMap::qtClose(void)
{
	if (s_qtOpenMap != NULL)
	{
		s_qtOpenMap->DestroyWindow();	// harmless no-op: the window was never Create()d
		delete s_qtOpenMap;
		s_qtOpenMap = NULL;
	}
}

// == populateMapListbox minus the listbox: the Maps\<name>\<name>.map directory walk fills
// the full list + the view. Like the MFC path, an empty result disables OK without ever
// re-enabling it (bug-compatible).
void OpenMap::qtMPopulateMain(Bool systemMaps)
{
	m_usingSystemDir = systemMaps;
#if defined(_DEBUG) || defined(_INTERNAL)
	::AfxGetApp()->WriteProfileInt(MAP_OPENSAVE_PANEL_SECTION, "UseSystemDir", m_usingSystemDir);
#endif

	HANDLE hFindFile = 0;
	WIN32_FIND_DATA findData;
	char dirBuf[_MAX_PATH];
	char findBuf[_MAX_PATH];
	char fileBuf[_MAX_PATH];

	if (systemMaps)
	{
		strcpy(dirBuf, "Maps\\");
	}
	else
	{
		strcpy(dirBuf, TheGlobalData->getPath_UserData().str());
		strcat(dirBuf, "Maps\\");
	}
	int len = strlen(dirBuf);
	if (len > 0 && dirBuf[len - 1] != '\\')
	{
		dirBuf[len++] = '\\';
		dirBuf[len] = 0;
	}
	s_qtView.RemoveAll();
	strcpy(findBuf, dirBuf);
	strcat(findBuf, "*.*");

	m_fullMapList.RemoveAll();
	Bool found = false;

	hFindFile = FindFirstFile(findBuf, &findData);
	if (hFindFile != INVALID_HANDLE_VALUE)
	{
		do {
			if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
			{
				continue;
			}
			if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				continue;
			}
			strcpy(fileBuf, dirBuf);
			strcat(fileBuf, findData.cFileName);
			strcat(fileBuf, "\\");
			strcat(fileBuf, findData.cFileName);
			strcat(fileBuf, ".map");
			try {
				CFileStatus status;
				if (CFile::GetStatus(fileBuf, status))
				{
					if (!(status.m_attribute & CFile::directory))
					{
						s_qtView.Add(CString(findData.cFileName));
						m_fullMapList.Add(findData.cFileName);
						found = true;
					}
				}
			} catch(...) {}
		} while (FindNextFile(hFindFile, &findData));
		if (hFindFile)
		{
			FindClose(hFindFile);
		}
	}
	if (found)
	{
		s_qtViewSel = 0;
	}
	else
	{
		s_qtViewSel = -1;
		s_qtOkEnabled = FALSE;
	}
}

// == populatePackedBigList minus the listbox.
void OpenMap::qtMPopulateBigs(void)
{
	m_packedMode = PM_LIST_BIGS;
	m_currentBig.Empty();
	s_qtView.RemoveAll();
	m_fullMapList.RemoveAll();
	m_packedMapPaths.RemoveAll();

	HANDLE hFind;
	WIN32_FIND_DATA fd;
	Bool found = false;
	hFind = FindFirstFile("*.big", &fd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do {
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				continue;
			}
			s_qtView.Add(CString(fd.cFileName));
			m_fullMapList.Add(fd.cFileName);
			found = true;
		} while (FindNextFile(hFind, &fd));
		FindClose(hFind);
	}
	if (found)
	{
		s_qtViewSel = 0;
		s_qtOkEnabled = TRUE;
	}
	else
	{
		::AfxMessageBox("No .big archives found in the WorldBuilder directory.");
		s_qtViewSel = -1;
		s_qtOkEnabled = FALSE;
	}
}

// == populatePackedMapList minus the listbox: archive walk + display-name extraction; the
// paths stay parallel to m_fullMapList for the pick-time resolve.
void OpenMap::qtMPopulateMapsInBig(const CString &bigPathRef)
{
	// bigPathRef often aliases an element of s_qtView (qtPick passes s_qtView[row] here). The
	// RemoveAll()s below free that element's buffer, leaving the reference dangling -- the later
	// m_currentBig = bigPath then copied freed memory, so the nested-map extract opened an EMPTY
	// archive filename and crashed. Take a private copy up front so it survives the clears.
	CString bigPath = bigPathRef;

	ArchiveFile *pArchive = TheArchiveFileSystem
		? TheArchiveFileSystem->openArchiveFile((const char *)bigPath) : NULL;
	if (!pArchive)
	{
		::AfxMessageBox("Could not open the selected .big archive.");
		return;
	}
	FilenameList maps;
	pArchive->getFileListInDirectory(AsciiString(""), AsciiString(""), AsciiString("*.map"), maps, TRUE);

	s_qtView.RemoveAll();
	m_fullMapList.RemoveAll();
	m_packedMapPaths.RemoveAll();

	Bool found = false;
	for (FilenameListIter it = maps.begin(); it != maps.end(); ++it)
	{
		AsciiString fullPath = *it;
		AsciiString display = fullPath;
		const char *slash = strrchr(display.str(), '\\');
		if (!slash)
		{
			slash = strrchr(display.str(), '/');
		}
		CString name = slash ? (slash + 1) : display.str();
		int dot = name.ReverseFind('.');
		if (dot > 0)
		{
			name = name.Left(dot);
		}
		s_qtView.Add(name);
		m_fullMapList.Add(name);
		m_packedMapPaths.Add(CString(fullPath.str()));
		found = true;
	}
	pArchive->close();
	delete pArchive;

	m_packedMode = PM_LIST_MAPS_IN_BIG;
	m_currentBig = bigPath;

	if (found)
	{
		s_qtViewSel = 0;
		s_qtOkEnabled = TRUE;
	}
	else
	{
		::AfxMessageBox("No maps found inside this .big archive.");
		s_qtViewSel = -1;
		s_qtOkEnabled = FALSE;
	}
}

int OpenMap::qtListCount(void)
{
	return (int)s_qtView.GetSize();
}

void OpenMap::qtListItem(int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	if (i >= 0 && i < (int)s_qtView.GetSize())
	{
		copyOut((LPCTSTR)s_qtView[i], buf, cap);
	}
}

// The preview .tga sits next to the .map inside the map's own folder -- the same
// path construction qtPick uses for the .map, with the extension swapped.
void OpenMap::qtItemPreviewPath(int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	if (i < 0 || i >= (int)s_qtView.GetSize())
	{
		return;
	}
	if (m_packedMode != PM_OFF)
	{
		return;	// archive rows have no folder on disk
	}
	CString name = s_qtView[i];
	CString path;
	if (m_usingSystemDir)
	{
		path = ".\\Maps\\" + name + "\\" + name + ".tga";
	}
	else
	{
		path = TheGlobalData->getPath_UserData().str();
		path = path + "Maps\\" + name + "\\" + name + ".tga";
	}
	copyOut((LPCTSTR)path, buf, cap);
}

// Packed rows: the <MapName>.tga sits next to the .map INSIDE the archive; read its
// bytes directly (same openFile/read pattern as extractPackedMap, no disk writes).
int OpenMap::qtItemPreviewData(int i, unsigned char *buf, int cap)
{
	if (buf == NULL || cap <= 0)
	{
		return 0;
	}
	if (m_packedMode != PM_LIST_MAPS_IN_BIG || i < 0 || i >= (int)s_qtView.GetSize())
	{
		return 0;
	}

	// Resolve the archive-internal .map path by display name (parallel arrays, same
	// as qtPick), then swap the extension for the preview.
	CString selName = s_qtView[i];
	CString archiveMapPath;
	for (int k = 0; k < m_fullMapList.GetSize(); ++k)
	{
		if (m_fullMapList[k].CompareNoCase(selName) == 0)
		{
			archiveMapPath = m_packedMapPaths[k];
			break;
		}
	}
	if (archiveMapPath.IsEmpty())
	{
		return 0;
	}
	int dot = archiveMapPath.ReverseFind('.');
	if (dot <= 0)
	{
		return 0;
	}
	CString tgaPath = archiveMapPath.Left(dot) + ".tga";

	ArchiveFile *pArchive = TheArchiveFileSystem
		? TheArchiveFileSystem->openArchiveFile((const char *)m_currentBig) : NULL;
	if (!pArchive)
	{
		return 0;
	}
	int got = 0;
	File *pf = pArchive->openFile((const char *)tgaPath, File::READ | File::BINARY);
	if (pf)
	{
		Int sz = pf->size();
		if (sz > 0 && sz <= cap)
		{
			got = pf->read(buf, sz);
			if (got < 0)
			{
				got = 0;
			}
		}
		pf->close();
	}
	pArchive->close();
	delete pArchive;
	return got;
}

int OpenMap::qtListCurSel(void)
{
	if (s_qtViewSel < 0 || s_qtViewSel >= (int)s_qtView.GetSize())
	{
		return -1;
	}
	return s_qtViewSel;
}

int OpenMap::qtOkEnabled(void)
{
	return s_qtOkEnabled ? 1 : 0;
}

int OpenMap::qtGetMode(void)
{
	if (m_packedMode != PM_OFF)
	{
		return WB_QT_MAPMODE_PACKED;
	}
	return m_usingSystemDir ? WB_QT_MAPMODE_SYSTEM : WB_QT_MAPMODE_USER;
}

void OpenMap::qtSetMode(int mode)
{
	// De-bridged: no radio controls to mirror; run the model-only fills directly
	// (== OnSystemMaps / OnUserMaps / OnPackedMaps).
	switch (mode)
	{
		case WB_QT_MAPMODE_SYSTEM:
			m_packedMode = PM_OFF;
			qtMPopulateMain(TRUE);
			break;
		case WB_QT_MAPMODE_PACKED:
			qtMPopulateBigs();
			break;
		default:
			m_packedMode = PM_OFF;
			qtMPopulateMain(FALSE);
			break;
	}
}

void OpenMap::qtSearch(const char *text)
{
	// == OnSearchMap minus the edit/listbox: filter m_fullMapList into the view. A no-match
	// search beeps and leaves the view empty, like the MFC listbox.
	CString search(text ? text : "");
	search.MakeLower();

	s_qtView.RemoveAll();
	if (search.IsEmpty())
	{
		for (int i = 0; i < m_fullMapList.GetSize(); i++)
		{
			s_qtView.Add(m_fullMapList[i]);
		}
		s_qtViewSel = (s_qtView.GetSize() == 0) ? -1 : 0;
		return;
	}
	bool found = false;
	for (int i = 0; i < m_fullMapList.GetSize(); i++)
	{
		CString name = m_fullMapList[i];
		CString lower = name;
		lower.MakeLower();
		if (lower.Find(search) != -1)
		{
			s_qtView.Add(name);
			found = true;
		}
	}
	if (!found)
	{
		::MessageBeep(MB_ICONEXCLAMATION); // no matches
		s_qtViewSel = -1;
	}
	else
	{
		s_qtViewSel = 0;
	}
}

void OpenMap::qtResetSearch(void)
{
	// == OnResetSearch minus the edit/listbox.
	s_qtView.RemoveAll();
	for (int i = 0; i < m_fullMapList.GetSize(); i++)
	{
		s_qtView.Add(m_fullMapList[i]);
	}
	s_qtViewSel = (m_fullMapList.GetSize() > 0) ? 0 : -1;
}

int OpenMap::qtPick(int row)
{
	s_qtViewSel = row;
	// Sentinel: every COMPLETING path sets a filename or the browse flag; the packed
	// drill-into-archive path touches neither (it relists and stays open).
	s_qtOpenInfo.filename = "";
	s_qtOpenInfo.browse = false;

	// == OnOK minus the listbox/focus reads, keyed by the view row.
	Bool rowValid = (row >= 0 && row < (int)s_qtView.GetSize());

	if (m_packedMode == PM_LIST_BIGS)
	{
		if (rowValid)
		{
			qtMPopulateMapsInBig(s_qtView[row]);	// descend into the archive; stay open
		}
	}
	else if (m_packedMode == PM_LIST_MAPS_IN_BIG)
	{
		if (rowValid)
		{
			// The view is filtered/sorted independently of m_packedMapPaths, so resolve the
			// archive path by matching the display name against m_fullMapList (parallel
			// arrays in archive-enumeration order) -- same as OnOK.
			CString selName = s_qtView[row];
			CString archiveMapPath;
			for (int i = 0; i < m_fullMapList.GetSize(); ++i)
			{
				if (m_fullMapList[i].CompareNoCase(selName) == 0)
				{
					archiveMapPath = m_packedMapPaths[i];
					break;
				}
			}
			if (archiveMapPath.IsEmpty())
			{
				::AfxMessageBox("Could not resolve the selected packed map.");
			}
			else
			{
				CString extracted;
				if (!extractPackedMap(m_currentBig, archiveMapPath, extracted))
				{
					::AfxMessageBox("Failed to extract the selected map from the archive.");
				}
				else
				{
					m_pInfo->filename = extracted;
				}
			}
		}
	}
	else
	{
		if (!rowValid)
		{
			m_pInfo->browse = true;
		}
		else
		{
			CString newName = s_qtView[row];
			if (m_usingSystemDir)
			{
				m_pInfo->filename = ".\\Maps\\" + newName + "\\" + newName + ".map";
			}
			else
			{
				m_pInfo->filename = TheGlobalData->getPath_UserData().str();
				m_pInfo->filename = m_pInfo->filename + "Maps\\" + newName + "\\" + newName + ".map";
			}
		}
	}
	return (s_qtOpenInfo.browse || !s_qtOpenInfo.filename.IsEmpty()) ? 1 : 0;
}

int OpenMap::qtBrowsePick(void)
{
	// == OnBrowse minus the window: packed mode pops the .big chooser (owner = the active
	// window) then relists -- not a completion; normal mode completes with the browse
	// fallback.
	s_qtOpenInfo.filename = "";
	s_qtOpenInfo.browse = false;
	if (m_packedMode != PM_OFF)
	{
		CFileDialog dlg(TRUE, "big", NULL,
			OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
			"BIG archives (*.big)|*.big|All files (*.*)|*.*||", this);
		dlg.m_ofn.lpstrTitle = "Select a .big archive";
		if (dlg.DoModal() == IDOK)
		{
			CString bigPath = dlg.GetPathName();
			qtMPopulateMapsInBig(bigPath);
		}
	}
	else
	{
		m_pInfo->browse = true;
	}
	return (s_qtOpenInfo.browse || !s_qtOpenInfo.filename.IsEmpty()) ? 1 : 0;
}

extern "C" void WBQtOpenMapData_Open(void)
{
	OpenMap::qtOpen();
}

extern "C" void WBQtOpenMapData_Close(void)
{
	OpenMap::qtClose();
}

extern "C" int WBQtOpenMapData_ListCount(void)
{
	OpenMap *dlg = OpenMap::qtInstance();
	return (dlg != NULL) ? dlg->qtListCount() : 0;
}

extern "C" void WBQtOpenMapData_ItemPreviewPath(int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	OpenMap *dlg = OpenMap::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtItemPreviewPath(i, buf, cap);
	}
}

extern "C" int WBQtOpenMapData_ItemPreviewData(int i, unsigned char *buf, int cap)
{
	OpenMap *dlg = OpenMap::qtInstance();
	return (dlg != NULL) ? dlg->qtItemPreviewData(i, buf, cap) : 0;
}

extern "C" void WBQtOpenMapData_ListItem(int i, char *buf, int cap)
{
	OpenMap *dlg = OpenMap::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtListItem(i, buf, cap);
	}
}

extern "C" int WBQtOpenMapData_ListCurSel(void)
{
	OpenMap *dlg = OpenMap::qtInstance();
	return (dlg != NULL) ? dlg->qtListCurSel() : -1;
}

extern "C" int WBQtOpenMapData_OkEnabled(void)
{
	OpenMap *dlg = OpenMap::qtInstance();
	return (dlg != NULL) ? dlg->qtOkEnabled() : 0;
}

extern "C" int WBQtOpenMapData_GetMode(void)
{
	OpenMap *dlg = OpenMap::qtInstance();
	return (dlg != NULL) ? dlg->qtGetMode() : WB_QT_MAPMODE_USER;
}

extern "C" void WBQtOpenMap_SetMode(int mode)
{
	OpenMap *dlg = OpenMap::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtSetMode(mode);
	}
}

extern "C" void WBQtOpenMap_Search(const char *text)
{
	OpenMap *dlg = OpenMap::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtSearch(text);
	}
}

extern "C" void WBQtOpenMap_ResetSearch(void)
{
	OpenMap *dlg = OpenMap::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtResetSearch();
	}
}

extern "C" int WBQtOpenMap_Pick(int row)
{
	OpenMap *dlg = OpenMap::qtInstance();
	return (dlg != NULL) ? dlg->qtPick(row) : 0;
}

extern "C" int WBQtOpenMap_BrowsePick(void)
{
	OpenMap *dlg = OpenMap::qtInstance();
	return (dlg != NULL) ? dlg->qtBrowsePick() : 0;
}

extern "C" void WBQtOpenMapData_GetResult(char *filenameOut, int cap, int *browseOut)
{
	copyOut((LPCTSTR)s_qtOpenInfo.filename, filenameOut, cap);
	if (browseOut != NULL)
	{
		*browseOut = s_qtOpenInfo.browse ? 1 : 0;
	}
}

// ================= Save Map (native; data + confirmation ports) =================

static CStringArray s_qtSaveMaps;

extern "C" int WBQtSaveMapData_GetUseSystemDir(void)
{
	// == the SaveMap ctor's profile read (FALSE outside debug/internal).
#if defined(_DEBUG) || defined(_INTERNAL)
	return ::AfxGetApp()->GetProfileInt(MAP_OPENSAVE_PANEL_SECTION, "UseSystemDir", TRUE) ? 1 : 0;
#else
	return 0;
#endif
}

extern "C" int WBQtSaveMapData_Enumerate(int systemMaps)
{
	// == SaveMap::populateMapListbox's folder walk (a map counts when
	// Maps\<name>\<name>.map exists), including the profile write.
#if defined(_DEBUG) || defined(_INTERNAL)
	::AfxGetApp()->WriteProfileInt(MAP_OPENSAVE_PANEL_SECTION, "UseSystemDir", systemMaps ? TRUE : FALSE);
#endif
	s_qtSaveMaps.RemoveAll();

	HANDLE hFindFile = 0;
	WIN32_FIND_DATA findData;
	char dirBuf[_MAX_PATH];
	char findBuf[_MAX_PATH];
	char fileBuf[_MAX_PATH];

	if (systemMaps)
		strcpy(dirBuf, ".\\Maps\\");
	else
		sprintf(dirBuf, "%sMaps\\", TheGlobalData->getPath_UserData().str());
	int len = strlen(dirBuf);
	if (len > 0 && dirBuf[len - 1] != '\\') {
		dirBuf[len++] = '\\';
		dirBuf[len] = 0;
	}
	strcpy(findBuf, dirBuf);
	strcat(findBuf, "*.*");

	hFindFile = FindFirstFile(findBuf, &findData);
	if (hFindFile != INVALID_HANDLE_VALUE) {
		do {
			if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
				continue;
			if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				continue;
			}
			strcpy(fileBuf, dirBuf);
			strcat(fileBuf, findData.cFileName);
			strcat(fileBuf, "\\");
			strcat(fileBuf, findData.cFileName);
			strcat(fileBuf, ".map");
			try {
				CFileStatus status;
				if (CFile::GetStatus(fileBuf, status)) {
					if (!(status.m_attribute & CFile::directory)) {
						s_qtSaveMaps.Add(findData.cFileName);
					}
				}
			} catch(...) {}
		} while (FindNextFile(hFindFile, &findData));
		if (hFindFile) FindClose(hFindFile);
	}
	return (int)s_qtSaveMaps.GetSize();
}

extern "C" void WBQtSaveMapData_GetMapName(int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	if (i >= 0 && i < s_qtSaveMaps.GetSize())
	{
		copyOut((LPCTSTR)s_qtSaveMaps[i], buf, cap);
	}
}

extern "C" int WBQtSaveMap_ConfirmOverwrite(const char *filename, int systemMaps)
{
	// == SaveMap::OnOK's existence check + IDS_REPLACEFILE yes/no prompt.
	CString testName;
	if (systemMaps)
		testName = ".\\Maps\\";
	else
	{
		testName = TheGlobalData->getPath_UserData().str();
		testName = testName + "Maps\\";
	}
	CString name(filename ? filename : "");
	testName += name + "\\" + name + ".map";
	CFileStatus status;
	if (CFile::GetStatus(testName, status)) {
		CString warn;
		warn.Format(IDS_REPLACEFILE, LPCTSTR(testName));
		Int ret = ::AfxMessageBox(warn, MB_YESNO);
		if (ret == IDNO) {
			return 0;
		}
	}
	return 1;
}

#endif // RTS_HAS_QT
