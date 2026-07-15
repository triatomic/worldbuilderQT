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

// WorldBuilderDoc.h : interface of the CWorldBuilderDoc class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_WORLDBUILDERDOC_H__FBA4134D_2826_11D5_8CE0_00010297BBAC__INCLUDED_)
#define AFX_WORLDBUILDERDOC_H__FBA4134D_2826_11D5_8CE0_00010297BBAC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Lib/BaseType.h"
#include "Common/MapObject.h"
#include "Tool.h"

class CWorldBuilderView;
class WbView3d;
class WorldHeightMapEdit;
class Undoable;
class DataChunkInput;
struct DataChunkInfo;

#define MAX_UNDOS 64

#define MIN_CELL_SIZE 1
#define MAX_CELL_SIZE 64

class CWorldBuilderDoc : public CDocument
{
	friend class COptionsPanel;

	enum {MAX_WAYPOINTS=16000}; ///@todo - make it dynamic.  jba.
protected: // create from serialization only
	CWorldBuilderDoc();
	DECLARE_DYNCREATE(CWorldBuilderDoc)

protected:
	WorldHeightMapEdit	*m_heightMap;
	Undoable						*m_undoList;  ///< Head of undo/redo list.
	int									m_maxUndos;
	int									m_curRedo;		///< 0 means no redos available.
	Bool								m_linkCenters;				///< Flag whether the centers of the 2d and 3d views track together.
 	Bool								m_needAutosave;			///< True if changes have been made since last autosave.
	Int									m_curWaypointID;
	Bool							    m_disableMapPrevGeneration;
	Bool								m_watchMapIni;			///< auto-reload map.ini when the file changes
	FILETIME							m_mapIniLastWrite;		///< last-seen map.ini mtime for the watch

protected:
	std::vector<ICoord2D> m_boundaries;

protected:	// waypoint stuff.
	MapObject		*m_waypointTable[MAX_WAYPOINTS];							
	Bool				m_waypointTableNeedsUpdate;
	struct {
		Int waypoint1;
		Int waypoint2;
		Bool processedFlag;
	} m_waypointLinks[MAX_WAYPOINTS];
	Int									m_numWaypointLinks;
protected:
	void updateWaypointTable(void);
	void compressWaypointIds(void);
	void updateLWL(MapObject *pWay, MapObject *pSrcWay);
public:
	void addWaypointLink(Int waypointID1, Int waypointID2);
	void removeWaypointLink(Int waypointID1, Int waypointID2);
	MapObject *getWaypointByID(Int waypointID);
	Int getNumWaypointLinks(void) {return m_numWaypointLinks;};
	void getWaypointLink(Int ndx, Int *waypoint1, Int *waypointID2);
	Bool waypointLinkExists(Int waypointID1, Int waypointID2);
	Bool isWaypointLinked(MapObject *pWay);
	void updateLinkedWaypointLabels(MapObject *pWay);
	
	// Boundary stuff
	Int getNumBoundaries(void) const ;
	void getBoundary(Int ndx, ICoord2D* border) const;
	void addBoundary(ICoord2D* boundaryToAdd);
	void changeBoundary(Int ndx, ICoord2D *border);
	void removeLastBoundary(void);
	void removeAllExtraBoundaries();

	// const std::vector<ICoord2D>& getBoundaries() const { return m_boundaries; }
	// void clearBoundaries() { m_boundaries.clear(); }

	// outNdx must not be NULL, but outHandle can be.
	// outHandle: 0 means BL, 1 means TL, 2 means TR, 3 means BR
	void findBoundaryNear(Coord3D *pt, float okDistance, Int *outNdx, Int *outHandle);

	static Bool ParseWaypointDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	Bool ParseWaypointData(DataChunkInput &file, DataChunkInfo *info, void *userData);

public: // overridden
	virtual BOOL DoSave(LPCTSTR lpszPathName, BOOL bReplace = TRUE);
	virtual BOOL DoFileSave();

// Attributes
public:
	void OptimizeTiles();
	void RefreshAndOptimizeHeightMap();

	WorldHeightMapEdit *GetHeightMap() {return m_heightMap;}
	void SetHeightMap(WorldHeightMapEdit *pMap, Bool doUpdate);

	void Create2DView();
	void Create3DView();

	CWorldBuilderView *Get2DView();
	WbView3d *Get3DView();

	static CWorldBuilderDoc *GetActiveDoc();
	static CWorldBuilderView *GetActive2DView();
	static WbView3d *GetActive3DView();

	CString getMapPath() const { return m_strPathName; }
	void LoadEditTime(const CString& mapPath);

	void invalObject(MapObject *pMapObj);
	void invalCell(int xIndex, int yIndex);

	void updateAllViews();
	void updateHeightMap(WorldHeightMap *htMap, Bool partial, const IRegion2D &partialRange);

