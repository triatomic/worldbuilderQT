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

// WorldBuilder.cpp : Defines the class behaviors for the application.
//

#include "StdAfx.h"
#include <eh.h>
// WBBuildStamp.h is generated at build time (CMakeLists.txt / GitCommitStamp.cmake) and defines
// WB_BUILD_COMMIT to the current git short hash. Fall back to "unknown" for a non-CMake build.
#if defined(__has_include)
#if __has_include("WBBuildStamp.h")
#include "WBBuildStamp.h"
#endif
#endif
#ifndef WB_BUILD_COMMIT
#define WB_BUILD_COMMIT "unknown"
#endif
#include "WorldBuilder.h"
#include "MainFrm.h"
#include "DialogFont.h"
#include "OpenMap.h"
#ifdef RTS_HAS_QT
#include "qt/panels/WBQtMapFileBridge.h"
#endif
#include "SplashScreen.h"
#include "textureloader.h"
#include "WorldBuilderDoc.h"
#include "WorldBuilderView.h"
#include "WBFrameWnd.h"
#include "wbview3d.h"

#include "TerrainMaterial.h"

//#include <wsys/StdFileSystem.h>
#include "W3DDevice/GameClient/W3DFileSystem.h"
#include "Common/GlobalData.h"
#include "WHeightMapEdit.h"
#include "WBParallel.h"
//#include "Common/GameFileSystem.h"
#include "Common/FileSystem.h"
#include "Common/ArchiveFileSystem.h"
#include "Common/LocalFileSystem.h"
#include "Common/CDManager.h"
#include "Common/Debug.h"
#include "Common/StackDump.h"
#include "Common/GameMemory.h"
#include "Common/Science.h"
#include "Common/ThingFactory.h"
#include "Common/INI.h"
#include "Common/GameAudio.h"
#include "Common/SpecialPower.h"
#include "Common/TerrainTypes.h"
#include "Common/DamageFX.h"
#include "Common/Upgrade.h"
#include "Common/ModuleFactory.h"
#include "Common/PlayerTemplate.h"
#include "Common/MultiplayerSettings.h"

#include "GameLogic/Armor.h"
#include "GameLogic/CaveSystem.h"
#include "GameLogic/CrateSystem.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/RankInfo.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/ScriptActions.h"
#include "GameClient/Anim2D.h"
#include "GameClient/GameText.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/Water.h"
#include "GameClient/TerrainRoads.h"
#include "GameClient/FXList.h"
#include "GameClient/VideoPlayer.h"
#include "GameLogic/Locomotor.h"

#include "W3DDevice/Common/W3DModuleFactory.h"
#include "W3DDevice/GameClient/W3DParticleSys.h"
#include "MilesAudioDevice/MilesAudioManager.h"

#include <io.h>
#include "Win32Device/GameClient/Win32Mouse.h"
#include "Win32Device/Common/Win32LocalFileSystem.h"
#include "Win32Device/Common/Win32BIGFileSystem.h"
#include "ToastDialog.h"

#include "Common/WellKnownKeys.h"
#include "Common/CriticalSection.h"

#ifdef RTS_HAS_QT
#include "qt/WBQtBridge.h"		// Phase 1 MFC -> Qt coexistence (experimental; opaque facade, no Qt headers leak here)
#include "qt/WBQtChromeBridge.h"
#include "qt/WBQtShortcuts.h"
#include "qt/WBQtMessageBox.h"
#include "qt/panels/WBQtEntityFinderBridge.h"
#include "qt/WBQtToast.h"
#endif

#ifdef _INTERNAL
// for occasional debugging...
//#pragma optimize("", off)
//#pragma MESSAGE("************************************** WARNING, optimization disabled for debugging purposes")
#endif

static SubsystemInterfaceList TheSubsystemListRecord;

// WorldBuilder historically left these NULL, which makes the memory pool and
// string allocators "silently non-threadsafe" (see CriticalSection.h). The game
// instantiates them in WinMain so the pools are safe under its worker threads.
// We now run parallel worker threads too (the minimap terrain resample via
// WBParallel), so WB must install real critical sections exactly as the game
// does in case a worker allocates via the pool / AsciiString. Uncontended cost
// is ~14ns per lock pair (measured) -- cheap insurance. Assigned at the very
// top of InitInstance, before any pooled allocation.
static CriticalSection critSecUnicode, critSecDma, critSecMemoryPool, critSecDebugLog;

template<class SUBSYSTEM>
void initSubsystem(SUBSYSTEM*& sysref, SUBSYSTEM* sys, const char* path1 = NULL, const char* path2 = NULL, const char* dirpath = NULL)
{
	sysref = sys;
	TheSubsystemListRecord.initSubsystem(sys, path1, path2, dirpath, NULL);
}


#define APP_SECTION "WorldbuilderApp"
#define OPEN_FILE_DIR "OpenDirectory"
#define GAME_DIR "GameDirectory"
#define ABOUT_SECTION "AboutWindow"

// The viewport resolution is [MainFrame] Width/Height -- the keys CMainFrame::
// adjustWindowSize reads to size the view + render target (reset3dEngineDisplaySize), the
// same keys the existing resolution menu writes. Render size and window size are coupled
// (Yves' model): the window is sized to match and the back buffer is stretched to fill it.
#define VIEWPORT_SECTION   "MainFrame"
#define KEY_VIEW_WIDTH     "Width"
#define KEY_VIEW_HEIGHT    "Height"

#define NEWLINE "\r\n"


Win32Mouse *TheWin32Mouse = NULL;
const char *gAppPrefix = "wb_"; /// So WB can have a different debug log file name.
const Char *g_strFile = "data\\Generals.str";
const Char *g_csfFile = "data\\%s\\Generals.csf";

static Bool g_aboutPageOn = false;

/////////////////////////////////////////////////////////////////////////////
// WBGameFileClass - extends the file system a bit so we can get at some 
// wb only data.  jba.

class WBGameFileClass : public GameFileClass
{

public:
	WBGameFileClass(char const *filename):GameFileClass(filename){};
	virtual char const * Set_Name(char const *filename);
};

//-------------------------------------------------------------------------------------------------
/** Sets the file name, and finds the GDI asset if present. */
//-------------------------------------------------------------------------------------------------
char const * WBGameFileClass::Set_Name( char const *filename )
{
	char const *pChar = GameFileClass::Set_Name(filename);
	if (this->Is_Available()) {
		return pChar; // it was found by the parent class.
	}

	if (TheFileSystem->doesFileExist(filename)) {
		strcpy( m_filePath, filename );
		m_fileExists = true;
	}
	return m_filename;
}



/////////////////////////////////////////////////////////////////////////////
// WB_W3DFileSystem - extends the file system a bit so we can get at some 
// wb only data.  jba.

class	WB_W3DFileSystem : public W3DFileSystem {
	virtual FileClass * Get_File( char const *filename );
};

//-------------------------------------------------------------------------------------------------
/** Gets a file with the specified filename. */
//-------------------------------------------------------------------------------------------------
FileClass * WB_W3DFileSystem::Get_File( char const *filename )
{
	WBGameFileClass *pFile = new WBGameFileClass( filename );
	if (!pFile->Is_Available()) {
		pFile->Set_Name(filename);
	}
	return pFile;
}




/////////////////////////////////////////////////////////////////////////////
// The one and only CWorldBuilderApp object

static CWorldBuilderApp theApp;
HWND ApplicationHWnd = NULL;

/**
	* The ApplicationHInstance is needed for the WOL code,
	* which needs it for the COM initialization of WOLAPI.DLL.
	* Of course, the WOL code is in gameengine, while the
	* HINSTANCE is only in the various projects' main files.
	* So, we need to create the HINSTANCE, even if it always
	* stays NULL.  Just to make COM happy.  Whee.
	*/
HINSTANCE ApplicationHInstance = NULL;

/////////////////////////////////////////////////////////////////////////////
// CWorldBuilderApp

BEGIN_MESSAGE_MAP(CWorldBuilderApp, CWinApp)
	//{{AFX_MSG_MAP(CWorldBuilderApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(IDM_RESET_WINDOWS, OnResetWindows)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_TEXTURESIZING_MAPCLIFFTEXTURES, OnTexturesizingMapclifftextures)
	ON_UPDATE_COMMAND_UI(ID_TEXTURESIZING_MAPCLIFFTEXTURES, OnUpdateTexturesizingMapclifftextures)
	//}}AFX_MSG_MAP
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
	// Standard print setup command
	ON_COMMAND(ID_FILE_PRINT_SETUP, CWinApp::OnFilePrintSetup)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
