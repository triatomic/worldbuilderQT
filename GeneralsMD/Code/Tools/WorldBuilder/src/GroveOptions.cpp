/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "CustomConfigProfile.h"
#include "StdAfx.h"
#include "Common/STLTypedefs.h"
#include "GroveOptions.h"
#include "Common/ThingFactory.h"
#include "Common/ThingSort.h"
#include "Common/ThingTemplate.h"

#define ARBITRARY_BUFF_SIZE		128
#define GROVE_INI_FILE			"Grovesets.ini"

#define MAX_SETS 20
#define TREES_PER_SET 11

/*extern*/ GroveOptions *TheGroveOptions = NULL;

void GroveOptions::makeMain(void)
{
	TheGroveOptions = this;
}

GroveOptions::GroveOptions(CWnd* pParent)
{

}

int GroveOptions::getNumTrees(void)
{
	CWnd* pWnd = GetDlgItem(IDC_Grove_NumberTrees);
	if (!pWnd) {
		return 0;
	}

	static char buff[ARBITRARY_BUFF_SIZE];
	pWnd->GetWindowText(buff, ARBITRARY_BUFF_SIZE - 1);
	return atoi(buff);
}


int GroveOptions::getNumType(int type)
{
	static char buff[ARBITRARY_BUFF_SIZE];

	if (type < 1 || type > 11) {
		return -1;
	}

	// If props-only is enabled, ignore all except type 11
    if (isUsePropsOnly() && type != 11) {
        return 0;
    }

	CWnd *pWnd;
	CComboBox* pBox;
	if (type == 1) {
		pWnd = GetDlgItem(IDC_Grove_Per1);
		pBox = (CComboBox*) GetDlgItem(IDC_Grove_Type1);
	} else if (type == 2) {
		pWnd = GetDlgItem(IDC_Grove_Per2);
		pBox = (CComboBox*) GetDlgItem(IDC_Grove_Type2);
	} else if (type == 3) {
		pWnd = GetDlgItem(IDC_Grove_Per3);
		pBox = (CComboBox*) GetDlgItem(IDC_Grove_Type3);
	} else if (type == 4) {
		pWnd = GetDlgItem(IDC_Grove_Per4);
		pBox = (CComboBox*) GetDlgItem(IDC_Grove_Type4);
	} else if (type == 5) {
		pWnd = GetDlgItem(IDC_Grove_Per5);
		pBox = (CComboBox*) GetDlgItem(IDC_Grove_Type5);
	} else if (type == 6) {
		pWnd = GetDlgItem(IDC_Grove_Per6);
		pBox = (CComboBox*) GetDlgItem(IDC_Grove_Type6);
	} else if (type == 7) {
		pWnd = GetDlgItem(IDC_Grove_Per7);
		pBox = (CComboBox*) GetDlgItem(IDC_Grove_Type7);
	} else if (type == 8) {
		pWnd = GetDlgItem(IDC_Grove_Per8);
		pBox = (CComboBox*) GetDlgItem(IDC_Grove_Type8);
	} else if (type == 9) {
		pWnd = GetDlgItem(IDC_Grove_Per9);
		pBox = (CComboBox*) GetDlgItem(IDC_Grove_Type9);
	} else if (type == 10) {
		pWnd = GetDlgItem(IDC_Grove_Per10);
		pBox = (CComboBox*) GetDlgItem(IDC_Grove_Type10);
	} else if (type == 11) {
		pWnd = GetDlgItem(IDC_Grove_Per11);
		pBox = (CComboBox*) GetDlgItem(IDC_Grove_Type11);
	}

	if (pWnd && pBox) {
		if (pBox->GetCurSel() > 0) {
			pWnd->GetWindowText(buff, ARBITRARY_BUFF_SIZE - 1);
			return atoi(buff);
		}
		return 0;
	}
	return -1;
}

AsciiString GroveOptions::getTypeName(int type)
{
	if (type < 1 || type > 11) {
		return "";
	}

	CComboBox *pComboBox;
	if (type == 1) {
		pComboBox = (CComboBox*) GetDlgItem(IDC_Grove_Type1);
	} else if (type == 2) {
		pComboBox = (CComboBox*) GetDlgItem(IDC_Grove_Type2);
	} else if (type == 3) {
		pComboBox = (CComboBox*) GetDlgItem(IDC_Grove_Type3);
	} else if (type == 4) {
		pComboBox = (CComboBox*) GetDlgItem(IDC_Grove_Type4);
	} else if (type == 5) {
		pComboBox = (CComboBox*) GetDlgItem(IDC_Grove_Type5);
	} else if (type == 6) {
		pComboBox = (CComboBox*) GetDlgItem(IDC_Grove_Type6);
	} else if (type == 7) {
		pComboBox = (CComboBox*) GetDlgItem(IDC_Grove_Type7);
	} else if (type == 8) {
		pComboBox = (CComboBox*) GetDlgItem(IDC_Grove_Type8);
	} else if (type == 9) {
		pComboBox = (CComboBox*) GetDlgItem(IDC_Grove_Type9);
	} else if (type == 10) {
		pComboBox = (CComboBox*) GetDlgItem(IDC_Grove_Type10);
	} else if (type == 11) {
		pComboBox = (CComboBox*) GetDlgItem(IDC_Grove_Type11);
	}

	int curSel = pComboBox->GetCurSel();
	if (curSel < 0 || (type == 11 && curSel >= mVecDisplayNames_PropsOnly.size()) || (type != 11 && curSel >= mVecDisplayNames.size())) {
		return "";
	}

	CString cstr;

	// Retrieve the correct list based on the type
	if (type == 11) {
		pComboBox->GetLBText(curSel, cstr);
		return cstr.GetBuffer(0); // Use mVecDisplayNames_PropsOnly
	} else {
		pComboBox->GetLBText(curSel, cstr);
		return cstr.GetBuffer(0); // Use mVecDisplayNames
	}
}

