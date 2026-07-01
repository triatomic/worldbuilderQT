// WBQtWaveBridge.h -- self-contained opaque facade for the Qt Wave Editor panel.
//
// Carries ONLY int/float/char* (no Qt or MFC types) so the MFC side can drive the Qt panel and
// the Qt panel can drive WaveEditorTool, without either side including the other's headers.
//
// The wave editor state lives on WaveEditorTool statics (waves, selection, mode, brush size) and
// DrawObject statics (overlay toggles); the reverse funcs below forward to them. The hidden MFC
// WaveEditorOptions stays created as the OFF fallback; WaveEditorTool's own .wak load/save logic
// (activate()'s once-per-map auto-load) is untouched.
#ifndef WB_QT_WAVE_BRIDGE_H
#define WB_QT_WAVE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// --- Reverse: Qt widget -> tool (implemented MFC-side, WBQtWaveBridge.cpp) ------------------
// Wave type. GetTypeName copies the CURRENT type's display name; the type list backs the
// right-click "retype selection" menu.
int  WBQtWave_GetTypeName(char *out, int cap);
void WBQtWave_CycleType(void);
int  WBQtWave_GetTypeCount(void);
int  WBQtWave_GetTypeNameAt(int i, char *out, int cap);
void WBQtWave_SetSelectedWavesType(int typeIndex);

// Commands. DeleteAll is unconditional -- the Qt panel asks for confirmation first (it clears the
// wave undo stack). Save/Reload go through the active doc like the MFC buttons.
void WBQtWave_Undo(void);
void WBQtWave_Save(void);
void WBQtWave_Reload(void);
void WBQtWave_DeleteSelected(void);
void WBQtWave_DeleteAll(void);

// Wave list. GetWaveRow fills the start/end coords + the type display name for row i.
int  WBQtWave_GetWaveCount(void);
int  WBQtWave_GetWaveRow(int i, float *sx, float *sy, float *ex, float *ey, char *typeOut, int cap);
int  WBQtWave_IsWaveSelected(int i);
int  WBQtWave_GetSelectedWave(void);	// the anchor (focused) wave, -1 if none
int  WBQtWave_GetSelectionCount(void);

// Multi-selection push (mirrors the MFC list -> tool sync): begin, add each selected row, end
// with the anchor (focused) row. ClearSelection drops the highlight (panel hidden).
void WBQtWave_BeginListSelection(void);
void WBQtWave_AddListSelection(int row);
void WBQtWave_EndListSelection(int anchorRow);
void WBQtWave_ClearSelection(void);

// Editor mode: 0 Create, 1 Manipulate, 2 Paint, 3 Bucket (== WaveEditorTool::EditorMode order).
int  WBQtWave_GetEditorMode(void);
void WBQtWave_SetEditorMode(int mode);

// Bucket brush radius in world units (the panel clamps to 30..5000 like the MFC slider).
int  WBQtWave_GetBrushSize(void);
void WBQtWave_SetBrushSize(int worldUnits);

// Overlay toggles. Wave lines route through the 3D view (keeps the View menu + registry in
// sync) with a DrawObject fallback; shoreline toggles DrawObject + invalidates the view.
int  WBQtWave_GetShowWaveLines(void);
void WBQtWave_SetShowWaveLines(int on);
int  WBQtWave_GetShowShoreline(void);
void WBQtWave_SetShowShoreline(int on);

void WBQtWave_InvalidateView(void);

// --- Forward: tool -> Qt widget (implemented Qt-side, WBQtWavePanel.cpp) --------------------
// PushRefresh re-seeds the type label + wave list (no-op when the panel isn't open); called from
// WaveEditorOptions::refresh(), the funnel every tool-side change already goes through.
// PushBrushSize re-seeds the brush slider + readout (the '['/']' view hotkeys).
void WBQtWave_PushRefresh(void);
void WBQtWave_PushBrushSize(void);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_WAVE_BRIDGE_H