static Int gFirstCP = 0;

// CWorldBuilderApp construction

CWorldBuilderApp::CWorldBuilderApp() :
	m_curTool(NULL),
	m_selTool(NULL),
	m_lockCurTool(0),
	m_3dtemplate(NULL),
	m_pasteMapObjList(NULL)
{

	for (Int i=0; i<NUM_VIEW_TOOLS; i++) {
		m_tools[i] = NULL;

	}
	m_tools[0] = &m_brushTool;
	m_tools[1] = &m_tileTool;
	m_tools[2] = &m_featherTool;
	m_tools[3] = &m_autoEdgeOutTool;
	m_tools[4] = &m_bigTileTool;
	m_tools[5] = &m_floodFillTool;
	m_tools[6] = &m_moundTool;
	m_tools[7] = &m_digTool;
	m_tools[8] = &m_eyedropperTool;
	m_tools[9] = &m_objectTool;
	m_tools[10] = &m_pointerTool;
	m_tools[11] = &m_blendEdgeTool;
	m_tools[12] = &m_groveTool;
	m_tools[13] = &m_meshMoldTool;	 
	m_tools[14] = &m_roadTool;
	m_tools[15] = &m_handScrollTool;
	m_tools[16] = &m_waypointTool;
	m_tools[17] = &m_polygonTool;
	m_tools[18] = &m_buildListTool;
	m_tools[19] = &m_fenceTool;
	m_tools[20] = &m_waterTool;
	m_tools[21] = &m_rampTool;
	m_tools[22] = &m_scorchTool;
	m_tools[23] = &m_borderTool;
	m_tools[24] = &m_rulerTool;
	m_tools[25] = &m_waveEditorTool;

	// set up initial values.
	m_brushTool.setHeight(16);
	m_brushTool.setWidth(3);
	m_brushTool.setFeather(3);
	m_moundTool.setMoundHeight(3);
	m_moundTool.setWidth(3);
	m_moundTool.setFeather(3);
	m_featherTool.setFeather(3);
	m_featherTool.setRadius(1);
	m_featherTool.setRate(2);
}

/////////////////////////////////////////////////////////////////////////////
// CWorldBuilderApp destructor

CWorldBuilderApp::~CWorldBuilderApp()
{
	m_curTool = NULL;
	m_selTool = NULL;
	m_bLaunchOnStartUp = true;

	for (Int i=0; i<NUM_VIEW_TOOLS; i++) {
		if (m_tools[i]) {
			m_tools[i] = NULL;
		}
	}
	_exit(0);
}


/////////////////////////////////////////////////////////////////////////////
// CWorldBuilderApp initialization

