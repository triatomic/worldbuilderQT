// WBQtChrome.h -- the Qt main-window chrome controller (Tier 4a-1: the menu bar). One
// instance lives for the process life once WBQtChrome_InstallMenuBar succeeds. It owns the
// QMenuBar inserted into the Phase-2 viewport-host column, refreshes command states on
// aboutToShow, rebuilds the File menu's MRU section, and runs the popup focus dance (see
// WBQtChrome.cpp for why that is menu-critical, not polish).
#ifndef WB_QT_CHROME_H
#define WB_QT_CHROME_H

#include <QIcon>
#include <QImage>
#include <QList>
#include <QObject>

class QAction;
class QEvent;
class QLabel;
class QMenu;
class QMenuBar;
class QTimer;
class QToolBar;
class QWidget;

class WBQtChromeController : public QObject
{
	Q_OBJECT
public:
	WBQtChromeController(QWidget *host, void *frameHwnd, void *hMenuBar);

	static WBQtChromeController *instance() { return s_instance; }

	bool popupActive() const { return m_openPopups > 0; }

	bool activateMenuByMnemonic(int letter);

	bool installToolBar();
	bool installStatusBar();

	// The WM_SETMESSAGESTRING push target: every frame status message (incl. the
	// per-mouse-move coordinate readout) lands here with zero polling.
	void setStatusText(const QString &text);

protected:
	bool eventFilter(QObject *obj, QEvent *event);

private slots:
	void onActionTriggered();
	void onMenuAboutToShow();
	void onMenuAboutToHide();
	void onMenuHovered(QAction *action);
	void checkPopupsClosed();
	void onToolActionHovered();
	void onToolBarTick();
	void onPaletteChanged();

private:
	void buildMenu(void *hMenu, QMenu *target);
	void addThemeMenu();
	QIcon standardToolIcon(int id) const;
	void applyStandardToolIcons();
	// Tier 4b (dark-mode icons): load a 16x15-cell toolbar strip bitmap (RGB(192,192,192)
	// keyed transparent) by resource id; re-slice the tool buttons from the theme's strip.
	QImage loadToolbarStrip(int resId, int &iconW, int &iconH) const;
	void applyToolbarStrip();
	QAction *makeCommandAction(QMenu *menu, const QString &text, int commandId);
	void refreshMenuState(QMenu *menu);
	void rebuildMruSection();

	static WBQtChromeController *s_instance;

	QWidget *m_host;
	void *m_frameHwnd;
	QMenuBar *m_menuBar;
	int m_openPopups;
	void *m_savedFocus;			// HWND holding focus before the first popup opened
	QMenu *m_mruMenu;			// the popup holding the MRU placeholder (File)
	QAction *m_mruPlaceholder;	// the resource's grayed "Recent File" item
	QList<QAction *> m_mruActions;
	QToolBar *m_toolBar;		// Tier 4b; NULL until installToolBar
	QTimer *m_toolBarTimer;		// sweeps button states + the key-lock indicators
	QWidget *m_statusRow;		// Tier 4c; NULL until installStatusBar
	QLabel *m_statusLabel;
	QLabel *m_capsLabel;
	QLabel *m_numLabel;
	QLabel *m_scrlLabel;
};

#endif // WB_QT_CHROME_H
