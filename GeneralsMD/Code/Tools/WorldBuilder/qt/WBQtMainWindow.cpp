// WBQtMainWindow.cpp -- see WBQtMainWindow.h. Geometry persistence keeps writing the same
// WorldBuilder.ini [MainFrame] Top/Left/Width/Height keys the MFC frame used (via the
// reverse callback WBQt_SaveMainWindowPlacement defined in src/WBQtHostBridge.cpp), so the
// INI stays the single store and the Qt-OFF build reads/writes the same values.
#include "WBQtMainWindow.h"

#include <qt_windows.h>

#include "WBQtBridge.h"
#include "WBQtToast.h"

#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QTimer>
#include <QUrl>

WBQtMainWindow *WBQtMainWindow::s_instance = NULL;

WBQtMainWindow::WBQtMainWindow(void *frameHwnd)
	: QMainWindow(NULL),
	m_frameHwnd(frameHwnd),
	m_placementTimer(NULL)
{
	s_instance = this;

	setObjectName("wbMainWindow");
	setWindowTitle("WorldBuilder");
	setAcceptDrops(true);

	// Debounced placement save: fires 500ms after the last move/resize, skipping
	// fullscreen/maximized states so the stored geometry stays the restorable one.
	m_placementTimer = new QTimer(this);
	m_placementTimer->setSingleShot(true);
	m_placementTimer->setInterval(500);
	connect(m_placementTimer, SIGNAL(timeout()), this, SLOT(savePlacement()));
}

void WBQtMainWindow::toggleFullscreen()
{
	if (isFullScreen())
	{
		showNormal();
	}
	else
	{
		showFullScreen();
		WBQtToast_Show("Press F11 or Escape to exit full screen", 20000, 1);
	}
}

void WBQtMainWindow::closeEvent(QCloseEvent *e)
{
	// Flush the placement store now -- a move/resize/maximize inside the 500ms debounce
	// window would otherwise be lost on exit.
	if (m_placementTimer != NULL)
	{
		m_placementTimer->stop();
	}
	savePlacement();
	// Never close directly: the hidden MFC frame owns the document lifecycle. POST (not
	// send) so the close runs from the message loop, not from inside Qt event delivery.
	e->ignore();
	if (m_frameHwnd != NULL && ::IsWindow(reinterpret_cast<HWND>(m_frameHwnd)))
	{
		::PostMessage(reinterpret_cast<HWND>(m_frameHwnd), WM_CLOSE, 0, 0);
	}
}

void WBQtMainWindow::moveEvent(QMoveEvent *e)
{
	QMainWindow::moveEvent(e);
	if (m_placementTimer != NULL)
	{
		m_placementTimer->start();
	}
}

void WBQtMainWindow::resizeEvent(QResizeEvent *e)
{
	QMainWindow::resizeEvent(e);
	if (m_placementTimer != NULL)
	{
		m_placementTimer->start();
	}
}

void WBQtMainWindow::savePlacement()
{
	if (isFullScreen() || isMinimized() || !isVisible())
	{
		return;
	}
	// The maximized flag persists separately; while maximized keep the stored NORMAL
	// geometry (it is what un-maximize and the next non-maximized launch restore to).
	WBQt_SaveMainWindowMaximized(isMaximized() ? 1 : 0);
	if (isMaximized())
	{
		return;
	}
	// frameGeometry() left/top pairs with the QWidget::move() used at restore; width()/
	// height() pair with resize(). Saving what we restore keeps the round-trip stable.
	WBQt_SaveMainWindowPlacement(frameGeometry().left(), frameGeometry().top(),
		width(), height());
}

void WBQtMainWindow::dragEnterEvent(QDragEnterEvent *e)
{
	// Accept a drag carrying at least one .map file -- the same filter the MFC frame's
	// OnDropFiles applied.
	if (e->mimeData()->hasUrls())
	{
		QList<QUrl> urls = e->mimeData()->urls();
		for (int i = 0; i < urls.size(); i++)
		{
			if (urls[i].isLocalFile()
				&& urls[i].toLocalFile().endsWith(".map", Qt::CaseInsensitive))
			{
				e->acceptProposedAction();
				return;
			}
		}
	}
}

void WBQtMainWindow::dropEvent(QDropEvent *e)
{
	if (!e->mimeData()->hasUrls())
	{
		return;
	}
	QList<QUrl> urls = e->mimeData()->urls();
	for (int i = 0; i < urls.size(); i++)
	{
		if (!urls[i].isLocalFile())
		{
			continue;
		}
		QString path = urls[i].toLocalFile();
		if (!path.endsWith(".map", Qt::CaseInsensitive))
		{
			continue;
		}
		QByteArray local = QFileInfo(path).absoluteFilePath().toLocal8Bit();
		// Native separators: the MFC doc system stores/compares Windows-style paths.
		local.replace('/', '\\');
		WBQt_OpenMapFileFromShell(local.constData());
	}
	e->acceptProposedAction();
}
