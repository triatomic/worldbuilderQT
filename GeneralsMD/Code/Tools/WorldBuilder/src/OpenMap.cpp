/*
**	Command & Conquer Generals Zero Hour(tm)
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

// OpenMap.cpp : implementation file
//

#include "StdAfx.h"
#include "WorldBuilder.h"
#include "OpenMap.h"
#include "Common/GlobalData.h"
#include "Common/ArchiveFile.h"
#include "Common/ArchiveFileSystem.h"
#include "Common/File.h"
#include "Common/FileSystem.h"

/////////////////////////////////////////////////////////////////////////////
// OpenMap dialog


OpenMap::OpenMap(TOpenMapInfo *pInfo, CWnd* pParent /*=NULL*/)
	: CDialog(OpenMap::IDD, pParent),
	m_pInfo(pInfo)
{
	m_pInfo->browse = false;
	m_packedMode = PM_OFF;
#if defined(_DEBUG) || defined(_INTERNAL)
	m_usingSystemDir = ::AfxGetApp()->GetProfileInt(MAP_OPENSAVE_PANEL_SECTION, "UseSystemDir", TRUE);
#else
	m_usingSystemDir = FALSE;
#endif

	//{{AFX_DATA_INIT(OpenMap)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void OpenMap::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(OpenMap)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(OpenMap, CDialog)
	//{{AFX_MSG_MAP(OpenMap)
	ON_BN_CLICKED(IDC_BROWSE, OnBrowse)
	ON_BN_CLICKED(IDC_SYSTEMMAPS, OnSystemMaps)
	ON_BN_CLICKED(IDC_USERMAPS, OnUserMaps)
	ON_BN_CLICKED(IDC_PACKED_MAPS, OnPackedMaps)
	ON_LBN_DBLCLK(IDC_OPEN_LIST, OnDblclkOpenList)

    ON_BN_CLICKED(IDC_MAP_FIND_BUTTON, OnSearchMap)
    ON_BN_CLICKED(IDC_MAP_SEARCH_RESET_BTN, OnResetSearch)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// OpenMap message handlers

void OpenMap::OnSystemMaps()
{
	m_packedMode = PM_OFF;
	populateMapListbox( TRUE );
}

void OpenMap::OnUserMaps()
{
	m_packedMode = PM_OFF;
	populateMapListbox( FALSE );
}

void OpenMap::OnPackedMaps()
{
	// Toggle into packed mode and list the .big archives in the current directory.
	populatePackedBigList();
}

void OpenMap::OnBrowse()
{
	// In Packed mode, Browse picks a .big archive from anywhere (e.g. outside the
	// game directory) and lists the maps inside it. In normal (System/User) mode,
	// Browse falls back to the standard .map file browser.
	if (m_packedMode != PM_OFF) {
		CFileDialog dlg(TRUE, "big", NULL,
			OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
			"BIG archives (*.big)|*.big|All files (*.*)|*.*||", this);
		dlg.m_ofn.lpstrTitle = "Select a .big archive";
		if (dlg.DoModal() != IDOK)
			return;	// cancelled; stay in the dialog

		CString bigPath = dlg.GetPathName();	// full path to the chosen .big
		populatePackedMapList(bigPath);			// list its maps; user then picks one
		return;
	}

	m_pInfo->browse = true;
	OnOK();
}

void OpenMap::OnOK() 
{

    // If Enter is pressed inside the search box -> perform search instead of opening a map
    if (GetFocus() == GetDlgItem(IDC_MAP_SEARCH_EDIT))
    {
        OnSearchMap();
        return; // prevent dialog from closing
    }

    // === original behavior ===
	CListBox *pList = (CListBox *)this->GetDlgItem(IDC_OPEN_LIST);
	if (pList == NULL) {
		OnCancel();
		return;
	}

	Int sel = pList->GetCurSel();

	// --- Packed mode: browsing .big archives -------------------------------
	if (m_packedMode == PM_LIST_BIGS) {
		if (sel == LB_ERR) return;	// nothing selected; keep dialog open
		CString bigName;
		pList->GetText(sel, bigName);
		populatePackedMapList(bigName);	// descend into the archive (lists maps)
		return;							// stay open so the user can pick a map
	}

	if (m_packedMode == PM_LIST_MAPS_IN_BIG) {
		if (sel == LB_ERR) return;

		// The listbox is LBS_SORT (and the search filter rebuilds it), so the
		// selection index does NOT line up with m_packedMapPaths. Resolve the
		// archive path by matching the selected display name against m_fullMapList
		// (names and paths are parallel, both in archive-enumeration order).
		CString selName;
		pList->GetText(sel, selName);

		CString archiveMapPath;
		for (int i = 0; i < m_fullMapList.GetSize(); ++i) {
			if (m_fullMapList[i].CompareNoCase(selName) == 0) {
				archiveMapPath = m_packedMapPaths[i];
				break;
			}
		}
		if (archiveMapPath.IsEmpty()) {
			::AfxMessageBox("Could not resolve the selected packed map.");
			return;
		}

		CString extracted;
		if (!extractPackedMap(m_currentBig, archiveMapPath, extracted)) {
			::AfxMessageBox("Failed to extract the selected map from the archive.");
			return;
		}
		m_pInfo->filename = extracted;
		CDialog::OnOK();
		return;
	}

	// --- Normal (unpacked) maps --------------------------------------------
	if (sel == LB_ERR) {
		m_pInfo->browse = true;
	} else {
		CString newName;
		pList->GetText(sel, newName );
		if (m_usingSystemDir)
			m_pInfo->filename = ".\\Maps\\" + newName + "\\" + newName + ".map";
		else
		{
			m_pInfo->filename = TheGlobalData->getPath_UserData().str();
			m_pInfo->filename = m_pInfo->filename + "Maps\\" + newName + "\\" + newName + ".map";
		}
	}
	CDialog::OnOK();
}

void OpenMap::populateMapListbox( Bool systemMaps )
{
	m_usingSystemDir = systemMaps;
#if defined(_DEBUG) || defined(_INTERNAL)
	::AfxGetApp()->WriteProfileInt(MAP_OPENSAVE_PANEL_SECTION, "UseSystemDir", m_usingSystemDir);
#endif

	HANDLE			hFindFile = 0;
	WIN32_FIND_DATA			findData;
	char				dirBuf[_MAX_PATH];
	char				findBuf[_MAX_PATH];
	char				fileBuf[_MAX_PATH];

	if (systemMaps)
		strcpy(dirBuf, "Maps\\");
	else
	{
		strcpy(dirBuf, TheGlobalData->getPath_UserData().str());
		strcat(dirBuf, "Maps\\");
	}

	int len = strlen(dirBuf);

	if (len > 0 && dirBuf[len - 1] != '\\') {
		dirBuf[len++] = '\\';
		dirBuf[len] = 0;
	}
	CListBox *pList = (CListBox *)this->GetDlgItem(IDC_OPEN_LIST);
	if (pList == NULL) return;
	pList->ResetContent();
	strcpy(findBuf, dirBuf);
	strcat(findBuf, "*.*");

    m_fullMapList.RemoveAll(); // before filling list
	Bool found = false;

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
						pList->AddString(findData.cFileName);
						m_fullMapList.Add(findData.cFileName);
						found = true;
					};
				}
			} catch(...) {}

		} while (FindNextFile(hFindFile, &findData));

		if (hFindFile) FindClose(hFindFile);
 	}
	if (found) {
		pList->SetCurSel(0);
	} else {
		CWnd *pOk = GetDlgItem(IDOK);
		if (pOk) pOk->EnableWindow(false);
	}
}

