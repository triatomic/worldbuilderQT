// WBQtChrome.h -- the Qt main-window chrome controller (Tier 4a-1: the menu bar). One
// instance lives for the process life once WBQtChrome_InstallMenuBar succeeds. It owns the
// QMenuBar inserted into the Phase-2 viewport-host column, refreshes command states on
// aboutToShow, rebuilds the File menu's MRU section, and runs the popup focus dance (see
// WBQtChrome.cpp for why that is menu-critical, not polish).
#ifndef WB_QT_CHROME_H
#define WB_QT_CHROME_H

#include <QObject>
#include <QList>

class QAction;
class QMenu;
class QMenuBar;
class QWidget;

class WBQtChromeController : public QObject
{
	Q_OBJECT
public:
	WBQtChromeController(QWidget *host, void *frameHwnd, void *hMenuBar);

	static WBQtChromeController *instance() { return s_instance; }

	bool popupActive() const { return m_openPopups > 0; }

	bool activateMenuByMnemonic(int letter);

private slots:
	void onActionTriggered();
	void onMenuAboutToShow();
	void onMenuAboutToHide();
	void onMenuHovered(QAction *action);
	void checkPopupsClosed();

private:
	void buildMenu(void *hMenu, QMenu *target);
	void addThemeMenu();
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
};

#endif // WB_QT_CHROME_H
