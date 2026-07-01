// WBQtBuildListBridge.cpp -- the MFC side of the Qt Build-List-panel seam. Plain MFC TU (no
// Qt include); reverse callbacks forward to the still-created-but-hidden MFC BuildList via its
// static qt* methods. Whole body guarded by RTS_HAS_QT so the OFF build compiles it to an
// empty object. The model walk / power calc / commands all live on BuildList itself.
#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "BuildList.h"
#include "qt/WBQtPanelBridge.h"

#ifdef RTS_HAS_QT
extern "C" {

int  WBQtBuildList_GetSideCount(void)              { return BuildList::qtGetSideCount(); }
int  WBQtBuildList_GetSideName(int i, char *o, int c){ return BuildList::qtGetSideName(i, o, c); }
int  WBQtBuildList_GetCurSide(void)                { return BuildList::qtGetCurSide(); }
void WBQtBuildList_SetCurSide(int i)               { BuildList::qtSetCurSide(i); }
int  WBQtBuildList_GetBuildCount(void)             { return BuildList::qtGetBuildCount(); }
int  WBQtBuildList_GetBuildName(int i, char *o, int c){ return BuildList::qtGetBuildName(i, o, c); }
int  WBQtBuildList_GetCurBuild(void)               { return BuildList::qtGetCurBuild(); }
void WBQtBuildList_SetCurBuild(int i)              { BuildList::qtSetCurBuild(i); }

int    WBQtBuildList_HasCurBuild(void)             { return BuildList::qtHasCurBuild(); }
double WBQtBuildList_GetAngle(void)                { return BuildList::qtGetAngle(); }
double WBQtBuildList_GetZ(void)                    { return BuildList::qtGetZ(); }
int    WBQtBuildList_GetAlreadyBuilt(void)         { return BuildList::qtGetAlreadyBuilt(); }
int    WBQtBuildList_GetRebuilds(void)             { return BuildList::qtGetRebuilds(); }
void   WBQtBuildList_SetAngle(double d)            { BuildList::qtSetAngle(d); }
void   WBQtBuildList_SetZ(double z)                { BuildList::qtSetZ(z); }
void   WBQtBuildList_SetAlreadyBuilt(int on)       { BuildList::qtSetAlreadyBuilt(on); }
void   WBQtBuildList_SetRebuilds(int nr)           { BuildList::qtSetRebuilds(nr); }

int  WBQtBuildList_GetPowerPercent(void)           { return BuildList::qtGetPowerPercent(); }

void WBQtBuildList_MoveUp(void)                    { BuildList::qtMoveUp(); }
void WBQtBuildList_MoveDown(void)                  { BuildList::qtMoveDown(); }
void WBQtBuildList_AddBuilding(void)               { BuildList::qtAddBuilding(); }
void WBQtBuildList_DeleteBuilding(void)            { BuildList::qtDeleteBuilding(); }
void WBQtBuildList_Export(void)                    { BuildList::qtExport(); }
void WBQtBuildList_Import(void)                    { BuildList::qtImport(); }
void WBQtBuildList_EditProps(void)                 { BuildList::qtEditProps(); }
int  WBQtBuildList_GetForcedShow(void)             { return BuildList::qtGetForcedShow(); }
void WBQtBuildList_SetForcedShow(int on)           { BuildList::qtSetForcedShow(on); }

}
#endif
