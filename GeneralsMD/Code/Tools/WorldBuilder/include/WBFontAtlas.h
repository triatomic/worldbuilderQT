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

#ifndef __WB_FONT_ATLAS_H_
#define __WB_FONT_ATLAS_H_

#include "Lib/BaseType.h"

struct IDirect3DDevice8;
struct IDirect3DTexture8;

// ----------------------------------------------------------------------------
// WBFontAtlas
//
// Builds a glyph atlas from a GDI font and renders text as D3D textured quads
// (drawn into the back buffer before End_Render, so the text is part of the
// presented frame -- no flicker, unlike GDI TextOut drawn on top of the D3D
// surface).
//
// Per-glyph ABC widths are derived robustly by *rendering each glyph and
// scanning the pixels* for its ink bounding box, rather than trusting
// GetCharABCWidths (which is unreliable with ClearType antialiasing, bitmap
// fonts, and font substitution). From the bounding box: A = left offset,
// B = box width; GetTextExtentPoint32 gives the advance (A+B+C), so C is
// derived. This is the approach that works across ClearType, bitmap fonts,
// font substitution, and odd glyphs like backslash.
// ----------------------------------------------------------------------------
class WBFontAtlas
{
public:
	WBFontAtlas();
	~WBFontAtlas();

	/// (Re)build the atlas for the given typeface/size. antialias selects
	/// ClearType-style smoothing. Releases any previous atlas. Safe to call
	/// at runtime to apply a font/AA toggle.
	Bool build(const char *faceName, Int heightPx, Bool bold, Bool antialias);

	/// Release the GPU texture (e.g. on device loss). The CPU atlas + metrics
	/// are kept, so reupload() can restore it without re-measuring glyphs.
	void releaseTexture();

	Bool isValid() const { return m_atlasBits != NULL; }

	/// Line height (font cell height) in pixels, for stacking label rows.
	Int  getLineHeight() const { return m_lineHeight; }

	/// Pixel width the string would occupy if drawn (sum of advances).
	Int  measureWidth(const char *str, Int len) const;

	// --- batched drawing (call between begin()/end()) ---

	/// Begin a batch. dev is the current device; viewW/viewH are the client
	/// pixel dimensions used to map pixel coords to the screen-space quad.
	void begin(IDirect3DDevice8 *dev, Int viewW, Int viewH);

	/// Queue a string at pixel (x,y) (top-left of the text cell) in color argb
	/// (0xAARRGGBB). If shadow is true, a 1px-offset black copy is queued first.
	void drawText(Int x, Int y, const char *str, Int len, UnsignedInt argb, Bool shadow);

	/// Flush all queued glyphs as textured quads into the current frame, then
	/// restore render state. Must be paired with begin().
	void end();

private:
	struct Glyph
	{
		Real u0, v0, u1, v1;	// atlas UVs of the ink box
		Int  a;					// left side bearing (advance before ink)
		Int  b;					// ink box width
		Int  c;					// right side bearing (advance after ink)
		Int  inkW, inkH;		// ink box size in pixels
		Int  inkTop;			// ink box top, relative to the font cell top
	};

	void ensureTexture();		///< (re)create + upload the GPU texture from m_atlasBits.
	void freeAll();

	enum { FIRST_CHAR = 32, LAST_CHAR = 126, NUM_GLYPHS = LAST_CHAR - FIRST_CHAR + 1 };

	Glyph			m_glyphs[NUM_GLYPHS];
	UnsignedInt    *m_atlasBits;	///< CPU copy of the atlas (A8R8G8B8), kept for reupload.
	Int				m_atlasW, m_atlasH;
	Int				m_lineHeight;

	IDirect3DDevice8  *m_dev;		///< valid only between begin()/end().
	IDirect3DTexture8 *m_texture;
	Bool			m_texDirty;		///< atlas changed; needs (re)upload.
	Bool			m_inBatch;		///< true between begin()/end(); drawText is a no-op otherwise.

	Int				m_viewW, m_viewH;

	// vertex batch
	struct TLVertex { Real x, y, z, rhw; UnsignedInt color; Real u, v; };
	TLVertex       *m_verts;
	Int				m_vertCount;
	Int				m_vertCap;

	void pushGlyphQuad(Int penX, Int penY, const Glyph &g, UnsignedInt argb);
	void flushBatch();
};

#endif // __WB_FONT_ATLAS_H_