BOOL OpenMap::OnInitDialog() 
{
	CDialog::OnInitDialog();

	CButton *pSystemMaps = (CButton *)this->GetDlgItem(IDC_SYSTEMMAPS);
	if (pSystemMaps != NULL)
		pSystemMaps->SetCheck( m_usingSystemDir );

	CButton *pUserMaps = (CButton *)this->GetDlgItem(IDC_USERMAPS);
	if (pUserMaps != NULL)
		pUserMaps->SetCheck( !m_usingSystemDir );

	// Packed Maps starts unselected (we open in System/User mode).
	CButton *pPacked = (CButton *)this->GetDlgItem(IDC_PACKED_MAPS);
	if (pPacked != NULL)
		pPacked->SetCheck( 0 );

#if !defined(_DEBUG) && !defined(_INTERNAL)
	if (pSystemMaps)
		pSystemMaps->ShowWindow( FALSE );
	if (pUserMaps)
		pUserMaps->ShowWindow( FALSE );
#endif

	populateMapListbox( m_usingSystemDir );

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void OpenMap::OnDblclkOpenList() 
{
	OnOK();
}

void OpenMap::OnSearchMap()
{
    CString search;
    GetDlgItemText(IDC_MAP_SEARCH_EDIT, search);
    search.MakeLower();

    CListBox* pList = (CListBox*)GetDlgItem(IDC_OPEN_LIST);
    if (!pList) return;

    pList->ResetContent();

    if (search.IsEmpty()) {
        for (int i=0;i<m_fullMapList.GetSize();i++)
            pList->AddString(m_fullMapList[i]);
        pList->SetCurSel(0);
        return;
    }

    bool found = false;

    for (int i=0;i<m_fullMapList.GetSize();i++)
    {
        CString name = m_fullMapList[i];
        CString lower = name; lower.MakeLower();

        if (lower.Find(search) != -1) {
            pList->AddString(name);
            found = true;
        }
    }

    if (!found) {
        ::MessageBeep(MB_ICONEXCLAMATION); // no matches
    } else {
        pList->SetCurSel(0);
    }
}

void OpenMap::OnResetSearch()
{
    SetDlgItemText(IDC_MAP_SEARCH_EDIT, "");
    CListBox* pList = (CListBox*)GetDlgItem(IDC_OPEN_LIST);
    if (!pList) return;

    pList->ResetContent();

    for (int i=0;i<m_fullMapList.GetSize();i++)
        pList->AddString(m_fullMapList[i]);

    if (m_fullMapList.GetSize()>0)
        pList->SetCurSel(0);
}

/////////////////////////////////////////////////////////////////////////////
// Packed Maps (.big archive) support

// Scan the current working directory for *.big archives and list them. Selecting
// one (OK / double-click) descends into it and lists the maps it contains.
void OpenMap::populatePackedBigList()
{
	m_packedMode = PM_LIST_BIGS;
	m_currentBig.Empty();

	CListBox *pList = (CListBox *)GetDlgItem(IDC_OPEN_LIST);
	if (!pList) return;
	pList->ResetContent();
	m_fullMapList.RemoveAll();
	m_packedMapPaths.RemoveAll();

	SetDlgItemText(IDC_MAP_SEARCH_EDIT, "");

	HANDLE hFind;
	WIN32_FIND_DATA fd;
	Bool found = false;
	hFind = FindFirstFile("*.big", &fd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;
			pList->AddString(fd.cFileName);
			m_fullMapList.Add(fd.cFileName);
			found = true;
		} while (FindNextFile(hFind, &fd));
		FindClose(hFind);
	}

	CWnd *pOk = GetDlgItem(IDOK);
	if (found) {
		pList->SetCurSel(0);
		if (pOk) pOk->EnableWindow(TRUE);
	} else {
		::AfxMessageBox("No .big archives found in the WorldBuilder directory.");
		if (pOk) pOk->EnableWindow(FALSE);
	}
}

