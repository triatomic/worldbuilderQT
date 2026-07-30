// WBQtComboStyle.cpp -- see WBQtComboStyle.h.
#include "WBQtComboStyle.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QCompleter>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QList>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QSortFilterProxyModel>
#include <QWidget>

// ComboSearch toggle (WBQtObjectBridge.cpp): type-to-search in the long drop-downs when on.
extern "C" int WBQtConfig_GetComboSearch(void);

namespace
{
	// Marks a combo we've already wired, so a re-call (e.g. after the item list is rebuilt)
	// doesn't stack a second completer/filter on it.
	const char *WB_COMBO_FILTERED = "wbComboFiltered";
	const char *WB_COMBO_CLAMPED = "wbComboClamped";
	const char *WB_COMBO_SEARCHABLE = "wbComboSearchable";

	// How tall a dropped-down list may get, in pixels. The MFC combos each declare a fixed
	// dropped height in WorldBuilder.rc (the COMBOBOX's 4th value, in dialog units): they run
	// 60..329 DLU, median 173. At ~1.6 DLU per pixel vertically that is ~37..206px, median
	// ~108 -- i.e. even the tallest MFC list was a couple of hundred pixels, never the
	// full-screen column Qt grows when it just sizes to the item count. 210 == the 329 DLU
	// maximum, so no list drops further than the roomiest MFC one did.
	const int WB_COMBO_MAX_POPUP_PX = 210;

	// Qt re-creates/re-sizes the popup every time it is shown, and the style sizes it AFTER any
	// maximumHeight we set on the view up front -- so clamping once in applyPopupScroll() does
	// not stick. This filter rides the popup's own window and re-applies the cap on each Show
	// (and on the Resize the style triggers), which is what actually holds it down.
	//
	// Shrinking alone is NOT enough: when the full-height list doesn't fit below the combo, Qt
	// flips the popup ABOVE it, anchoring its BOTTOM edge to the combo's top. Cutting the height
	// while leaving y alone then leaves the popup floating off the combo. So after resizing we
	// re-anchor it: below the combo if there's room (the normal case), else directly above.
	class WBQtPopupClamp : public QObject
	{
	public:
		explicit WBQtPopupClamp(QComboBox *combo) : QObject(combo), m_combo(combo) {}

	protected:
		virtual bool eventFilter(QObject *watched, QEvent *event)
		{
			if (event->type() != QEvent::Show && event->type() != QEvent::Resize)
			{
				return QObject::eventFilter(watched, event);
			}
			QWidget *popup = qobject_cast<QWidget *>(watched);
			if (popup == NULL || m_combo == NULL || popup->height() <= WB_COMBO_MAX_POPUP_PX)
			{
				return QObject::eventFilter(watched, event);
			}

			// Shrinking the popup WINDOW alone leaves the view inside it at its full content
			// height (all N rows), merely CLIPPED by the smaller window: the view's viewport
			// still spans every item, so its scroll range stays 0..0 -- no scrollbar, and the
			// entries past the cut are unreachable. Bound the view to the window as well, which
			// is what gives it something to scroll.
			QAbstractItemView *view = m_combo->view();
			if (view != NULL)
			{
				// The view is inset in the popup by the container's frame; keep that inset so
				// the list doesn't overhang the border.
				int inset = 0;
				if (view->parentWidget() == popup)
				{
					inset = popup->height() - view->height();
				}
				if (inset < 0)
				{
					inset = 0;
				}
				view->setMaximumHeight(WB_COMBO_MAX_POPUP_PX);
				view->resize(view->width(), WB_COMBO_MAX_POPUP_PX - inset);
			}

			popup->setMaximumHeight(WB_COMBO_MAX_POPUP_PX);
			popup->resize(popup->width(), WB_COMBO_MAX_POPUP_PX);

			// Re-anchor to the combo in screen coords (the popup is a top-level window).
			QPoint below = m_combo->mapToGlobal(QPoint(0, m_combo->height()));
			QScreen *scr = QGuiApplication::screenAt(below);
			if (scr == NULL)
			{
				scr = QGuiApplication::primaryScreen();
			}
			if (scr == NULL)
			{
				return QObject::eventFilter(watched, event);
			}
			QRect screen = scr->availableGeometry();
			int y = below.y();
			if (y + WB_COMBO_MAX_POPUP_PX > screen.bottom())
			{
				// No room below -- open upward, bottom edge on the combo's top, like Qt does.
				int above = m_combo->mapToGlobal(QPoint(0, 0)).y() - WB_COMBO_MAX_POPUP_PX;
				if (above >= screen.top())
				{
					y = above;
				}
			}
			popup->move(popup->x(), y);
			return QObject::eventFilter(watched, event);
		}