int GroveOptions::getTotalTreePerc(void)
{
    if (isUsePropsOnly()) {
        return getNumType(11); // only props count
    }
	
	static char buff[ARBITRARY_BUFF_SIZE];

	CWnd* pWnd = GetDlgItem(IDC_Grove_PerTotal);
	if (!pWnd) {
		return -1;
	}

	if (pWnd) {
		pWnd->GetWindowText(buff, ARBITRARY_BUFF_SIZE - 1);
		return atoi(buff);
	}
	return -1;
}

Bool GroveOptions::getCanPlaceInWater(void)
{
	CButton* pButt;

	pButt = (CButton*) GetDlgItem(IDC_Grove_AllowWaterPlacement);
	if (pButt) {
		return (pButt->GetState() != 0);
	}
	return false;
}

Bool GroveOptions::getCanPlaceOnCliffs(void)
{
	CButton* pButt;

	pButt = (CButton*) GetDlgItem(IDC_Grove_AllowCliffPlacement);
	if (pButt) {
		return (pButt->GetState() != 0);
	}
	return false;
}

BOOL GroveOptions::OnInitDialog()
{
	_buildTreeList();
	_buildTreeListProps();


	CWnd *pWnd = GetDlgItem(IDC_OBJECT_HEIGHT_EDIT);
	if (pWnd) {
		CString s;
		s.Format("%d",MAGIC_GROUND_Z);
		pWnd->SetWindowText(s);
	}

	CRect rect;
	pWnd = GetDlgItem(IDC_TERRAIN_SWATCHES);
	pWnd->GetWindowRect(&rect);
	ScreenToClient(&rect);
	rect.DeflateRect(2,2,2,2);
	m_objectPreview.Create(NULL, "", WS_CHILD, rect, this, IDC_TERRAIN_SWATCHES);
	m_objectPreview.ShowWindow(SW_SHOW);

	_setTreesToLists();
	_setDefaultRatios();
	_updateTreeWeights();
	_setDefaultNumTrees();
	_setDefaultPlacementAllowed();
	return true;
}

void GroveOptions::OnMove(int x, int y)
{
  /**
   * Adriane [Deathscythe] -- Bug fix
   * This is required to save the top and left position values.
   * The handler is defined in COptionsPanel and must be called explicitly.
   */
	COptionsPanel::OnMove(x, y); // forward to base 
}

GroveOptions::~GroveOptions()
{
	TheGroveOptions = NULL;
}

/**
 * Adriane [Deatscythe]
 * I got bored talking with chatGPT -- i had to abandon this feature for now
 */
void GroveOptions::OnSaveSetName()
{
// 	DEBUG_LOG(("test\n"));
// 	CComboBox* pSetNameBox = (CComboBox*) GetDlgItem(IDC_Grove_SetName);
// 	if (!pSetNameBox) return;

// 	int selIndex = pSetNameBox->GetCurSel();
// 	if (selIndex == CB_ERR) return;

// 	CString selText;
// 	pSetNameBox->GetWindowText(selText); 
// 	DEBUG_LOG(("testX %s\n", selText));

// 	CString setNameKey;
// 	setNameKey.Format("SetName%d", selIndex);
// 	CustomConfigProfile::WriteString("AdrianeGroveOptions", setNameKey, selText);
}



// void GroveOptions::OnOK() 
// {
// 	CComboBox* pSetNameBox = (CComboBox*) GetDlgItem(IDC_Grove_SetName);
// 	if (pSetNameBox) {
// 		int selIndex = pSetNameBox->GetCurSel();
// 		if (selIndex != CB_ERR) {
// 			CString selText;
// 			pSetNameBox->GetWindowText(selText);
// 			CustomConfigProfile::WriteString("AdrianeGroveOptions", "SetNameText", selText);
	
// 			CString setNameKey;
// 			setNameKey.Format("SetName%d", selIndex);
// 			CustomConfigProfile::WriteString("AdrianeGroveOptions", setNameKey, selText);
// 		}
// 	}
// }

void GroveOptions::OnSelchangeGroveSetName()
{
	CComboBox* pSetNameBox = (CComboBox*) GetDlgItem(IDC_Grove_SetName);
	if (!pSetNameBox) return;

	int selIndex = pSetNameBox->GetCurSel();
	if (selIndex == CB_ERR) return;

	// CString selText;
	// pSetNameBox->GetWindowText(selText); // <- use GetWindowText instead of GetLBText

	// Save to profile
	CustomConfigProfile::WriteInt("AdrianeGroveOptions", "SetNameIndex", selIndex, GROVE_INI_FILE);
	// CustomConfigProfile::WriteString("AdrianeGroveOptions", "SetNameText", selText);

	// CString setNameKey;
	// setNameKey.Format("SetName%d", selIndex);
	// CustomConfigProfile::WriteString("AdrianeGroveOptions", setNameKey, selText);

	_loadSet(selIndex);
}


