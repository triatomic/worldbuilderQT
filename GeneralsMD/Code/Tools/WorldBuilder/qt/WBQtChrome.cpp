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
#include <QBoxLayout>
#include <QEvent>
#include <QImage>
#include <QLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPixmap>
#include <QStatusBar>
#include <QString>
#include <QStyle>
#include <QTimer>
#include <QToolBar>

#include "resource.h"		// ID_QTTHEME_* (pure #defines; res is on the qt include path)
#include "WBQtTheme.h"

#include <string.h>

// MFC-standard command IDs (afxres.h values; afx headers cannot be included here).
#define WBQT_ID_FILE_MRU_FIRST	0xE110	// == ID_FILE_MRU_FILE1
#define WBQT_ID_FILE_MRU_LAST	0xE11F	// == ID_FILE_MRU_FILE16
#define WBQT_ID_VIEW_TOOLBAR	0xE800	// == ID_VIEW_TOOLBAR
#define WBQT_ID_VIEW_STATUS_BAR	0xE801	// == ID_VIEW_STATUS_BAR
#define WBQT_ID_FILE_NEW		0xE100	// == ID_FILE_NEW
#define WBQT_ID_FILE_OPEN		0xE101	// == ID_FILE_OPEN
#define WBQT_ID_FILE_SAVE		0xE103	// == ID_FILE_SAVE
#define WBQT_ID_EDIT_COPY		0xE122	// == ID_EDIT_COPY
#define WBQT_ID_EDIT_CUT		0xE123	// == ID_EDIT_CUT
#define WBQT_ID_EDIT_PASTE		0xE125	// == ID_EDIT_PASTE

// Defined in WBQtBridge.cpp: the Phase-2 viewport-host column the chrome inserts into,
// and the hosted 3D view's HWND (for putting keyboard focus back after toolbar clicks).
QWidget *WBQt_GetViewportHostWidget(void);
void *WBQt_GetHostedViewWindow(void);

WBQtChromeController *WBQtChromeController::s_instance = NULL;

WBQtChromeController::WBQtChromeController(QWidget *host, void *frameHwnd, void *hMenuBar)
	: QObject(host),
	m_host(host),
	m_frameHwnd(frameHwnd),
	m_menuBar(NULL),
	m_openPopups(0),
	m_savedFocus(NULL),
	m_mruMenu(NULL),
	m_mruPlaceholder(NULL),
	m_toolBar(NULL),
	m_toolBarTimer(NULL),
	m_statusRow(NULL),
	m_statusLabel(NULL),
	m_capsLabel(NULL),
	m_numLabel(NULL),
	m_scrlLabel(NULL)
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

	QMainWindow *mainWin = qobject_cast<QMainWindow *>(host);
	if (mainWin != NULL)
	{
		// Stage 1 inverted: the host IS the top-level QMainWindow -- native menu bar. Its
		// caption is themed by the app-wide event filter like every Qt top-level, so no
		// registerNativeTopLevel here.
		mainWin->setMenuBar(m_menuBar);
	}
	else
	{
		// Legacy: setMenuBar (not addWidget) so the bar gets menu-bar sizing above the
		// layout's rows.
		host->layout()->setMenuBar(m_menuBar);

		// Tier 4a-2: with the client chrome now Qt, theme the frame's native caption too
		// (applied now and on every theme switch).
		WBQtTheme::registerNativeTopLevel(frameHwnd);
	}
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

