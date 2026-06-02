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

#include "StdAfx.h"
#include "WBFontAtlas.h"

#include <d3d8.h>
#include "dx8wrapper.h"

// Atlas layout: glyphs are packed left-to-right into rows of fixed cell height.
// We keep a small inter-glyph margin so bilinear sampling never bleeds a
// neighbor's ink into a glyph.
static const Int ATLAS_MARGIN = 1;

WBFontAtlas::WBFontAtlas() :
	m_atlasBits(NULL),
	m_atlasW(0),
	m_atlasH(0),
	m_lineHeight(0),
	m_dev(NULL),
	m_texture(NULL),
	m_texDirty(false),
	m_inBatch(false),
	m_viewW(0),
	m_viewH(0),
	m_verts(NULL),
	m_vertCount(0),
	m_vertCap(0)
{
	::memset(m_glyphs, 0, sizeof(m_glyphs));
}

WBFontAtlas::~WBFontAtlas()
{
	freeAll();
}

void WBFontAtlas::freeAll()
{
	releaseTexture();
	if (m_atlasBits) { delete [] m_atlasBits; m_atlasBits = NULL; }
	if (m_verts)     { delete [] m_verts;     m_verts = NULL; m_vertCap = 0; }
	m_atlasW = m_atlasH = m_lineHeight = 0;
	::memset(m_glyphs, 0, sizeof(m_glyphs));
}

void WBFontAtlas::releaseTexture()
{
	if (m_texture) { m_texture->Release(); m_texture = NULL; }
	m_texDirty = true;
}

