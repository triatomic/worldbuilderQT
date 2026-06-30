set(WORLDBUILDER_SRC
    "src/addplayerdialog.cpp"
    "src/AutoEdgeOutTool.cpp"
    "src/BaseBuildProps.cpp"
    "src/BlendEdgeTool.cpp"
    "src/BlendMaterial.cpp"
    "src/BorderTool.cpp"
    "src/brushoptions.cpp"
    "src/BrushTool.cpp"
    "src/BuildList.cpp"
    "src/BuildListTool.cpp"
    "src/CameraOptions.cpp"
    "src/CButtonShowColor.cpp"
    "src/CButtonShowColor.h"
    "src/CellWidth.cpp"
    "src/CFixTeamOwnerDialog.cpp"
    "src/ContourOptions.cpp"
    "src/CUndoable.cpp"
    "src/DialogFont.cpp"
    "src/DrawObject.cpp"
    "src/EditAction.cpp"
    "src/EditCondition.cpp"
    "src/EditCoordParameter.cpp"
    "src/EditGroup.cpp"
    "src/EditObjectParameter.cpp"
    "src/EditParameter.cpp"
    "src/ExportScriptsOptions.cpp"
    "src/EyedropperTool.cpp"
    "src/FeatherOptions.cpp"
    "src/FeatherTool.cpp"
    "src/FenceOptions.cpp"
    "src/FenceTool.cpp"
    "src/FloodFillTool.cpp"
    "src/GlobalLightOptions.cpp"
    "src/GroveOptions.cpp"
    "src/GroveTool.cpp"
    "src/HandScrollTool.cpp"
    "src/ImpassableOptions.cpp"
    "src/LayersList.cpp"
    "src/LightOptions.cpp"
    "src/MainFrm.cpp"
    "src/mapobjectprops.cpp"
    "src/MapPreview.cpp"
    "src/MinimapDialog.cpp"
    "src/MapSettings.cpp"
    "src/MeshMoldOptions.cpp"
    "src/MeshMoldTool.cpp"
    "src/MoundOptions.cpp"
    "src/MoundTool.cpp"
    "src/MyToolbar.cpp"
    "src/NewHeightMap.cpp"
    "src/ObjectOptions.cpp"
    "src/ObjectPreview.cpp"
    "src/ObjectTool.cpp"
    "src/OpenMap.cpp"
    "src/OptionsPanel.cpp"
    "src/PickUnitDialog.cpp"
    "src/playerlistdlg.cpp"
    "src/PointerTool.cpp"
    "src/PolygonTool.cpp"
    "src/propedit.cpp"
    "src/RampOptions.cpp"
    "src/RampTool.cpp"
    "src/RoadOptions.cpp"
    "src/RoadTool.cpp"
    "src/RulerOptions.cpp"
    "src/RulerTool.cpp"
    "src/WaveEditorOptions.cpp"
    "src/WaveEditorTool.cpp"
    "src/SaveMap.cpp"
    "src/ScorchOptions.cpp"
    "src/ScorchTool.cpp"
    "src/ScriptActionsFalse.cpp"
    "src/ScriptActionsTrue.cpp"
    "src/ScriptConditions.cpp"
    "src/ScriptDialog.cpp"
    "src/ScriptProperties.cpp"
    "src/SelectMacrotexture.cpp"
    "src/ShadowOptions.cpp"
    "src/SplashScreen.cpp"
    "src/StdAfx.cpp"
    "src/TeamBehavior.cpp"
    "src/TeamGeneric.cpp"
    "src/TeamIdentity.cpp"
    "src/TeamObjectProperties.cpp"
    "src/TeamReinforcement.cpp"
    "src/teamsdialog.cpp"
    "src/TerrainMaterial.cpp"
    "src/TerrainModal.cpp"
    "src/TerrainSwatches.cpp"
    "src/TileTool.cpp"
    "src/Tool.cpp"
    "src/TracingOverlayOptions.cpp"
    "src/WaterOptions.cpp"
    "src/WaterTool.cpp"
    "src/WaypointOptions.cpp"
    "src/WaypointTool.cpp"
    "src/WBFontAtlas.cpp"
    "src/WBFrameWnd.cpp"
    "src/WBHeightMap.cpp"
    "src/WBPopupSlider.cpp"
    "src/WBQtHostBridge.cpp"
    "src/WBQtFeatherBridge.cpp"
    "src/wbview.cpp"
    "src/wbview3d.cpp"
    "src/WHeightMapEdit.cpp"
    "src/WorldBuilder.cpp"
    "src/ToastDialog.cpp"
    "src/WorldBuilderDoc.cpp"
    "src/WorldBuilderView.cpp"
    "include/addplayerdialog.h"
    "include/AutoEdgeOutTool.h"
    "include/BaseBuildProps.h"
    "include/BlendEdgeTool.h"
    "include/BlendMaterial.h"
    "include/BorderTool.h"
    "include/brushoptions.h"
    "include/BrushTool.h"
    "include/BuildList.h"
    "include/BuildListTool.h"
    "include/CameraOptions.h"
    "include/CellWidth.h"
    "include/CFixTeamOwnerDialog.h"
    "include/ContourOptions.h"
    "include/CUndoable.h"
    "include/DialogFont.h"
    "include/DrawObject.h"
    "include/EditAction.h"
    "include/EditCondition.h"
    "include/EditCoordParameter.h"
    "include/EditGroup.h"
    "include/EditObjectParameter.h"
    "include/EditParameter.h"
    "include/ExportScriptsOptions.h"
    "include/EyedropperTool.h"
    "include/FeatherOptions.h"
    "include/FeatherTool.h"
    "include/FenceOptions.h"
    "include/FenceTool.h"
    "include/FloodFillTool.h"
    "include/GlobalLightOptions.h"
    "include/GroveOptions.h"
    "include/GroveTool.h"
    "include/HandScrollTool.h"
    "include/ImpassableOptions.h"
    "include/LayersList.h"
    "include/LightOptions.h"
    "include/MainFrm.h"
    "include/mapobjectprops.h"
    "include/MapPreview.h"
    "include/MinimapDialog.h"
    "include/MapSettings.h"
    "include/MeshMoldOptions.h"
    "include/MeshMoldTool.h"
    "include/MoundOptions.h"
    "include/MoundTool.h"
    "include/MyToolbar.h"
    "include/NewHeightMap.h"
    "include/ObjectOptions.h"
    "include/ObjectPreview.h"
    "include/ObjectTool.h"
    "include/OpenMap.h"
    "include/OptionsPanel.h"
    "include/PickUnitDialog.h"
    "include/playerlistdlg.h"
    "include/PointerTool.h"
    "include/PolygonTool.h"
    "include/propedit.h"
    "include/RampOptions.h"
    "include/RampTool.h"
    "include/RoadOptions.h"
    "include/RoadTool.h"
    "include/RulerOptions.h"
    "include/RulerTool.h"
    "include/WaveEditorOptions.h"
    "include/WaveEditorTool.h"
    "include/SaveMap.h"
    "include/ScorchOptions.h"
    "include/ScorchTool.h"
    "include/ScriptActionsFalse.h"
    "include/ScriptActionsTrue.h"
    "include/ScriptConditions.h"
    "include/ScriptDialog.h"
    "include/ScriptProperties.h"
    "include/SelectMacrotexture.h"
    "include/ShadowOptions.h"
    "include/SplashScreen.h"
    "include/StdAfx.h"
    "include/TeamBehavior.h"
    "include/TeamGeneric.h"
    "include/TeamIdentity.h"
    "include/TeamReinforcement.h"
    "include/teamsdialog.h"
    "include/TerrainMaterial.h"
    "include/TerrainModal.h"
    "include/TerrainSwatches.h"
    "include/TileTool.h"
    "include/Tool.h"
    "include/TracingOverlayOptions.h"
    "include/WaterOptions.h"
    "include/WaterTool.h"
    "include/WaypointOptions.h"
    "include/WaypointTool.h"
    "include/WBFrameWnd.h"
    "include/WBHeightMap.h"
    "include/wbview.h"
    "include/wbview3d.h"
    "include/WHeightMapEdit.h"
    "include/ToastDialog.h"
    "include/CustomConfigProfile.h"
    "include/WorldBuilder.h"
    "include/WorldBuilderDoc.h"
    "include/WorldBuilderView.h"
)

