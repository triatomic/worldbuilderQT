// WBQtEntityFinderDialog.cpp -- see WBQtEntityFinderDialog.h. Layout mirrors
// IDD_ABOUTBOX: the left column (version/credits/Discord, Object Finder, Waypoint
// Finder, Visual Settings, launch-on-startup) and the collapsible hotkey-list panel on
// the right (search + read-only list; the MFC Expand/Shrink pair becomes one toggle,
// persisted in the same ShrinkHotkeyList profile value).
#include "WBQtEntityFinderDialog.h"
#include "ui_WBQtEntityFinderDialog.h"
#include "WBQtComboStyle.h"
#include "WBQtEntityFinderBridge.h"

// NewSearch toggle (WBQtObjectBridge.cpp): live-filter search in the tree pickers.
extern "C" int  WBQtConfig_GetNewSearch(void);
extern "C" void WBQtConfig_SetNewSearch(int on);

// Render Particles toggle (WBQtObjectBridge.cpp): startup-only live particle preview.
extern "C" int  WBQtObject_GetRenderParticles(void);
extern "C" void WBQtObject_SetRenderParticles(int on);

// Tutorial popups toggle (WBQtObjectBridge.cpp): the one-time hint toasts + Ctrl+A confirm.
extern "C" int  WBQtObject_GetTutorialPrompts(void);
extern "C" void WBQtObject_SetTutorialPrompts(int on);

#include <QApplication>
#include <QCheckBox>
#include <QMessageBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextCursor>
#include <QUrl>
#include <QWindow>

#include <qt_windows.h>

#include <string.h>

#include "resource.h"		// IDB_PHLOGO (pure #defines; res is on the qt include path)

// Defined in WBQtBridge.cpp: the Qt main window's HWND when inverted, else the passed
// MFC frame HWND -- the native owner for standalone Qt top-levels.
void *WBQt_EffectiveOwnerHwnd(void *frameHwnd);
// Defined in WBQtBridge.cpp: the Qt main window as a QWidget* (NULL when not inverted).
QWidget *WBQt_MainWindowWidget(void);

namespace
{
	const int kTextCap = 8192;
	const int kNameCap = 256;
	// The second logo static's resource id is a bare number in the RC (no symbol).
	const int kSecondLogoId = 28727;