	private:
		QComboBox *m_combo;
	};

	// A searchable combo is editable only so the user can TYPE to narrow the list -- it is still
	// a pick-only control. Left alone, a half-typed or non-matching string would sit in the field
	// while the actual selection is something else entirely, which in a properties panel reads as
	// "this object's team is <whatever I typed>". On focus-out (and on Escape) put the current
	// item's text back; a committed pick goes through activated(), which sets the text itself.
	class WBQtSearchRevert : public QObject
	{
	public:
		WBQtSearchRevert(QComboBox *combo, QSortFilterProxyModel *proxy)
			: QObject(combo), m_combo(combo), m_proxy(proxy) {}

	protected:
		virtual bool eventFilter(QObject *watched, QEvent *event)
		{
			if (m_combo == NULL)
			{
				return QObject::eventFilter(watched, event);
			}
			bool revert = (event->type() == QEvent::FocusOut);
			if (event->type() == QEvent::KeyPress)
			{
				QKeyEvent *key = static_cast<QKeyEvent *>(event);
				revert = (key->key() == Qt::Key_Escape);
			}
			if (revert)
			{
				int cur = m_combo->currentIndex();
				QString text = (cur >= 0) ? m_combo->itemText(cur) : QString();
				if (m_combo->lineEdit() != NULL && m_combo->lineEdit()->text() != text)
				{
					m_combo->lineEdit()->setText(text);
				}
				// Drop the narrowing too, so the next drop-down opens on the full list
				// instead of the leftovers of the last thing typed.
				if (m_proxy != NULL)
				{
					m_proxy->setFilterFixedString(QString());
				}
			}
			return QObject::eventFilter(watched, event);
		}

	private:
		QComboBox *m_combo;
		QSortFilterProxyModel *m_proxy;
	};
}

void WBQtComboStyle::applyPopupScroll(QComboBox *combo)
{
	if (combo == NULL)
	{
		return;
	}

	// The MFC drop-downs are WS_VSCROLL: a long list SCROLLS inside a fixed-height popup rather
	// than growing the popup to fit every item (which is what Qt does by default -- a 200-entry
	// Sound list becomes a full-screen-tall column).
	QAbstractItemView *view = combo->view();
	if (view != NULL)
	{
		view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

		// Clamp the popup WINDOW (the view's parent), which is what the style sizes; capping
		// the view alone doesn't hold (the style re-sizes the window around it). The filter
		// caps BOTH -- the window for the geometry, the view so the list actually scrolls
		// instead of being clipped. Installed once per combo -- see WBQtPopupClamp.
		if (!combo->property(WB_COMBO_CLAMPED).toBool())
		{
			QWidget *popup = view->parentWidget();
			if (popup != NULL)
			{
				popup->installEventFilter(new WBQtPopupClamp(combo));
				combo->setProperty(WB_COMBO_CLAMPED, true);
			}
		}
	}

	// Belt and braces: under Fusion this alone bounds the popup, and it keeps the item count
	// sane if a style ever sizes the popup before our filter sees the Show. Measure the row
	// rather than assuming 20px: the actual height is style- and font-dependent (21px under
	// Fusion at the default font), and guessing low lets the popup overshoot the cap.
	int rowPx = 0;
	if (view != NULL && combo->count() > 0)
	{
		rowPx = view->sizeHintForRow(0);
	}
	if (rowPx <= 0)
	{
		rowPx = 20;	// empty combo (populated later) -- fall back to the old estimate
	}
	combo->setMaxVisibleItems(WB_COMBO_MAX_POPUP_PX / rowPx);
}

void WBQtComboStyle::applyPopupScrollRecursive(QWidget *root)
{
	if (root == NULL)
	{
		return;
	}
	QList<QComboBox *> combos = root->findChildren<QComboBox *>();
	for (int i = 0; i < combos.size(); ++i)
	{
		applyPopupScroll(combos.at(i));
	}
}