// ----------------------------------------------------------------------------
// Build the atlas: render each glyph into a scratch DIB, pixel-scan its ink box
// to derive ABC widths (robust under ClearType / bitmap fonts / substitution),
// then copy the ink into the shared atlas bitmap.
// ----------------------------------------------------------------------------
Bool WBFontAtlas::build(const char *faceName, Int heightPx, Bool bold, Bool antialias)
{
	freeAll();

	// --- create the GDI font ---
	LOGFONT lf;
	::memset(&lf, 0, sizeof(lf));
	lf.lfHeight         = heightPx;
	lf.lfWeight         = bold ? FW_BOLD : FW_NORMAL;
	lf.lfCharSet        = DEFAULT_CHARSET;
	lf.lfOutPrecision   = OUT_DEFAULT_PRECIS;
	lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
	lf.lfQuality        = antialias ? CLEARTYPE_QUALITY : NONANTIALIASED_QUALITY;
	lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	::lstrcpyn(lf.lfFaceName, faceName ? faceName : "Arial", LF_FACESIZE);

	HFONT font = ::CreateFontIndirect(&lf);
	if (!font)
		return FALSE;

	// --- scratch DIB to render a single glyph into ---
	// Worst-case size: a glyph never exceeds ~2x the cell in either axis.
	const Int scratchW = (heightPx * 2) + 8;
	const Int scratchH = (heightPx * 2) + 8;

	HDC screenDC = ::GetDC(NULL);
	HDC dc = ::CreateCompatibleDC(screenDC);
	::ReleaseDC(NULL, screenDC);

	BITMAPINFO bmi;
	::memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth       = scratchW;
	bmi.bmiHeader.biHeight      = -scratchH;	// top-down
	bmi.bmiHeader.biPlanes      = 1;
	bmi.bmiHeader.biBitCount    = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void *scratchBits = NULL;
	HBITMAP scratch = ::CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &scratchBits, NULL, 0);
	if (!scratch || !scratchBits) {
		if (scratch) ::DeleteObject(scratch);
		::DeleteDC(dc);
		::DeleteObject(font);
		return FALSE;
	}

	HGDIOBJ oldBmp  = ::SelectObject(dc, scratch);
	HGDIOBJ oldFont = ::SelectObject(dc, font);
	::SetBkMode(dc, OPAQUE);
	::SetBkColor(dc, RGB(0, 0, 0));
	::SetTextColor(dc, RGB(255, 255, 255));

	TEXTMETRIC tm;
	::GetTextMetrics(dc, &tm);
	m_lineHeight = tm.tmHeight;

	// We render each glyph at a fixed origin inside the scratch, leaving a left
	// pad so a negative A (overhang) still lands inside the bitmap.
	const Int originX = 4;
	const Int originY = 2;

	// First pass: render + measure each glyph, recording its ink box and ABC.
	struct Tmp { Int inkLeft, inkTop, inkW, inkH; UnsignedInt *ink; };
	Tmp *tmp = new Tmp[NUM_GLYPHS];
	::memset(tmp, 0, sizeof(Tmp) * NUM_GLYPHS);

	Int maxInkH = 1;
	for (Int ci = 0; ci < NUM_GLYPHS; ++ci) {
		char ch = (char)(FIRST_CHAR + ci);

		// Clear scratch to black.
		::memset(scratchBits, 0, (size_t)scratchW * scratchH * 4);

		::TextOut(dc, originX, originY, &ch, 1);
		::GdiFlush();

		// Advance width (A + B + C) from the text extent.
		SIZE ext;
		::GetTextExtentPoint32(dc, &ch, 1, &ext);
		Int advance = ext.cx;

		// Pixel-scan for the ink bounding box (any non-black pixel is ink; with
		// ClearType the edges are gray, so test luminance > 0).
		const UnsignedInt *px = (const UnsignedInt *)scratchBits;
		Int minX = scratchW, minY = scratchH, maxX = -1, maxY = -1;
		for (Int y = 0; y < scratchH; ++y) {
			const UnsignedInt *row = px + y * scratchW;
			for (Int x = 0; x < scratchW; ++x) {
				if ((row[x] & 0x00FFFFFF) != 0) {
					if (x < minX) minX = x;
					if (x > maxX) maxX = x;
					if (y < minY) minY = y;
					if (y > maxY) maxY = y;
				}
			}
		}

		Glyph &g = m_glyphs[ci];
		if (maxX < 0) {
			// Blank glyph (space): no ink. A=0, B=0, C=advance.
			g.a = 0; g.b = 0; g.c = advance;
			g.inkW = g.inkH = 0; g.inkTop = 0;
			tmp[ci].ink = NULL;
			continue;
		}

		Int inkW = maxX - minX + 1;
		Int inkH = maxY - minY + 1;

		// A = ink left offset relative to the pen origin.
		// B = ink box width.
		// C = advance - A - B  (may be negative for overhanging glyphs; clamp >= 0
		//     only matters for packing, the value is still used for the pen advance).
		g.a = minX - originX;
		g.b = inkW;
		g.c = advance - g.a - g.b;
		g.inkW = inkW;
		g.inkH = inkH;
		g.inkTop = minY - originY;	// ink top relative to the cell top

		// Copy ink pixels out for the packing pass.
		tmp[ci].inkLeft = minX;
		tmp[ci].inkTop  = minY;
		tmp[ci].inkW    = inkW;
		tmp[ci].inkH    = inkH;
		tmp[ci].ink     = new UnsignedInt[inkW * inkH];
		for (Int y = 0; y < inkH; ++y) {
			const UnsignedInt *src = px + (minY + y) * scratchW + minX;
			::memcpy(tmp[ci].ink + y * inkW, src, inkW * 4);
		}
		if (inkH > maxInkH) maxInkH = inkH;
	}

	// --- pack into a single atlas row-strip ---
	// Simple shelf packer: fixed atlas width, wrap to new rows as needed.
	const Int cellH = maxInkH + ATLAS_MARGIN;
	const Int atlasW = 512;
	Int penX = ATLAS_MARGIN, penY = ATLAS_MARGIN, rows = 1;
	for (Int ci = 0; ci < NUM_GLYPHS; ++ci) {
		Int w = tmp[ci].inkW;
		if (w <= 0) continue;
		if (penX + w + ATLAS_MARGIN > atlasW) {
			penX = ATLAS_MARGIN;
			penY += cellH;
			++rows;
		}
		// stash provisional position in the glyph (finalized after we know atlasH)
		m_glyphs[ci].u0 = (Real)penX;		// temporarily store pixel x in u0
		m_glyphs[ci].v0 = (Real)penY;		// temporarily store pixel y in v0
		penX += w + ATLAS_MARGIN;
	}
	Int atlasH = rows * cellH + ATLAS_MARGIN;

	// Round atlas height up to keep things tidy (not strictly required).
	m_atlasW = atlasW;
	m_atlasH = atlasH;
	m_atlasBits = new UnsignedInt[m_atlasW * m_atlasH];
	::memset(m_atlasBits, 0, (size_t)m_atlasW * m_atlasH * 4);

	// Blit each glyph's ink into the atlas, convert to A8R8G8B8 with
	// alpha = luminance (ClearType coverage), RGB forced white so the vertex
	// diffuse color tints the glyph via MODULATE.
	for (Int ci = 0; ci < NUM_GLYPHS; ++ci) {
		Glyph &g = m_glyphs[ci];
		Int w = tmp[ci].inkW, h = tmp[ci].inkH;
		if (w <= 0 || tmp[ci].ink == NULL) {
			g.u0 = g.v0 = g.u1 = g.v1 = 0.0f;
			continue;
		}
		Int dstX = (Int)g.u0;	// reread provisional pixel position
		Int dstY = (Int)g.v0;
		for (Int y = 0; y < h; ++y) {
			UnsignedInt *dst = m_atlasBits + (dstY + y) * m_atlasW + dstX;
			const UnsignedInt *src = tmp[ci].ink + y * w;
			for (Int x = 0; x < w; ++x) {
				UnsignedInt c = src[x];
				UnsignedInt b = c & 0xFF;
				UnsignedInt gr = (c >> 8) & 0xFF;
				UnsignedInt r = (c >> 16) & 0xFF;
				UnsignedInt a = r; if (gr > a) a = gr; if (b > a) a = b;	// luminance ~ max channel
				dst[x] = (a << 24) | 0x00FFFFFF;
			}
		}
		// Finalize UVs.
		g.u0 = (Real)dstX / (Real)m_atlasW;
		g.v0 = (Real)dstY / (Real)m_atlasH;
		g.u1 = (Real)(dstX + w) / (Real)m_atlasW;
		g.v1 = (Real)(dstY + h) / (Real)m_atlasH;
		delete [] tmp[ci].ink;
	}
	delete [] tmp;

	::SelectObject(dc, oldFont);
	::SelectObject(dc, oldBmp);
	::DeleteObject(scratch);
	::DeleteDC(dc);
	::DeleteObject(font);

	m_texDirty = true;	// needs upload on first draw
	return TRUE;
}

