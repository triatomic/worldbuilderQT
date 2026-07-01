// WBQtLayersBridge.cpp -- MFC side of the Qt Layers List facade.
//
// Forwards the extern "C" reverse callbacks (qt/panels/WBQtLayersBridge.h) to the
// LayersList::qt* statics. Whole body behind RTS_HAS_QT; empty TU when Qt is OFF.

#include "StdAfx.h"

#ifdef RTS_HAS_QT

#include "resource.h"
#include "Lib/BaseType.h"
#include "LayersList.h"
#include "qt/panels/WBQtLayersBridge.h"

extern "C" int WBQtLayers_GetLayerCount(void)
{
	return LayersList::qtGetLayerCount();
}

extern "C" int WBQtLayers_GetLayerName(int i, char *out, int cap)
{
	return LayersList::qtGetLayerName(i, out, cap);
}

extern "C" int WBQtLayers_GetLayerState(int i)
{
	return LayersList::qtGetLayerState(i);
}

extern "C" int WBQtLayers_GetItemCount(int layer)
{
	return LayersList::qtGetItemCount(layer);
}

extern "C" int WBQtLayers_GetItemLabel(int layer, int j, char *out, int cap)
{
	return LayersList::qtGetItemLabel(layer, j, out, cap);
}

extern "C" int WBQtLayers_NewLayer(char *nameOut, int cap)
{
	return LayersList::qtNewLayer(nameOut, cap);
}

extern "C" void WBQtLayers_DeleteLayer(const char *name)
{
	LayersList::qtDeleteLayer(name);
}

extern "C" void WBQtLayers_ToggleHideLayer(const char *name)
{
	LayersList::qtToggleHideLayer(name);
}

extern "C" void WBQtLayers_ToggleActiveLayer(const char *name)
{
	LayersList::qtToggleActiveLayer(name);
}

extern "C" int WBQtLayers_RenameLayer(const char *oldName, const char *newName)
{
	return LayersList::qtRenameLayer(oldName, newName);
}

extern "C" void WBQtLayers_MergeLayerInto(const char *src, const char *dst)
{
	LayersList::qtMergeLayerInto(src, dst);
}

extern "C" void WBQtLayers_MoveObjectToLayer(const char *label, const char *layerName)
{
	LayersList::qtMoveObjectToLayer(label, layerName);
}

extern "C" void WBQtLayers_MoveViewSelectionToLayer(const char *layerName)
{
	LayersList::qtMoveViewSelectionToLayer(layerName);
}

extern "C" void WBQtLayers_SelectItem(const char *label)
{
	LayersList::qtSelectItem(label);
}

#endif // RTS_HAS_QT