BOOL CWorldBuilderApp::InitInstance()
{

	// Install the allocator critical sections before ANY pooled allocation, so
	// the memory pool and string allocators are genuinely thread-safe under our
	// parallel worker threads. (The game does the equivalent in WinMain; WB used
	// to leave these NULL and rely on being single-threaded.)
	TheUnicodeStringCriticalSection = &critSecUnicode;
	TheDmaCriticalSection = &critSecDma;
	TheMemoryPoolCriticalSection = &critSecMemoryPool;
	TheDebugLogCriticalSection = &critSecDebugLog;

	ApplicationHWnd = GetDesktopWindow();

	// initialization
  _set_se_translator( DumpExceptionInfo ); // Hook that allows stack trace.

	// start the log
	DEBUG_INIT(DEBUG_FLAGS_DEFAULT);

#ifdef DEBUG_LOGGING
	// Turn on console output jba [3/20/2003]
	DebugSetFlags(DebugGetFlags() | DEBUG_FLAG_LOG_TO_CONSOLE);
#endif

	DEBUG_LOG(("starting Worldbuilder.\n"));

#ifdef _INTERNAL
	DEBUG_LOG(("_INTERNAL defined.\n"));
#endif
#ifdef _DEBUG
	DEBUG_LOG(("_DEBUG defined.\n"));
#endif
	initMemoryManager();
#ifdef MEMORYPOOL_CHECKPOINTING
	gFirstCP = TheMemoryPoolFactory->debugSetCheckpoint();
#endif

	SplashScreen loadWindow;
	loadWindow.Create(IDD_LOADING, loadWindow.GetDesktopWindow());
	loadWindow.SetWindowText("Loading Worldbuilder");
	loadWindow.ShowWindow(SW_SHOW);
	loadWindow.UpdateWindow();

	// The bottom-center loading label is auto-positioned in SplashScreen::OnInitDialog;
	// just report progress at coarse milestones as init proceeds.
	loadWindow.setProgress("Starting up...");

	// not part of the subsystem list, because it should normally never be reset!
	TheNameKeyGenerator = new NameKeyGenerator;
	TheNameKeyGenerator->init();

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	// Set the current directory to the app directory.
	char buf[_MAX_PATH];
	GetModuleFileName(NULL, buf, sizeof(buf));
	char *pEnd = buf + strlen(buf);
	while (pEnd != buf) {
		if (*pEnd == '\\') {
			*pEnd = 0;
			break;
		}
		pEnd--;
	}
	::SetCurrentDirectory(buf);

	loadWindow.setProgress("Initializing file system...");
	TheFileSystem = new FileSystem;

	initSubsystem(TheLocalFileSystem, (LocalFileSystem*)new Win32LocalFileSystem);
	initSubsystem(TheArchiveFileSystem, (ArchiveFileSystem*)new Win32BIGFileSystem);

	// Just for kicks, get the HINSTANCE that WOL would need
	// if we were going to use it, which we aren't.
	ApplicationHInstance = AfxGetInstanceHandle();

	INI ini;

	initSubsystem(TheWritableGlobalData, new GlobalData(), "Data\\INI\\Default\\GameData.ini", "Data\\INI\\GameData.ini");
	
#if defined(_DEBUG) || defined(_INTERNAL)
	ini.load( AsciiString( "Data\\INI\\GameDataDebug.ini" ), INI_LOAD_MULTIFILE, NULL );
#endif

#ifdef DEBUG_CRASHING
	TheWritableGlobalData->m_debugIgnoreAsserts = false;
#endif

#if defined(_INTERNAL)
	// leave on asserts for a while. jba. [4/15/2003] TheWritableGlobalData->m_debugIgnoreAsserts = true;
#endif
	DEBUG_LOG(("TheWritableGlobalData %x\n", TheWritableGlobalData));
#if 1
	// srj sez: put INI into our user data folder, not the ap dir
	free((void*)m_pszProfileName);
	strcpy(buf, TheGlobalData->getPath_UserData().str());
	strcat(buf, "WorldBuilder.ini");
#else
	strcat(buf, "//");
	strcat(buf, m_pszProfileName);
	free((void*)m_pszProfileName);
#endif
	m_pszProfileName = (const char *)malloc(strlen(buf)+2);
	strcpy((char*)m_pszProfileName, buf);

	// ensure the user maps dir exists
	sprintf(buf, "%sMaps\\", TheGlobalData->getPath_UserData().str());
	CreateDirectory(buf, NULL);

	// read the water settings from INI (must do prior to initing GameClient, apparently)
	ini.load( AsciiString( "Data\\INI\\Default\\Water.ini" ), INI_LOAD_OVERWRITE, NULL );
	ini.load( AsciiString( "Data\\INI\\Water.ini" ), INI_LOAD_OVERWRITE, NULL );

	loadWindow.setProgress("Loading game data...");
	initSubsystem(TheGameText, CreateGameTextInterface());
	initSubsystem(TheScienceStore, new ScienceStore(), "Data\\INI\\Default\\Science.ini", "Data\\INI\\Science.ini");
	initSubsystem(TheMultiplayerSettings, new MultiplayerSettings(), "Data\\INI\\Default\\Multiplayer.ini", "Data\\INI\\Multiplayer.ini");
	initSubsystem(TheTerrainTypes, new TerrainTypeCollection(), "Data\\INI\\Default\\Terrain.ini", "Data\\INI\\Terrain.ini");
	initSubsystem(TheTerrainRoads, new TerrainRoadCollection(), "Data\\INI\\Default\\Roads.ini", "Data\\INI\\Roads.ini");

	WorldHeightMapEdit::init();

	initSubsystem(TheScriptEngine, (ScriptEngine*)(new ScriptEngine()));

	TheScriptEngine->turnBreezeOff(); // stop the tree sway.

	//  [2/11/2003]
	ini.load( AsciiString( "Data\\Scripts\\Scripts.ini" ), INI_LOAD_OVERWRITE, NULL );

	// need this before TheAudio in case we're running off of CD - TheAudio can try to open Music.big on the CD...
	initSubsystem(TheCDManager, CreateCDManager(), NULL);
	initSubsystem(TheAudio, (AudioManager*)new MilesAudioManager());
	if (!TheAudio->isMusicAlreadyLoaded())
		return FALSE;

	initSubsystem(TheVideoPlayer, (VideoPlayerInterface*)(new VideoPlayer()));
	initSubsystem(TheModuleFactory, (ModuleFactory*)(new W3DModuleFactory()));
	initSubsystem(TheSidesList, new SidesList());
	initSubsystem(TheCaveSystem, new CaveSystem());
	initSubsystem(TheRankInfoStore, new RankInfoStore(), NULL, "Data\\INI\\Rank.ini");
	initSubsystem(ThePlayerTemplateStore, new PlayerTemplateStore(), "Data\\INI\\Default\\PlayerTemplate.ini", "Data\\INI\\PlayerTemplate.ini");
	initSubsystem(TheSpecialPowerStore, new SpecialPowerStore(), "Data\\INI\\Default\\SpecialPower.ini", "Data\\INI\\SpecialPower.ini" );
	initSubsystem(TheParticleSystemManager, (ParticleSystemManager*)(new W3DParticleSystemManager()));
	initSubsystem(TheFXListStore, new FXListStore(), "Data\\INI\\Default\\FXList.ini", "Data\\INI\\FXList.ini");
	initSubsystem(TheWeaponStore, new WeaponStore(), NULL, "Data\\INI\\Weapon.ini");
	initSubsystem(TheObjectCreationListStore, new ObjectCreationListStore(), "Data\\INI\\Default\\ObjectCreationList.ini", "Data\\INI\\ObjectCreationList.ini");
	initSubsystem(TheLocomotorStore, new LocomotorStore(), NULL, "Data\\INI\\Locomotor.ini");
	initSubsystem(TheDamageFXStore, new DamageFXStore(), NULL, "Data\\INI\\DamageFX.ini");
	initSubsystem(TheArmorStore, new ArmorStore(), NULL, "Data\\INI\\Armor.ini");
	loadWindow.setProgress("Loading objects...");
	initSubsystem(TheThingFactory, new ThingFactory(), "Data\\INI\\Default\\Object.ini", NULL, "Data\\INI\\Object");
	initSubsystem(TheCrateSystem, new CrateSystem(), "Data\\INI\\Default\\Crate.ini", "Data\\INI\\Crate.ini");
	initSubsystem(TheUpgradeCenter, new UpgradeCenter, "Data\\INI\\Default\\Upgrade.ini", "Data\\INI\\Upgrade.ini");
	initSubsystem(TheAnim2DCollection, new Anim2DCollection ); //Init's itself.

	loadWindow.setProgress("Finalizing...");
	TheSubsystemListRecord.postProcessLoadAll();

	TheW3DFileSystem = new WB_W3DFileSystem;

	// Just to be sure - wb doesn't do well with half res terrain.
	DEBUG_ASSERTCRASH(!TheGlobalData->m_useHalfHeightMap, ("TheGlobalData->m_useHalfHeightMap : Don't use this setting in WB."));
	TheWritableGlobalData->m_useHalfHeightMap = false;

#if defined(_DEBUG) || defined(_INTERNAL)
	// WB never uses the shroud.
	TheWritableGlobalData->m_shroudOn = FALSE;
#endif

	TheWritableGlobalData->m_isWorldBuilder = TRUE;

	// Change the registry key under which our settings are stored.
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization.
	//SetRegistryKey(_T("Local AppWizard-Generated Applications"));

	LoadStdProfileSettings(10);  // Load standard INI file options (including MRU)

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.
 
	m_3dtemplate = new CSingleDocTemplate(
		IDR_MAPDOC,
		RUNTIME_CLASS(CWorldBuilderDoc),
		RUNTIME_CLASS(CWB3dFrameWnd), 
		RUNTIME_CLASS(WbView3d));

	AddDocTemplate(m_3dtemplate);

#ifdef RTS_HAS_QT
	// Stage 3: bring up Qt (QApplication + theme) BEFORE ProcessShellCommand opens any
	// command-line / double-clicked map. That map's validation runs the missing-unit and
	// missing-texture pickers; with qApp already alive they take the native Qt path
	// (WBQtPickUnitDialog / WBQtTerrainModalDialog) instead of the MFC DoModal fallback.
	// Qt is pumped from inside MFC's CWinApp::Run() (QMfcApp::pluginInstance), so MFC still
	// owns the loop; the QMainWindow itself is created later, after the doc/view exist.
	WBQt_Startup();
#endif

#ifdef MDI
	CMainFrame* pMainFrame = new CMainFrame; 
	if (!pMainFrame->LoadFrame(IDR_MAPDOC)) 
		return FALSE; 
	m_pMainWnd = pMainFrame; 
#endif

	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// Dispatch commands specified on the command line
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	loadWindow.setProgress("Starting WorldBuilder...");

	// The one and only window has been initialized, so show and update it. Stage 1
	// inversion: when the Qt main window takes over as the visible top-level, the MFC
	// frame stays hidden -- the Qt block below shows it only on the legacy/fallback path.
#ifdef RTS_HAS_QT
	if (!WBQt_InversionActive())
	{
		m_pMainWnd->ShowWindow(SW_SHOW);
		m_pMainWnd->UpdateWindow();
	}
#else
	m_pMainWnd->ShowWindow(SW_SHOW);
	m_pMainWnd->UpdateWindow();
#endif

#ifdef RTS_HAS_QT
	// Stage 1 inversion: a Qt QMainWindow becomes the visible top-level (native chrome +
	// the viewport as its central widget); the MFC frame stays alive but HIDDEN as the
	// command-routing hub (menu/toolbar actions still post WM_COMMAND to it). If the
	// main window cannot be created, the legacy chrome-in-frame path below runs and
	// shows the frame as before.
	{
		WbView3d *p3d = CWorldBuilderDoc::GetActive3DView();
		CMainFrame *pFrame = CMainFrame::GetMainFrame();
		Bool inverted = false;
		if (p3d != NULL && pFrame != NULL)
		{
			Int mainLeft = GetProfileInt(MAIN_FRAME_SECTION, "Left", 100);
			Int mainTop = GetProfileInt(MAIN_FRAME_SECTION, "Top", 100);
			Int mainWidth = GetProfileInt(MAIN_FRAME_SECTION, "Width", THREE_D_VIEW_WIDTH);
			Int mainHeight = GetProfileInt(MAIN_FRAME_SECTION, "Height", THREE_D_VIEW_HEIGHT);
			Int mainMaximized = GetProfileInt(MAIN_FRAME_SECTION, "Maximized", 0);
			if (WBQt_CreateMainWindow(pFrame->GetSafeHwnd(), mainLeft, mainTop, mainWidth, mainHeight, mainMaximized))
			{
				// Chrome first (the menu/toolbar/status become native QMainWindow bars),
				// then the viewport into the central pane, then ONE show so the window
				// appears fully laid out (a single device-size cascade).
				if (WBQtChrome_InstallMenuBar(pFrame->GetSafeHwnd(), ::GetMenu(pFrame->GetSafeHwnd())))
				{
					::SetMenu(pFrame->GetSafeHwnd(), NULL);
					if (WBQtChrome_InstallToolBar())
					{
						WBQtChrome_SetMfcBarVisible(0, 0);
					}
					if (WBQtChrome_InstallStatusBar())
					{
						WBQtChrome_SetMfcBarVisible(1, 0);
					}
				}
				pFrame->m_qtViewportHost = (HWND)WBQt_HostViewport(pFrame->GetSafeHwnd(), p3d->GetSafeHwnd());
				// Seed the Qt title from the frame's composed one: the doc opened during
				// ProcessShellCommand, before the Qt window existed, so the
				// OnUpdateFrameTitle mirror had nothing to push into yet.
				{
					CString frameTitle;
					pFrame->GetWindowText(frameTitle);
					WBQt_SetMainWindowTitle((LPCTSTR)frameTitle);
				}
				WBQt_ShowMainWindow();
				// The Qt main window (the tool-window owner) now exists and the doc is loaded,
				// so reopen the Layers List / Minimap Qt windows if they were open last session
				// (deferred from CMainFrame::OnCreate, where the owner did not exist yet).
				p3d->qtRestoreStartupWindows();
				inverted = true;
			}
		}
		if (!inverted)
		{
			// Legacy/fallback: the MFC frame stays the visible top-level -- show it now
			// (the startup show above was skipped) and build the chrome column inside it.
			WBQt_DisableInversion();
			m_pMainWnd->ShowWindow(SW_SHOW);
			m_pMainWnd->UpdateWindow();
			if (p3d != NULL && pFrame != NULL)
			{
			pFrame->m_qtViewportHost = (HWND)WBQt_HostViewport(pFrame->GetSafeHwnd(), p3d->GetSafeHwnd());
			pFrame->positionQtViewportHost();
			// Tier 4a: the Qt menu bar replaces the MFC menu -- walk the live menu into
			// the chrome, then detach it. The Theme popup is appended natively by the
			// chrome, so addQtThemeMenu is skipped on this path.
			if (WBQtChrome_InstallMenuBar(pFrame->GetSafeHwnd(), ::GetMenu(pFrame->GetSafeHwnd())))
			{
				::SetMenu(pFrame->GetSafeHwnd(), NULL);
				pFrame->positionQtViewportHost();
				// Tier 4b: the Qt toolbar joins the same column; retire the MFC toolbar
				// (SetMfcBarVisible re-flows the viewport host).
				if (WBQtChrome_InstallToolBar())
				{
					WBQtChrome_SetMfcBarVisible(0, 0);
				}
				// Tier 4c: the Qt status row replaces the (hidden but still written) MFC
				// status bar; messages arrive via the WM_SETMESSAGESTRING mirror.
				if (WBQtChrome_InstallStatusBar())
				{
					WBQtChrome_SetMfcBarVisible(1, 0);
				}
			}
			else
			{
			pFrame->addQtThemeMenu();
			}
			}
		}
	}
#endif

	// Parse command line for standard shell commands, DDE, file open
//	CCommandLineInfo cmdInfo;
//	ParseCommandLine(cmdInfo);

	// Dispatch commands specified on the command line
//	if (!ProcessShellCommand(cmdInfo))
//		return FALSE;

	selectPointerTool();   

	// WB_BUILD_COMMIT is the short git hash stamped into WBBuildStamp.h at build time (included at
	// the top of this file; see CMakeLists.txt / GitCommitStamp.cmake). The message is HTML so the
	// report link is clickable in the Qt message box (WBQtMessageBox enables rich-text link handling).
	AfxMessageBox(
		"WARNING: This build of WorldBuilder is a work in progress.<br><br>"
		"This version is still in testing and may corrupt or break your map, so please make a backup before using it.<br><br>"
		"Build Version: <a href=\"https://github.com/triatomic/worldbuilderQT/commit/" WB_BUILD_COMMIT "\">" WB_BUILD_COMMIT "</a><br>"
		"If you find a bug or want to suggest a feature, please report it on our <a href=\"https://github.com/triatomic/worldbuilderQT/issues\">GitHub</a><br>",
		MB_ICONEXCLAMATION | MB_OK
	);

#ifdef RTS_HAS_QT
	if (!WBQtToast_Show("Press F11 to enter full screen", 5000, 1))
	{
#endif
	CToastDialog* pToast = new CToastDialog(
        _T("Press F11 to enter full screen"),
        5000, true);
    pToast->Create(CToastDialog::IDD);
    pToast->ShowWindow(SW_SHOWNOACTIVATE);
#ifdef RTS_HAS_QT
	}
#endif


	// Load GameDirectory
	CString gameDir = this->GetProfileString(APP_SECTION, GAME_DIR);
	m_gameDirectory = gameDir;

	if (m_gameDirectory != AsciiString("")){
		DEBUG_LOG(("gameDirectory=%s \n", m_gameDirectory.str()));
	} else {
		DEBUG_LOG(("Warning! Game directory not initialized.\n"));
	}

	CString openDir = this->GetProfileString(APP_SECTION, OPEN_FILE_DIR);
	m_currentDirectory = openDir;

	// Apply the user's saved dialog-font choice app-wide. Installed before the startup
	// About dialog so it gets the chosen font too. Read once here -> changing the combo
	// takes effect on the next launch.
	InstallDialogFontHook();

	m_bLaunchOnStartUp=::AfxGetApp()->GetProfileInt(ABOUT_SECTION, "LaunchOnStartUp", 1);
	if(m_bLaunchOnStartUp){
		OnAppAbout();
	}

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CWorldBuilderApp message handlers

BOOL CWorldBuilderApp::OnCmdMsg(UINT nID, int nCode, void* pExtra,
							AFX_CMDHANDLERINFO* pHandlerInfo)
{
	// If pHandlerInfo is NULL, then handle the message
	if (pHandlerInfo == NULL)
	{
		for (Int i=0; i<NUM_VIEW_TOOLS; i++) {
			Tool *pTool = m_tools[i];
			if (pTool==NULL) continue;
			if ((Int)nID == pTool->getToolID()) {
				if (nCode == CN_COMMAND)
				{
					// Handle WM_COMMAND message
					setActiveTool(pTool);
				}
				else if (nCode == CN_UPDATE_COMMAND_UI)
				{
					// Update UI element state
					CCmdUI *pUI = (CCmdUI*)pExtra;
					pUI->SetCheck(m_curTool == pTool?1:0);	
					pUI->Enable(true);
				}
				return TRUE;
			}
		}
	}

	// If we didn't process the command, call the base class
	// version of OnCmdMsg so the message-map can handle the message
	return CWinApp::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

//=============================================================================
// CWorldBuilderApp::selectPointerTool
//=============================================================================
/** Sets the active tool to the pointer, and clears the selection. */
//=============================================================================
void CWorldBuilderApp::selectPointerTool(void) 
{
	setActiveTool(&m_pointerTool);
	// Clear selection.
	m_pointerTool.clearSelection();
}

//=============================================================================
// CWorldBuilderApp::setActiveTool
//=============================================================================
/** Sets the active tool, and activates it after deactivating the current tool. */
//=============================================================================
void CWorldBuilderApp::setActiveTool(Tool *pNewTool) 
{
	if (m_curTool == pNewTool) {
		// same tool
		return;
	}
	if (m_selTool && m_selTool != pNewTool) {
		m_selTool->deactivate();
	}
	if (pNewTool) {
		pNewTool->activate();
	}
	m_curTool = pNewTool;
	m_selTool = pNewTool;
}

//=============================================================================
// CWorldBuilderApp::updateCurTool
//=============================================================================
/** Checks to see if any key modifiers (ctrl or alt) are pressed.  If so, 
selectes the appropriate tool, else uses the normal tool. */
//=============================================================================
void CWorldBuilderApp::updateCurTool(Bool forceHand)
{
	Tool *curTool = m_curTool;
	DEBUG_ASSERTCRASH((m_lockCurTool>=0),("oops"));
	if (!m_lockCurTool) {	 // don't change tools that are doing something.
		if (forceHand || (0x8000 & ::GetAsyncKeyState(VK_SPACE))) {
			// Space bar gives scroll hand.
			m_curTool = &m_handScrollTool;
		} else if (0x8000 & ::GetAsyncKeyState(VK_MENU)) {
			// Alt key gives eyedropper.
			m_curTool = &m_eyedropperTool;
		} else if (0x8000 & ::GetAsyncKeyState(VK_CONTROL) && m_curTool != &m_fenceTool
							 && m_curTool != &m_waveEditorTool) {
			// Control key gives pointer.  The fence and wave-editor tools use Ctrl
			// themselves (wave editor: Ctrl+drag rotates a wave), so don't hijack them.
			m_curTool = &m_pointerTool;
		} else {
			// Else the tool selected in the tool palette.
			m_curTool = m_selTool;
		}
	}
	if (curTool != m_curTool) {
		m_curTool->activate();
	}
}

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	BOOL m_bLaunchOnStartUpAbout;
	BOOL m_bShrinkHKlist;
	MapObject* m_lastSelectedObject;
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
		// No message handlers
	//}}AFX_MSG
	afx_msg void OnCenterOnSelectedButtonWP();
	afx_msg void OnCenterOnSelectedButtonObject();
	afx_msg void OnFindButtonClicked();
	afx_msg void OnExitSizeMove();
	afx_msg void OnLaunchOnStartup();
	void OnCenterOnSelected(Bool findObject); 
	afx_msg void OnRefreshQueryObject();
	afx_msg void OnOpenLinkDiscord();
	afx_msg void OnRefreshQueryWaypoint();
	afx_msg void OnDialogFontChanged();
	afx_msg void OnViewportResolutionChanged();
	afx_msg void OnWindowPosChanging(WINDOWPOS* lpwndpos);

	// Distinct width/height pairs the OS reports for the primary display, sorted; index
	// matches the IDC_VIEWPORT_RESOLUTION combo. Populated in OnInitDialog.
	void populateResolutionCombo();
	void saveViewportResolution();	///< Write the combo's current pick to ScreenCX/ScreenCY.
	enum { MAX_RESOLUTIONS = 128 };
	int m_resW[MAX_RESOLUTIONS];
	int m_resH[MAX_RESOLUTIONS];
	int m_numResolutions;

	void DoShrink();
	afx_msg void OnShrink();
	afx_msg void OnExpand();
	virtual void OnOK();
	virtual void OnClose();
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()
};

static CAboutDlg* g_pAboutDlg = NULL;

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	ON_WM_EXITSIZEMOVE()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_FIND_HKEY_BUTTON, OnFindButtonClicked)
	ON_BN_CLICKED(IDC_EXPAND, OnExpand)
	ON_BN_CLICKED(IDC_SHRINK, OnShrink)

	ON_BN_CLICKED(IDC_FIND_OBJ_BUTTON, OnCenterOnSelectedButtonObject)
	ON_BN_CLICKED(IDC_REFRESH_OBJ_BUTTON, OnRefreshQueryObject)

	ON_BN_CLICKED(IDC_OPEN_LINK_DISCORD, OnOpenLinkDiscord)

	ON_BN_CLICKED(IDC_FIND_WP_BUTTON, OnCenterOnSelectedButtonWP)
	ON_BN_CLICKED(IDC_REFRESH_WP_BUTTON, OnRefreshQueryWaypoint)

	ON_BN_CLICKED(IDC_LAUNCH_ONSTARTUP, OnLaunchOnStartup)
	ON_CBN_SELCHANGE(IDC_DIALOG_FONT, OnDialogFontChanged)
	ON_CBN_SELCHANGE(IDC_VIEWPORT_RESOLUTION, OnViewportResolutionChanged)
	ON_WM_WINDOWPOSCHANGING()
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// This can only be clicked once unless vice versa have been clicked get me?
void CAboutDlg::OnExpand() 
{
	CRect rect;
	GetWindowRect(&rect);

	const int expandBy = 350;
	const int maxWidth = 690; // Nice number

	int currentWidth = rect.Width();
	int newWidth = min(currentWidth + expandBy, maxWidth); // Clamp to maxWidth
	int delta = newWidth - currentWidth;

	// Expand the window to the left
	// MoveWindow expects coordinates relative to parent, convert
	// ScreenToClient(&rect);
	SetWindowPos(NULL, rect.left - 500, rect.top, newWidth, rect.Height(), SWP_NOZORDER | SWP_NOMOVE);

	GetDlgItem(IDC_EXPAND)->EnableWindow(FALSE);

	::AfxGetApp()->WriteProfileInt(ABOUT_SECTION, "ShrinkHotkeyList",  0);
}