	// Load an RT_BITMAP resource as a QPixmap (32bpp GetDIBits, same technique as the
	// chrome's toolbar strip); null pixmap when missing.
	QPixmap loadResourceBitmap(int id)
	{
		HINSTANCE inst = ::GetModuleHandleA(NULL);
		HBITMAP hbm = (HBITMAP)::LoadImageA(inst, MAKEINTRESOURCEA(id), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
		if (hbm == NULL)
		{
			return QPixmap();
		}
		BITMAP bm;
		memset(&bm, 0, sizeof(bm));
		::GetObjectA(hbm, sizeof(bm), &bm);
		QImage img(bm.bmWidth, bm.bmHeight, QImage::Format_RGB32);
		BITMAPINFO bi;
		memset(&bi, 0, sizeof(bi));
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = bm.bmWidth;
		bi.bmiHeader.biHeight = -bm.bmHeight;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;
		HDC dc = ::GetDC(NULL);
		int got = ::GetDIBits(dc, hbm, 0, bm.bmHeight, img.bits(), &bi, DIB_RGB_COLORS);
		::ReleaseDC(NULL, dc);
		::DeleteObject(hbm);
		if (got != bm.bmHeight)
		{
			return QPixmap();
		}
		return QPixmap::fromImage(img);
	}
}

WBQtEntityFinderDialog *WBQtEntityFinderDialog::s_instance = NULL;

WBQtEntityFinderDialog::WBQtEntityFinderDialog(void *frameHwnd)
	: QWidget(NULL, Qt::Window),
	m_ui(new Ui::WBQtEntityFinderDialog)
{
	s_instance = this;

	// The static widget tree lives in WBQtEntityFinderDialog.ui; bind the members
	// the logic below uses, then wire what Designer can't express.
	m_ui->setupUi(this);

	m_objectCombo = m_ui->objectCombo;
	m_waypointCombo = m_ui->waypointCombo;

	// == the MFC IDC_FIND_QUERY_OBJ / IDC_FIND_QUERY_WP (CBS_DROPDOWN): type to narrow the list.
	WBQtComboStyle::applyTypeToFilter(m_objectCombo);
	WBQtComboStyle::applyTypeToFilter(m_waypointCombo);
	WBQtComboStyle::applyPopupScrollRecursive(this);
	m_fontCombo = m_ui->fontCombo;
	m_resolutionCombo = m_ui->resolutionCombo;
	m_undoSpin = m_ui->undoSpin;
	m_launchCheck = m_ui->launchCheck;
	m_newSearchCheck = m_ui->newSearchCheck;
	m_tutorialPromptsCheck = m_ui->tutorialPromptsCheck;
	m_renderParticlesCheck = m_ui->renderParticlesCheck;
	m_toggleButton = m_ui->toggleButton;
	m_hotkeyPanel = m_ui->hotkeyPanel;
	m_searchEdit = m_ui->searchEdit;
	m_hotkeyText = m_ui->hotkeyText;

	// ---------------- left column: the dynamic logo row ----------------
	// The WB logo bitmap ships with a solid white background; key it out with a SOFT
	// white matte: alpha ramps with the pixel's distance from pure white, and the kept
	// fraction is un-blended from the white matte -- so the anti-aliased edge pixels of
	// the artwork fade smoothly into the dark theme instead of leaving a hard, fringed
	// silhouette (what a binary threshold produced).
	QPixmap logo2 = loadResourceBitmap(kSecondLogoId);
	if (!logo2.isNull())
	{
		const int kSoft = 32;	// ramp width: min-channel 255..(255-kSoft) -> alpha 0..255
		QImage img = logo2.toImage().convertToFormat(QImage::Format_ARGB32);
		for (int y = 0; y < img.height(); ++y)
		{
			QRgb *row = reinterpret_cast<QRgb *>(img.scanLine(y));
			for (int x = 0; x < img.width(); ++x)
			{
				const int r = qRed(row[x]);
				const int g = qGreen(row[x]);
				const int b = qBlue(row[x]);
				const int d = 255 - qMin(r, qMin(g, b));	// 0 == pure white
				if (d >= kSoft)
				{
					continue;	// solid artwork pixel, keep as-is
				}
				if (d == 0)
				{
					row[x] = qRgba(0, 0, 0, 0);
					continue;
				}
				// alpha = d/kSoft; recovered channel = (c - (1-alpha)*255) / alpha,
				// which un-mixes the white that BitBlt baked into the edge pixel.
				const int a = d * 255 / kSoft;
				const int ur = qBound(0, 255 + ((r - 255) * kSoft) / d, 255);
				const int ug = qBound(0, 255 + ((g - 255) * kSoft) / d, 255);
				const int ub = qBound(0, 255 + ((b - 255) * kSoft) / d, 255);
				row[x] = qRgba(ur, ug, ub, a);
			}
		}
		logo2 = QPixmap::fromImage(img);
	}
	// A painted modern-Qt-logo tile (brand green + white "Qt") replaces the old
	// IDB_PHLOGO flag bitmap; sized to match the WB logo so the row stays aligned.
	{
		const int kTile = logo2.isNull() ? 48 : logo2.height();
		QPixmap qtLogo(kTile, kTile);
		qtLogo.fill(Qt::transparent);
		QPainter p(&qtLogo);
		p.setRenderHint(QPainter::Antialiasing, true);
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(0x41, 0xCD, 0x52));
		p.drawRoundedRect(QRectF(0, 0, kTile, kTile), kTile / 6.0, kTile / 6.0);
		QFont qtFont = font();
		qtFont.setBold(true);
		qtFont.setPixelSize(kTile * 5 / 9);
		p.setFont(qtFont);
		p.setPen(Qt::white);
		p.drawText(QRect(0, 0, kTile, kTile), Qt::AlignCenter, "Qt");
		p.end();
		QLabel *l = new QLabel(this);
		l->setPixmap(qtLogo);
		m_ui->logoRow->addWidget(l);
	}
	if (!logo2.isNull())
	{
		QLabel *l = new QLabel(this);
		l->setPixmap(logo2);
		m_ui->logoRow->addWidget(l);
	}
	m_ui->logoRow->addStretch(1);

	connect(m_ui->discordBtn, SIGNAL(clicked()), this, SLOT(onOpenDiscord()));
	connect(m_ui->objectFindBtn, SIGNAL(clicked()), this, SLOT(onFindObject()));
	connect(m_ui->objectRefreshBtn, SIGNAL(clicked()), this, SLOT(onRefreshObjects()));
	connect(m_ui->waypointFindBtn, SIGNAL(clicked()), this, SLOT(onFindWaypoint()));
	connect(m_ui->waypointRefreshBtn, SIGNAL(clicked()), this, SLOT(onRefreshWaypoints()));
	connect(m_launchCheck, SIGNAL(toggled(bool)), this, SLOT(onLaunchOnStartupToggled(bool)));
	connect(m_toggleButton, SIGNAL(clicked()), this, SLOT(onToggleHotkeyPanel()));

