// WBQtChrome.cpp -- see WBQtChrome.h. The Qt menu bar is built by walking the frame's
// live HMENU (labels keep their '&' mnemonics and "\tCtrl+O" hints, which QMenu renders
// as the right-aligned shortcut column), inserted into the Phase-2 viewport-host column
// via QLayout::setMenuBar. Commands are delivered as posted WM_COMMANDs; enable/check
// state is pulled through WBQtChromeData_QueryCommand on every popup aboutToShow.
//
// THE FOCUS DANCE (menu-critical): Qt popups open without activation, so Win32 keyboard
// focus stays on the D3D view while a menu is up -- the frame's PreTranslateMessage would
// translate Q/W/E into tool switches and the arrow keys would never reach the menu. So on
// the first aboutToShow we move focus to the chrome host HWND (keys then dispatch into Qt,
// which routes them to the active popup) and restore it when the last popup closes; the
// frame side has the matching WBQtChrome_PopupActive() guard.
#include "WBQtChromeBridge.h"
#include "WBQtChrome.h"

#include <qt_windows.h>

#include <QAction>
#include <QApplication>
#include <QLayout>
#include <QMenu>
#include <QMenuBar>
#include <QString>
#include <QTimer>

#include "resource.h"		// ID_QTTHEME_* (pure #defines; res is on the qt include path)
#include "WBQtTheme.h"

#include <string.h>

// MFC-standard command IDs (afxres.h values; afx headers cannot be included here).
#define WBQT_ID_FILE_MRU_FIRST	0xE110	// == ID_FILE_MRU_FILE1
#define WBQT_ID_FILE_MRU_LAST	0xE11F	// == ID_FILE_MRU_FILE16
#define WBQT_ID_VIEW_TOOLBAR	0xE800	// == ID_VIEW_TOOLBAR
#define WBQT_ID_VIEW_STATUS_BAR	0xE801	// == ID_VIEW_STATUS_BAR

// Defined in WBQtBridge.cpp: the Phase-2 viewport-host column the chrome inserts into.
QWidget *WBQt_GetViewportHostWidget(void);

WBQtChromeController *WBQtChromeController::s_instance = NULL;

WBQtChromeController::WBQtChromeController(QWidget *host, void *frameHwnd, void *hMenuBar)
	: QObject(host),
	m_host(host),
	m_frameHwnd(frameHwnd),
	m_menuBar(NULL),
	m_openPopups(0),
	m_savedFocus(NULL),
	m_mruMenu(NULL),
	m_mruPlaceholder(NULL)
{
	s_instance = this;

	m_menuBar = new QMenuBar(host);

	// The bar level of IDR_MAPDOC is all popups; walk each into a QMenu.
	HMENU hBar = reinterpret_cast<HMENU>(hMenuBar);
	int count = ::GetMenuItemCount(hBar);
	for (int i = 0; i < count; i++)
	{
		char text[512];
		text[0] = 0;
		MENUITEMINFOA info;
		memset(&info, 0, sizeof(info));
		info.cbSize = sizeof(info);
		info.fMask = MIIM_FTYPE | MIIM_ID | MIIM_SUBMENU | MIIM_STRING;
		info.dwTypeData = text;
		info.cch = sizeof(text) - 1;
		if (!::GetMenuItemInfoA(hBar, (UINT)i, TRUE, &info) || info.hSubMenu == NULL)
		{
			continue;
		}
		QMenu *menu = m_menuBar->addMenu(QString::fromLocal8Bit(text));
		connect(menu, SIGNAL(aboutToShow()), this, SLOT(onMenuAboutToShow()));
		connect(menu, SIGNAL(aboutToHide()), this, SLOT(onMenuAboutToHide()));
		connect(menu, SIGNAL(hovered(QAction*)), this, SLOT(onMenuHovered(QAction*)));
		buildMenu(info.hSubMenu, menu);
	}

	// == addQtThemeMenu, native: the IDs route to CMainFrame's existing ON_COMMAND_RANGE /
	// update handlers, so these are ordinary command actions.
	addThemeMenu();

	// setMenuBar (not addWidget) so the bar gets menu-bar sizing above the layout's rows.
	host->layout()->setMenuBar(m_menuBar);

	// Tier 4a-2: with the client chrome now Qt, theme the frame's native caption too
	// (applied now and on every theme switch).
	WBQtTheme::registerNativeTopLevel(frameHwnd);
}

// Tier 4a-2: Alt+letter from the frame -- open the top-level menu whose '&' mnemonic
// matches. Returns false when no menu matches (the frame then lets the key fall through).
bool WBQtChromeController::activateMenuByMnemonic(int letter)
{
	QList<QAction *> actions = m_menuBar->actions();
	for (int i = 0; i < actions.size(); i++)
	{
		QString text = actions[i]->text();
		for (int c = 0; c + 1 < text.length(); c++)
		{
			if (text[c] != QChar('&'))
			{
				continue;
			}
			if (text[c + 1] == QChar('&'))
			{
				c++;
				continue;
			}
			if (text[c + 1].toUpper().unicode() == letter)
			{
				// Opens the popup; the aboutToShow focus dance then routes the keyboard.
				m_menuBar->setActiveAction(actions[i]);
				return true;
			}
			break;
		}
	}
	return false;
}

