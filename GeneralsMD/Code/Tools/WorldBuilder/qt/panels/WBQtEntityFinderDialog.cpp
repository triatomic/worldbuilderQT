// WBQtEntityFinderDialog.cpp -- see WBQtEntityFinderDialog.h. Layout mirrors
// IDD_ABOUTBOX: the left column (version/credits/Discord, Object Finder, Waypoint
// Finder, Visual Settings, launch-on-startup) and the collapsible hotkey-list panel on
// the right (search + read-only list; the MFC Expand/Shrink pair becomes one toggle,
// persisted in the same ShrinkHotkeyList profile value).
#include "WBQtEntityFinderDialog.h"
#include "WBQtEntityFinderBridge.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QUrl>
#include <QVBoxLayout>

#include <qt_windows.h>

#include <string.h>

#include "resource.h"		// IDB_PHLOGO (pure #defines; res is on the qt include path)

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
	m_objectCombo(NULL),
	m_waypointCombo(NULL),
	m_fontCombo(NULL),
	m_resolutionCombo(NULL),
	m_launchCheck(NULL),
	m_toggleButton(NULL),
	m_hotkeyPanel(NULL),
	m_searchEdit(NULL),
	m_hotkeyText(NULL)
{
	s_instance = this;
	setWindowTitle("Help / Entity Finder / Shortcut Finder");

	QHBoxLayout *root = new QHBoxLayout(this);

	// ---------------- left column ----------------
	QVBoxLayout *left = new QVBoxLayout();
	root->addLayout(left);

	left->addWidget(new QLabel("Adriane [ Deathscythe ] & Triatomic | Community Worldbuilder V4.1.2qt_b", this));

	QHBoxLayout *logoRow = new QHBoxLayout();
	// The WB logo bitmap ships with a solid white background; knock it out to transparent
	// (small tolerance for off-white edge pixels) so it sits cleanly on the dark theme.
	QPixmap logo2 = loadResourceBitmap(kSecondLogoId);
	if (!logo2.isNull())
	{
		QImage img = logo2.toImage().convertToFormat(QImage::Format_ARGB32);
		for (int y = 0; y < img.height(); ++y)
		{
			QRgb *row = reinterpret_cast<QRgb *>(img.scanLine(y));
			for (int x = 0; x < img.width(); ++x)
			{
				if (qRed(row[x]) >= 0xF0 && qGreen(row[x]) >= 0xF0 && qBlue(row[x]) >= 0xF0)
				{
					row[x] = qRgba(0, 0, 0, 0);
				}
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
		logoRow->addWidget(l);
	}
	if (!logo2.isNull())
	{
		QLabel *l = new QLabel(this);
		l->setPixmap(logo2);
		logoRow->addWidget(l);
	}
	logoRow->addStretch(1);
	left->addLayout(logoRow);

	QLabel *disclaimer = new QLabel(
		"This is a fan-modified tool of the original ZH worldbuilder. "
		"All trademarks belong to their respective owners.", this);
	disclaimer->setWordWrap(true);
	left->addWidget(disclaimer);

	QGroupBox *credits = new QGroupBox("Collaborators / Testers:", this);
	QVBoxLayout *creditsLay = new QVBoxLayout(credits);
	QLabel *creditsText = new QLabel(
		"Kabuse [Hotkey layout] | Gramantio | Veloxious | Muska | Vite | Sgtmyers | WWB2 | BKR | Emil", credits);
	creditsText->setWordWrap(true);
	creditsLay->addWidget(creditsText);
	left->addWidget(credits);

	QHBoxLayout *discordRow = new QHBoxLayout();
	discordRow->addWidget(new QLabel("Discord Server:", this));
	discordRow->addWidget(new QLabel("https://discord.gg/tJ6zyGb", this), 1);
	QPushButton *discordButton = new QPushButton("Open Link", this);
	discordButton->setAutoDefault(false);
	discordRow->addWidget(discordButton);
	left->addLayout(discordRow);
	connect(discordButton, SIGNAL(clicked()), this, SLOT(onOpenDiscord()));

	QGroupBox *objectGroup = new QGroupBox("Object Finder:", this);
	QHBoxLayout *objectRow = new QHBoxLayout(objectGroup);
	m_objectCombo = new QComboBox(objectGroup);
	m_objectCombo->setEditable(true);
	objectRow->addWidget(m_objectCombo, 1);
	QPushButton *objectFind = new QPushButton("Find", objectGroup);
	objectFind->setAutoDefault(false);
	objectRow->addWidget(objectFind);
	QPushButton *objectRefresh = new QPushButton("Refresh", objectGroup);
	objectRefresh->setAutoDefault(false);
	objectRow->addWidget(objectRefresh);
	left->addWidget(objectGroup);
	connect(objectFind, SIGNAL(clicked()), this, SLOT(onFindObject()));
	connect(objectRefresh, SIGNAL(clicked()), this, SLOT(onRefreshObjects()));

	QGroupBox *waypointGroup = new QGroupBox("Waypoint Finder:", this);
	QHBoxLayout *waypointRow = new QHBoxLayout(waypointGroup);
	m_waypointCombo = new QComboBox(waypointGroup);
	m_waypointCombo->setEditable(true);
	waypointRow->addWidget(m_waypointCombo, 1);
	QPushButton *waypointFind = new QPushButton("Find", waypointGroup);
	waypointFind->setAutoDefault(false);
	waypointRow->addWidget(waypointFind);
	QPushButton *waypointRefresh = new QPushButton("Refresh", waypointGroup);
	waypointRefresh->setAutoDefault(false);
	waypointRow->addWidget(waypointRefresh);
	left->addWidget(waypointGroup);
	connect(waypointFind, SIGNAL(clicked()), this, SLOT(onFindWaypoint()));
	connect(waypointRefresh, SIGNAL(clicked()), this, SLOT(onRefreshWaypoints()));

	QGroupBox *visual = new QGroupBox("Visual Settings:", this);
	QVBoxLayout *visualLay = new QVBoxLayout(visual);
	QHBoxLayout *fontRow = new QHBoxLayout();
	m_fontCombo = new QComboBox(visual);
	fontRow->addWidget(m_fontCombo, 1);
	fontRow->addWidget(new QLabel("(applies on restart)", visual));
	visualLay->addLayout(fontRow);
	QHBoxLayout *resolutionRow = new QHBoxLayout();
	m_resolutionCombo = new QComboBox(visual);
	resolutionRow->addWidget(m_resolutionCombo, 1);
	resolutionRow->addWidget(new QLabel("Viewport resolution", visual));
	visualLay->addLayout(resolutionRow);
	left->addWidget(visual);

	left->addStretch(1);

	QHBoxLayout *bottomRow = new QHBoxLayout();
	m_launchCheck = new QCheckBox("Launch this window on app startup", this);
	bottomRow->addWidget(m_launchCheck, 1);
	m_toggleButton = new QPushButton(this);
	m_toggleButton->setAutoDefault(false);
	bottomRow->addWidget(m_toggleButton);
	left->addLayout(bottomRow);
	connect(m_launchCheck, SIGNAL(toggled(bool)), this, SLOT(onLaunchOnStartupToggled(bool)));
	connect(m_toggleButton, SIGNAL(clicked()), this, SLOT(onToggleHotkeyPanel()));

	// ---------------- right: the hotkey list panel ----------------
	m_hotkeyPanel = new QWidget(this);
	QVBoxLayout *panel = new QVBoxLayout(m_hotkeyPanel);
	panel->setContentsMargins(0, 0, 0, 0);
	QHBoxLayout *searchRow = new QHBoxLayout();
	searchRow->addWidget(new QLabel("Hotkey List    Search:", m_hotkeyPanel));
	m_searchEdit = new QLineEdit(m_hotkeyPanel);
	searchRow->addWidget(m_searchEdit, 1);
	QPushButton *findButton = new QPushButton("Find", m_hotkeyPanel);
	findButton->setAutoDefault(false);
	searchRow->addWidget(findButton);
	panel->addLayout(searchRow);
	m_hotkeyText = new QPlainTextEdit(m_hotkeyPanel);
	m_hotkeyText->setReadOnly(true);
	m_hotkeyText->setTabStopDistance(m_hotkeyText->fontMetrics().horizontalAdvance(QChar('x')) * 8);
	panel->addWidget(m_hotkeyText, 1);
	root->addWidget(m_hotkeyPanel, 1);
	connect(findButton, SIGNAL(clicked()), this, SLOT(onFindHotkey()));
	connect(m_searchEdit, SIGNAL(returnPressed()), this, SLOT(onFindHotkey()));

	// ---------------- seed ----------------
	char text[kTextCap];
	text[0] = 0;
	WBQtEntityFinderData_GetHotkeyText(text, sizeof(text));
	m_hotkeyText->setPlainText(QString::fromLocal8Bit(text).replace("\r\n", "\n"));

	m_launchCheck->blockSignals(true);
	m_launchCheck->setChecked(WBQtEntityFinderData_GetProfileInt("LaunchOnStartUp", 1) != 0);
	m_launchCheck->blockSignals(false);

	populateFonts();
	populateResolutions();
	refreshFinders();

	applyHotkeyPanelState(WBQtEntityFinderData_GetProfileInt("ShrinkHotkeyList", 0) == 0);

	// Owned by the frame so it stacks above the main window without stealing its
	// taskbar entry (== the MFC dialog's ownership).
	if (frameHwnd != NULL)
	{
		::SetWindowLongPtr(reinterpret_cast<HWND>(winId()), GWLP_HWNDPARENT,
			reinterpret_cast<LONG_PTR>(frameHwnd));
	}

	int top = WBQtEntityFinderData_GetProfileInt("Top", -1);
	int leftPos = WBQtEntityFinderData_GetProfileInt("Left", -1);
	if (top != -1 && leftPos != -1)
	{
		move(leftPos, top);
	}
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

extern "C" int WBQtEntityFinder_OwnsFocus(void)
{
	WBQtEntityFinderDialog *dlg = WBQtEntityFinderDialog::instance();
	return (dlg != NULL && dlg->isVisible() && dlg->ownsWin32Focus()) ? 1 : 0;
}
