// WBQtMeshMoldBridge.cpp -- the MFC side of the Qt MeshMold-panel seam. See WBQtFenceBridge.cpp
// for the pattern. Plain MFC TU (no Qt include); reverse callbacks resolved against the exe at
// the final link. Whole body guarded by RTS_HAS_QT so the OFF build compiles it to an empty
// object.
//
// The MFC MeshMoldOptions is still created as the hidden OFF fallback and owns the selection
// statics MeshMoldTool reads: m_meshModelName (the selected .w3d mold), m_currentAngle,
// m_currentScale, m_currentHeight, m_doingPreview, and m_raiseOnly / m_lowerOnly. This bridge
// lets the Qt MeshMold panel drive those statics + fire the same command handlers (Apply /
// open-folder / open-link) through MeshMoldOptions::qt* (declared in MeshMoldOptions.h; defined
// here so they can reach the protected instance handlers via m_staticThis without churning
// MeshMoldOptions.cpp). The mold list is enumerated the same way OnInitDialog builds its tree:
// the .w3d files under data\Editor\Molds via TheFileSystem.
#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "MeshMoldOptions.h"
#include "MeshMoldTool.h"
#include "WorldBuilderDoc.h"
#include "Common/FileSystem.h"
#include "qt/panels/WBQtMeshMoldBridge.h"

#include <vector>

#ifdef RTS_HAS_QT

//----------------------------------------------------------------------------------------
// MeshMoldOptions Qt-support statics (declared in MeshMoldOptions.h; defined here so they can
// reach the protected selection state / command handlers without churning MeshMoldOptions.cpp).
// The MAP_HEIGHT_SCALE cells->feet conversion for height lives here (engine header, MFC side).
//----------------------------------------------------------------------------------------
void MeshMoldOptions::qtSelectModel(const char *name)
{
	if (m_staticThis == NULL || name == NULL)
	{
		return;
	}
	// Mirrors the MFC TVN_SELCHANGED path: store the model name, and if previewing re-place
	// the preview mesh so the new mold shows immediately.
	m_staticThis->m_meshModelName = AsciiString(name);
	if (m_doingPreview)
	{
		MeshMoldTool::updateMeshLocation(false);
	}
}

int MeshMoldOptions::qtGetSelectedModel(char *nameOut, int cap)
{
	if (nameOut == NULL || cap <= 0)
	{
		return 0;
	}
	AsciiString name = getModelName();	// m_staticThis->m_meshModelName (or "")
	strncpy(nameOut, name.str(), cap - 1);
	nameOut[cap - 1] = 0;
	return 1;
}

int MeshMoldOptions::qtGetAngle(void)
{
	return m_currentAngle;
}

void MeshMoldOptions::qtSetAngle(int angleDegrees)
{
	// setAngle stores m_currentAngle and (when not mid-update) writes the MFC edit box. Angle
	// alone does not move the preview mesh (matches PopSliderChanged / OnChangeAngleEdit).
	setAngle(angleDegrees);
}

int MeshMoldOptions::qtGetScalePercent(void)
{
	return (int)floor(m_currentScale * 100 + 0.5f);
}

void MeshMoldOptions::qtSetScalePercent(int scalePercent)
{
	// m_currentScale is a fraction (percent/100). setScale stores it, writes the edit box, and
	// re-runs updateMeshLocation (matches the scale slider / edit path).
	setScale(scalePercent / 100.0f);
}

int MeshMoldOptions::qtGetHeightRaw(void)
{
	return (int)floor(m_currentHeight / MAP_HEIGHT_SCALE + 0.5f);
}

void MeshMoldOptions::qtSetHeightRaw(int heightRaw)
{
	// The raw slider unit maps to world height via MAP_HEIGHT_SCALE (PopSliderChanged does the
	// same). setHeight stores it, writes the edit box, and re-runs updateMeshLocation.
	setHeight(heightRaw * MAP_HEIGHT_SCALE);
}

int MeshMoldOptions::qtGetPreview(void)
{
	return m_doingPreview ? 1 : 0;
}

void MeshMoldOptions::qtSetPreview(int on)
{
	// Mirrors OnPreview: set the flag then re-run updateMeshLocation(true) so the preview mesh
	// appears / clears.
	m_doingPreview = (on != 0);
	MeshMoldTool::updateMeshLocation(true);
}

void MeshMoldOptions::qtApplyMesh(void)
{
	// Mirrors OnApplyMesh.
	MeshMoldTool::apply(CWorldBuilderDoc::GetActiveDoc());
}

int MeshMoldOptions::qtGetRaiseMode(void)
{
	if (m_raiseOnly)
	{
		return 0;
	}
	if (m_lowerOnly)
	{
		return 2;
	}
	return 1;	// raise + lower
}

void MeshMoldOptions::qtSetRaiseMode(int mode)
{
	// Mirrors OnRaise / OnRaiseLower / OnLower: set the two flags, then re-place the preview
	// mesh when previewing.
	if (mode == 0)
	{
		m_raiseOnly = true;
		m_lowerOnly = false;
	}
	else if (mode == 2)
	{
		m_raiseOnly = false;
		m_lowerOnly = true;
	}
	else
	{
		m_raiseOnly = false;
		m_lowerOnly = false;
	}
	if (m_doingPreview)
	{
		MeshMoldTool::updateMeshLocation(false);
	}
}

