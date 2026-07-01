// WBQtLayersBridge.h -- self-contained opaque facade for the Qt Layers List window.
//
// Carries ONLY int/char* so the MFC side and the Qt side never include each other's headers.
// The hidden MFC LayersList (TheLayersList, still Create()d by CMainFrame) stays the model owner
// -- the whole codebase feeds it as objects/triggers are created, deleted and loaded -- and its
// updateUIFromList() refresh funnel pushes to the Qt window. The reverse funcs below are the MFC
// command handlers keyed by layer/item NAME instead of the last-clicked MFC tree item.
//
// Layer state: 0 = shown, 1 = hidden, 2 = active (the layer new objects are added to).
#ifndef WB_QT_LAYERS_BRIDGE_H
#define WB_QT_LAYERS_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// --- Reverse: Qt window -> hidden MFC model (implemented in WBQtLayersBridge.cpp) ------------
int  WBQtLayers_GetLayerCount(void);
int  WBQtLayers_GetLayerName(int i, char *out, int cap);
int  WBQtLayers_GetLayerState(int i);
int  WBQtLayers_GetItemCount(int layer);
int  WBQtLayers_GetItemLabel(int layer, int j, char *out, int cap);

// Commands. NewLayer creates "New Layer N" and returns its name (the Qt window starts an inline
// rename on it); DeleteLayer merges the layer's contents into the active/default layer first
// (the default and trigger layers can't be deleted); RenameLayer returns 0 when refused (the
// trigger layer can't be renamed).
int  WBQtLayers_NewLayer(char *nameOut, int cap);
void WBQtLayers_DeleteLayer(const char *name);
void WBQtLayers_ToggleHideLayer(const char *name);
void WBQtLayers_ToggleActiveLayer(const char *name);
int  WBQtLayers_RenameLayer(const char *oldName, const char *newName);
void WBQtLayers_MergeLayerInto(const char *src, const char *dst);
void WBQtLayers_MoveObjectToLayer(const char *label, const char *layerName);
void WBQtLayers_MoveViewSelectionToLayer(const char *layerName);
// Select on map: a layer name selects everything in the layer; an item label selects that
// object/trigger (everything else is deselected first).
void WBQtLayers_SelectItem(const char *label);

// --- Forward: MFC -> Qt window (implemented Qt-side, WBQtLayersPanel.cpp) --------------------
// Called from LayersList::updateUIFromList() whenever the model changes (no-op when the Qt
// window isn't open). Note updateUIFromList early-outs while updates are disabled, i.e. while
// the window is closed -- the View-menu toggle re-enables updates when it opens the Qt window.
void WBQtLayers_PushRefresh(void);

// Open / close the Qt window (from WbView3d::OnViewLayersList in Qt mode).
void WBQtLayers_Open(void *frameHwnd);
void WBQtLayers_Close(void);

// Non-zero when the Qt window (or a child, e.g. the inline rename editor) holds the Win32
// keyboard focus (frame accelerator skip).
int  WBQtLayers_OwnsFocus(void);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_LAYERS_BRIDGE_H