Int WBFontAtlas::measureWidth(const char *str, Int len) const
{
	if (!str) return 0;
	Int w = 0;
	for (Int i = 0; i < len; ++i) {
		unsigned char ch = (unsigned char)str[i];
		if (ch < FIRST_CHAR || ch > LAST_CHAR) ch = '?';
		const Glyph &g = m_glyphs[ch - FIRST_CHAR];
		w += g.a + g.b + g.c;
	}
	return w;
}

void WBFontAtlas::ensureTexture()
{
	if (!m_dev || !m_atlasBits) return;
	if (m_texture && !m_texDirty) return;

	if (m_texture) { m_texture->Release(); m_texture = NULL; }

	if (FAILED(m_dev->CreateTexture(m_atlasW, m_atlasH, 1, 0, D3DFMT_A8R8G8B8,
			D3DPOOL_MANAGED, &m_texture)) || !m_texture) {
		m_texture = NULL;
		return;
	}

	D3DLOCKED_RECT lr;
	if (SUCCEEDED(m_texture->LockRect(0, &lr, NULL, 0))) {
		const unsigned char *src = (const unsigned char *)m_atlasBits;
		unsigned char *dst = (unsigned char *)lr.pBits;
		for (Int y = 0; y < m_atlasH; ++y)
			::memcpy(dst + y * lr.Pitch, src + (size_t)y * m_atlasW * 4, m_atlasW * 4);
		m_texture->UnlockRect(0);
	}
	m_texDirty = false;
}

void WBFontAtlas::begin(IDirect3DDevice8 *dev, Int viewW, Int viewH)
{
	m_dev = dev;
	m_viewW = viewW;
	m_viewH = viewH;
	m_vertCount = 0;
	m_inBatch = true;
	if (!m_dev) return;
	ensureTexture();
}