// Open a .big archive and list the maps inside it. The listbox shows the map name;
// m_packedMapPaths holds the parallel in-archive .map path used for extraction.
void OpenMap::populatePackedMapList( const CString &bigPath )
{
	ArchiveFile *pArchive = TheArchiveFileSystem
		? TheArchiveFileSystem->openArchiveFile((const char *)bigPath) : NULL;
	if (!pArchive) {
		::AfxMessageBox("Could not open the selected .big archive.");
		return;
	}

	// Recursively list every .map inside the archive (full paths, e.g.
	// "Maps\MapName\MapName.map").
	FilenameList maps;
	pArchive->getFileListInDirectory(AsciiString(""), AsciiString(""), AsciiString("*.map"), maps, TRUE);

	CListBox *pList = (CListBox *)GetDlgItem(IDC_OPEN_LIST);
	if (!pList) { pArchive->close(); delete pArchive; return; }
	pList->ResetContent();
	m_fullMapList.RemoveAll();
	m_packedMapPaths.RemoveAll();

	Bool found = false;
	for (FilenameListIter it = maps.begin(); it != maps.end(); ++it) {
		AsciiString fullPath = *it;

		// Display the bare map name (file title without extension).
		AsciiString display = fullPath;
		const char *slash = strrchr(display.str(), '\\');
		if (!slash) slash = strrchr(display.str(), '/');
		CString name = slash ? (slash + 1) : display.str();
		int dot = name.ReverseFind('.');
		if (dot > 0) name = name.Left(dot);

		pList->AddString(name);
		m_fullMapList.Add(name);
		m_packedMapPaths.Add(CString(fullPath.str()));
		found = true;
	}

	pArchive->close();
	delete pArchive;

	m_packedMode = PM_LIST_MAPS_IN_BIG;
	m_currentBig = bigPath;

	CWnd *pOk = GetDlgItem(IDOK);
	if (found) {
		pList->SetCurSel(0);
		if (pOk) pOk->EnableWindow(TRUE);
	} else {
		::AfxMessageBox("No maps found inside this .big archive.");
		if (pOk) pOk->EnableWindow(FALSE);
	}
}

