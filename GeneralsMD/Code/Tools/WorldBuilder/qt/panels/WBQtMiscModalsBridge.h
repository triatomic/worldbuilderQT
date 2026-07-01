// WBQtMiscModalsBridge.h -- opaque facade for the Tier 3a workflow modals: the small,
// self-contained MFC dialogs migrated to native Qt (ShadowOptions, ImpassableOptions,
// SelectMacrotexture, MapSettings, ExportScriptsOptions).
//
// These read/write global engine state directly (TheW3DShadowManager, TheTerrainRenderObject,
// TheWritableGlobalData, MapObject::getWorldDict, the ExportScriptsOptions statics). The MFC
// side (src/WBQtMiscModalsBridge.cpp) ports the OnInitDialog / OnOK / live-apply handlers; the
// Qt side (qt/panels/WBQtMiscModals.cpp) is the widgets. Plain C surface (int / double /
// const char* only).
#ifndef WB_QT_MISC_MODALS_BRIDGE_H
#define WB_QT_MISC_MODALS_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// ================= MFC -> Qt (implemented in qt/panels/WBQtMiscModals.cpp) =================
// Each runs its dialog modal over the WB frame (disabled for the modal's lifetime). Return 1
// on OK, 0 on cancel; the ..._Run entry points that have no cancel path return 1.

int WBQtShadowOptions_Run(void *frameHwnd);
int WBQtImpassableOptions_Run(void *frameHwnd);
int WBQtSelectMacrotexture_Run(void *frameHwnd);
int WBQtMapSettings_Run(void *frameHwnd);
int WBQtExportScriptsOptions_Run(void *frameHwnd);

// ================= Qt -> MFC (implemented in src/WBQtMiscModalsBridge.cpp) =================

// --- Shadow Options: RGB + intensity (each 0..1), live-applied to the shadow manager ---
void WBQtShadow_Get(double *red, double *green, double *blue, double *intensity);
void WBQtShadow_Apply(double red, double green, double blue, double intensity);

// --- Impassable Options: show-slope angle (degrees), clamped to [0, 89.9] ---
// Begin turns the impassable-area overlay on (saving the prior state) and snapshots the slope;
// End restores the overlay and, when accepted==0 (cancel), reverts the slope to the snapshot.
void   WBQtImpassable_Begin(void);
void   WBQtImpassable_End(int accepted);
double WBQtImpassable_GetSlope(void);
double WBQtImpassable_SetSlope(double slope);	// returns the clamped value actually stored
void   WBQtImpassable_Preview(void);

// --- Select Macrotexture: enumerate the .tga list; apply one live on selection ---
int  WBQtMacrotexture_GetCount(void);			// item 0.. are files; the last is "***Default"
void WBQtMacrotexture_GetName(int i, char *buf, int cap);
int  WBQtMacrotexture_IsDefault(int i);
void WBQtMacrotexture_Apply(int i);

// --- Map Settings: time-of-day / weather / compression combos + map name ---
int  WBQtMapSettings_GetTimeOfDayCount(void);
void WBQtMapSettings_GetTimeOfDayName(int i, char *buf, int cap);
int  WBQtMapSettings_GetTimeOfDayIndex(void);
int  WBQtMapSettings_GetWeatherCount(void);
void WBQtMapSettings_GetWeatherName(int i, char *buf, int cap);
int  WBQtMapSettings_GetWeatherIndex(void);
int  WBQtMapSettings_GetCompressionCount(void);
void WBQtMapSettings_GetCompressionName(int i, char *buf, int cap);
int  WBQtMapSettings_GetCompressionIndex(void);
void WBQtMapSettings_GetMapName(char *buf, int cap);
void WBQtMapSettings_Store(int timeOfDayIndex, int weatherIndex, int compressionIndex, const char *mapName);

// --- Export Scripts Options: the six persisted flags ---
void WBQtExportScripts_Get(int *waypoints, int *triggers, int *units, int *teams, int *sides, int *allScripts);
void WBQtExportScripts_Store(int waypoints, int triggers, int units, int teams, int sides, int allScripts);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_MISC_MODALS_BRIDGE_H