void WBFontAtlas::pushGlyphQuad(Int penX, Int penY, const Glyph &g, UnsignedInt argb)
{
	if (g.inkW <= 0) return;	// blank glyph

	// Grow the vertex batch as needed (6 verts per glyph = 2 triangles).
	if (m_vertCount + 6 > m_vertCap) {
		Int newCap = m_vertCap ? m_vertCap * 2 : 4096;
		while (m_vertCount + 6 > newCap) newCap *= 2;
		TLVertex *nv = new TLVertex[newCap];
		if (m_verts) {
			::memcpy(nv, m_verts, sizeof(TLVertex) * m_vertCount);
			delete [] m_verts;
		}
		m_verts = nv;
		m_vertCap = newCap;
	}

	// Place the ink box: x starts at pen + A; y at cell top + inkTop.
	Real x0 = (Real)(penX + g.a) - 0.5f;
	Real y0 = (Real)(penY + g.inkTop) - 0.5f;
	Real x1 = x0 + (Real)g.inkW;
	Real y1 = y0 + (Real)g.inkH;

	TLVertex v[4] = {
		{ x0, y0, 0.0f, 1.0f, argb, g.u0, g.v0 },
		{ x1, y0, 0.0f, 1.0f, argb, g.u1, g.v0 },
		{ x0, y1, 0.0f, 1.0f, argb, g.u0, g.v1 },
		{ x1, y1, 0.0f, 1.0f, argb, g.u1, g.v1 },
	};
	// Two triangles: 0-1-2, 2-1-3.
	m_verts[m_vertCount++] = v[0];
	m_verts[m_vertCount++] = v[1];
	m_verts[m_vertCount++] = v[2];
	m_verts[m_vertCount++] = v[2];
	m_verts[m_vertCount++] = v[1];
	m_verts[m_vertCount++] = v[3];
}

void WBFontAtlas::drawText(Int x, Int y, const char *str, Int len, UnsignedInt argb, Bool shadow)
{
	if (!m_inBatch || !str || len <= 0 || !m_texture) return;

	// Shadow first (1px down-right, black) so the main text sits on top.
	if (shadow) {
		Int penX = x + 1, penY = y + 1;
		UnsignedInt sa = (argb >> 24) & 0xFF;
		UnsignedInt shadowColor = (sa << 24);	// black, same alpha
		for (Int i = 0; i < len; ++i) {
			unsigned char ch = (unsigned char)str[i];
			if (ch < FIRST_CHAR || ch > LAST_CHAR) ch = '?';
			const Glyph &g = m_glyphs[ch - FIRST_CHAR];
			pushGlyphQuad(penX, penY, g, shadowColor);
			penX += g.a + g.b + g.c;
		}
	}

	Int penX = x, penY = y;
	for (Int i = 0; i < len; ++i) {
		unsigned char ch = (unsigned char)str[i];
		if (ch < FIRST_CHAR || ch > LAST_CHAR) ch = '?';
		const Glyph &g = m_glyphs[ch - FIRST_CHAR];
		pushGlyphQuad(penX, penY, g, argb);
		penX += g.a + g.b + g.c;
	}
}