add_executable(z_worldbuilder WIN32)

target_sources(z_worldbuilder PRIVATE ${WORLDBUILDER_SRC})

target_include_directories(z_worldbuilder PRIVATE
    .
    include
    res
)

target_link_libraries(z_worldbuilder PRIVATE
    d3d8lib
    dbghelplib
    imm32
    vfw32
    winmm
    core_browserdispatch
    z_debug
    z_gameengine
    z_gameenginedevice
    z_profile
    zi_gameengine_include
    zi_gameenginedevice_include
    zi_always
)

if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
    # /LARGEADDRESSAWARE lets this 32-bit exe use up to 4GB of address space on
    # 64-bit Windows (instead of the default 2GB), giving big maps more RAM headroom.
    #
    # /PDBALTPATH:%_PDB% embeds only the PDB *filename* in the exe (not the absolute
    # build path). WorldBuilder opens its own PDB at startup to symbolicate crash
    # dumps; without this, a copy of the exe deployed elsewhere (e.g. a game install)
    # reaches back via the embedded absolute path and holds the build-dir PDB open,
    # which then makes the linker fail with LNK1201 on the next build. With just the
    # filename, the running copy looks beside itself and never locks the build PDB.
    target_link_options(z_worldbuilder PRIVATE /LARGEADDRESSAWARE /PDBALTPATH:%_PDB% /NODEFAULTLIB:libci.lib /NODEFAULTLIB:libc.lib)
    target_compile_definitions(z_worldbuilder PRIVATE _AFXDLL)
    target_sources(z_worldbuilder PRIVATE res/WorldBuilder.rc)
    # Per-config exe names so a Debug and a Release build deployed to the same folder
    # don't clobber each other: WorldBuilderZH_d.exe (Debug) vs WorldBuilderZH_r.exe (Release).
    # Other configs (Internal/Profile are Release-config; MinSizeRel/RelWithDebInfo) keep the
    # base name.
    set_target_properties(z_worldbuilder PROPERTIES
        OUTPUT_NAME WorldBuilderZH
        OUTPUT_NAME_DEBUG WorldBuilderZH_d
        OUTPUT_NAME_RELEASE WorldBuilderZH_r)