	// ---------------- right: the hotkey list panel ----------------
	m_hotkeyText->setTabStopDistance(m_hotkeyText->fontMetrics().horizontalAdvance(QChar('x')) * 8);
	connect(m_ui->hotkeyFindBtn, SIGNAL(clicked()), this, SLOT(onFindHotkey()));
	connect(m_searchEdit, SIGNAL(returnPressed()), this, SLOT(onFindHotkey()));

	// ---------------- seed ----------------
	char text[kTextCap];
	text[0] = 0;
	WBQtEntityFinderData_GetHotkeyText(text, sizeof(text));
	m_hotkeyText->setPlainText(QString::fromLocal8Bit(text).replace("\r\n", "\n"));

	m_launchCheck->blockSignals(true);
	m_launchCheck->setChecked(WBQtEntityFinderData_GetProfileInt("LaunchOnStartUp", 1) != 0);
	m_launchCheck->blockSignals(false);

	m_undoSpin->blockSignals(true);
	m_undoSpin->setValue(WBQtEntityFinderData_GetMaxUndos());
	m_undoSpin->blockSignals(false);
	connect(m_undoSpin, SIGNAL(valueChanged(int)), this, SLOT(onMaxUndosChanged(int)));

	m_newSearchCheck->blockSignals(true);
	m_newSearchCheck->setChecked(WBQtConfig_GetNewSearch() != 0);
	m_newSearchCheck->blockSignals(false);
	connect(m_newSearchCheck, SIGNAL(toggled(bool)), this, SLOT(onNewSearchToggled(bool)));

	m_tutorialPromptsCheck->blockSignals(true);
	m_tutorialPromptsCheck->setChecked(WBQtObject_GetTutorialPrompts() != 0);
	m_tutorialPromptsCheck->blockSignals(false);
	connect(m_tutorialPromptsCheck, SIGNAL(toggled(bool)), this, SLOT(onTutorialPromptsToggled(bool)));

	m_renderParticlesCheck->blockSignals(true);
	m_renderParticlesCheck->setChecked(WBQtObject_GetRenderParticles() != 0);
	m_renderParticlesCheck->blockSignals(false);
	connect(m_renderParticlesCheck, SIGNAL(toggled(bool)), this, SLOT(onRenderParticlesToggled(bool)));

	populateFonts();
	populateResolutions();
	refreshFinders();

	applyHotkeyPanelState(WBQtEntityFinderData_GetProfileInt("ShrinkHotkeyList", 0) == 0);

	// Owned by the visible top-level (the Qt main window when inverted, else the MFC
	// frame) so it stacks above the main window without stealing its taskbar entry
	// (== the MFC dialog's ownership). The ownership must go through the QWindow
	// transient parent: the Windows QPA maintains GWLP_HWNDPARENT from that property and
	// resets a raw SetWindowLongPtr write back to 0 on its next internal update (same
	// fix as the Script Editor).
	QWidget *mainWidget = WBQt_MainWindowWidget();
	if (mainWidget != NULL)
	{
		winId();				// force the QWindow to exist before setting its owner
		mainWidget->winId();
		windowHandle()->setTransientParent(mainWidget->windowHandle());
	}
	else if (frameHwnd != NULL)
	{
		::SetWindowLongPtr(reinterpret_cast<HWND>(winId()), GWLP_HWNDPARENT,
			reinterpret_cast<LONG_PTR>(WBQt_EffectiveOwnerHwnd(frameHwnd)));
	}

	int top = WBQtEntityFinderData_GetProfileInt("Top", -1);
	int leftPos = WBQtEntityFinderData_GetProfileInt("Left", -1);
	if (top != -1 && leftPos != -1)
	{
		move(leftPos, top);
	}
}

WBQtEntityFinderDialog::~WBQtEntityFinderDialog()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

bool WBQtEntityFinderDialog::ownsWin32Focus() const
{
	HWND focus = ::GetFocus();
	HWND self = reinterpret_cast<HWND>(const_cast<WBQtEntityFinderDialog *>(this)->winId());
	return (focus != NULL && (focus == self || ::IsChild(self, focus)));
}