void CAboutDlg::DoShrink(){
	CRect rect;
	GetWindowRect(&rect);
	int newWidth = max(rect.Width() - 350, 300); // Don't shrink below 300 width (adjust as needed)

	// ScreenToClient(&rect);
	SetWindowPos(NULL, rect.left + 350, rect.top, newWidth, rect.Height(), SWP_NOZORDER | SWP_NOMOVE);

	GetDlgItem(IDC_EXPAND)->EnableWindow(TRUE);
}

void CAboutDlg::OnShrink() 
{
	::AfxGetApp()->WriteProfileInt(ABOUT_SECTION, "ShrinkHotkeyList",  1);
	DoShrink();
}

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	m_bLaunchOnStartUpAbout = true;
	m_numResolutions = 0;
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::OnOpenLinkDiscord() 
{
	ShellExecute(NULL, "open", "https://discord.gg/tJ6zyGb", NULL, NULL, SW_SHOWNORMAL);
}

void CAboutDlg::OnRefreshQueryObject()
{
	CComboBox* pCombo = (CComboBox*)GetDlgItem(IDC_FIND_QUERY_OBJ);
	if (!pCombo) return;

	pCombo->ResetContent();

	MapObject* pMapObj = MapObject::getFirstMapObject();
	while (pMapObj)
	{
		Bool exists;
		AsciiString objName = pMapObj->getProperties()->getAsciiString(TheKey_objectName, &exists);

		if (exists && !objName.isEmpty() && !pMapObj->getFlag(FLAG_ROAD_FLAGS))
		{
			pCombo->AddString(objName.str());
		}

		pMapObj = pMapObj->getNext();
	}
}