// Tier 4b: build the Qt toolbar from the exe's own IDR_MAINFRAME TOOLBAR resource and its
// bitmap strip -- exactly the two pieces MFC's LoadToolBar consumed.
bool WBQtChromeController::installToolBar()
{
	if (m_toolBar != NULL)
	{
		return true;
	}

	// RT_TOOLBAR (241, the MFC resource type): WORD version(1)/width/height/count then
	// ids[] (0 = a separator). The bitmap strip holds one width x height image per
	// NON-separator id, in order.
	HINSTANCE inst = ::GetModuleHandleA(NULL);
	HRSRC res = ::FindResourceA(inst, MAKEINTRESOURCEA(IDR_MAINFRAME), MAKEINTRESOURCEA(241));
	if (res == NULL)
	{
		return false;
	}
	HGLOBAL global = ::LoadResource(inst, res);
	if (global == NULL)
	{
		return false;
	}
	const WORD *data = reinterpret_cast<const WORD *>(::LockResource(global));
	if (data == NULL || data[0] != 1)
	{
		return false;
	}
	int iconW = data[1];
	int iconH = data[2];
	int itemCount = data[3];
	const WORD *ids = data + 4;
	if (iconW <= 0 || iconH <= 0 || itemCount <= 0)
	{
		return false;
	}

	// The strip, converted to 32bpp so the source depth doesn't matter. MFC's
	// AfxLoadSysColorBitmap treats RGB(192,192,192) as the button face -- transparent here.
	QImage strip;
	{
		HBITMAP hbm = (HBITMAP)::LoadImageA(inst, MAKEINTRESOURCEA(IDR_MAINFRAME),
			IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
		if (hbm == NULL)
		{
			return false;
		}
		BITMAP bm;
		memset(&bm, 0, sizeof(bm));
		::GetObjectA(hbm, sizeof(bm), &bm);
		QImage img(bm.bmWidth, bm.bmHeight, QImage::Format_ARGB32);
		BITMAPINFO bi;
		memset(&bi, 0, sizeof(bi));
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = bm.bmWidth;
		bi.bmiHeader.biHeight = -bm.bmHeight;	// top-down rows, matching QImage
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;
		HDC dc = ::GetDC(NULL);
		int got = ::GetDIBits(dc, hbm, 0, bm.bmHeight, img.bits(), &bi, DIB_RGB_COLORS);
		::ReleaseDC(NULL, dc);
		::DeleteObject(hbm);
		if (got != bm.bmHeight)
		{
			return false;
		}
		for (int y = 0; y < img.height(); y++)
		{
			QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
			for (int x = 0; x < img.width(); x++)
			{
				if ((line[x] & 0x00FFFFFF) == 0x00C0C0C0)
				{
					line[x] = 0;
				}
				else
				{
					line[x] |= 0xFF000000;
				}
			}
		}
		strip = img;
	}

	m_toolBar = new QToolBar(m_host);
	m_toolBar->setMovable(false);
	m_toolBar->setIconSize(QSize(iconW, iconH));
	m_toolBar->installEventFilter(this);	// Leave -> clear the flyby text

	int imageIndex = 0;
	for (int i = 0; i < itemCount; i++)
	{
		int id = (int)ids[i];
		if (id == 0)
		{
			m_toolBar->addSeparator();
			continue;
		}
		QIcon icon(QPixmap::fromImage(strip.copy(imageIndex * iconW, 0, iconW, iconH)));
		imageIndex++;
		QAction *action = m_toolBar->addAction(icon, QString());
		action->setData(id);
		action->setProperty("wbToolButton", true);
		char text[512];
		text[0] = 0;
		if (WBQtChromeData_GetTooltip(id, text, sizeof(text)))
		{
			action->setToolTip(QString::fromLocal8Bit(text));
		}
		connect(action, SIGNAL(triggered()), this, SLOT(onActionTriggered()));
		connect(action, SIGNAL(hovered()), this, SLOT(onToolActionHovered()));
	}

	// Inverted: a native QMainWindow toolbar row. Legacy: above the viewport pane; the
	// menu bar sits higher still (QLayout::setMenuBar).
	QMainWindow *mainWin = qobject_cast<QMainWindow *>(m_host);
	if (mainWin != NULL)
	{
		mainWin->addToolBar(m_toolBar);
	}
	else
	{
		QBoxLayout *box = qobject_cast<QBoxLayout *>(m_host->layout());
		if (box != NULL)
		{
			box->insertWidget(0, m_toolBar);
		}
	}

	// The six standard file/edit buttons swap their dated strip images for Qt-native
	// icons; re-derived on theme switches (the glyphs paint in the palette text colour).
	applyStandardToolIcons();
	connect(qApp, SIGNAL(paletteChanged(QPalette)), this, SLOT(onPaletteChanged()));

	// The active-tool highlight changes without any menu opening, so sweep the button
	// states on a modest timer -- the same trivial update handlers MFC ran on idle.
	m_toolBarTimer = new QTimer(this);
	m_toolBarTimer->setInterval(250);
	connect(m_toolBarTimer, SIGNAL(timeout()), this, SLOT(onToolBarTick()));
	m_toolBarTimer->start();
	return true;
}

// Qt-native icons for New/Open/Save/Cut/Copy/Paste. The first three come from the style's
// standard set; cut/copy/paste have no QStyle standard icon on Windows, so they are drawn
// as flat glyphs in the button-text colour (theme-aware). Everything is rendered for the
// toolbar's icon size so the six match the rest of the strip.
QIcon WBQtChromeController::standardToolIcon(int id) const
{
	QStyle *style = QApplication::style();
	switch (id)
	{
		case WBQT_ID_FILE_NEW:
			return style->standardIcon(QStyle::SP_FileIcon);
		case WBQT_ID_FILE_OPEN:
			return style->standardIcon(QStyle::SP_DirOpenIcon);
		case WBQT_ID_FILE_SAVE:
			return style->standardIcon(QStyle::SP_DialogSaveButton);
	}

	// Drawn at 2x the toolbar icon size for crisper antialiased downscaling.
	QSize iconSize = (m_toolBar != NULL) ? m_toolBar->iconSize() : QSize(16, 15);
	QPixmap pm(iconSize * 2);
	pm.fill(Qt::transparent);
	{
		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing, true);
		QColor ink = QApplication::palette().color(QPalette::ButtonText);
		QPen pen(ink);
		pen.setWidthF(2.0);
		p.setPen(pen);
		// Coordinates assume the nominal 32x30 canvas (16x15 * 2); scale for safety.
		p.scale(pm.width() / 32.0, pm.height() / 30.0);
		if (id == WBQT_ID_EDIT_CUT)
		{
			// Scissors: crossed blades into two finger rings.
			p.drawLine(QPointF(9, 3), QPointF(22, 20));
			p.drawLine(QPointF(23, 3), QPointF(10, 20));
			p.setBrush(Qt::NoBrush);
			p.drawEllipse(QRectF(5, 20, 8, 7));
			p.drawEllipse(QRectF(19, 20, 8, 7));
		}
		else if (id == WBQT_ID_EDIT_COPY)
		{
			// Two offset pages.
			p.setBrush(Qt::NoBrush);
			p.drawRect(QRectF(6, 3, 13, 17));
			p.setBrush(QApplication::palette().color(QPalette::Button));
			p.drawRect(QRectF(13, 10, 13, 17));
		}
		else if (id == WBQT_ID_EDIT_PASTE)
		{
			// Clipboard: board, top clip, two content lines.
			p.setBrush(Qt::NoBrush);
			p.drawRoundedRect(QRectF(7, 4, 18, 23), 2, 2);
			p.setBrush(ink);
			p.drawRect(QRectF(12, 1, 8, 6));
			p.drawLine(QPointF(11, 14), QPointF(21, 14));
			p.drawLine(QPointF(11, 19), QPointF(21, 19));
		}
	}
	return QIcon(pm);
}

