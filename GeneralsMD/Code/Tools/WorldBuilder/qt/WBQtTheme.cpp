// WBQtTheme.cpp -- see WBQtTheme.h.
//
// This is a Qt + Win32 translation unit (no MFC). NOMINMAX is set on the target so
// windows.h does not clobber std::min/std::max used by Qt headers; Qt headers are
// included before windows.h for the same reason.
#include "WBQtTheme.h"

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QObject>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QWidget>
#include <QWidgetList>

#include <windows.h>
#include <dwmapi.h>

namespace
{

	// DWMWA_USE_IMMERSIVE_DARK_MODE is 20 on Windows 10 2004+ / Windows 11, but was 19
	// on the 1809-1909 builds. Try the modern value first, fall back to the old one.
	const DWORD WB_DWMWA_DARK_MODE_NEW = 20;
	const DWORD WB_DWMWA_DARK_MODE_OLD = 19;

	// The native style/palette captured before we ever switch to Fusion, so "Light"
	// can restore the exact original look.
	bool s_defaultsCaptured = false;
	QString s_defaultStyleName;
	QPalette s_defaultPalette;

	// Tier 4a-2: a native (non-Qt) top-level whose caption follows the theme -- the MFC
	// main frame, registered once its client chrome is Qt.
	HWND s_nativeTopLevel = NULL;

	// The dark/light state most recently pushed by applyCurrentTheme, so a live OS theme
	// change (onSystemThemeChanged) can tell a real flip from WM_SETTINGCHANGE noise.
	bool s_lastAppliedDark = false;

	void setWindowDarkTitleBar(HWND hwnd, bool dark)
	{
		if (hwnd == NULL)
		{
			return;
		}
		BOOL on = dark ? TRUE : FALSE;
		if (FAILED(DwmSetWindowAttribute(hwnd, WB_DWMWA_DARK_MODE_NEW, &on, sizeof(on))))
		{
			DwmSetWindowAttribute(hwnd, WB_DWMWA_DARK_MODE_OLD, &on, sizeof(on));
		}
	}

	QPalette buildDarkPalette()
	{
		const QColor window(45, 45, 48);
		const QColor base(30, 30, 30);
		const QColor text(220, 220, 220);
		const QColor disabled(130, 130, 130);
		const QColor highlight(38, 79, 120);

		QPalette p;
		p.setColor(QPalette::Window, window);
		p.setColor(QPalette::WindowText, text);
		p.setColor(QPalette::Base, base);
		p.setColor(QPalette::AlternateBase, window);
		p.setColor(QPalette::ToolTipBase, window);
		p.setColor(QPalette::ToolTipText, text);
		p.setColor(QPalette::Text, text);
		p.setColor(QPalette::Button, window);
		p.setColor(QPalette::ButtonText, text);
		p.setColor(QPalette::BrightText, QColor(255, 80, 80));
		p.setColor(QPalette::Link, QColor(94, 160, 230));
		p.setColor(QPalette::Highlight, highlight);
		p.setColor(QPalette::HighlightedText, text);

		p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
		p.setColor(QPalette::Disabled, QPalette::Text, disabled);
		p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
		return p;
	}

	// A visible etched frame around every QGroupBox, so the migrated panels read as the boxed
	// MFC sections (General / Logical / Distance / ...) instead of the near-borderless Fusion
	// default. The border colour is pulled from the active palette so it tracks dark/light, and
	// the title is notched into the top-left of the frame like the Win32 GROUPBOX.
	const char *const WB_GROUPBOX_QSS =
		"QGroupBox {"
		" border: 1px solid palette(mid);"
		" border-radius: 3px;"
		" margin-top: 7px;"
		"}"
		"QGroupBox::title {"
		" subcontrol-origin: margin;"
		" subcontrol-position: top left;"
		" left: 8px;"
		" padding: 0 3px;"
		"}";

	// Push the current theme onto the QApplication and re-title-bar every open
	// top-level Qt window. New windows are handled by DarkTitleBarFilter below.
	void applyCurrentTheme()
	{
		const bool dark = WBQtTheme::effectiveDark();
		s_lastAppliedDark = dark;
		if (dark)
		{
			qApp->setStyle(QStyleFactory::create("Fusion"));
			qApp->setPalette(buildDarkPalette());
		}
		else
		{
			if (!s_defaultStyleName.isEmpty())
			{
				qApp->setStyle(QStyleFactory::create(s_defaultStyleName));
			}
			qApp->setPalette(s_defaultPalette);
		}

		// Re-apply after the style change (setStyle clears the app stylesheet on some styles).
		qApp->setStyleSheet(WB_GROUPBOX_QSS);

		const QWidgetList tops = qApp->topLevelWidgets();
		for (int i = 0; i < tops.size(); ++i)
		{
			QWidget *w = tops.at(i);
			if (w->isWindow())
			{
				setWindowDarkTitleBar(reinterpret_cast<HWND>(w->winId()), dark);
			}
		}

		if (s_nativeTopLevel != NULL && ::IsWindow(s_nativeTopLevel))
		{
			setWindowDarkTitleBar(s_nativeTopLevel, dark);
		}
	}

	// Application-wide filter: the first time a top-level Qt window is shown, set its
	// native title bar to match the current theme. This is what makes the chosen theme
	// automatic for every Qt window added during the migration -- no per-window code.
	class ThemeTitleBarFilter : public QObject
	{
	public:
		explicit ThemeTitleBarFilter(QObject *owner)
			: QObject(owner)
		{
		}