void CAboutDlg::OnRefreshQueryWaypoint()
{
	CComboBox* pCombo = (CComboBox*)GetDlgItem(IDC_FIND_QUERY_WP);
	if (!pCombo) return;

	pCombo->ResetContent();

	MapObject* pMapObj = MapObject::getFirstMapObject();
	while (pMapObj)
	{
		if (pMapObj->isWaypoint()) {
			AsciiString wayName = pMapObj->getWaypointName();
			if (wayName.isNotEmpty() && pCombo->FindStringExact(-1, wayName.str()) == CB_ERR) {
				pCombo->AddString(wayName.str());
			}
		}

		pMapObj = pMapObj->getNext();
	}
}


void CAboutDlg::OnCenterOnSelected(Bool findObject = true) 
{
	CComboBox* pCombo = (CComboBox*)GetDlgItem(findObject ? IDC_FIND_QUERY_OBJ : IDC_FIND_QUERY_WP);
	if (!pCombo) return;

	CString selectedName;
	pCombo->GetWindowText(selectedName);

	if (selectedName.IsEmpty())
		return;

	MapObject* mapObject = MapObject::getFirstMapObject();
	while (mapObject) {
		if (findObject) {
			// Looking for a regular object by object name
			Bool exists;
			AsciiString objName = mapObject->getProperties()->getAsciiString(TheKey_objectName, &exists);

			if (exists && !objName.isEmpty() && selectedName == objName.str()) {
				const Coord3D* objectPosition = mapObject->getLocation();
				WbView3d* p3View = CWorldBuilderDoc::GetActive3DView();
				if (p3View) {
					p3View->setCenterInView(objectPosition->x / MAP_XY_FACTOR, objectPosition->y / MAP_XY_FACTOR);
				}
				return;
			}
		}
		else if (mapObject->isWaypoint()) {
			AsciiString wayName = mapObject->getWaypointName();
			if (!wayName.isEmpty() && selectedName == wayName.str()) {
				const Coord3D* objectPosition = mapObject->getLocation();
				WbView3d* p3View = CWorldBuilderDoc::GetActive3DView();
				if (p3View) {
					p3View->setCenterInView(objectPosition->x / MAP_XY_FACTOR, objectPosition->y / MAP_XY_FACTOR);
				}
				return;
			}
		}

		mapObject = mapObject->getNext();
	}
}


void CAboutDlg::OnCenterOnSelectedButtonObject() 
{
	OnCenterOnSelected();
}

void CAboutDlg::OnCenterOnSelectedButtonWP() 
{
	OnCenterOnSelected(false);
}