	/// Gets an xy index into the height map from a pixel location.
	Bool getCellIndexFromCoord(Coord3D pt, CPoint *ndxP);

	void getCoordFromCellIndex(CPoint ndx, Coord3D* pt);

	/// Gets a real xy location from a pixel point.
	Bool getCellPositionFromCoord(Coord3D pt,  Coord3D *locP);
	
	/// Gets all of the indices within widthOutside of the rectangle and places them into
	/// allIndices
	Bool getAllIndexesInRect(const Coord3D* bl, const Coord3D* br, 
													 const Coord3D* tl, const Coord3D* tr,
													 Int widthOutside, VecHeightMapIndexes* allIndices);

	/// Gets the arrow point location.
	void getObjArrowPoint(MapObject *pObj, Coord3D *location);

	void syncViewCenters(Real x, Real y);

	Bool needAutoSave(void) {return m_needAutosave;};

	Int getNextWaypointID(void) { return ++m_curWaypointID;};

	void setNextWaypointID(Int newMax) { if (newMax>m_curWaypointID) m_curWaypointID = newMax;};

	void autoSave(void);
	void validate(void);
// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWorldBuilderDoc)
	public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
	virtual BOOL CanCloseFrame(CFrameWnd* pFrame);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CWorldBuilderDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
	void AddAndDoUndoable(Undoable *pUndo);
	// Undo history depth: persisted setting ([MainFrame] MaxUndos, Entity Finder UI);
	// clamped 1..999 -- terrain undos hold heightmap snapshots, so keep it sane.
	void setMaxUndos(Int count);
// Generated message map functions
protected:
	//{{AFX_MSG(CWorldBuilderDoc)
	afx_msg void OnEditRedo();
	afx_msg void OnUpdateEditRedo(CCmdUI* pCmdUI);
	afx_msg void OnEditUndo();
	afx_msg void OnUpdateEditUndo(CCmdUI* pCmdUI);
	afx_msg void OnTsInfo();
	afx_msg void OnTsCanonical();
	afx_msg void OnUpdateTsCanonical(CCmdUI* pCmdUI);
	afx_msg void OnFileResize();
#ifdef RTS_HAS_QT
	afx_msg void OnFileClose();
#endif
	afx_msg void OnGenerateMapStrAndIni();

	// Map.ini loader commands (File > Map.ini submenu).
	afx_msg void OnReloadMapIni();
	afx_msg void OnCheckMapIni();
	afx_msg void OnToggleWatchMapIni();
	afx_msg void OnUpdateWatchMapIni(CCmdUI* pCmdUI);
	afx_msg void OnToggleVerboseMapIni();
	afx_msg void OnUpdateVerboseMapIni(CCmdUI* pCmdUI);
public:
	// Called from CMainFrame's timer while auto-reload is on: reload if map.ini changed.
	void pollMapIniWatch();
protected:

	afx_msg void OnJumpToMapFolderWBData();
	afx_msg void OnJumpToMapFolder();
	afx_msg void OnJumpToAutoSaveFolder();
	afx_msg void OnOpenWorldbuilderSettings();

	void OpenGameFolder(Bool data);
	afx_msg void OnOpenGameFolder();
	afx_msg void OnOpenDataFolder();

	void OnJumpToGame(Bool withDebug, Bool waveEdit);
	afx_msg void OnJumpToGameWithoutDebug();
	afx_msg void OnJumpToGameWithDebug();
	afx_msg void OnJumpToGameWithWaveEdit();

	afx_msg void OnViewDisableMapPrevGen();
	afx_msg void OnUpdateDisableMapPrevGen(CCmdUI* pCmdUI);

	afx_msg void OnTsRemap();
	afx_msg void OnEditLinkCenters();
	afx_msg void OnUpdateEditLinkCenters(CCmdUI* pCmdUI);
	afx_msg void OnViewTimeOfDay();
	afx_msg void OnWindow2dwindow();
	afx_msg void OnUpdateWindow2dwindow(CCmdUI* pCmdUI);
	afx_msg void OnViewReloadtextures();
	afx_msg void OnEditScripts();
	afx_msg void OnViewHome();
	afx_msg void OnTexturesizingTile4x4();
	afx_msg void OnUpdateTexturesizingTile4x4(CCmdUI* pCmdUI);
	afx_msg void OnTexturesizingTile6x6();
	afx_msg void OnUpdateTexturesizingTile6x6(CCmdUI* pCmdUI);
	afx_msg void OnTexturesizingTile8x8();
	afx_msg void OnUpdateTexturesizingTile8x8(CCmdUI* pCmdUI);
	afx_msg void OnDumpDocToText();
	afx_msg void OnRemoveclifftexmapping();
	afx_msg void OnTogglePitchAndRotation();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WORLDBUILDERDOC_H__FBA4134D_2826_11D5_8CE0_00010297BBAC__INCLUDED_)
