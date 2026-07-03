// WBQtMainWindow.h -- the stage-1 inversion: the Qt QMainWindow that IS WorldBuilder's
// visible top-level window. It owns the chrome (menu bar / toolbar / status bar, installed
// by WBQtChrome) and hosts the D3D8 viewport as its central widget (the QWinHost pane built
// by WBQtBridge). The MFC CWB3dFrameWnd stays alive but HIDDEN as the command-routing hub:
// every menu/toolbar action still posts WM_COMMAND to it, and closing this window routes a
// WM_CLOSE to it so the untouched MFC save-prompt/teardown path runs.
#ifndef WB_QT_MAIN_WINDOW_H
#define WB_QT_MAIN_WINDOW_H

#include <QMainWindow>

class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QMoveEvent;
class QResizeEvent;
class QTimer;

class WBQtMainWindow : public QMainWindow
{
	Q_OBJECT
public:
	explicit WBQtMainWindow(void *frameHwnd);

	static WBQtMainWindow *instance() { return s_instance; }

	// F11 / Esc fullscreen -- replaces the MFC frame's style-strip Enter/ExitFullScreen
	// (which would now act on a hidden window). Chrome stays visible, caption goes away.
	void toggleFullscreen();
	bool fullscreenActive() const { return isFullScreen(); }

protected:
	// Route the close to the hidden MFC frame: its untouched CFrameWnd::OnClose runs the
	// SaveModified prompt and the whole teardown chain (OnDestroy -> unhost -> WM_QUIT).
	// If the user cancels the save prompt the frame survives and we simply stay open.
	virtual void closeEvent(QCloseEvent *e);
	virtual void moveEvent(QMoveEvent *e);
	virtual void resizeEvent(QResizeEvent *e);
	virtual void dragEnterEvent(QDragEnterEvent *e);
	virtual void dropEvent(QDropEvent *e);

private slots:
	void savePlacement();

private:
	static WBQtMainWindow *s_instance;

	void *m_frameHwnd;
	QTimer *m_placementTimer;	// debounces the [MainFrame] Top/Left/Width/Height writes
};

#endif // WB_QT_MAIN_WINDOW_H