void MeshMoldOptions::qtOpenMoldsFolder(void)
{
	// Reuse the existing protected handler via m_staticThis (a static member of the same class
	// may call the protected instance method) so the ShellExecute logic isn't duplicated.
	if (m_staticThis != NULL)
	{
		m_staticThis->OnOpenMoldsFolder();
	}
}

void MeshMoldOptions::qtOpenLink(void)
{
	if (m_staticThis != NULL)
	{
		m_staticThis->OnOpenLinkCreateMolds();
	}
}

//----------------------------------------------------------------------------------------
// Mold enumeration: the same disk scan OnInitDialog does -- the .w3d models under
// data\Editor\Molds, with the extension + any leading path stripped to the leaf name. Cached in
// a stable, index-ordered list rebuilt on WBQtMeshMold_GetCount so GetName(i) stays consistent.
//----------------------------------------------------------------------------------------
namespace
{
	std::vector<AsciiString> g_moldNames;

	void rebuildMoldNames(void)
	{
		g_moldNames.clear();

		char dirBuf[_MAX_PATH];
		char fileBuf[_MAX_PATH];
		int i;

		strcpy(dirBuf, ".\\data\\Editor\\Molds");
		int len = strlen(dirBuf);
		if (len > 0 && dirBuf[len - 1] != '\\')
		{
			dirBuf[len++] = '\\';
			dirBuf[len] = 0;
		}

		FilenameList filenameList;
		TheFileSystem->getFileListInDirectory(AsciiString(dirBuf), AsciiString("*.w3d"), filenameList, FALSE);

		FilenameList::iterator it = filenameList.begin();
		for (; it != filenameList.end(); ++it)
		{
			AsciiString filename = *it;
			len = filename.getLength();
			if (len < 5)
			{
				continue;
			}
			strcpy(fileBuf, filename.str());
			for (i = strlen(fileBuf) - 1; i > 0; i--)
			{
				if (fileBuf[i] == '.')
				{
					// strip off .w3d file extension.
					fileBuf[i] = 0;
				}
			}
			char *nameStart = fileBuf;
			for (i = 0; i < (int)strlen(fileBuf) - 1; i++)
			{
				if (fileBuf[i] == '\\')
				{
					nameStart = fileBuf + i + 1;
				}
			}
			g_moldNames.push_back(AsciiString(nameStart));
		}
	}
}

extern "C" {

int WBQtMeshMold_GetCount(void)
{
	rebuildMoldNames();
	return (int)g_moldNames.size();
}

int WBQtMeshMold_GetName(int index, char *nameOut, int cap)
{
	if (nameOut == NULL || cap <= 0 || index < 0 || index >= (int)g_moldNames.size())
	{
		return 0;
	}
	strncpy(nameOut, g_moldNames[index].str(), cap - 1);
	nameOut[cap - 1] = 0;
	return 1;
}

void WBQtMeshMold_SelectName(const char *name)
{
	MeshMoldOptions::qtSelectModel(name);
}

int WBQtMeshMold_GetSelectedName(char *nameOut, int cap)
{
	return MeshMoldOptions::qtGetSelectedModel(nameOut, cap);
}

int WBQtMeshMold_GetAngle(void)
{
	return MeshMoldOptions::qtGetAngle();
}

void WBQtMeshMold_SetAngle(int angleDegrees)
{
	MeshMoldOptions::qtSetAngle(angleDegrees);
}

int WBQtMeshMold_GetScalePercent(void)
{
	return MeshMoldOptions::qtGetScalePercent();
}

void WBQtMeshMold_SetScalePercent(int scalePercent)
{
	MeshMoldOptions::qtSetScalePercent(scalePercent);
}

int WBQtMeshMold_GetHeightRaw(void)
{
	return MeshMoldOptions::qtGetHeightRaw();
}

void WBQtMeshMold_SetHeightRaw(int heightRaw)
{
	MeshMoldOptions::qtSetHeightRaw(heightRaw);
}

int WBQtMeshMold_GetPreview(void)
{
	return MeshMoldOptions::qtGetPreview();
}

void WBQtMeshMold_SetPreview(int on)
{
	MeshMoldOptions::qtSetPreview(on);
}

void WBQtMeshMold_ApplyMesh(void)
{
	MeshMoldOptions::qtApplyMesh();
}

int WBQtMeshMold_GetRaiseMode(void)
{
	return MeshMoldOptions::qtGetRaiseMode();
}

void WBQtMeshMold_SetRaiseMode(int mode)
{
	MeshMoldOptions::qtSetRaiseMode(mode);
}

void WBQtMeshMold_OpenMoldsFolder(void)
{
	MeshMoldOptions::qtOpenMoldsFolder();
}

void WBQtMeshMold_OpenLink(void)
{
	MeshMoldOptions::qtOpenLink();
}

}
#endif
