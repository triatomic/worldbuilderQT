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

#ifndef GROVEOPTIONS_H
#define GROVEOPTIONS_H

#pragma once

#include <map>
#include <vector>
#include "WorldBuilder.h"
#include "OptionsPanel.h"
#include "Common/STLTypedefs.h"

// Used to store Display names in conjunction with internal names.
typedef std::pair<AsciiString, UnicodeString>							PairNameDisplayName;
typedef std::vector<PairNameDisplayName>									VecPairNameDisplayName;
typedef std::vector<PairNameDisplayName>::iterator				VecPairNameDisplayNameIt;


// This is a utility function useful to get a display string from a pair of AsciiString
// UnicodeStrings. It attempts to use the UnicodeString, and if that fails then turns
// to the AsciiString
// As a last resort, it returns the EmptyString.
UnicodeString GetDisplayNameFromPair(const PairNameDisplayName *pNamePair);

class GroveOptions : public COptionsPanel
{
	protected:
		std::vector<std::pair<Int, Int> >	mVecGroup;
		VecPairNameDisplayName mVecDisplayNames;
		VecPairNameDisplayName mVecDisplayNames_PropsOnly;
		ObjectPreview			m_objectPreview;

		Int	mNumTrees;
	
	public:
		GroveOptions(CWnd* pParent = NULL);
		~GroveOptions();
		void makeMain(void);

		virtual BOOL OnInitDialog();
		int getNumTrees(void);
		int getNumType(int type);
		AsciiString getTypeName(int type);
		int getTotalTreePerc(void);
		Bool getCanPlaceInWater(void);
		Bool getCanPlaceOnCliffs(void);

	protected:
		void _setTreesToLists(void);
		void _buildTreeList(void);
		void _buildTreeListProps(void);
		void _setDefaultRatios(void);
		void _setDefaultNumTrees(void);
		void _setDefaultPlacementAllowed(void);

		void _loadSet(int setIndex);
		void OnSaveSetName();
		void OnSelchangeGroveSetName();

		Bool isUsePropsOnly() const;

		afx_msg void _updateTreeWeights(void);
		afx_msg void _updateTreeCount(void);
		afx_msg void _updateGroveMakeup(void);
		afx_msg void _updatePlacementAllowed(void);
		afx_msg void OnDropDownGroveSetName();
		afx_msg void OnOpenGroveSettings();

		virtual void OnMove(int x, int y);
		virtual void OnOK();
		virtual void OnClose();
#ifdef RTS_HAS_QT
	public:
		// Qt panel support (WBQtGroveBridge): the Qt Grove panel drives these hidden MFC
		// dialog controls so the TheGroveOptions getters GroveTool reads keep returning the
		// right thing. Defined (guarded) in GroveOptions.cpp; instance methods reached via
		// TheGroveOptions from src/WBQtGroveBridge.cpp.
		int  qtGetTreeTypeCount(int type);                 // count for a combo (type 1..11)
		int  qtGetTreeTypeName(int type, int index, char *out, int cap); // combo entry text
		int  qtGetTreeTypeSel(int type);                   // current combo selection
		void qtSetTreeTypeSel(int type, int index);        // set combo selection (+preview)
		int  qtGetWeight(int type);                        // Per<type> edit value
		void qtSetWeight(int type, int value);             // write Per<type> edit + resave
		int  qtGetTotalPerc(void);                         // the running total display
		int  qtGetNumTrees(void);                          // NumberTrees edit value
		void qtSetNumTrees(int value);                     // write NumberTrees edit + persist
		int  qtGetAllowWater(void);
		void qtSetAllowWater(int on);
		int  qtGetAllowCliff(void);
		void qtSetAllowCliff(int on);
		int  qtGetUsePropsOnly(void);
		void qtSetUsePropsOnly(int on);
		int  qtGetSetCount(void);                          // number of named sets
		int  qtGetSetName(int index, char *out, int cap);  // set-name combo entry text
		int  qtGetCurrentSet(void);                        // current set selection
		void qtSelectSet(int index);                       // pick a set (loads its makeup)
		void qtSaveSet(void);                              // Save button
		void qtOpenSettings(void);                         // Settings button
		int  qtGetPreviewSize(int *widthOut, int *heightOut);
		int  qtRenderPreview(unsigned char *bgrOut, int cap); // preview of last-changed combo
#endif
	DECLARE_MESSAGE_MAP()
};

extern GroveOptions *TheGroveOptions;

#endif