void WBQtChromeController::applyStandardToolIcons()
{
	if (m_toolBar == NULL)
	{
		return;
	}
	QList<QAction *> actions = m_toolBar->actions();
	for (int i = 0; i < actions.size(); i++)
	{
		bool ok = false;
		int id = actions[i]->data().toInt(&ok);
		if (!ok)
		{
			continue;
		}
		if (id == WBQT_ID_FILE_NEW || id == WBQT_ID_FILE_OPEN || id == WBQT_ID_FILE_SAVE
			|| id == WBQT_ID_EDIT_CUT || id == WBQT_ID_EDIT_COPY || id == WBQT_ID_EDIT_PASTE)
		{
			actions[i]->setIcon(standardToolIcon(id));
		}
	}
}

void WBQtChromeController::onPaletteChanged()
{
	applyStandardToolIcons();
}

void WBQtChromeController::onToolBarTick()
{
	// Tier 4c: the key-lock indicators ride the same tick.
	if (m_capsLabel != NULL)
	{
		m_capsLabel->setEnabled((::GetKeyState(VK_CAPITAL) & 1) != 0);
		m_numLabel->setEnabled((::GetKeyState(VK_NUMLOCK) & 1) != 0);
		m_scrlLabel->setEnabled((::GetKeyState(VK_SCROLL) & 1) != 0);
	}
	if (m_toolBar == NULL || !m_toolBar->isVisible())
	{
		return;
	}
	QList<QAction *> actions = m_toolBar->actions();
	for (int i = 0; i < actions.size(); i++)
	{
		QAction *action = actions[i];
		if (action->isSeparator())
		{
			continue;
		}
		bool ok = false;
		int id = action->data().toInt(&ok);
		if (!ok || id == 0)
		{
			continue;
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

void WBQtChromeController::onToolActionHovered()
{
	onMenuHovered(qobject_cast<QAction *>(sender()));
}

// Tier 4c: the status row -- message label + the classic CAP/NUM/SCRL indicators. The
// message text is PUSHED from CMainFrame's WM_SETMESSAGESTRING override (every
// SetMessageText writer, including the per-mouse-move readout, with zero polling).
bool WBQtChromeController::installStatusBar()
{
	if (m_statusRow != NULL)
	{
		return true;
	}
	QMainWindow *mainWin = qobject_cast<QMainWindow *>(m_host);
	if (mainWin != NULL)
	{
		// Inverted: the native QMainWindow status bar carries the same label set (message
		// stretches left, key-lock indicators pinned right as permanent widgets). Keeping
		// m_statusRow pointed at the QStatusBar (a QWidget) preserves the View > Status Bar
		// visibility toggle unchanged.
		QStatusBar *bar = mainWin->statusBar();
		bar->setSizeGripEnabled(true);
		m_statusRow = bar;
		m_statusLabel = new QLabel(bar);
		bar->addWidget(m_statusLabel, 1);
		m_capsLabel = new QLabel("CAP", bar);
		bar->addPermanentWidget(m_capsLabel);
		m_numLabel = new QLabel("NUM", bar);
		bar->addPermanentWidget(m_numLabel);
		m_scrlLabel = new QLabel("SCRL", bar);
		bar->addPermanentWidget(m_scrlLabel);
	}
	else
	{
	m_statusRow = new QWidget(m_host);
	// Cap the row at its natural one-line height: the column's leftover vertical space
	// must all go to the viewport pane, not get split with this row (which otherwise
	// stretches into a giant status bar -- QWidget/QLabel default to growable).
	m_statusRow->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	QHBoxLayout *row = new QHBoxLayout(m_statusRow);
	row->setContentsMargins(6, 2, 6, 2);
	row->setSpacing(10);
	m_statusLabel = new QLabel(m_statusRow);
	row->addWidget(m_statusLabel, 1);
	m_capsLabel = new QLabel("CAP", m_statusRow);
	row->addWidget(m_capsLabel);
	m_numLabel = new QLabel("NUM", m_statusRow);
	row->addWidget(m_numLabel);
	m_scrlLabel = new QLabel("SCRL", m_statusRow);
	row->addWidget(m_scrlLabel);

	QBoxLayout *box = qobject_cast<QBoxLayout *>(m_host->layout());
	if (box != NULL)
	{
		box->addWidget(m_statusRow);	// below the viewport pane
	}
	}

	// The indicators ride the sweep timer; create it here if the toolbar didn't.
	if (m_toolBarTimer == NULL)
	{
		m_toolBarTimer = new QTimer(this);
		m_toolBarTimer->setInterval(250);
		connect(m_toolBarTimer, SIGNAL(timeout()), this, SLOT(onToolBarTick()));
		m_toolBarTimer->start();
	}
	return true;
}

void WBQtChromeController::setStatusText(const QString &text)
{
	if (m_statusLabel != NULL)
	{
		m_statusLabel->setText(text);
	}
}

bool WBQtChromeController::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == m_toolBar && event->type() == QEvent::Leave)
	{
		WBQtChrome_SetFrameStatusText("");
	}
	return QObject::eventFilter(obj, event);
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
	if (id == WBQT_ID_VIEW_TOOLBAR)
	{
		// CFrameWnd's built-in would un-hide the MFC bar behind the Qt chrome. Toggle
		// the Qt toolbar once it exists (4b), the still-real MFC one before that.
		if (m_toolBar != NULL)
		{
			m_toolBar->setVisible(!m_toolBar->isVisible());
		}
		else
		{
			WBQtChrome_ToggleMfcBar(0);
		}
		return;
	}
	if (id == WBQT_ID_VIEW_STATUS_BAR)
	{
		if (m_statusRow != NULL)
		{
			m_statusRow->setVisible(!m_statusRow->isVisible());
		}
		else
		{
			WBQtChrome_ToggleMfcBar(1);
		}
		return;
	}
	// Same delivery as a native menu: a posted WM_COMMAND dispatched by the frame after
	// the menu unwinds; MFC routes it frame -> active view -> doc -> app.
	::PostMessage(reinterpret_cast<HWND>(m_frameHwnd), WM_COMMAND, MAKEWPARAM(id, 0), 0);
	if (action->property("wbToolButton").toBool())
	{
		// Toolbar clicks: put the keyboard back on the 3D view so the single-key tool
		// hotkeys and WbView key handling keep flowing without an extra viewport click.
		void *view = WBQt_GetHostedViewWindow();
		if (view != NULL)
		{
			::SetFocus(reinterpret_cast<HWND>(view));
		}
	}
	// Refresh the pressed/enabled states promptly after the (posted) command lands,
	// instead of waiting out the sweep timer.
	QTimer::singleShot(120, this, SLOT(onToolBarTick()));
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
		if (id == WBQT_ID_VIEW_TOOLBAR)
		{
			action->setCheckable(true);
			action->setChecked((m_toolBar != NULL) ? m_toolBar->isVisible()
				: (WBQtChrome_IsMfcBarVisible(0) != 0));
			continue;
		}
		if (id == WBQT_ID_VIEW_STATUS_BAR)
		{
			action->setCheckable(true);
			action->setChecked((m_statusRow != NULL) ? m_statusRow->isVisible()
				: (WBQtChrome_IsMfcBarVisible(1) != 0));
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

extern "C" int WBQtChrome_InstallToolBar(void)
{
	WBQtChromeController *chrome = WBQtChromeController::instance();
	return (chrome != NULL && chrome->installToolBar()) ? 1 : 0;
}

extern "C" int WBQtChrome_InstallStatusBar(void)
{
	WBQtChromeController *chrome = WBQtChromeController::instance();
	return (chrome != NULL && chrome->installStatusBar()) ? 1 : 0;
}

extern "C" void WBQtChrome_SetStatusText(const char *text)
{
	WBQtChromeController *chrome = WBQtChromeController::instance();
	if (chrome != NULL)
	{
		chrome->setStatusText(QString::fromLocal8Bit((text != NULL) ? text : ""));
	}
}