void WBQtEntityFinderDialog::refreshFinders()
{
	char name[kNameCap];

	QString objectText = m_objectCombo->currentText();
	m_objectCombo->clear();
	int objectCount = WBQtEntityFinderData_BuildObjects();
	for (int i = 0; i < objectCount; i++)
	{
		name[0] = 0;
		WBQtEntityFinderData_GetObject(i, name, sizeof(name));
		m_objectCombo->addItem(QString::fromLocal8Bit(name));
	}
	m_objectCombo->model()->sort(0);
	m_objectCombo->setEditText(objectText);

	QString waypointText = m_waypointCombo->currentText();
	m_waypointCombo->clear();
	int waypointCount = WBQtEntityFinderData_BuildWaypoints();
	for (int i = 0; i < waypointCount; i++)
	{
		name[0] = 0;
		WBQtEntityFinderData_GetWaypoint(i, name, sizeof(name));
		m_waypointCombo->addItem(QString::fromLocal8Bit(name));
	}
	m_waypointCombo->model()->sort(0);
	m_waypointCombo->setEditText(waypointText);
}

void WBQtEntityFinderDialog::populateFonts()
{
	m_fontCombo->blockSignals(true);
	m_fontCombo->clear();
	char label[kNameCap];
	int count = WBQtEntityFinderData_GetFontCount();
	for (int i = 0; i < count; i++)
	{
		label[0] = 0;
		WBQtEntityFinderData_GetFontLabel(i, label, sizeof(label));
		m_fontCombo->addItem(QString::fromLocal8Bit(label));
	}
	int sel = WBQtEntityFinderData_GetFontSel();
	if (sel >= 0 && sel < m_fontCombo->count())
	{
		m_fontCombo->setCurrentIndex(sel);
	}
	m_fontCombo->blockSignals(false);
	connect(m_fontCombo, SIGNAL(activated(int)), this, SLOT(onFontChanged(int)));
}

// == CAboutDlg::populateResolutionCombo: every distinct WxH the primary display reports,
// ascending, plus the saved size if the monitor doesn't advertise it.
void WBQtEntityFinderDialog::populateResolutions()
{
	m_resolutionCombo->blockSignals(true);
	m_resolutionCombo->clear();

	QList<QPoint> modes;
	DEVMODEA dm;
	memset(&dm, 0, sizeof(dm));
	dm.dmSize = sizeof(dm);
	for (int iMode = 0; ::EnumDisplaySettingsA(NULL, iMode, &dm); ++iMode)
	{
		int w = (int)dm.dmPelsWidth;
		int h = (int)dm.dmPelsHeight;
		if (w <= 0 || h <= 0)
		{
			continue;
		}
		int pos = 0;
		bool dup = false;
		while (pos < modes.size())
		{
			if (modes[pos].x() == w && modes[pos].y() == h)
			{
				dup = true;
				break;
			}
			if (modes[pos].x() > w || (modes[pos].x() == w && modes[pos].y() > h))
			{
				break;
			}
			++pos;
		}
		if (!dup)
		{
			modes.insert(pos, QPoint(w, h));
		}
	}

	int savedW = 0;
	int savedH = 0;
	WBQtEntityFinderData_GetSavedResolution(&savedW, &savedH);
	if (savedW > 0 && savedH > 0)
	{
		bool found = false;
		for (int i = 0; i < modes.size(); i++)
		{
			if (modes[i].x() == savedW && modes[i].y() == savedH)
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			modes.append(QPoint(savedW, savedH));
		}
	}

	int selIndex = -1;
	for (int i = 0; i < modes.size(); i++)
	{
		m_resolutionCombo->addItem(QString("%1 x %2").arg(modes[i].x()).arg(modes[i].y()), modes[i]);
		if (modes[i].x() == savedW && modes[i].y() == savedH)
		{
			selIndex = i;
		}
	}
	if (selIndex >= 0)
	{
		m_resolutionCombo->setCurrentIndex(selIndex);
	}
	m_resolutionCombo->blockSignals(false);
	connect(m_resolutionCombo, SIGNAL(activated(int)), this, SLOT(onResolutionChanged(int)));
}

void WBQtEntityFinderDialog::applyHotkeyPanelState(bool visible)
{
	m_hotkeyPanel->setVisible(visible);
	m_toggleButton->setText(visible ? "<< Hide Hotkey List" : "Hotkey List >>");
	adjustSize();
}

void WBQtEntityFinderDialog::onToggleHotkeyPanel()
{
	bool nowVisible = !m_hotkeyPanel->isVisible();
	applyHotkeyPanelState(nowVisible);
	// Same profile value the MFC Expand/Shrink pair used (1 = shrunk/hidden).
	WBQtEntityFinder_SetProfileInt("ShrinkHotkeyList", nowVisible ? 0 : 1);
}

