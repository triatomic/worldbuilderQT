// WBQtMapGenBridge.h -- C facade for the Map Generator modal.
//
// Carries ONLY ints so the MFC side and the Qt side never include each other's
// headers.
#ifndef WB_QT_MAP_GEN_BRIDGE_H
#define WB_QT_MAP_GEN_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/// Runs the Map Generator settings dialog.
///
/// The values are in/out: they seed the controls, and on OK they come back with
/// whatever the user chose. Returns 0 when cancelled, leaving them untouched.
int WBQtMapGen_Run(void *frameHwnd, int *seed, int *numPlayers, int *baseHeight,
	int *doCliffs, int *cliffDensity, int *doTextures, int *doTrees, int *treeDensity,
	int *doRocks, int *doPlayers, int *doSupplies,
	int *roadMode);

/// Smallest playable width/height that fits this many players, and the size of
/// the map that is currently open (0 when there isn't one). The dialog uses these
/// to warn that generating will enlarge the map.
int WBQtMapGen_GetMinimumSize(int numPlayers);
void WBQtMapGen_GetCurrentSize(int *widthOut, int *heightOut);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_MAP_GEN_BRIDGE_H
