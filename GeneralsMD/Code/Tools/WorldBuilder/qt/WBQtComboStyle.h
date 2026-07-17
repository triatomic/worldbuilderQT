// WBQtComboStyle.h -- shared behaviour for the migrated Qt combo boxes.
//
// Two MFC behaviours the straight Qt port lost:
//
// 1. SCROLLBARS / POPUP HEIGHT. Every MFC COMBOBOX in WorldBuilder.rc carries WS_VSCROLL and a
//    FIXED dropped height (the COMBOBOX's 4th value: 60..329 dialog units, median 173 -- i.e.
//    ~37..206px, never more than a couple of hundred). A long list scrolls inside that. Qt
//    instead grows the popup to fit the item count, so a big list (Sound, Object) drops a
//    full-screen-tall column. applyPopupScroll() clamps the popup and lets it scroll.
//
// 2. TYPING. MFC's CBS_DROPDOWN combos accept typed text (CBS_DROPDOWNLIST ones don't). Only
//    four controls in the .rc are CBS_DROPDOWN: IDC_FIND_QUERY_OBJ / IDC_FIND_QUERY_WP (the
//    Entity Finder's object + waypoint finders), IDC_REBUILDS (Build List) and
//    IDC_TRANSPORT_COMBO (Team Sheet). applyTypeToFilter() gives exactly those the typing
//    behaviour -- and goes one better than MFC's prefix jump by FILTERING the popup to the
//    matching entries (substring, case-insensitive), matching the NewSearch live-filter the tree
//    pickers already use. Pick-only combos are left alone, as in MFC.
#ifndef WB_QT_COMBO_STYLE_H
#define WB_QT_COMBO_STYLE_H

class QComboBox;
class QWidget;

namespace WBQtComboStyle
{
	// Give `combo`'s drop-down an always-on vertical scrollbar and a bounded popup height
	// (== the MFC WS_VSCROLL drop-downs). Safe on any combo; a NULL combo is ignored.
	// Call AFTER the combo is populated, or on an empty combo -- it does not depend on items.
	void applyPopupScroll(QComboBox *combo);

	// applyPopupScroll() for every QComboBox under `root` (findChildren), so a dialog gets the
	// MFC scroll behaviour in one call after setupUi() instead of naming each combo -- and any
	// combo added to its .ui later is covered automatically. A NULL root is ignored.
	void applyPopupScrollRecursive(QWidget *root);

	// Make `combo` editable and filter its drop-down to the entries matching the typed text.
	// Implies applyPopupScroll(). Only for the combos MFC marked CBS_DROPDOWN (see above).
	// The combo keeps emitting its usual signals; the filter only affects what the popup shows.
	// Call AFTER the combo is populated; safe to re-call when the item list is rebuilt.
	void applyTypeToFilter(QComboBox *combo);
}

#endif // WB_QT_COMBO_STYLE_H