BOOL CAboutDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	m_bShrinkHKlist=::AfxGetApp()->GetProfileInt(ABOUT_SECTION, "ShrinkHotkeyList", 0);
	if(m_bShrinkHKlist) DoShrink();

	g_aboutPageOn = true;

	m_bLaunchOnStartUpAbout=::AfxGetApp()->GetProfileInt(ABOUT_SECTION, "LaunchOnStartUp", 1);
	CButton *pButton = (CButton*)GetDlgItem(IDC_LAUNCH_ONSTARTUP);
	pButton->SetCheck(m_bLaunchOnStartUpAbout ? 1:0);

	// Populate the dialog-font combo from the single source of truth and select the
	// saved choice. Fonts that can clip the layouts get a "(may clip)" suffix as a
	// warning; the change applies on the next launch (see DialogFont.cpp).
	CComboBox *pFontCombo = (CComboBox*)GetDlgItem(IDC_DIALOG_FONT);
	if (pFontCombo) {
		pFontCombo->ResetContent();
		for (int i = 0; i < GetDialogFontChoiceCount(); ++i) {
			const DialogFontChoice &fc = GetDialogFontChoice(i);
			CString label = fc.displayName;
			if (fc.mayClip)
				label += " (may clip)";
			pFontCombo->AddString(label);
		}
		pFontCombo->SetCurSel(LoadDialogFontChoice());
	}

	// Populate the viewport-resolution combo from the OS-reported display modes and
	// preselect the saved ScreenCX/ScreenCY.
	populateResolutionCombo();

    // Load the icon for the app about
    HWND hWnd = GetSafeHwnd();
    HICON hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_MAINFRAME));
    SetClassLong(hWnd, GCL_HICON, (LONG)hIcon);
    SetClassLong(hWnd, GCL_HICONSM, (LONG)hIcon);
	
	OnRefreshQueryObject();
	OnRefreshQueryWaypoint();

	return TRUE;  // return TRUE unless you set the focus to a control
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

void CAboutDlg::OnLaunchOnStartup()
{
	CButton *pButton = (CButton*)GetDlgItem(IDC_LAUNCH_ONSTARTUP);
	m_bLaunchOnStartUpAbout = (pButton->GetCheck() == 1);
	::AfxGetApp()->WriteProfileInt(ABOUT_SECTION, "LaunchOnStartUp", m_bLaunchOnStartUpAbout ? 1 : 0);
}

// Enumerate the distinct resolutions the OS reports for the primary display (via
// EnumDisplaySettings), fill the combo, and preselect the saved ScreenCX/ScreenCY. If
// the saved size isn't one the monitor advertises (e.g. a hand-edited INI), it's added
// so the user still sees their current value rather than a blank/wrong selection.
void CAboutDlg::populateResolutionCombo()
{
	CComboBox *pCombo = (CComboBox*)GetDlgItem(IDC_VIEWPORT_RESOLUTION);
	if (!pCombo)
		return;

	m_numResolutions = 0;

	// Walk all display modes for the current adapter; keep each distinct WxH once. Modes
	// come sorted-ish but we insert in ascending (W,H) order and dedup explicitly.
	DEVMODE dm;
	memset(&dm, 0, sizeof(dm));
	dm.dmSize = sizeof(dm);
	for (int iMode = 0; ::EnumDisplaySettings(NULL, iMode, &dm); ++iMode)
	{
		int w = (int)dm.dmPelsWidth;
		int h = (int)dm.dmPelsHeight;
		if (w <= 0 || h <= 0)
			continue;

		// Find the sorted insert position; skip if already present.
		int pos = 0;
		Bool dup = false;
		while (pos < m_numResolutions)
		{
			if (m_resW[pos] == w && m_resH[pos] == h) { dup = true; break; }
			if (m_resW[pos] > w || (m_resW[pos] == w && m_resH[pos] > h)) break;
			++pos;
		}
		if (dup || m_numResolutions >= MAX_RESOLUTIONS)
			continue;

		for (int k = m_numResolutions; k > pos; --k) { m_resW[k] = m_resW[k-1]; m_resH[k] = m_resH[k-1]; }
		m_resW[pos] = w;
		m_resH[pos] = h;
		++m_numResolutions;
	}

	// Read the currently-saved viewport resolution. 0 = unset.
	int savedCX = ::AfxGetApp()->GetProfileInt(VIEWPORT_SECTION, KEY_VIEW_WIDTH, 0);
	int savedCY = ::AfxGetApp()->GetProfileInt(VIEWPORT_SECTION, KEY_VIEW_HEIGHT, 0);

	// If the saved size isn't among the enumerated modes, insert it so it's selectable.
	if (savedCX > 0 && savedCY > 0)
	{
		Bool found = false;
		for (int i = 0; i < m_numResolutions; ++i)
			if (m_resW[i] == savedCX && m_resH[i] == savedCY) { found = true; break; }
		if (!found && m_numResolutions < MAX_RESOLUTIONS)
		{
			m_resW[m_numResolutions] = savedCX;
			m_resH[m_numResolutions] = savedCY;
			++m_numResolutions;
		}
	}

	pCombo->ResetContent();
	int selIndex = -1;
	for (int i = 0; i < m_numResolutions; ++i)
	{
		CString label;
		label.Format("%d x %d", m_resW[i], m_resH[i]);
		pCombo->AddString(label);
		if (m_resW[i] == savedCX && m_resH[i] == savedCY)
			selIndex = i;
	}
	if (selIndex >= 0)
		pCombo->SetCurSel(selIndex);
}

// Write the combo's current pick to [MainFrame] Width/Height and apply it live the same way
// the View > resolution menu does: adjustWindowSize(forcedResolution=true) resizes the
// frame window and the 3D render target together (Yves' coupled model -- the back buffer is
// stretched to fill the window, no aspect distortion since they share the same size).
void CAboutDlg::saveViewportResolution()
{
	CComboBox *pCombo = (CComboBox*)GetDlgItem(IDC_VIEWPORT_RESOLUTION);
	if (!pCombo)
		return;
	int sel = pCombo->GetCurSel();
	if (sel == CB_ERR || sel < 0 || sel >= m_numResolutions)
		return;
	::AfxGetApp()->WriteProfileInt(VIEWPORT_SECTION, KEY_VIEW_WIDTH,  m_resW[sel]);
	::AfxGetApp()->WriteProfileInt(VIEWPORT_SECTION, KEY_VIEW_HEIGHT, m_resH[sel]);

	// Apply live (forcedResolution=true uses the values we just wrote).
	if (CMainFrame::GetMainFrame())
		CMainFrame::GetMainFrame()->adjustWindowSize(true, false);
}

void CAboutDlg::OnViewportResolutionChanged()
{
	saveViewportResolution();
}

void CAboutDlg::OnDialogFontChanged()
{
	CComboBox *pFontCombo = (CComboBox*)GetDlgItem(IDC_DIALOG_FONT);
	if (!pFontCombo)
		return;
	int sel = pFontCombo->GetCurSel();
	if (sel == CB_ERR)
		return;
	// Persist the choice; it takes effect on the next launch (the font hook reads
	// the INI once at startup). The static "(applies on restart)" label tells the user.
	SaveDialogFontChoice(sel);
}

void CAboutDlg::OnWindowPosChanging(WINDOWPOS* lpwndpos)
{
    if (IsIconic())
    {
        // Stop the OS from repositioning the minimized window
        lpwndpos->flags |= SWP_NOMOVE;
    }

    CDialog::OnWindowPosChanging(lpwndpos);
}


// Persist the dialog position once the user finishes moving it. WM_MOVE fires on every
// pixel of the drag (hundreds of INI writes); WM_EXITSIZEMOVE fires once, on release.
void CAboutDlg::OnExitSizeMove()
{
	CDialog::OnExitSizeMove();

	if (this->IsWindowVisible() && !this->IsIconic()) {
		CRect frameRect;
		GetWindowRect(&frameRect);
		::AfxGetApp()->WriteProfileInt(ABOUT_SECTION, "Top", frameRect.top);
		::AfxGetApp()->WriteProfileInt(ABOUT_SECTION, "Left", frameRect.left);
	}
}