void GroveOptions::_setTreesToLists()
{
	CString str;
	
	const int treeTypeComboIDs[TREES_PER_SET] = {
		IDC_Grove_Type1,
		IDC_Grove_Type2,
		IDC_Grove_Type3,
		IDC_Grove_Type4,
		IDC_Grove_Type5,
		IDC_Grove_Type6,
		IDC_Grove_Type7,
		IDC_Grove_Type8,
		IDC_Grove_Type9,
		IDC_Grove_Type10,
		IDC_Grove_Type11,
	};

	// Fill all 5 tree type combo boxes with model display names
	for (VecPairNameDisplayNameIt it = mVecDisplayNames.begin(); it != mVecDisplayNames.end(); ++it) {
		str = it->first.str();

		for (int i = 0; i < TREES_PER_SET; ++i) {
			CComboBox* pComboBox = (CComboBox*) GetDlgItem(treeTypeComboIDs[i]);
			if (pComboBox) {
				pComboBox->AddString(str);
			}
		}
	}

	// Add a blank entry at the end for each combo box
	str = "";
	for (int i = 0; i < TREES_PER_SET; ++i) {
		CComboBox* pComboBox = (CComboBox*) GetDlgItem(treeTypeComboIDs[i]);
		if (pComboBox) {
			pComboBox->AddString(str);
		}
	}

	// Fill IDC_Grove_Type11 with model display names from mVecDisplayNames_PropsOnly
	CComboBox* pComboBox11 = (CComboBox*) GetDlgItem(IDC_Grove_Type11);
	if (pComboBox11) {
		pComboBox11->ResetContent(); // Clear existing content
		for (VecPairNameDisplayNameIt it = mVecDisplayNames_PropsOnly.begin(); it != mVecDisplayNames_PropsOnly.end(); ++it) {
			str = it->first.str();
			pComboBox11->AddString(str);
		}
		pComboBox11->AddString(""); // Add a blank entry at the end for consistency
	}

	// Fill Set Name combo box
	CComboBox* pSetNameBox = (CComboBox*) GetDlgItem(IDC_Grove_SetName);
	if (pSetNameBox) {
		pSetNameBox->ResetContent();

		CString setNameKey, setName;
		for (int i = 0; i < MAX_SETS; ++i) {
			setNameKey.Format("SetName%d", i);

			// fallback default like "Set A", "Set B", ...
			CString defaultName;
			defaultName.Format("Set %c", 'A' + i);

			// read from INI
			setName = CustomConfigProfile::ReadString(
				"AdrianeGroveOptions", 
				setNameKey, 
				defaultName, 
				GROVE_INI_FILE
			);

			// if key was missing, ensure it's created in the INI
			if (setName.CompareNoCase(defaultName) == 0) {
				CustomConfigProfile::WriteString("AdrianeGroveOptions", setNameKey, defaultName, GROVE_INI_FILE);
			}

			pSetNameBox->AddString(setName);
		}

		int selIndex = CustomConfigProfile::ReadInt("AdrianeGroveOptions", "SetNameIndex", 0, GROVE_INI_FILE);
		pSetNameBox->SetCurSel(selIndex);
		_loadSet(selIndex);
	}
}

void GroveOptions::_loadSet(int setIndex)
{
    // Read all tree indices in one line
    CString key;
    key.Format("TreeTypeSet%d", setIndex);
    CString line = CustomConfigProfile::ReadString("AdrianeGroveOptions", key, "", GROVE_INI_FILE);

    int indices[TREES_PER_SET] = { 0 };
    int count = 0;

    int start = 0;
    while (count < TREES_PER_SET) {
        int comma = line.Find(',', start);
        CString token = (comma == -1) ? line.Mid(start) : line.Mid(start, comma - start);
        indices[count] = atoi(token);
        if (comma == -1) break;
        start = comma + 1;
        count++;
    }

    // Same array as in _updateGroveMakeup
    const int treeTypeComboIDs[TREES_PER_SET] = {
        IDC_Grove_Type1,
        IDC_Grove_Type2,
        IDC_Grove_Type3,
        IDC_Grove_Type4,
        IDC_Grove_Type5,
        IDC_Grove_Type6,
        IDC_Grove_Type7,
        IDC_Grove_Type8,
        IDC_Grove_Type9,
        IDC_Grove_Type10,
        IDC_Grove_Type11
    };

    // Apply to tree type combo boxes
    for (int i = 0; i < TREES_PER_SET; ++i) {
        CComboBox* pComboBox = (CComboBox*) GetDlgItem(treeTypeComboIDs[i]);
        if (pComboBox) {
            int maxIndex = (i == 10) ? mVecDisplayNames_PropsOnly.size() : mVecDisplayNames.size();
            if (maxIndex > 0) {
                pComboBox->SetCurSel(indices[i] % (maxIndex + 1));
            } else {
                pComboBox->SetCurSel(0);
            }
        }
    }

    _setDefaultRatios();
}