// == CAboutDlg::OnFindButtonClicked: case-insensitive, continues after the current hit,
// wraps, beeps when the query is empty or missing.
void WBQtEntityFinderDialog::onFindHotkey()
{
	QString query = m_searchEdit->text();
	if (query.isEmpty())
	{
		QApplication::beep();
		QTextCursor clear = m_hotkeyText->textCursor();
		clear.clearSelection();
		m_hotkeyText->setTextCursor(clear);
		return;
	}
	if (m_hotkeyText->find(query))
	{
		return;
	}
	// Wrap to the top and retry.
	QTextCursor top = m_hotkeyText->textCursor();
	top.movePosition(QTextCursor::Start);
	m_hotkeyText->setTextCursor(top);
	if (!m_hotkeyText->find(query))
	{
		QApplication::beep();
	}
}

void WBQtEntityFinderDialog::onFindObject()
{
	QByteArray name = m_objectCombo->currentText().toLocal8Bit();
	WBQtEntityFinder_CenterOn(name.constData(), 0);
}

void WBQtEntityFinderDialog::onRefreshObjects()
{
	refreshFinders();
}

void WBQtEntityFinderDialog::onFindWaypoint()
{
	QByteArray name = m_waypointCombo->currentText().toLocal8Bit();
	WBQtEntityFinder_CenterOn(name.constData(), 1);
}

void WBQtEntityFinderDialog::onRefreshWaypoints()
{
	refreshFinders();
}

void WBQtEntityFinderDialog::onOpenDiscord()
{
	QDesktopServices::openUrl(QUrl("https://discord.gg/tJ6zyGb"));
}

void WBQtEntityFinderDialog::onLaunchOnStartupToggled(bool on)
{
	WBQtEntityFinder_SetProfileInt("LaunchOnStartUp", on ? 1 : 0);
}

void WBQtEntityFinderDialog::onMaxUndosChanged(int value)
{
	WBQtEntityFinder_SetMaxUndos(value);
}

void WBQtEntityFinderDialog::onNewSearchToggled(bool on)
{
	// Persisted; the tree pickers read it when they are next opened.
	WBQtConfig_SetNewSearch(on ? 1 : 0);
}

void WBQtEntityFinderDialog::onTutorialPromptsToggled(bool on)
{
	// Persisted; every hint site reads it live, so this takes effect immediately.
	WBQtObject_SetTutorialPrompts(on ? 1 : 0);
}

void WBQtEntityFinderDialog::onRenderParticlesToggled(bool on)
{
	// Startup-only opt-in: persist the choice; the particle runtime stands up during WbView3d
	// init, so it takes effect on the next WB launch.
	WBQtObject_SetRenderParticles(on ? 1 : 0);
	QMessageBox::information(this, "Render Particles",
		on ? "Particle preview will be ON the next time you start WorldBuilder."
		   : "Particle preview will be OFF the next time you start WorldBuilder.");
}

void WBQtEntityFinderDialog::onFontChanged(int index)
{
	WBQtEntityFinder_SetFontSel(index);
}

void WBQtEntityFinderDialog::onResolutionChanged(int index)
{
	QPoint mode = m_resolutionCombo->itemData(index).toPoint();
	if (mode.x() > 0 && mode.y() > 0)
	{
		WBQtEntityFinder_SetResolution(mode.x(), mode.y());
	}
}

void WBQtEntityFinderDialog::moveEvent(QMoveEvent *event)
{
	QWidget::moveEvent(event);
	if (isVisible() && !isMinimized())
	{
		WBQtEntityFinder_SetProfileInt("Top", frameGeometry().top());
		WBQtEntityFinder_SetProfileInt("Left", frameGeometry().left());
	}
}

void WBQtEntityFinderDialog::closeEvent(QCloseEvent *event)
{
	// Cached singleton: hide, don't destroy (reopening re-shows and re-seeds finders).
	hide();
	event->ignore();
}

// ===================== the C entry points =====================

extern "C" int WBQtEntityFinder_Open(void *frameHwnd)
{
	if (qApp == NULL)
	{
		return 0;	// Qt not up -- the caller falls back to the MFC dialog
	}
	WBQtEntityFinderDialog *dlg = WBQtEntityFinderDialog::instance();
	if (dlg == NULL)
	{
		dlg = new WBQtEntityFinderDialog(frameHwnd);
	}
	else
	{
		dlg->refreshFinders();
	}
	dlg->show();
	dlg->raise();
	dlg->activateWindow();
	return 1;
}

extern "C" void WBQtEntityFinder_MoveTo(int left, int top)
{
	WBQtEntityFinderDialog *dlg = WBQtEntityFinderDialog::instance();
	if (dlg != NULL && dlg->isVisible())
	{
		dlg->move(left, top);
	}
}
