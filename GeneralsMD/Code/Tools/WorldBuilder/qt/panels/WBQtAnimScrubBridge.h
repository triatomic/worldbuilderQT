// WBQtAnimScrubBridge.h -- self-contained opaque facade for the Qt Animation Scrubber window.
//
// Carries ONLY primitives and a char buffer so the MFC side and the Qt side never include each
// other's headers (same rule as WBQtCameraBridge.h). The scrubber poses the SELECTED object's draw
// modules at a chosen point through their animations, instead of at the resting frame the view
// normally uses -- see WbView3d::setAnimationScrub.
#ifndef WB_QT_ANIM_SCRUB_BRIDGE_H
#define WB_QT_ANIM_SCRUB_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// --- Reverse: Qt window -> view (implemented in WBQtAnimScrubBridge.cpp) ---------------------

// Describe what is selected and scrubbable. Fills nameBuf with the selected object's template
// name, frameCount with the LONGEST animation among its draw modules, and moduleCount with how
// many of those modules actually animate. Returns 0 when nothing is selected -- in which case the
// panel greys out. frameCount == 0 means "selected, but nothing on it animates".
int WBQtAnimScrub_GetSelection(char *nameBuf, int nameBufSize, int *frameCount, int *moduleCount);

// Pose the selection at `fraction` (0..1) through its animations. Each draw module maps the
// fraction onto its OWN frame count, so modules of different lengths stay in step. Rebuilds the
// object, so this is a redraw-weight call -- fine on slider drag, not per mouse-move.
void WBQtAnimScrub_SetFraction(double fraction);

// Stop scrubbing: every object returns to its normal resting pose.
void WBQtAnimScrub_Clear(void);

// The fraction currently applied (0..1), so the panel can re-seed its slider on show.
double WBQtAnimScrub_GetFraction(void);

// --- Forward: MFC -> Qt window (implemented Qt-side, WBQtAnimScrubPanel.cpp) -----------------

// Selection changed -> re-read the selected object into the panel. Cheap when closed.
void WBQtAnimScrub_PushRefresh(void);

// Open the Qt window (from the View > Models > Animation Scrubber handler).
void WBQtAnimScrub_Open(void *frameHwnd);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_ANIM_SCRUB_BRIDGE_H