void GroveOptions::_updateGroveMakeup()
{
	// Save current Set Name selection
	CComboBox* pSetNameBox = (CComboBox*)GetDlgItem(IDC_Grove_SetName);
	int setIndex = 0;
	if (pSetNameBox) {
		setIndex = pSetNameBox->GetCurSel();
		CustomConfigProfile::WriteInt("AdrianeGroveOptions", "SetNameIndex", setIndex, GROVE_INI_FILE);
	}

	const int treeTypeComboIDs[TREES_PER_SET] = {
		IDC_Grove_Type1,
		IDC_Grove_Type2,
		IDC_Grove_Type3,
		IDC_Grove_Type4,
		IDC_Grove_Type5,
		IDC_Grove_Type6,
		IDC_Grove_Type7,
		IDC_Grove_Type8,
		IDC_Grove_Type9,
		IDC_Grove_Type10,
		IDC_Grove_Type11
	};

	CString saveLine;
	for (int i = 0; i < TREES_PER_SET; ++i) {
		CComboBox* pComboBox = (CComboBox*) GetDlgItem(treeTypeComboIDs[i]);
		int curSel = (pComboBox ? pComboBox->GetCurSel() : 0);

		CString part;
		part.Format("%d,", curSel);
		saveLine += part;
	}


	saveLine.TrimRight(','); // Remove trailing comma

	CString key;
	key.Format("TreeTypeSet%d", setIndex);
	CustomConfigProfile::WriteString("AdrianeGroveOptions", key, saveLine, GROVE_INI_FILE);

	//--------------------------------------------------------------------------
	// Update the object preview based on whichever dropdown was last changed.
	//--------------------------------------------------------------------------
	// Find the combo box that triggered the update.
	CWnd* pFocus = GetFocus();
	if (!pFocus)
		return;

	int ctrlID = pFocus->GetDlgCtrlID();

	// Only handle tree-type combos
	bool isTreeCombo = false;
	for (int b = 0; b < TREES_PER_SET; ++b)
	{
		if (ctrlID == treeTypeComboIDs[b])
		{
			isTreeCombo = true;
			break;
		}
	}
	if (!isTreeCombo)
		return;

	// Extract template name directly from the dropdown text
	CComboBox* pCombo = (CComboBox*)GetDlgItem(ctrlID);
	if (!pCombo)
		return;

	int sel = pCombo->GetCurSel();
	if (sel <= 0)
	{
		m_objectPreview.SetThingTemplate(NULL);
		m_objectPreview.Invalidate();
		return;
	}

	CString text;
	pCombo->GetLBText(sel, text);

	// Find the template by name
	const ThingTemplate* tpl =
		TheThingFactory->findTemplate(AsciiString(text));

	// Apply preview
	m_objectPreview.SetThingTemplate(tpl);
	m_objectPreview.Invalidate();
}

void GroveOptions::_buildTreeList(void)
{
	const ThingTemplate* pTemplate;
	for (pTemplate = TheThingFactory->firstTemplate(); pTemplate; pTemplate = pTemplate->friend_getNextTemplate()) {
		if (pTemplate->getEditorSorting() == ES_SHRUBBERY) {
			PairNameDisplayName currentName;
			currentName.first = pTemplate->getName();
			currentName.second = pTemplate->getDisplayName();
			mVecDisplayNames.push_back(currentName);
		}
	}
}

void GroveOptions::_buildTreeListProps(void)
{
	const ThingTemplate* pTemplate;
	for (pTemplate = TheThingFactory->firstTemplate(); pTemplate; pTemplate = pTemplate->friend_getNextTemplate()) {
		if (pTemplate->getEditorSorting() == ES_MISC_MAN_MADE || pTemplate->getEditorSorting() == ES_MISC_NATURAL) {
			PairNameDisplayName currentName;
			currentName.first = pTemplate->getName();
			currentName.second = pTemplate->getDisplayName();
			mVecDisplayNames_PropsOnly.push_back(currentName);
		}
	}
}


void GroveOptions::_setDefaultRatios(void)
{
    static char buff[ARBITRARY_BUFF_SIZE];
    const int ids[11] = {
        IDC_Grove_Per1, IDC_Grove_Per2, IDC_Grove_Per3, IDC_Grove_Per4, IDC_Grove_Per5,
        IDC_Grove_Per6, IDC_Grove_Per7, IDC_Grove_Per8, IDC_Grove_Per9, IDC_Grove_Per10, IDC_Grove_Per11
    };

    // which set are we on?
    CComboBox* pSetNameBox = (CComboBox*)GetDlgItem(IDC_Grove_SetName);
    int setIndex = (pSetNameBox ? pSetNameBox->GetCurSel() : 0);

    CString key;
    key.Format("DefaultRatiosSet%d", setIndex);
    CString line = CustomConfigProfile::ReadString("AdrianeGroveOptions", key, "", GROVE_INI_FILE);

    int ratios[11] = { 0 };
    int count = 0, start = 0;
    while (count < 11) {
        int comma = line.Find(',', start);
        CString token = (comma == -1) ? line.Mid(start) : line.Mid(start, comma - start);
        ratios[count] = atoi(token);
        if (comma == -1) break;
        start = comma + 1;
        count++;
    }

    for (int i = 0; i < 11; ++i) {
        CWnd* pWnd = GetDlgItem(ids[i]);
        if (pWnd) {
            sprintf(buff, "%d", ratios[i]);
            pWnd->SetWindowText(buff);
        }
    }
}