void WBQtComboStyle::applyTypeToFilter(QComboBox *combo)
{
	if (combo == NULL)
	{
		return;
	}
	if (combo->property(WB_COMBO_FILTERED).toBool())
	{
		return;	// already wired; the completer follows the model, so a refill needs no rework
	}

	// == the MFC CBS_DROPDOWN combos: the user can type. MFC did a prefix jump; we filter the
	// popup to the matching entries instead (like the NewSearch tree pickers).
	combo->setEditable(true);
	combo->setInsertPolicy(QComboBox::NoInsert);	// typing must never add a bogus entry

	// An editable combo installs its OWN completer on the line edit when setEditable() runs, so
	// ours must be set afterwards (as here) or it is simply replaced. Set it on the LINE EDIT:
	// QComboBox::setCompleter() is a convenience that forwards to the line edit anyway, and going
	// direct keeps working if the combo's editability is toggled later.
	QCompleter *completer = new QCompleter(combo->model(), combo);
	completer->setCompletionMode(QCompleter::PopupCompletion);
	completer->setCaseSensitivity(Qt::CaseInsensitive);
	completer->setFilterMode(Qt::MatchContains);	// substring, not just prefix
	completer->setCompletionColumn(0);
	completer->setCompletionRole(Qt::DisplayRole);
	if (combo->lineEdit() != NULL)
	{
		combo->lineEdit()->setCompleter(completer);
	}

	combo->setProperty(WB_COMBO_FILTERED, true);

	// Scroll/clamp LAST: setEditable() above rebuilds the combo's internals, so the popup this
	// hooks must be the one that survives.
	applyPopupScroll(combo);
}

void WBQtComboStyle::applySearchable(QComboBox *combo)
{
	if (combo == NULL)
	{
		return;
	}
	if (WBQtConfig_GetComboSearch() == 0)
	{
		// Gated off (the default): leave the combo plain pick-only, exactly as before this
		// existed. Still bound the popup -- that is the separate MFC WS_VSCROLL behaviour.
		if (combo->property(WB_COMBO_SEARCHABLE).toBool())
		{
			// Turned off while this combo was already wired (the team pickers re-apply on
			// every repopulate). Drop back to pick-only rather than leaving a stale text
			// field behind. The completer goes with the line edit setEditable(false) frees.
			combo->setEditable(false);
			combo->setProperty(WB_COMBO_SEARCHABLE, false);
		}
		applyPopupScroll(combo);
		return;
	}
	if (combo->property(WB_COMBO_SEARCHABLE).toBool())
	{
		// Already wired. The proxy tracks the combo's model, so a refill needs no rework --
		// but the popup bound is re-applied below now that the rows can be measured.
		applyPopupScroll(combo);
		return;
	}

	combo->setFocusPolicy(Qt::StrongFocus);
	combo->setEditable(true);
	combo->setInsertPolicy(QComboBox::NoInsert);	// typing must never add a bogus entry

	// The proxy filters the COMPLETER only -- the combo keeps its own model, so item indices
	// (which every panel here uses to talk to the MFC bridge) never shift under the filter.
	QSortFilterProxyModel *proxy = new QSortFilterProxyModel(combo);
	proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
	proxy->setSourceModel(combo->model());

	// UnfilteredPopupCompletion: the completer shows whatever the proxy currently holds rather
	// than applying its own prefix match on top, so the popup IS the filtered list.
	QCompleter *completer = new QCompleter(proxy, combo);
	completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
	completer->setCaseSensitivity(Qt::CaseInsensitive);
	completer->setCompletionColumn(0);
	completer->setCompletionRole(Qt::DisplayRole);

	// setEditable() installs the combo's own completer, so ours must be set after it.
	combo->setCompleter(completer);

	if (combo->lineEdit() != NULL)
	{
		// textEdited (not textChanged): only USER typing re-filters. Setting the field
		// programmatically -- the revert below, or a selection push -- must not narrow the list.
		QObject::connect(combo->lineEdit(), SIGNAL(textEdited(QString)),
			proxy, SLOT(setFilterFixedString(QString)));
		combo->lineEdit()->installEventFilter(new WBQtSearchRevert(combo, proxy));
	}

	combo->setProperty(WB_COMBO_SEARCHABLE, true);

	// Scroll/clamp LAST: setEditable() above rebuilds the combo's internals.
	applyPopupScroll(combo);
}
