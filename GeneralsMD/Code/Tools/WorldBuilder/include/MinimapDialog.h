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

#pragma once

#ifndef __MINIMAP_DIALOG_H_
#define __MINIMAP_DIALOG_H_

#include "Lib/BaseType.h"

#define MINIMAP_SECTION "MinimapDialog"

// Configurable sampling resolution bounds. The pixel buffer is heap-allocated to
// m_resolution * m_resolution UnsignedInts, so large sizes don't bloat the object.
enum
{
	MINIMAP_RES_MIN     = 128,
	MINIMAP_RES_DEFAULT = 128,
	MINIMAP_RES_MAX     = 2048,
};

// The Minimap is a floating modeless tool window. Click/drag recenters the 3D
// viewport (in heightmap CELL units; setupCamera scales by MAP_XY_FACTOR).
class MinimapDialog : public CDialog
{
public:
	enum { IDD = IDD_MINIMAP };
	MinimapDialog(CWnd *pParent = NULL);
	virtual ~MinimapDialog();

	void rebuildTerrain();			///< Resample the terrain (expensive); then composite objects.
	void refreshObjects();			///< Cheap: re-composite cached terrain + objects (no resample).
	void requestRebuild(Bool terrainChanged = true);	///< Object/camera changes refresh instantly; terrain edits are throttled.

	// --- configuration (persisted to registry under MINIMAP_SECTION) ---
	void setShowObjects(Bool show);
	Bool getShowObjects() const { return m_showObjects; }

	void setRefreshDelayMs(Int ms);		///< 0 = manual (rebuild only on load/toggle).
	Int  getRefreshDelayMs() const { return m_refreshDelayMs; }

	void setResolution(Int res);		///< Clamped to [MINIMAP_RES_MIN, MINIMAP_RES_MAX].
	Int  getResolution() const { return m_resolution; }

protected:
	virtual BOOL OnInitDialog();
	virtual void OnCancel();
	virtual void OnOK();

	afx_msg void OnPaint();
	afx_msg void OnMove(int x, int y);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnTimer(UINT_PTR nIDEvent);

	DECLARE_MESSAGE_MAP()

private:
	void interpolateColorForHeight(RGBColor *color, Real height,
		Real hiZ, Real midZ, Real loZ);
	Bool minimapToWorld(Int mx, Int my, Real *worldX, Real *worldY);
	void centerViewAtClient(CPoint point);
	void allocBuffer();				///< (Re)allocate the buffers for the current resolution.
	void drawObjects();				///< Overlay map objects (units/structures) onto the buffer.
	void drawViewBoxOverlay(HDC hdc, Int clientW, Int clientH);	///< GDI camera-frustum box (display res).
	void fillRect(Int cx, Int cy, Int w, Int h, UnsignedInt color);	///< centered, clipped buffer fill.
	void fillDiamond(Int cx, Int cy, Int size, UnsignedInt color);	///< centered, clipped diamond fill (units).
	Bool worldToMinimap(Real worldX, Real worldY, Int *mx, Int *my);	///< world coords -> minimap cell.
	inline UnsignedInt &pixel(Int x, Int y) { return m_pixelBuffer[y * m_resolution + x]; }

	UnsignedInt *m_pixelBuffer;		///< composited (terrain + objects), shown via the DIB.
	UnsignedInt *m_terrainBuffer;	///< cached terrain-only resample; reused when only objects change.
	Bool m_terrainValid;			///< m_terrainBuffer holds a current resample.
	Int  m_resolution;				///< current sampling/buffer edge (square).
	Bool m_terrainBuilt;
	Bool m_dragging;
	Bool m_rebuildPending;			///< A terrain change is waiting to be resampled.

	Bool m_showObjects;				///< draw unit/structure dots over the terrain.
	Int  m_refreshDelayMs;			///< throttle delay; 0 = manual.
};

extern MinimapDialog *TheMinimapDialog;

#endif // __MINIMAP_DIALOG_H_