void WBQtChromeController::buildMenu(void *hMenuVoid, QMenu *target)
{
	HMENU hMenu = reinterpret_cast<HMENU>(hMenuVoid);
	int count = ::GetMenuItemCount(hMenu);
	for (int i = 0; i < count; i++)
	{
		char text[512];
		text[0] = 0;
		MENUITEMINFOA info;
		memset(&info, 0, sizeof(info));
		info.cbSize = sizeof(info);
		info.fMask = MIIM_FTYPE | MIIM_ID | MIIM_SUBMENU | MIIM_STRING;
		info.dwTypeData = text;
		info.cch = sizeof(text) - 1;
		if (!::GetMenuItemInfoA(hMenu, (UINT)i, TRUE, &info))
		{
			continue;
		}
		if ((info.fType & MFT_SEPARATOR) != 0)
		{
			target->addSeparator();
			continue;
		}
		QString label = QString::fromLocal8Bit(text);
		if (info.hSubMenu != NULL)
		{
			QMenu *sub = target->addMenu(label);
			connect(sub, SIGNAL(aboutToShow()), this, SLOT(onMenuAboutToShow()));
			connect(sub, SIGNAL(aboutToHide()), this, SLOT(onMenuAboutToHide()));
			connect(sub, SIGNAL(hovered(QAction*)), this, SLOT(onMenuHovered(QAction*)));
			buildMenu(info.hSubMenu, sub);
			continue;
		}
		int id = (int)info.wID;
		if (id >= WBQT_ID_FILE_MRU_FIRST && id <= WBQT_ID_FILE_MRU_LAST)
		{
			if (id == WBQT_ID_FILE_MRU_FIRST)
			{
				// The resource's grayed "Recent File" placeholder; the real entries are
				// rebuilt from the MFC CRecentFileList on every File aboutToShow.
				m_mruPlaceholder = target->addAction(label);
				m_mruPlaceholder->setEnabled(false);
				m_mruMenu = target;
			}
			continue;
		}
		makeCommandAction(target, label, id);
	}
}

void WBQtChromeController::addThemeMenu()
{
	QMenu *theme = m_menuBar->addMenu("&Theme");
	connect(theme, SIGNAL(aboutToShow()), this, SLOT(onMenuAboutToShow()));
	connect(theme, SIGNAL(aboutToHide()), this, SLOT(onMenuAboutToHide()));
	connect(theme, SIGNAL(hovered(QAction*)), this, SLOT(onMenuHovered(QAction*)));
	makeCommandAction(theme, "&System (follow Windows)", ID_QTTHEME_SYSTEM);
	makeCommandAction(theme, "&Dark", ID_QTTHEME_DARK);
	makeCommandAction(theme, "&Light", ID_QTTHEME_LIGHT);
}

QAction *WBQtChromeController::makeCommandAction(QMenu *menu, const QString &text, int commandId)
{
	QAction *action = menu->addAction(text);
	action->setData(commandId);
	connect(action, SIGNAL(triggered()), this, SLOT(onActionTriggered()));
	return action;
}

void WBQtChromeController::onActionTriggered()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (action == NULL)
	{
		return;
	}
	bool ok = false;
	int id = action->data().toInt(&ok);
	if (!ok || id == 0)
	{
		return;
	}
	if (id == WBQT_ID_VIEW_TOOLBAR || id == WBQT_ID_VIEW_STATUS_BAR)
	{
		// CFrameWnd's built-in would un-hide the MFC bar behind the Qt chrome; toggle it
		// deliberately (the MFC bars are still the real toolbar/status bar until 4b/4c)
		// and re-flow the viewport host.
		WBQtChrome_ToggleMfcBar((id == WBQT_ID_VIEW_STATUS_BAR) ? 1 : 0);
		return;
	}
	// Same delivery as a native menu: a posted WM_COMMAND dispatched by the frame after
	// the menu unwinds; MFC routes it frame -> active view -> doc -> app.
	::PostMessage(reinterpret_cast<HWND>(m_frameHwnd), WM_COMMAND, MAKEWPARAM(id, 0), 0);
}

void WBQtChromeController::onMenuAboutToShow()
{
	if (m_openPopups == 0)
	{
		m_savedFocus = reinterpret_cast<void *>(::GetFocus());
		::SetFocus(reinterpret_cast<HWND>(m_host->winId()));
	}
	m_openPopups++;
	QMenu *menu = qobject_cast<QMenu *>(sender());
	if (menu != NULL)
	{
		if (menu == m_mruMenu)
		{
			rebuildMruSection();
		}
		refreshMenuState(menu);
	}
}