void GroveOptions::_setDefaultNumTrees(void)
{
	CWnd* pWnd = GetDlgItem(IDC_Grove_NumberTrees);
	if (!pWnd) {
		return;
	}

	int defaultNumTrees = CustomConfigProfile::ReadInt("AdrianeGroveOptions", "NumberofTrees", 10, GROVE_INI_FILE);
	static char buff[ARBITRARY_BUFF_SIZE];
	sprintf(buff, "%d", defaultNumTrees);

	pWnd->SetWindowText(buff);
}

void GroveOptions::_setDefaultPlacementAllowed(void)
{
	CButton* pButt;
	int state;

	pButt = (CButton*) GetDlgItem(IDC_Grove_AllowCliffPlacement);
	if (pButt) {
		state = CustomConfigProfile::ReadInt("AdrianeGroveOptions", "AllowCliffPlace", 1);
		pButt->SetCheck(state);
	}

	pButt = (CButton*) GetDlgItem(IDC_Grove_AllowWaterPlacement);
	if (pButt) {
		state = CustomConfigProfile::ReadInt("AdrianeGroveOptions", "AllowWaterPlace", 1);
		pButt->SetCheck(state);
	}

	pButt = (CButton*) GetDlgItem(IDC_Grove_UsePropsOnly);
	if (pButt) {
		state = CustomConfigProfile::ReadInt("AdrianeGroveOptions", "UsePropsOnly", 1);
		pButt->SetCheck(state);
	}
}
void GroveOptions::_updateTreeWeights(void)
{
    static char buff[ARBITRARY_BUFF_SIZE];
    const int ids[11] = {
        IDC_Grove_Per1, IDC_Grove_Per2, IDC_Grove_Per3, IDC_Grove_Per4, IDC_Grove_Per5,
        IDC_Grove_Per6, IDC_Grove_Per7, IDC_Grove_Per8, IDC_Grove_Per9, IDC_Grove_Per10, IDC_Grove_Per11
    };

    bool propsOnly = isUsePropsOnly();
    CString saveLine;
    int total = 0;

	for (int i = 0; i < 11; ++i) {
		CWnd* pWnd = GetDlgItem(ids[i]);
		int ratio = 0;
		if (pWnd) {
			pWnd->GetWindowText(buff, ARBITRARY_BUFF_SIZE - 1);
			ratio = atoi(buff);
		}

		// Save always
		CString part;
		part.Format("%d,", ratio);
		saveLine += part;

		// But only add to total if allowed
		if (!propsOnly || i == 10) { 
			total += ratio;
		}
	}

    saveLine.TrimRight(',');

    // save to ini per set
    CComboBox* pSetNameBox = (CComboBox*)GetDlgItem(IDC_Grove_SetName);
    int setIndex = (pSetNameBox ? pSetNameBox->GetCurSel() : 0);

    CString key;
    key.Format("DefaultRatiosSet%d", setIndex);
    CustomConfigProfile::WriteString("AdrianeGroveOptions", key, saveLine, GROVE_INI_FILE);

    // update total display
    CWnd* pWndTotal = GetDlgItem(IDC_Grove_PerTotal);
    if (pWndTotal) {
        sprintf(buff, "%d", total);
        pWndTotal->SetWindowText(buff);
    }

    DEBUG_LOG(("Saved %s = %s\n", (LPCSTR)key, (LPCSTR)saveLine));
}


void GroveOptions::_updateTreeCount(void)
{
	static char buff[ARBITRARY_BUFF_SIZE];	
	CWnd* pWnd = GetDlgItem(IDC_Grove_NumberTrees);
	if (pWnd) {
		pWnd->GetWindowText(buff, ARBITRARY_BUFF_SIZE - 1);
		int val = atoi(buff);
		CustomConfigProfile::WriteInt("AdrianeGroveOptions", "NumberofTrees", val, GROVE_INI_FILE);
	}
}

void GroveOptions::_updatePlacementAllowed(void)
{
	// huh huh huh-huh
	CButton* pButt;

	pButt = (CButton*) GetDlgItem(IDC_Grove_AllowCliffPlacement);
	if (pButt) {
		CustomConfigProfile::WriteInt("AdrianeGroveOptions", "AllowCliffPlace", pButt->GetCheck());
	}

	pButt = (CButton*) GetDlgItem(IDC_Grove_AllowWaterPlacement);
	if (pButt) {
		CustomConfigProfile::WriteInt("AdrianeGroveOptions", "AllowWaterPlace", pButt->GetCheck());
	}

	pButt = (CButton*) GetDlgItem(IDC_Grove_UsePropsOnly);
	if (pButt) {
		CustomConfigProfile::WriteInt("AdrianeGroveOptions", "UsePropsOnly", pButt->GetCheck());
	}


}

Bool GroveOptions::isUsePropsOnly() const
{
    CButton* pButt = (CButton*) GetDlgItem(IDC_Grove_UsePropsOnly);
    return (pButt && pButt->GetCheck() != 0);
}

void GroveOptions::OnOK()
{

}

void GroveOptions::OnClose()
{


}