void CAboutDlg::OnOK() 
{
    // Check if the IDC_OBJECT_SEARCH_EDIT control is focused
    if (GetFocus() == GetDlgItem(IDC_FIND_QUERY) || GetFocus() == GetDlgItem(IDC_HOTKEYLIST) )
    {
        OnFindButtonClicked();  // Trigger search on "Enter" key press
    }
    // else
    // {
    //     CDialog::OnOK();  // Call the default OK behavior if the search box isn't focused
    // }
}

void CAboutDlg::OnClose()
{
	g_aboutPageOn = false;
	CDialog::OnClose();
}

void CAboutDlg::OnFindButtonClicked()
{
	CEdit* pFindBox = (CEdit*)GetDlgItem(IDC_FIND_QUERY);
	CEdit* pHotkeyList = (CEdit*)GetDlgItem(IDC_HOTKEYLIST);
	if (!pFindBox || !pHotkeyList) return;

	CString query, content;
	pFindBox->GetWindowText(query);
	pHotkeyList->GetWindowText(content);

	if (query.IsEmpty())
	{
		MessageBeep(MB_ICONEXCLAMATION);
		pHotkeyList->SetSel(-1, -1); // Clear selection
		return;
	}

	// Case-insensitive search
	CString contentLower = content;
	CString queryLower = query;
	contentLower.MakeLower();
	queryLower.MakeLower();

	int startChar = 0, endChar = 0;
	pHotkeyList->GetSel(startChar, endChar);

	// Start search just after current selection
	int pos = contentLower.Find(queryLower, endChar);

	// Wrap around if not found
	if (pos == -1 && endChar > 0)
		pos = contentLower.Find(queryLower, 0);

	if (pos != -1)
	{
		pHotkeyList->SetFocus();
		pHotkeyList->SetSel(pos, pos + query.GetLength());
		// GetDlgItem(IDC_FIND_QUERY)->SetFocus();
	}
	else
	{
		MessageBeep(MB_ICONEXCLAMATION);
		pHotkeyList->SetSel(-1, -1); // Clear selection if not found
		// GetDlgItem(IDC_FIND_QUERY)->SetFocus();
	}
}