void WBFontAtlas::flushBatch()
{
	if (!m_dev || m_vertCount < 3 || !m_texture) return;

	IDirect3DDevice8 *dev = m_dev;

	// Save the render/texture/sampler states we touch.
	DWORD oldAlphaBlend, oldSrcBlend, oldDestBlend, oldZEnable, oldZWrite, oldCull, oldLighting, oldFog, oldAlphaTest;
	dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlphaBlend);
	dev->GetRenderState(D3DRS_SRCBLEND,         &oldSrcBlend);
	dev->GetRenderState(D3DRS_DESTBLEND,        &oldDestBlend);
	dev->GetRenderState(D3DRS_ZENABLE,          &oldZEnable);
	dev->GetRenderState(D3DRS_ZWRITEENABLE,     &oldZWrite);
	dev->GetRenderState(D3DRS_CULLMODE,         &oldCull);
	dev->GetRenderState(D3DRS_LIGHTING,         &oldLighting);
	dev->GetRenderState(D3DRS_FOGENABLE,        &oldFog);
	dev->GetRenderState(D3DRS_ALPHATESTENABLE,  &oldAlphaTest);

	DWORD oldColorOp, oldColorArg1, oldColorArg2, oldAlphaOp, oldAlphaArg1, oldAlphaArg2;
	dev->GetTextureStageState(0, D3DTSS_COLOROP,   &oldColorOp);
	dev->GetTextureStageState(0, D3DTSS_COLORARG1, &oldColorArg1);
	dev->GetTextureStageState(0, D3DTSS_COLORARG2, &oldColorArg2);
	dev->GetTextureStageState(0, D3DTSS_ALPHAOP,   &oldAlphaOp);
	dev->GetTextureStageState(0, D3DTSS_ALPHAARG1, &oldAlphaArg1);
	dev->GetTextureStageState(0, D3DTSS_ALPHAARG2, &oldAlphaArg2);

	DWORD oldMin, oldMag, oldMip, oldAddrU, oldAddrV;
	dev->GetTextureStageState(0, D3DTSS_MINFILTER, &oldMin);
	dev->GetTextureStageState(0, D3DTSS_MAGFILTER, &oldMag);
	dev->GetTextureStageState(0, D3DTSS_MIPFILTER, &oldMip);
	dev->GetTextureStageState(0, D3DTSS_ADDRESSU,  &oldAddrU);
	dev->GetTextureStageState(0, D3DTSS_ADDRESSV,  &oldAddrV);

	IDirect3DBaseTexture8 *oldTex = NULL;
	dev->GetTexture(0, &oldTex);

	// Set our state: alpha-blended, no z, no cull, MODULATE(tex.alpha, diffuse).
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	dev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
	dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	dev->SetRenderState(D3DRS_ZENABLE,      D3DZB_FALSE);
	dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	dev->SetRenderState(D3DRS_CULLMODE,     D3DCULL_NONE);
	dev->SetRenderState(D3DRS_LIGHTING,     FALSE);
	dev->SetRenderState(D3DRS_FOGENABLE,    FALSE);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

	dev->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
	dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	dev->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
	dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	dev->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_POINT);
	dev->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
	dev->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
	dev->SetTextureStageState(0, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP);
	dev->SetTextureStageState(0, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP);

	dev->SetTexture(0, m_texture);
	dev->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);

	dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, m_vertCount / 3, m_verts, sizeof(TLVertex));

	// Restore.
	dev->SetTexture(0, oldTex);
	if (oldTex) oldTex->Release();

	dev->SetTextureStageState(0, D3DTSS_COLOROP,   oldColorOp);
	dev->SetTextureStageState(0, D3DTSS_COLORARG1, oldColorArg1);
	dev->SetTextureStageState(0, D3DTSS_COLORARG2, oldColorArg2);
	dev->SetTextureStageState(0, D3DTSS_ALPHAOP,   oldAlphaOp);
	dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, oldAlphaArg1);
	dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, oldAlphaArg2);
	dev->SetTextureStageState(0, D3DTSS_MINFILTER, oldMin);
	dev->SetTextureStageState(0, D3DTSS_MAGFILTER, oldMag);
	dev->SetTextureStageState(0, D3DTSS_MIPFILTER, oldMip);
	dev->SetTextureStageState(0, D3DTSS_ADDRESSU,  oldAddrU);
	dev->SetTextureStageState(0, D3DTSS_ADDRESSV,  oldAddrV);

	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlphaBlend);
	dev->SetRenderState(D3DRS_SRCBLEND,         oldSrcBlend);
	dev->SetRenderState(D3DRS_DESTBLEND,        oldDestBlend);
	dev->SetRenderState(D3DRS_ZENABLE,          oldZEnable);
	dev->SetRenderState(D3DRS_ZWRITEENABLE,     oldZWrite);
	dev->SetRenderState(D3DRS_CULLMODE,         oldCull);
	dev->SetRenderState(D3DRS_LIGHTING,         oldLighting);
	dev->SetRenderState(D3DRS_FOGENABLE,        oldFog);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE,  oldAlphaTest);

	DX8Wrapper::Invalidate_Cached_Render_States();
}

void WBFontAtlas::end()
{
	flushBatch();
	// NOTE: deliberately do NOT clear m_vertCount here. The built batch is
	// retained so reissue() can redraw it next frame when the view is unchanged.
	// begin() resets m_vertCount = 0 before the next rebuild, so keeping it here
	// is safe.
	m_dev = NULL;
	m_inBatch = false;
}

void WBFontAtlas::reissue(IDirect3DDevice8 *dev, Int viewW, Int viewH)
{
	// Redraw the geometry retained from the previous begin()/end() batch, without
	// rebuilding it. Used when the caller knows the view hasn't changed.
	if (!dev || m_vertCount < 3 || !m_texture) return;
	m_dev = dev;
	m_viewW = viewW;
	m_viewH = viewH;
	ensureTexture();
	flushBatch();
	m_dev = NULL;
}