UnicodeString GetDisplayNameFromPair(const PairNameDisplayName* pNamePair)
{
	if (!pNamePair) {
		return UnicodeString::TheEmptyString;
	}

	if (!pNamePair->second.isEmpty()) {
		return pNamePair->second;
	}

	// The unicode portion of the pair was empty. We need to use the Ascii version instead.
	UnicodeString retStr;
	retStr.translate(pNamePair->first);
	return retStr;
}

void GroveOptions::OnDropDownGroveSetName()
{
    CComboBox* pSetNameBox = (CComboBox*) GetDlgItem(IDC_Grove_SetName);
    if (!pSetNameBox) return;

    pSetNameBox->ResetContent();

    CString setNameKey, setName;
	for (int i = 0; i < MAX_SETS; ++i) {
		setNameKey.Format("SetName%d", i);

		// build default placeholder name
		CString defaultName;
		defaultName.Format("Set %c", 'A' + i);

		// read or fallback
		setName = CustomConfigProfile::ReadString(
			"AdrianeGroveOptions", 
			setNameKey, 
			defaultName, 
			GROVE_INI_FILE
		);

		// if missing, ensure it's written to the INI
		if (setName.CompareNoCase(defaultName) == 0) {
			CustomConfigProfile::WriteString(
				"AdrianeGroveOptions", 
				setNameKey, 
				defaultName, 
				GROVE_INI_FILE
			);
		}

		pSetNameBox->AddString(setName);
	}

    // restore current selection index
    int selIndex = CustomConfigProfile::ReadInt("AdrianeGroveOptions", "SetNameIndex", 0, GROVE_INI_FILE);
    if (selIndex >= 0 && selIndex < MAX_SETS) {
        pSetNameBox->SetCurSel(selIndex);
    }
}

void GroveOptions::OnOpenGroveSettings()
{
	try {
		// Build the path to the INI file
		CString iniPath;
		iniPath.Format("%sGrovesets.ini", TheGlobalData->getPath_UserData().str());

		// Open the file with the default editor (usually Notepad)
		ShellExecute(NULL, "open", iniPath, NULL, NULL, SW_SHOW);

	} catch (...) {
	}
}

BEGIN_MESSAGE_MAP(GroveOptions, CDialog)
	ON_WM_MOVE()
	ON_EN_KILLFOCUS(IDC_Grove_Per1, _updateTreeWeights)
	ON_EN_KILLFOCUS(IDC_Grove_Per2, _updateTreeWeights)
	ON_EN_KILLFOCUS(IDC_Grove_Per3, _updateTreeWeights)
	ON_EN_KILLFOCUS(IDC_Grove_Per4, _updateTreeWeights)
	ON_EN_KILLFOCUS(IDC_Grove_Per5, _updateTreeWeights)
	ON_EN_KILLFOCUS(IDC_Grove_Per6, _updateTreeWeights)
	ON_EN_KILLFOCUS(IDC_Grove_Per7, _updateTreeWeights)
	ON_EN_KILLFOCUS(IDC_Grove_Per8, _updateTreeWeights)
	ON_EN_KILLFOCUS(IDC_Grove_Per9, _updateTreeWeights)
	ON_EN_KILLFOCUS(IDC_Grove_Per10, _updateTreeWeights)
	ON_EN_KILLFOCUS(IDC_Grove_Per11, _updateTreeWeights)
	ON_EN_KILLFOCUS(IDC_Grove_NumberTrees, _updateTreeCount)
	ON_CBN_SELENDOK(IDC_Grove_Type1, _updateGroveMakeup)
	ON_CBN_SELENDOK(IDC_Grove_Type2, _updateGroveMakeup)
	ON_CBN_SELENDOK(IDC_Grove_Type3, _updateGroveMakeup)
	ON_CBN_SELENDOK(IDC_Grove_Type4, _updateGroveMakeup)
	ON_CBN_SELENDOK(IDC_Grove_Type5, _updateGroveMakeup)
	ON_CBN_SELENDOK(IDC_Grove_Type6, _updateGroveMakeup)
	ON_CBN_SELENDOK(IDC_Grove_Type7, _updateGroveMakeup)
	ON_CBN_SELENDOK(IDC_Grove_Type8, _updateGroveMakeup)
	ON_CBN_SELENDOK(IDC_Grove_Type9, _updateGroveMakeup)
	ON_CBN_SELENDOK(IDC_Grove_Type10, _updateGroveMakeup)
	ON_CBN_SELENDOK(IDC_Grove_Type11, _updateGroveMakeup)
	ON_BN_CLICKED(IDC_Grove_AllowCliffPlacement, _updatePlacementAllowed)
	ON_BN_CLICKED(IDC_Grove_AllowWaterPlacement, _updatePlacementAllowed)
	ON_BN_CLICKED(IDC_Grove_UsePropsOnly, _updatePlacementAllowed)
	ON_CBN_SELCHANGE(IDC_Grove_SetName, OnSelchangeGroveSetName)
	ON_CBN_DROPDOWN(IDC_Grove_SetName, OnDropDownGroveSetName)
	ON_BN_CLICKED(IDC_Grove_SaveSet, OnSaveSetName)
	ON_BN_CLICKED(IDC_Grove_Settings, OnOpenGroveSettings)
END_MESSAGE_MAP()