else()
    set_target_properties(z_worldbuilder PROPERTIES OUTPUT_NAME worldbuilderzh)
endif()

# --- Experimental Qt coexistence (Phase 1 of MFC -> Qt migration) ----------------
# OFF by default; when OFF nothing below runs and z_worldbuilder is byte-for-byte the
# normal MFC build. When ON, all Qt code lives in a SEPARATE static lib so AUTOMOC
# only scans the handful of Qt files (never the big hand-maintained MFC source list).
# Configure with a 32-bit Qt5, e.g. -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2019.
if(RTS_ENABLE_WORLDBUILDER_QT)
    find_package(Qt5 COMPONENTS Widgets REQUIRED)

    # qmfcapp.cpp is the ONLY translation unit that mixes MFC (afxwin.h) and Qt; the
    # rest is pure Qt reached through the opaque WBQtBridge facade.
    add_library(z_worldbuilder_qt STATIC
        "qt/3rdparty/qtwinmigrate/qmfcapp.cpp"
        "qt/3rdparty/qtwinmigrate/qmfcapp.h"
        "qt/3rdparty/qtwinmigrate/qwinwidget.cpp"
        "qt/3rdparty/qtwinmigrate/qwinwidget.h"
        "qt/3rdparty/qtwinmigrate/qwinhost.cpp"
        "qt/3rdparty/qtwinmigrate/qwinhost.h"
        "qt/WBQtBridge.cpp"
        "qt/WBQtBridge.h"
        "qt/WBQtTheme.cpp"
        "qt/WBQtTheme.h"
        "qt/WBQtTestWindow.cpp"
        "qt/WBQtTestWindow.h"
        "qt/WBQtPanelBridge.h"
        "qt/WBQtOptionsPanels.cpp"
        "qt/panels/WBQtFeatherPanel.cpp"
        "qt/panels/WBQtFeatherPanel.h")

    set_target_properties(z_worldbuilder_qt PROPERTIES AUTOMOC ON)

    target_include_directories(z_worldbuilder_qt PRIVATE
        qt
        qt/panels
        res
        qt/3rdparty/qtwinmigrate)

    # _AFXDLL: match the exe's shared-MFC config so afxwin.h resolves identically in
    # qmfcapp.cpp. NOMINMAX: keep windows.h min/max macros from breaking Qt headers in
    # that mixed TU. RTS_HAS_QT: compile-time gate for the bootstrap calls.
    target_compile_definitions(z_worldbuilder_qt PRIVATE _AFXDLL NOMINMAX RTS_HAS_QT)

    # dwmapi: DwmSetWindowAttribute for the dark title bar (WBQtTheme).
    # Qt5::GuiPrivate: qwinwidget.cpp uses the qpa platform-native-interface (a Qt private
    # header) to resolve a QWindow's HWND for MFC embedding.
    target_link_libraries(z_worldbuilder_qt PUBLIC Qt5::Widgets PRIVATE Qt5::GuiPrivate dwmapi)

    # Link the Qt lib into the exe and turn on the guarded bootstrap in WorldBuilder.cpp.
    target_link_libraries(z_worldbuilder PRIVATE z_worldbuilder_qt)
    target_compile_definitions(z_worldbuilder PRIVATE RTS_HAS_QT)

    # WorldBuilder is an MFC app: it already has MFC's WinMain and no main(). Stop Qt's
    # CMake config from auto-linking qtmain.lib (which supplies a WinMain -> main shim and
    # would otherwise cause "unresolved external symbol _main").
    set_target_properties(z_worldbuilder PROPERTIES Qt5_NO_LINK_QTMAIN ON)

    # Copy the Qt runtime (Qt5 DLLs + platforms/qwindows.dll) next to the exe so a dev
    # run straight from the build tree resolves the Qt libraries and platform plugin.
    get_target_property(_qt5_qmake Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(_qt5_bin "${_qt5_qmake}" DIRECTORY)
    add_custom_command(TARGET z_worldbuilder POST_BUILD
        COMMAND "${_qt5_bin}/windeployqt.exe"
                --no-translations --no-system-d3d-compiler --no-opengl-sw --no-compiler-runtime
                "$<IF:$<CONFIG:Debug>,--debug,--release>"
                "$<TARGET_FILE:z_worldbuilder>"
        COMMENT "Deploying Qt runtime next to WorldBuilder (RTS_ENABLE_WORLDBUILDER_QT)"
        VERBATIM)
endif()