// This is supposed to be called upon under a new opened map
void CWorldBuilderApp::OnRefreshAppAbout()
{
	if(!g_aboutPageOn) return; //  if we are not even on then dont do a refresh...

	static CAboutDlg* pAboutDlg = NULL;

	if (pAboutDlg != NULL) {
		if (pAboutDlg->GetSafeHwnd()) {
			pAboutDlg->DestroyWindow();
		}
		delete pAboutDlg;
		pAboutDlg = NULL;
	}

	g_aboutPageOn = true;

	pAboutDlg = new CAboutDlg;

	if (pAboutDlg->Create(IDD_ABOUTBOX, AfxGetMainWnd()))
	{
		int top  = ::AfxGetApp()->GetProfileInt(ABOUT_SECTION, "Top", -1);
		int left = ::AfxGetApp()->GetProfileInt(ABOUT_SECTION, "Left", -1);

		if (top != -1 && left != -1)
		{
			CRect rect;
			pAboutDlg->GetWindowRect(&rect);
			int width  = rect.Width();
			int height = rect.Height();

			pAboutDlg->SetWindowPos(NULL, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
		}

		pAboutDlg->ShowWindow(SW_SHOW);
	}
	else
	{
		delete pAboutDlg;
		pAboutDlg = NULL;
	}
}

// App command to run the dialog
void CWorldBuilderApp::OnAppAbout()
{
#ifdef RTS_HAS_QT
	if (WBQtEntityFinder_Open(::AfxGetMainWnd() ? ::AfxGetMainWnd()->GetSafeHwnd() : NULL))
	{
		return;
	}
#endif
	if (g_aboutPageOn) {
		return;
	}
	g_aboutPageOn = true;

	g_pAboutDlg = new CAboutDlg;
	CAboutDlg* pAboutDlg = g_pAboutDlg;

	if (pAboutDlg->Create(IDD_ABOUTBOX, AfxGetMainWnd()))
	{
		int top  = ::AfxGetApp()->GetProfileInt(ABOUT_SECTION, "Top", -1);
		int left = ::AfxGetApp()->GetProfileInt(ABOUT_SECTION, "Left", -1);

		if (top != -1 && left != -1)
		{
			CRect rect;
			pAboutDlg->GetWindowRect(&rect);
			int width  = rect.Width();
			int height = rect.Height();

			pAboutDlg->SetWindowPos(NULL, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
		}

		pAboutDlg->ShowWindow(SW_SHOW);
	}
	else
	{
		delete pAboutDlg;
	}

	CEdit* pEdit = (CEdit*)pAboutDlg->GetDlgItem(IDC_HOTKEYLIST);

	// NOTE: this table is mirrored in src/WBQtEntityFinderBridge.cpp (the Qt Entity
	// Finder) -- keep the two in sync.
	struct HotkeyEntry
	{
		const char* key;
		const char* description;
	};
	
	HotkeyEntry hotkeys[] =
	{
		{ "Tab", "Lock Horizontal" },
		{ "Q", "Brush Tool" },
		{ "W", "Add Brush" },
		{ "E", "Subtract Brush" },
		{ "R", "Feather Tool" },
		{ "T", "Mold Tool  " }, // artificially spaced it out -- theres a bug with tabs
		{ "Y", "Water Tool" },
		{ "A", "Tile Tool    " }, // artificially spaced it out -- theres a bug with tabs
		{ "S", "Big Tile Tool" },
		{ "D", "Tile Flood Fill" },
		{ "F", "Auto Edge Out Tool" },
		{ "G", "Blend Edge Tool" },
		{ "Z", "Place Object Tool" },
		{ "X", "Road Tool" },
		{ "C", "Grove Tool" },
		{ "V", "Ramp Tool" },
		{ "B", "Scorch Tool" },
		{ "N", "Fence Tool" },
		{ "M", "Build List Tool" },
		{ "F1", "Waypoint Tool" },
		{ "F2", "Polygon Tool" },
		{ "F3", "Border Tool" },
		{ "F4", "Script Editor" },
		{ "F5", "Team Editor" },

		{ "==", "===============================" },

		{ "Ctrl+M", "Select Similar" },
		{ "Ctrl+R", "Replace Selected..." },

		{ "Ctrl+W", "Show Wireframe 3D View" },
		{ "Ctrl+F", "Show From Top Down View" },
		{ "Ctrl+U", "Show Clouds" }, 
		{ "Ctrl+D", "Change Time Of Day" },
		{ "Ctrl+I", "Show Impassable Areas" },
		{ "Ctrl+A", "Show All of 3D Map" },
		{ "Ctrl+Shift+G", "Snap To Grid" },

		{ "Ctrl+Q", "Show Ruler Grid"},
		{ "Ctrl+E", "Show Water"},

		{ "==", "===============================" },

		{ "Alt+1", "Show Objects Icons" },
		{ "Alt+2", "Show Waypoints" },
		{ "Alt+3", "Show Polygon Triggers" },
		{ "Alt+4", "Show Labels" },
		{ "Alt+5", "Show Models" },
		{ "Alt+6", "Show Bounding Boxes" },
		{ "Alt+7", "Show Sight Ranges" },
		{ "Alt+8", "Show Weapon Ranges" },
		{ "Alt+9", "Show Map Boundaries" },
		{ "Alt+0", "Use Fixed Colored Waypoints" },

		{ "==", "===============================" },

		{ "Ctrl+0", "Select Anything" },
        { "Ctrl+1", "Select Buildings" },
        { "Ctrl+2", "Select Infantry" },
        { "Ctrl+3", "Select Vehicles" },
        { "Ctrl+4", "Select Shrubbery" },
        { "Ctrl+5", "Select Props" },
        { "Ctrl+6", "Select Natural" },
        { "Ctrl+7", "Select Debris / Scorch" },
        { "Ctrl+8", "Select Waypoints & Areas" },
        { "Ctrl+9", "Select Roads / Bridges" },

		{ "==", "===============================" },

		{ "Ctrl+Z", "Undo Mode  " },
		{ "Ctrl+X", "Cut Mode   " }, // artificially spaced it out -- theres a bug with tabs
		{ "Ctrl+C", "Copy Mode  " },
		{ "Ctrl+V", "Paste Mode " }, // artificially spaced it out -- theres a bug with tabs
		{ "Ctrl+N", "New File   " },
		{ "Ctrl+O", "Open File" },
		{ "Ctrl+S", "Save File" },
		
	};
	
	CString text;
	int numHotkeys = sizeof(hotkeys) / sizeof(hotkeys[0]);
	const int maxFirstColumnLength = 17;

	for (int i = 0; i < numHotkeys; i++)
	{
		// Check for separator
		if (strcmp(hotkeys[i].key, "==") == 0)
		{
			CString separatorLine;
			separatorLine.Format("%s\t%s", hotkeys[i].key, hotkeys[i].description);
			text += separatorLine + NEWLINE;
			continue;
		}

		CString firstEntry;
		firstEntry.Format("%s\t%s", hotkeys[i].key, hotkeys[i].description);

		// Look ahead to check if next entry is a separator or too long
		bool isLast = (i + 1 >= numHotkeys);
		bool nextIsSeparator = !isLast && strcmp(hotkeys[i + 1].key, "==") == 0;

		if (strlen(hotkeys[i].description) >= maxFirstColumnLength || isLast || nextIsSeparator)
		{
			text += firstEntry + "\t" + NEWLINE;
		}
		else
		{
			CString secondEntry;
			secondEntry.Format("%s\t%s", hotkeys[i + 1].key, hotkeys[i + 1].description);
			text += firstEntry + "\t" + secondEntry + "\t" + NEWLINE;
			i++; // Skip the next one since it's already used
		}
	}
	
	pEdit->SetWindowText(text);	
}


/////////////////////////////////////////////////////////////////////////////
// CWorldBuilderApp message handlers

#ifdef RTS_HAS_QT
LRESULT CWorldBuilderApp::ProcessWndProcException(CException *e, const MSG *pMsg)
{
	char name[128];
	name[0] = 0;
	if (e != NULL && e->GetRuntimeClass() != NULL && e->GetRuntimeClass()->m_lpszClassName != NULL)
	{
		strncpy(name, e->GetRuntimeClass()->m_lpszClassName, sizeof(name) - 1);
		name[sizeof(name) - 1] = 0;
	}
	char desc[256];
	desc[0] = 0;
	if (e != NULL)
	{
		e->GetErrorMessage(desc, sizeof(desc));
	}
	DEBUG_LOG(("ProcessWndProcException: '%s' (%s) during msg 0x%x wParam=0x%x\n",
		name, desc, pMsg ? pMsg->message : 0, pMsg ? (unsigned)pMsg->wParam : 0));
	return CWinApp::ProcessWndProcException(e, pMsg);
}

BOOL CWorldBuilderApp::PreTranslateMessage(MSG *pMsg)
{
	// The WBQtShortcuts table owns the WorldBuilder hotkeys once the window is inverted
	// (WBQtShortcuts_TranslateKey no-ops when not inverted, so the accelerator table
	// keeps working on the fallback path). A hit is consumed here; everything else falls
	// through to the base, which still does tooltip relay etc.
	if (WBQtShortcuts_TranslateKey(pMsg))
	{
		return TRUE;
	}
	// When a floating Qt tool window (e.g. an Object Properties text field) owns focus, skip
	// the base PreTranslate for key/char messages: it would TranslateAccelerator the still-loaded
	// MFC accel table (Ctrl+C/V/X/Z ...) and steal those keys from the focused QLineEdit, firing
	// the object command instead. Returning FALSE lets the message dispatch straight to Qt.
	if ((pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN
			|| pMsg->message == WM_CHAR || pMsg->message == WM_KEYUP || pMsg->message == WM_SYSKEYUP)
		&& WBQtShortcuts_QtToolWindowOwnsFocus())
	{
		return FALSE;
	}
	return CWinApp::PreTranslateMessage(pMsg);
}

int CWorldBuilderApp::DoMessageBox(LPCTSTR lpszPrompt, UINT nType, UINT nIDPrompt)
{
	// Parented Qt box when the main window is up; else the MFC default (returns 0).
	int rc = WBQtMessageBox_Show(lpszPrompt, NULL, nType);
	if (rc != 0)
	{
		return rc;
	}
	return CWinApp::DoMessageBox(lpszPrompt, nType, nIDPrompt);
}
#endif

int CWorldBuilderApp::ExitInstance()
{
#ifdef RTS_HAS_QT
	// Tear down Qt FIRST (it came up last). Must be explicit: the app's global dtor
	// calls _exit(0) right after this returns, so static/atexit teardown never runs.
	WBQt_Shutdown();
#endif

	RemoveDialogFontHook();

	// Join and destroy the parallel worker pool before tearing down subsystems,
	// so no worker outlives the data it reads.
	WBParallel::shutdown();

	WriteProfileString(APP_SECTION, OPEN_FILE_DIR, m_currentDirectory.str());
	m_currentDirectory.clear();

	ScriptList::reset();

	TheSubsystemListRecord.shutdownAll();

	WorldHeightMapEdit::shutdown();

	delete TheFileSystem;
	TheFileSystem = NULL;

	delete TheW3DFileSystem;
	TheW3DFileSystem = NULL;

	delete TheNameKeyGenerator;
	TheNameKeyGenerator = NULL;

#ifdef MEMORYPOOL_CHECKPOINTING
	Int lastCP = TheMemoryPoolFactory->debugSetCheckpoint();
#endif
#ifdef MEMORYPOOL_CHECKPOINTING
	TheMemoryPoolFactory->debugMemoryReport(REPORT_FACTORYINFO | REPORT_CP_LEAKS | REPORT_CP_STACKTRACE, gFirstCP, lastCP);
#endif
	#ifdef MEMORYPOOL_DEBUG
		TheMemoryPoolFactory->debugMemoryReport(REPORT_POOLINFO | REPORT_POOL_OVERFLOW | REPORT_SIMPLE_LEAKS, 0, 0);
	#endif
	shutdownMemoryManager();
	DEBUG_SHUTDOWN();

	return CWinApp::ExitInstance();
}

void CWorldBuilderApp::OnResetWindows() 
{
	if (CMainFrame::GetMainFrame()) {
		CMainFrame::GetMainFrame()->ResetWindowPositions();
	}
	
	int top  = 50;
	int left = 50;

	// Reset saved About position
	::AfxGetApp()->WriteProfileInt(ABOUT_SECTION, "Top", top);
	::AfxGetApp()->WriteProfileInt(ABOUT_SECTION, "Left", left);

#ifdef RTS_HAS_QT
	WBQtEntityFinder_MoveTo(left, top);
#endif
	// Force move About dialog if open
	if (g_pAboutDlg && ::IsWindow(g_pAboutDlg->m_hWnd) && g_aboutPageOn) {
		g_pAboutDlg->SetWindowPos(NULL, left, top, 0, 0,
			SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
	}
}

void CWorldBuilderApp::OnFileOpen() 
{
#ifdef DO_MAPS_IN_DIRECTORIES
#ifdef RTS_HAS_QT
	{
		char qtFilename[_MAX_PATH];
		int qtBrowse = 0;
		if (WBQtOpenMap_Run(::AfxGetMainWnd()->GetSafeHwnd(), qtFilename, sizeof(qtFilename), &qtBrowse) != 0)
		{
			if (!qtBrowse)
			{
				OpenDocumentFile(qtFilename);
				return;
			}
		}
		else
		{
			// cancelled so return.
			return;
		}
	}
#else
	TOpenMapInfo info;
	OpenMap mapDlg(&info);
	if (mapDlg.DoModal() == IDOK) {
		if (!info.browse) {
			OpenDocumentFile(info.filename);
			return;
		}
	}	else {
		// cancelled so return.
		return;
	}
#endif
#endif

	CFileStatus status;
	if (m_currentDirectory != AsciiString("")) try {
		if (CFile::GetStatus(m_currentDirectory.str(), status)) {
			if (status.m_attribute & CFile::directory) {
				::SetCurrentDirectory(m_currentDirectory.str());
			}
		}
	} catch(...) {}

	CWinApp::OnFileOpen();
}

void CWorldBuilderApp::OnTexturesizingMapclifftextures() 
{
	setActiveTool(&m_floodFillTool);
	m_floodFillTool.setAdjustCliffs(true);
	
}

void CWorldBuilderApp::OnUpdateTexturesizingMapclifftextures(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	
}