#ifdef RTS_HAS_QT
//----------------------------------------------------------------------------------------
// GroveOptions Qt-support methods (declared in GroveOptions.h). The Qt Grove panel drives
// the hidden MFC dialog controls through these so the TheGroveOptions getters GroveTool
// reads (getNumTrees / getNumType / getTypeName / getTotalTreePerc / getCanPlace*) keep
// returning the right values. They reuse the same GetDlgItem paths and the existing On*
// handlers so behaviour matches the MFC panel exactly.
//----------------------------------------------------------------------------------------

// Combo/edit control IDs by type (1..11), mirroring the arrays used throughout this file.
static const int kGroveTypeComboIDs[TREES_PER_SET] = {
	IDC_Grove_Type1, IDC_Grove_Type2, IDC_Grove_Type3, IDC_Grove_Type4, IDC_Grove_Type5,
	IDC_Grove_Type6, IDC_Grove_Type7, IDC_Grove_Type8, IDC_Grove_Type9, IDC_Grove_Type10,
	IDC_Grove_Type11
};
static const int kGrovePerIDs[TREES_PER_SET] = {
	IDC_Grove_Per1, IDC_Grove_Per2, IDC_Grove_Per3, IDC_Grove_Per4, IDC_Grove_Per5,
	IDC_Grove_Per6, IDC_Grove_Per7, IDC_Grove_Per8, IDC_Grove_Per9, IDC_Grove_Per10,
	IDC_Grove_Per11
};

// The MFC preview is a fixed 128x128 BGR image (see the PREVIEW dimensions in ObjectPreview.cpp,
// which are file-scope there, so mirror the literals here like the other Qt bridges do).
#define WBQT_GROVE_PREVIEW_W 128
#define WBQT_GROVE_PREVIEW_H 128

// Which tree-type combo last drove the preview (1..11), 0 == none. Only one GroveOptions
// instance ever exists (TheGroveOptions), so a file-scope latch is sufficient and avoids
// changing the class layout.
static int gGroveQtPreviewType = 0;

int GroveOptions::qtGetTreeTypeCount(int type)
{
	if (type < 1 || type > TREES_PER_SET)
	{
		return 0;
	}
	CComboBox *pBox = (CComboBox*) GetDlgItem(kGroveTypeComboIDs[type - 1]);
	if (pBox == NULL)
	{
		return 0;
	}
	return pBox->GetCount();
}

int GroveOptions::qtGetTreeTypeName(int type, int index, char *out, int cap)
{
	if (out == NULL || cap <= 0)
	{
		return 0;
	}
	out[0] = 0;
	if (type < 1 || type > TREES_PER_SET)
	{
		return 0;
	}
	CComboBox *pBox = (CComboBox*) GetDlgItem(kGroveTypeComboIDs[type - 1]);
	if (pBox == NULL || index < 0 || index >= pBox->GetCount())
	{
		return 0;
	}
	CString cstr;
	pBox->GetLBText(index, cstr);
	strncpy(out, (LPCSTR)cstr, cap - 1);
	out[cap - 1] = 0;
	return 1;
}

int GroveOptions::qtGetTreeTypeSel(int type)
{
	if (type < 1 || type > TREES_PER_SET)
	{
		return -1;
	}
	CComboBox *pBox = (CComboBox*) GetDlgItem(kGroveTypeComboIDs[type - 1]);
	if (pBox == NULL)
	{
		return -1;
	}
	return pBox->GetCurSel();
}

void GroveOptions::qtSetTreeTypeSel(int type, int index)
{
	if (type < 1 || type > TREES_PER_SET)
	{
		return;
	}
	CComboBox *pBox = (CComboBox*) GetDlgItem(kGroveTypeComboIDs[type - 1]);
	if (pBox == NULL)
	{
		return;
	}
	pBox->SetCurSel(index);
	gGroveQtPreviewType = type;
	// Persist the makeup exactly like the MFC ON_CBN_SELENDOK handler would. The preview
	// there is driven off GetFocus(); here we render it explicitly via qtRenderPreview.
	_updateGroveMakeup();
}

int GroveOptions::qtGetWeight(int type)
{
	if (type < 1 || type > TREES_PER_SET)
	{
		return 0;
	}
	CWnd *pWnd = GetDlgItem(kGrovePerIDs[type - 1]);
	if (pWnd == NULL)
	{
		return 0;
	}
	char buff[ARBITRARY_BUFF_SIZE];
	pWnd->GetWindowText(buff, ARBITRARY_BUFF_SIZE - 1);
	return atoi(buff);
}

void GroveOptions::qtSetWeight(int type, int value)
{
	if (type < 1 || type > TREES_PER_SET)
	{
		return;
	}
	CWnd *pWnd = GetDlgItem(kGrovePerIDs[type - 1]);
	if (pWnd == NULL)
	{
		return;
	}
	char buff[ARBITRARY_BUFF_SIZE];
	sprintf(buff, "%d", value);
	pWnd->SetWindowText(buff);
	// Recompute + persist the weights and refresh the total, like ON_EN_KILLFOCUS.
	_updateTreeWeights();
}

int GroveOptions::qtGetTotalPerc(void)
{
	return getTotalTreePerc();
}

int GroveOptions::qtGetNumTrees(void)
{
	return getNumTrees();
}