	protected:
		// True for windows that embed a native (non-Qt) child HWND -- the QWinWidget
		// hosts themselves (viewport chrome / owner bridges) and any window holding a
		// QWinHost (e.g. the adopted minimap). Layered-window transparency around
		// native children is not worth the risk; they are excluded from the fade.
		static bool embedsNativeChild(const QWidget *w)
		{
			if (w->inherits("QWinWidget"))
			{
				return true;
			}
			const QList<QWidget *> kids = w->findChildren<QWidget *>();
			for (int i = 0; i < kids.size(); ++i)
			{
				if (kids.at(i)->inherits("QWinHost"))
				{
					return true;
				}
			}
			return false;
		}

		virtual bool eventFilter(QObject *obj, QEvent *event)
		{
			if (event->type() == QEvent::Show && obj->isWidgetType())
			{
				QWidget *w = static_cast<QWidget *>(obj);
				if (w->isWindow() && w->property("wbTitleBarThemed").isNull())
				{
					w->setProperty("wbTitleBarThemed", true);
					setWindowDarkTitleBar(reinterpret_cast<HWND>(w->winId()),
						WBQtTheme::effectiveDark());

					// Anti-flash: a freshly created top-level maps as an UNPAINTED
					// (white) surface until its first paint flushes -- a bright flash
					// against the dark theme, worst on big panels whose first paint
					// is slow (e.g. Object Properties). Show it fully transparent and
					// restore opacity right after the first paint, with a timeout
					// safety net in case a window never paints.
					if (WBQtTheme::effectiveDark() && !embedsNativeChild(w))
					{
						w->setProperty("wbShowFadePending", true);
						w->setWindowOpacity(0.0);
						QWidget *shown = w;
						// Generous fallback: the real restore is the first-Paint handler
						// below. This only fires if a shown window somehow never paints;
						// it must comfortably exceed the worst first-populate stall (the
						// Object Properties sound combo), or it un-hides a white window
						// mid-load.
						QTimer::singleShot(4000, shown, [shown]() {
							if (shown->property("wbShowFadePending").toBool())
							{
								shown->setProperty("wbShowFadePending", QVariant());
								shown->setWindowOpacity(1.0);
							}
						});
					}
				}
			}
			else if (event->type() == QEvent::Paint && obj->isWidgetType())
			{
				QWidget *w = static_cast<QWidget *>(obj);
				if (w->isWindow() && w->property("wbShowFadePending").toBool())
				{
					// The first paint has landed in the backing store; flip the window
					// visible on the next event-loop turn, after that paint flushes.
					w->setProperty("wbShowFadePending", QVariant());
					QWidget *painted = w;
					QTimer::singleShot(0, painted, [painted]() {
						painted->setWindowOpacity(1.0);
					});
				}
			}
			return QObject::eventFilter(obj, event);
		}
	};

	// QSettings is non-copyable, so each accessor constructs its own (cheap).
	const char *const WB_QT_SETTINGS_PATH =
		"HKEY_CURRENT_USER\\Software\\WorldBuilderZH\\Qt";

}

bool WBQtTheme::osPrefersDark()
{
	// AppsUseLightTheme: 0 = dark, 1 = light. Absent (older Windows) -> treat as light.
	QSettings s("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
		QSettings::NativeFormat);
	const QVariant v = s.value("AppsUseLightTheme");
	if (!v.isValid())
	{
		return false;
	}
	return v.toInt() == 0;
}

WBQtTheme::Mode WBQtTheme::mode()
{
	QSettings s(WB_QT_SETTINGS_PATH, QSettings::NativeFormat);
	const int m = s.value("Theme", (int)ModeSystem).toInt();
	if (m == ModeDark)
	{
		return ModeDark;
	}
	if (m == ModeLight)
	{
		return ModeLight;
	}
	return ModeSystem;
}

bool WBQtTheme::effectiveDark()
{
	switch (mode())
	{
		case ModeDark:
			return true;
		case ModeLight:
			return false;
		default:
			return osPrefersDark();
	}
}

void WBQtTheme::setMode(Mode m)
{
	QSettings s(WB_QT_SETTINGS_PATH, QSettings::NativeFormat);
	s.setValue("Theme", (int)m);
	applyCurrentTheme();
}

void WBQtTheme::applyApplicationTheme()
{
	if (!s_defaultsCaptured)
	{
		// Capture the native look BEFORE any switch so "Light" restores it exactly.
		s_defaultStyleName = qApp->style() ? qApp->style()->objectName() : QString();
		s_defaultPalette = qApp->palette();
		s_defaultsCaptured = true;

		// Theme every top-level Qt window as it appears (now and going forward).
		qApp->installEventFilter(new ThemeTitleBarFilter(qApp));
	}

	applyCurrentTheme();
}

void WBQtTheme::onSystemThemeChanged()
{
	// WM_SETTINGCHANGE fires for many unrelated settings (and more than once per theme
	// switch), so bail unless the effective dark/light state really changed. Forced
	// Dark / Light modes ignore the OS entirely.
	if (!s_defaultsCaptured || mode() != ModeSystem)
	{
		return;
	}
	if (osPrefersDark() == s_lastAppliedDark)
	{
		return;
	}
	applyCurrentTheme();
}

void WBQtTheme::registerNativeTopLevel(void *hwnd)
{
	s_nativeTopLevel = reinterpret_cast<HWND>(hwnd);
	if (s_nativeTopLevel != NULL && ::IsWindow(s_nativeTopLevel))
	{
		setWindowDarkTitleBar(s_nativeTopLevel, effectiveDark());
	}
}
