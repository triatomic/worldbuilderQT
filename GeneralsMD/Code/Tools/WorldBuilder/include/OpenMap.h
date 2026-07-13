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

#if !defined(AFX_OPENMAP_H__D3FD3B43_B0B7_42F6_BB77_2380A8B9945B__INCLUDED_)
#define AFX_OPENMAP_H__D3FD3B43_B0B7_42F6_BB77_2380A8B9945B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// OpenMap.h : header file
//

#define MAP_OPENSAVE_PANEL_SECTION "MapOpenSavePanel"

typedef struct {
	CString filename;
	Bool		browse;
} TOpenMapInfo;

/////////////////////////////////////////////////////////////////////////////
// OpenMap dialog

class OpenMap : public CDialog
{
// Construction
public:
	OpenMap(TOpenMapInfo *pInfo, CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(OpenMap)
	enum { IDD = IDD_OPEN_MAP };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(OpenMap)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	TOpenMapInfo *m_pInfo;
	void populateMapListbox( Bool systemMaps );
	void OnSearchMap();
	void OnResetSearch();
	Bool m_usingSystemDir;
    CStringArray m_fullMapList;   // original unfiltered list

	// --- Packed Maps (.big archive) support ---
	enum PackedMode { PM_OFF = 0, PM_LIST_BIGS, PM_LIST_MAPS_IN_BIG };
	PackedMode   m_packedMode;
	CString      m_currentBig;            ///< path of the .big currently being browsed (PM_LIST_MAPS_IN_BIG)
	CStringArray m_packedMapPaths;        ///< in-archive .map paths, parallel to the listbox entries
	void populatePackedBigList();         ///< scan cwd for *.big, list them
	void populatePackedMapList( const CString &bigPath ); ///< open a .big, list the maps inside
	Bool extractPackedMap( const CString &bigPath, const CString &archiveMapPath, CString &outMapPath ); ///< extract a map folder to temp; outMapPath = extracted .map
#ifdef RTS_HAS_QT
// Qt seam (Tier 3d): the Qt Open Map dialog drives THIS dialog created hidden -- the
// system/user/packed-.big population, search filter and the OnOK resolution/extraction
// are reused verbatim. Definitions live in src/WBQtMapFileBridge.cpp.
public:
	static OpenMap *qtOpen(void);
	static void qtClose(void);
	static OpenMap *qtInstance(void);
	int  qtListCount(void);
	void qtListItem(int i, char *buf, int cap);
	// Preview thumbnail bytes (<name>.tga next to the .map) for row i -- read from disk
	// in the system/user modes, or straight out of the current .big in packed mode.
	// Returns the byte count, or 0 (no preview / buffer too small).
	int  qtItemPreviewData(int i, unsigned char *buf, int cap);
	// Shared bits of the pick/preview paths: the system-vs-user map file path policy,
	// and the packed display-name -> archive-internal .map path resolve.
	CString qtMapFilePath(const CString &name, const char *ext);
	CString qtResolveArchiveMapPath(const CString &selName);
	int  qtListCurSel(void);
	int  qtOkEnabled(void);
	int  qtGetMode(void);
	void qtSetMode(int mode);
	void qtSearch(const char *text);
	void qtResetSearch(void);
	int  qtPick(int row);
	int  qtBrowsePick(void);
	// De-bridged (windowless) fills -- branch qt-debridge. The dialog window is never
	// Create()d; the view model (rows/selection/ok state) lives in WBQtMapFileBridge.cpp
	// and these replicate the populate* handlers' enumeration minus the listbox.
	void qtMPopulateMain(Bool systemMaps);
	void qtMPopulateBigs(void);
	void qtMPopulateMapsInBig(const CString &bigPath);
#endif

protected:

	// Generated message map functions
	//{{AFX_MSG(OpenMap)
	afx_msg void OnBrowse();
	afx_msg void OnSystemMaps();
	afx_msg void OnUserMaps();
	afx_msg void OnPackedMaps();
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	afx_msg void OnDblclkOpenList();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPENMAP_H__D3FD3B43_B0B7_42F6_BB77_2380A8B9945B__INCLUDED_)