void WBQtChromeController::onMenuAboutToHide()
{
	m_openPopups--;
	// Sibling-menu navigation fires hide/show pairs; defer so the count settles before
	// deciding the menu session is over.
	QTimer::singleShot(0, this, SLOT(checkPopupsClosed()));
}

void WBQtChromeController::checkPopupsClosed()
{
	if (m_openPopups > 0)
	{
		return;
	}
	m_openPopups = 0;
	HWND prev = reinterpret_cast<HWND>(m_savedFocus);
	m_savedFocus = NULL;
	if (prev != NULL && ::IsWindow(prev))
	{
		::SetFocus(prev);
	}
	WBQtChrome_SetFrameStatusText("");
}

void WBQtChromeController::onMenuHovered(QAction *action)
{
	char prompt[512];
	prompt[0] = 0;
	if (action != NULL)
	{
		bool ok = false;
		int id = action->data().toInt(&ok);
		if (ok && id != 0)
		{
			WBQtChromeData_GetPrompt(id, prompt, sizeof(prompt));
		}
	}
	WBQtChrome_SetFrameStatusText(prompt);
}

void WBQtChromeController::refreshMenuState(QMenu *menu)
{
	QList<QAction *> actions = menu->actions();
	for (int i = 0; i < actions.size(); i++)
	{
		QAction *action = actions[i];
		if (action->isSeparator() || action->menu() != NULL)
		{
			continue;
		}
		bool ok = false;
		int id = action->data().toInt(&ok);
		if (!ok || id == 0)
		{
			continue;
		}
		if (id == WBQT_ID_VIEW_TOOLBAR || id == WBQT_ID_VIEW_STATUS_BAR)
		{
			action->setCheckable(true);
			action->setChecked(WBQtChrome_IsMfcBarVisible((id == WBQT_ID_VIEW_STATUS_BAR) ? 1 : 0) != 0);
			continue;
		}
		if (id >= WBQT_ID_FILE_MRU_FIRST && id <= WBQT_ID_FILE_MRU_LAST)
		{
			continue;	// CWinApp's MRU update handler drives a real menu; never query it
		}
		int enabled = 1;
		int checked = -1;
		if (WBQtChromeData_QueryCommand(id, &enabled, &checked))
		{
			action->setEnabled(enabled != 0);
			if (checked >= 0)
			{
				action->setCheckable(true);
				action->setChecked(checked != 0);
			}
		}
	}
}

void WBQtChromeController::rebuildMruSection()
{
	if (m_mruMenu == NULL || m_mruPlaceholder == NULL)
	{
		return;
	}
	for (int i = 0; i < m_mruActions.size(); i++)
	{
		m_mruMenu->removeAction(m_mruActions[i]);
		delete m_mruActions[i];
	}
	m_mruActions.clear();

	int count = WBQtChromeData_GetMruCount();
	int added = 0;
	for (int i = 0; i < count && i < 16; i++)
	{
		char path[1024];
		path[0] = 0;
		if (WBQtChromeData_GetMruPath(i, path, sizeof(path)) == 0 || path[0] == 0)
		{
			continue;
		}
		// data = the ACTUAL list index (CWinApp::OnOpenRecentFile opens by index), the
		// label number is just the visible ordinal.
		QAction *action = new QAction(QString("&%1 %2").arg(added + 1).arg(QString::fromLocal8Bit(path)), m_mruMenu);
		action->setData(WBQT_ID_FILE_MRU_FIRST + i);
		connect(action, SIGNAL(triggered()), this, SLOT(onActionTriggered()));
		m_mruMenu->insertAction(m_mruPlaceholder, action);
		m_mruActions.append(action);
		added++;
	}
	// == MFC: the grayed placeholder shows only while the list is empty.
	m_mruPlaceholder->setVisible(added == 0);
}

// ===================== the C entry points =====================

extern "C" int WBQtChrome_InstallMenuBar(void *frameHwnd, void *hMenuBar)
{
	if (qApp == NULL || frameHwnd == NULL || hMenuBar == NULL)
	{
		return 0;
	}
	if (WBQtChromeController::instance() != NULL)
	{
		return 1;
	}
	QWidget *host = WBQt_GetViewportHostWidget();
	if (host == NULL || host->layout() == NULL)
	{
		return 0;
	}
	new WBQtChromeController(host, frameHwnd, hMenuBar);
	return 1;
}

extern "C" int WBQtChrome_IsInstalled(void)
{
	return (WBQtChromeController::instance() != NULL) ? 1 : 0;
}

extern "C" int WBQtChrome_PopupActive(void)
{
	WBQtChromeController *chrome = WBQtChromeController::instance();
	return (chrome != NULL && chrome->popupActive()) ? 1 : 0;
}

extern "C" int WBQtChrome_ActivateMenu(int letter)
{
	WBQtChromeController *chrome = WBQtChromeController::instance();
	return (chrome != NULL && chrome->activateMenuByMnemonic(letter)) ? 1 : 0;
}