// Extract a map's entire folder (the .map plus sibling files: map.str, map.ini, and
// the <MapName>.tga preview) from the archive into <UserData>\UnpackedMaps\<MapName>\,
// and return the path of the extracted .map file in outMapPath. Returns FALSE on failure.
Bool OpenMap::extractPackedMap( const CString &bigPath, const CString &archiveMapPath, CString &outMapPath )
{
	ArchiveFile *pArchive = TheArchiveFileSystem
		? TheArchiveFileSystem->openArchiveFile((const char *)bigPath) : NULL;
	if (!pArchive)
		return FALSE;

	// Archive folder of this map, e.g. "Maps\MapName" (drop the trailing "\MapName.map").
	CString mapDirC = archiveMapPath;
	{
		int s = mapDirC.ReverseFind('\\');
		if (s < 0) s = mapDirC.ReverseFind('/');
		mapDirC = (s >= 0) ? mapDirC.Left(s) : CString("");
	}
	AsciiString mapArchivePath = (const char *)archiveMapPath;
	AsciiString mapDir = (const char *)mapDirC;

	// Map name (folder leaf), used for the temp output dir.
	CString leaf = archiveMapPath;
	{
		int s = leaf.ReverseFind('\\');
		if (s < 0) s = leaf.ReverseFind('/');
		if (s >= 0) leaf = leaf.Mid(s + 1);
		int d = leaf.ReverseFind('.');
		if (d > 0) leaf = leaf.Left(d);
	}

	// Collect every file in that map folder (recursively) so map.str / map.ini /
	// the <MapName>.tga preview all come along, not just the .map.
	FilenameList files;
	pArchive->getFileListInDirectory(mapDir, mapDir, AsciiString("*"), files, TRUE);

	// Belt-and-suspenders: make sure the .map and the well-known sidecar files are in
	// the list even if the directory enumeration missed them (case/format quirks).
	files.insert(mapArchivePath);
	{
		const char *sidecars[] = { "map.ini", "map.str" };
		for (int k = 0; k < 2; ++k) {
			AsciiString p = mapDir;
			if (p.getLength() > 0 && !p.endsWith("\\")) p.concat('\\');
			p.concat(sidecars[k]);
			files.insert(p);
		}
		// Preview is <MapName>.tga next to the .map.
		AsciiString prev = mapDir;
		if (prev.getLength() > 0 && !prev.endsWith("\\")) prev.concat('\\');
		prev.concat(leaf); prev.concat(".tga");
		files.insert(prev);
	}

	// Extract into a dedicated UnpackedMaps folder:  <UserData>\UnpackedMaps\<MapName>\
	// Kept separate from the user's own Maps so archive extractions never overwrite
	// hand-authored maps; saves from WorldBuilder land here too.
	CString unpackedRoot = TheGlobalData->getPath_UserData().str();
	if (!unpackedRoot.IsEmpty() && unpackedRoot.Right(1) != "\\")
		unpackedRoot += "\\";
	unpackedRoot += "UnpackedMaps";

	CString outDir;
	outDir.Format("%s\\%s\\", (LPCTSTR)unpackedRoot, (LPCTSTR)leaf);

	// If this map was already extracted/edited, don't silently clobber it.
	CString existingMap;
	existingMap.Format("%s%s.map", (LPCTSTR)outDir, (LPCTSTR)leaf);
	CFileStatus st;
	if (CFile::GetStatus(existingMap, st)) {
		CString msg;
		msg.Format("An unpacked map named \"%s\" already exists.\n\nOverwrite it with the copy from the archive?",
			(LPCTSTR)leaf);
		if (::AfxMessageBox(msg, MB_YESNO | MB_ICONQUESTION) != IDYES) {
			// Keep the existing one; open it as-is.
			outMapPath = existingMap;
			pArchive->close();
			delete pArchive;
			return TRUE;
		}
	}

	// Create the directory chain (UnpackedMaps, then UnpackedMaps\<MapName>).
	::CreateDirectory(unpackedRoot, NULL);
	CString outDirNoSlash = outDir.Left(outDir.GetLength() - 1);
	::CreateDirectory(outDirNoSlash, NULL);

	Bool wroteMap = FALSE;
	for (FilenameListIter it = files.begin(); it != files.end(); ++it) {
		AsciiString srcPath = *it;	// full archive path

		// Output filename = leaf of srcPath.
		CString srcCStr = srcPath.str();
		int s = srcCStr.ReverseFind('\\');
		if (s < 0) s = srcCStr.ReverseFind('/');
		CString fileLeaf = (s >= 0) ? srcCStr.Mid(s + 1) : srcCStr;
		CString destPath = outDir + fileLeaf;

		// Read the file out of the archive.
		File *pf = pArchive->openFile(srcPath.str(), File::READ | File::BINARY);
		if (!pf)
			continue;
		Int sz = pf->size();
		if (sz < 0) { pf->close(); continue; }

		char *buf = new char[sz > 0 ? sz : 1];
		Int got = (sz > 0) ? pf->read(buf, sz) : 0;
		pf->close();

		FILE *out = ::fopen((LPCTSTR)destPath, "wb");
		if (out) {
			if (got > 0) ::fwrite(buf, 1, got, out);
			::fclose(out);
		}
		delete [] buf;

		// Is this the .map file?
		if (fileLeaf.GetLength() >= 4 &&
			fileLeaf.Right(4).CompareNoCase(".map") == 0) {
			outMapPath = destPath;
			wroteMap = TRUE;
		}
	}

	pArchive->close();
	delete pArchive;

	return wroteMap;
}

// optional: auto filter as user types
// void OpenMap::OnSearchEditChange()
// {
//     OnSearchMap();
// }