void GroveOptions::qtSetNumTrees(int value)
{
	CWnd *pWnd = GetDlgItem(IDC_Grove_NumberTrees);
	if (pWnd == NULL)
	{
		return;
	}
	char buff[ARBITRARY_BUFF_SIZE];
	sprintf(buff, "%d", value);
	pWnd->SetWindowText(buff);
	_updateTreeCount();
}

int GroveOptions::qtGetAllowWater(void)
{
	return getCanPlaceInWater() ? 1 : 0;
}

void GroveOptions::qtSetAllowWater(int on)
{
	CButton *pButt = (CButton*) GetDlgItem(IDC_Grove_AllowWaterPlacement);
	if (pButt != NULL)
	{
		pButt->SetCheck(on ? 1 : 0);
		_updatePlacementAllowed();
	}
}

int GroveOptions::qtGetAllowCliff(void)
{
	return getCanPlaceOnCliffs() ? 1 : 0;
}

void GroveOptions::qtSetAllowCliff(int on)
{
	CButton *pButt = (CButton*) GetDlgItem(IDC_Grove_AllowCliffPlacement);
	if (pButt != NULL)
	{
		pButt->SetCheck(on ? 1 : 0);
		_updatePlacementAllowed();
	}
}

int GroveOptions::qtGetUsePropsOnly(void)
{
	return isUsePropsOnly() ? 1 : 0;
}

void GroveOptions::qtSetUsePropsOnly(int on)
{
	CButton *pButt = (CButton*) GetDlgItem(IDC_Grove_UsePropsOnly);
	if (pButt != NULL)
	{
		pButt->SetCheck(on ? 1 : 0);
		_updatePlacementAllowed();
		// Props-only changes what the totals count; refresh the display like the MFC panel.
		_updateTreeWeights();
	}
}

int GroveOptions::qtGetSetCount(void)
{
	CComboBox *pBox = (CComboBox*) GetDlgItem(IDC_Grove_SetName);
	if (pBox == NULL)
	{
		return 0;
	}
	return pBox->GetCount();
}

int GroveOptions::qtGetSetName(int index, char *out, int cap)
{
	if (out == NULL || cap <= 0)
	{
		return 0;
	}
	out[0] = 0;
	CComboBox *pBox = (CComboBox*) GetDlgItem(IDC_Grove_SetName);
	if (pBox == NULL || index < 0 || index >= pBox->GetCount())
	{
		return 0;
	}
	CString cstr;
	pBox->GetLBText(index, cstr);
	strncpy(out, (LPCSTR)cstr, cap - 1);
	out[cap - 1] = 0;
	return 1;
}

int GroveOptions::qtGetCurrentSet(void)
{
	CComboBox *pBox = (CComboBox*) GetDlgItem(IDC_Grove_SetName);
	if (pBox == NULL)
	{
		return 0;
	}
	return pBox->GetCurSel();
}

void GroveOptions::qtSelectSet(int index)
{
	CComboBox *pBox = (CComboBox*) GetDlgItem(IDC_Grove_SetName);
	if (pBox == NULL)
	{
		return;
	}
	pBox->SetCurSel(index);
	// Mirror OnSelchangeGroveSetName: persist the index and load the set makeup + ratios.
	CustomConfigProfile::WriteInt("AdrianeGroveOptions", "SetNameIndex", index, GROVE_INI_FILE);
	_loadSet(index);
}

void GroveOptions::qtSaveSet(void)
{
	OnSaveSetName();
}

void GroveOptions::qtOpenSettings(void)
{
	OnOpenGroveSettings();
}

int GroveOptions::qtGetPreviewSize(int *widthOut, int *heightOut)
{
	if (widthOut != NULL)
	{
		*widthOut = WBQT_GROVE_PREVIEW_W;
	}
	if (heightOut != NULL)
	{
		*heightOut = WBQT_GROVE_PREVIEW_H;
	}
	return 1;
}

int GroveOptions::qtRenderPreview(unsigned char *bgrOut, int cap)
{
	if (bgrOut == NULL || cap < WBQT_GROVE_PREVIEW_W * WBQT_GROVE_PREVIEW_H * 3)
	{
		return 0;
	}
	// Resolve the template from the combo that last drove the preview, mirroring the MFC
	// _updateGroveMakeup preview branch (sel <= 0 == the blank entry == no preview).
	const ThingTemplate *tpl = NULL;
	if (gGroveQtPreviewType >= 1 && gGroveQtPreviewType <= TREES_PER_SET)
	{
		CComboBox *pCombo = (CComboBox*) GetDlgItem(kGroveTypeComboIDs[gGroveQtPreviewType - 1]);
		if (pCombo != NULL)
		{
			int sel = pCombo->GetCurSel();
			if (sel > 0)
			{
				CString text;
				pCombo->GetLBText(sel, text);
				tpl = TheThingFactory->findTemplate(AsciiString((LPCSTR)text));
			}
		}
	}
	const UnsignedByte *data = ObjectPreview::qtRenderTemplatePreview(tpl);
	if (data == NULL)
	{
		return 0;
	}
	memcpy(bgrOut, data, WBQT_GROVE_PREVIEW_W * WBQT_GROVE_PREVIEW_H * 3);
	return 1;
}
#endif