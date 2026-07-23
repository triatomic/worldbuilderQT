// WBQtScriptEditBridge.cpp -- MFC side of the Qt script-edit-dialog seam (Tier 2a/2b). Plain
// MFC TU (no Qt include). Implements the Qt->MFC half of qt/panels/WBQtScriptEditBridge.h:
// reads/writes the caller-owned Script* and performs the condition/action list surgery, porting
// the property-page handlers (ScriptProperties / ScriptConditionsDlg / ScriptActionsTrue/False)
// verbatim -- same insertion points, same selection-after-rebuild semantics. The New/Edit ops
// pop the Qt condition/action editor (WBQtCondAct_Run, Tier 2b) where the pages popped the MFC
// EditCondition / EditAction modals. Whole body guarded by RTS_HAS_QT so the OFF build compiles
// it to an empty object.
#include "StdAfx.h"
#include "resource.h"				// IDD_ScriptDialog (used in ScriptDialog.h's IDD enum)
#include "Lib/BaseType.h"
#include "WorldBuilderDoc.h"		// MapObject (ScriptDialog.h has a MapObject* member)
#include "GameLogic/PolygonTrigger.h"	// PolygonTrigger (ditto)
#include "GameLogic/Scripts.h"
#include "ScriptDialog.h"			// updateScriptWarning + SCRIPT_DIALOG_SECTION
#include "qt/panels/WBQtScriptEditBridge.h"
#include "qt/panels/WBQtCondActBridge.h"

#ifdef RTS_HAS_QT

// Registered by the Qt script-edit dialog while it runs; kept for MFC sub-modals that still
// need an explicit owner (the parameter editors resolve theirs via GetActiveWindow).
static HWND s_qtModalOwner = NULL;

extern "C" void WBQtScriptEdit_SetModalOwner(void *hwnd)
{
	s_qtModalOwner = reinterpret_cast<HWND>(hwnd);
}

static void copyOut(const AsciiString &str, char *buf, int cap)
{
	if (buf == NULL || cap <= 0)
	{
		return;
	}
	strncpy(buf, str.str(), cap - 1);
	buf[cap - 1] = 0;
}

// ================= Properties tab =================

extern "C" void WBQtScriptEditData_GetText(void *script, int field, char *buf, int cap)
{
	Script *pScript = static_cast<Script *>(script);
	switch (field)
	{
		case WB_QT_SCRIPTEDIT_TEXT_NAME:
			copyOut(pScript->getName(), buf, cap);
			break;
		case WB_QT_SCRIPTEDIT_TEXT_COMMENT:
			copyOut(pScript->getComment(), buf, cap);
			break;
		case WB_QT_SCRIPTEDIT_TEXT_CONDITION_COMMENT:
			copyOut(pScript->getConditionComment(), buf, cap);
			break;
		case WB_QT_SCRIPTEDIT_TEXT_ACTION_COMMENT:
			copyOut(pScript->getActionComment(), buf, cap);
			break;
		default:
			if (buf != NULL && cap > 0)
			{
				buf[0] = 0;
			}
			break;
	}
}

extern "C" void WBQtScriptEditData_SetText(void *script, int field, const char *text)
{
	Script *pScript = static_cast<Script *>(script);
	AsciiString str(text ? text : "");
	switch (field)
	{
		case WB_QT_SCRIPTEDIT_TEXT_NAME:
			pScript->setName(str);
			break;
		case WB_QT_SCRIPTEDIT_TEXT_COMMENT:
			pScript->setComment(str);
			break;
		case WB_QT_SCRIPTEDIT_TEXT_CONDITION_COMMENT:
			pScript->setConditionComment(str);
			break;
		case WB_QT_SCRIPTEDIT_TEXT_ACTION_COMMENT:
			pScript->setActionComment(str);
			break;
		default:
			break;
	}
}

extern "C" int WBQtScriptEditData_GetFlag(void *script, int flag)
{
	Script *pScript = static_cast<Script *>(script);
	switch (flag)
	{
		case WB_QT_SCRIPTEDIT_FLAG_ACTIVE:		return pScript->isActive() ? 1 : 0;
		case WB_QT_SCRIPTEDIT_FLAG_ONE_SHOT:	return pScript->isOneShot() ? 1 : 0;
		case WB_QT_SCRIPTEDIT_FLAG_EASY:		return pScript->isEasy() ? 1 : 0;
		case WB_QT_SCRIPTEDIT_FLAG_NORMAL:		return pScript->isNormal() ? 1 : 0;
		case WB_QT_SCRIPTEDIT_FLAG_HARD:		return pScript->isHard() ? 1 : 0;
		case WB_QT_SCRIPTEDIT_FLAG_SUBROUTINE:	return pScript->isSubroutine() ? 1 : 0;
		default:								return 0;
	}
}

extern "C" void WBQtScriptEditData_SetFlag(void *script, int flag, int value)
{
	Script *pScript = static_cast<Script *>(script);
	Bool on = (value != 0);
	switch (flag)
	{
		case WB_QT_SCRIPTEDIT_FLAG_ACTIVE:		pScript->setActive(on); break;
		case WB_QT_SCRIPTEDIT_FLAG_ONE_SHOT:	pScript->setOneShot(on); break;
		case WB_QT_SCRIPTEDIT_FLAG_EASY:		pScript->setEasy(on); break;
		case WB_QT_SCRIPTEDIT_FLAG_NORMAL:		pScript->setNormal(on); break;
		case WB_QT_SCRIPTEDIT_FLAG_HARD:		pScript->setHard(on); break;
		case WB_QT_SCRIPTEDIT_FLAG_SUBROUTINE:	pScript->setSubroutine(on); break;
		default: break;
	}
}

extern "C" int WBQtScriptEditData_GetDelaySeconds(void *script)
{
	return static_cast<Script *>(script)->getDelayEvalSeconds();
}

extern "C" void WBQtScriptEditData_SetDelaySeconds(void *script, int seconds)
{
	static_cast<Script *>(script)->setDelayEvalSeconds(seconds);
}

// ================= Smart Copy setting =================
// == ScriptConditionsDlg/ScriptActionsTrue::OnSmartCopy + the OnSetActive profile read.

extern "C" int WBQtScriptEdit_GetSmartCopy(void)
{
	return ::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "SmartCopyDeep", 0) ? 1 : 0;
}

extern "C" void WBQtScriptEdit_SetSmartCopy(int enabled)
{
	::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "SmartCopyDeep", enabled ? 1 : 0);
	if (enabled && !::AfxGetApp()->GetProfileInt("ToolTips", "SmartCopyInfoShown", 0))
	{
		AfxMessageBox(
			"This feature will auto increment values on your copied script's parameters\n\n"
			"Example:   Add  1  to counter 'Counter01' -> click copy ->   Add  1  to counter 'Counter02'\n\n"
			"Note: This does not support all parameters. Contact Adriane if you want other parameters to be supported adios.",
			MB_OK | MB_ICONINFORMATION
		);
		::AfxGetApp()->WriteProfileInt("ToolTips", "SmartCopyInfoShown", 1);
	}
}

// == ScriptConditionsDlg/ScriptActionsTrue::incrementStringNumber (identical copies there).
static AsciiString qtIncrementStringNumber(const AsciiString &input)
{
	const char *str = input.str();
	int len = strlen(str);

	int pos = len - 1;
	while (pos >= 0 && isdigit(str[pos]))
	{
		pos--;
	}

	if (pos == len - 1)
	{
		return input;
	}

	CString prefix(str, pos + 1);
	CString numberStr(str + pos + 1);
	int number = atoi(numberStr);
	number++;

	CString result;
	result.Format("%s%0*d", prefix, numberStr.GetLength(), number);
	return AsciiString(result);
}

// Shared parameter sweep of applySmartCopyToCondition / applySmartCopyToAction (their type
// lists are identical); pass each Parameter through the increment.
static void qtApplySmartCopyToParameter(Parameter *param)
{
	if (param == NULL)
	{
		return;
	}
	if (
		param->getParameterType() == Parameter::TEXT_STRING ||
		param->getParameterType() == Parameter::TEAM ||
		param->getParameterType() == Parameter::WAYPOINT ||
		param->getParameterType() == Parameter::SCRIPT ||
		param->getParameterType() == Parameter::SCRIPT_SUBROUTINE ||
		param->getParameterType() == Parameter::UNIT ||
		param->getParameterType() == Parameter::REVEALNAME ||
		param->getParameterType() == Parameter::COUNTER ||
		param->getParameterType() == Parameter::FLAG ||
		param->getParameterType() == Parameter::SIDE
	)
	{
		AsciiString newVal = qtIncrementStringNumber(param->getString());
		param->friend_setString(newVal);
	}
}

// ================= Conditions tab =================
// Row model == the MFC listbox rows: per OrCondition an "*** IF ***"/"*** OR ***" header row,
// then one row per Condition in that block.

// Resolve a row to its (orCondition, condition) pair == OnSelchangeConditionList's walk.
// Returns true if the row exists; *outCond is NULL for an OR header row.
static Bool qtResolveConditionRow(Script *pScript, int row, OrCondition **outOr, Condition **outCond)
{
	*outOr = NULL;
	*outCond = NULL;
	if (row < 0)
	{
		return false;
	}
	int count = row + 1;
	OrCondition *pOr = pScript->getOrCondition();
	while (pOr)
	{
		count--;
		if (count == 0)
		{
			*outOr = pOr;
			return true;
		}
		Condition *pCond = pOr->getFirstAndCondition();
		while (pCond)
		{
			count--;
			if (count == 0)
			{
				*outOr = pOr;
				*outCond = pCond;
				return true;
			}
			pCond = pCond->getNext();
		}
		pOr = pOr->getNextOrCondition();
	}
	return false;
}

// Row of a (orCondition, condition) pair after surgery == setSel's walk (condition matched by
// pointer across ALL blocks; pCond NULL selects pOr's header row). Returns -1 if not found.
static int qtFindConditionRow(Script *pScript, OrCondition *pOr, Condition *pCond)
{
	int count = 0;
	OrCondition *pCur = pScript->getOrCondition();
	while (pCur)
	{
		if (pCur == pOr && pCond == NULL)
		{
			return count;
		}
		count++;
		Condition *pC = pCur->getFirstAndCondition();
		while (pC)
		{
			if (pC == pCond)
			{
				return count;
			}
			count++;
			pC = pC->getNext();
		}
		pCur = pCur->getNextOrCondition();
	}
	return -1;
}

extern "C" int WBQtScriptEditData_GetConditionRowCount(void *script)
{
	Script *pScript = static_cast<Script *>(script);
	// loadList() refreshed the warning state on every rebuild; the Qt tab reloads through here.
	ScriptDialog::updateScriptWarning(pScript);
	int count = 0;
	OrCondition *pOr = pScript->getOrCondition();
	while (pOr)
	{
		count++;
		Condition *pCond = pOr->getFirstAndCondition();
		while (pCond)
		{
			count++;
			pCond = pCond->getNext();
		}
		pOr = pOr->getNextOrCondition();
	}
	return count;
}

extern "C" int WBQtScriptEditData_GetConditionRow(void *script, int row, char *buf, int cap)
{
	Script *pScript = static_cast<Script *>(script);
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	if (row < 0)
	{
		return -1;
	}
	// == loadList()'s label building.
	int cur = 0;
	int orIndex = 0;
	OrCondition *pOr = pScript->getOrCondition();
	while (pOr)
	{
		if (cur == row)
		{
			copyOut(AsciiString((orIndex == 0) ? "*** IF ***" : "*** OR ***"), buf, cap);
			return 0;
		}
		cur++;
		Condition *pCond = pOr->getFirstAndCondition();
		Bool first = true;
		while (pCond)
		{
			if (cur == row)
			{
				AsciiString label;
				if (first)
				{
					label = "  ";
				}
				else
				{
					label = "  *AND* ";
				}
				label.concat(pCond->getUiText());
				copyOut(label, buf, cap);
				return 1;
			}
			first = false;
			cur++;
			pCond = pCond->getNext();
		}
		orIndex++;
		pOr = pOr->getNextOrCondition();
	}
	return -1;
}

// Does this parameter carry `value` as an OBJECT_TYPE? "???" (the ui placeholder the [Missing]
// tag shows for empty values) matches an empty parameter string.
static Bool paramCarriesObjectValue(Parameter *param, const AsciiString &value, Bool wantEmpty)
{
	if (param == NULL || param->getParameterType() != Parameter::OBJECT_TYPE)
	{
		return false;
	}
	if (wantEmpty)
	{
		return param->getString().isEmpty();
	}
	return param->getString() == value;
}

// Locate the first condition/action of the CURRENT script carrying an OBJECT_TYPE parameter with
// this value ("???" matches empty values). tabOut = the edit dialog's tab (1=Conditions,
// 2=Actions if true, 3=Actions if false); rowOut = the row in that tab's list. Lives HERE, next
// to GetConditionRow/qtResolveConditionRow, because the conditions half must count the
// *** IF ***/*** OR *** header rows exactly the way those enumerate them -- if the list's row
// model ever changes, all of them change together. Returns 1 when found, 0 otherwise. Backs the
// detail pane's [Missing] links (declared in qt/WBQtPanelBridge.h).
extern "C" int WBQtScript_FindObjectParamLocation(const char *value, int *tabOut, int *rowOut)
{
	if (tabOut != NULL) { *tabOut = -1; }
	if (rowOut != NULL) { *rowOut = -1; }
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	Script *pScript = (dlg != NULL) ? dlg->qtCurScript() : NULL;
	if (pScript == NULL || value == NULL)
	{
		return 0;
	}
	AsciiString wanted(value);
	Bool wantEmpty = (wanted == "???");

	// Conditions: rows mirror the edit dialog's list (an IF/OR header row per OrCondition,
	// == GetConditionRow above).
	int row = 0;
	for (OrCondition *pOr = pScript->getOrCondition(); pOr != NULL; pOr = pOr->getNextOrCondition())
	{
		row++;	// the *** IF *** / *** OR *** header row
		for (Condition *c = pOr->getFirstAndCondition(); c != NULL; c = c->getNext())
		{
			for (int p = 0; p < c->getNumParameters(); ++p)
			{
				if (paramCarriesObjectValue(c->getParameter(p), wanted, wantEmpty))
				{
					if (tabOut != NULL) { *tabOut = 1; }
					if (rowOut != NULL) { *rowOut = row; }
					return 1;
				}
			}
			row++;
		}
	}

	// Action lists: plain 0-based indexes (tab 2 = if true, tab 3 = if false).
	for (int pass = 0; pass < 2; ++pass)
	{
		int index = 0;
		ScriptAction *a = (pass == 0) ? pScript->getAction() : pScript->getFalseAction();
		for (; a != NULL; a = a->getNext())
		{
			for (int p = 0; p < a->getNumParameters(); ++p)
			{
				if (paramCarriesObjectValue(a->getParameter(p), wanted, wantEmpty))
				{
					if (tabOut != NULL) { *tabOut = 2 + pass; }
					if (rowOut != NULL) { *rowOut = index; }
					return 1;
				}
			}
			index++;
		}
	}
	return 0;
}

extern "C" int WBQtScriptEdit_ConditionNew(void *script, int row)
{
	// == ScriptConditionsDlg::OnNew.
	Script *pScript = static_cast<Script *>(script);
	OrCondition *pSelOr = NULL;
	Condition *pSelCond = NULL;
	qtResolveConditionRow(pScript, row, &pSelOr, &pSelCond);

	Condition *pCond = newInstance( Condition)(Condition::CONDITION_TRUE);
	if (WBQtCondAct_Run(pCond, 0) != 0)
	{
		if (pSelCond)
		{
			pCond->setNextCondition(pSelCond->getNext());
			pSelCond->setNextCondition(pCond);
		}
		else
		{
			if (pSelOr == NULL)
			{
				OrCondition *pOr = newInstance( OrCondition);
				pOr->setNextOrCondition(pScript->getOrCondition());
				pScript->setOrCondition(pOr);
				pSelOr = pOr;
			}
			pCond->setNextCondition(pSelOr->getFirstAndCondition());
			pSelOr->setFirstAndCondition(pCond);
		}
		return qtFindConditionRow(pScript, pSelOr, pCond);
	}
	pCond->deleteInstance();
	return -1;
}

extern "C" int WBQtScriptEdit_ConditionEdit(void *script, int row)
{
	// == ScriptConditionsDlg::OnEditCondition (modal result intentionally ignored there too).
	Script *pScript = static_cast<Script *>(script);
	OrCondition *pSelOr = NULL;
	Condition *pSelCond = NULL;
	if (!qtResolveConditionRow(pScript, row, &pSelOr, &pSelCond) || pSelCond == NULL)
	{
		return -1;
	}
	WBQtCondAct_Run(pSelCond, 0);
	ScriptDialog::updateScriptWarning(pScript);
	return qtFindConditionRow(pScript, pSelOr, pSelCond);
}

extern "C" int WBQtScriptEdit_ConditionOr(void *script, int row)
{
	// == ScriptConditionsDlg::OnOr.
	Script *pScript = static_cast<Script *>(script);
	OrCondition *pSelOr = NULL;
	Condition *pSelCond = NULL;
	qtResolveConditionRow(pScript, row, &pSelOr, &pSelCond);

	OrCondition *pOr = newInstance( OrCondition);
	if (pSelOr)
	{
		pOr->setNextOrCondition(pSelOr->getNextOrCondition());
		pSelOr->setNextOrCondition(pOr);
	}
	else
	{
		pOr->setNextOrCondition(pScript->getOrCondition());
		pScript->setOrCondition(pOr);
	}
	return qtFindConditionRow(pScript, pOr, NULL);
}

extern "C" int WBQtScriptEdit_ConditionCopy(void *script, int row)
{
	// == ScriptConditionsDlg::OnCopy.
	Script *pScript = static_cast<Script *>(script);
	OrCondition *pSelOr = NULL;
	Condition *pSelCond = NULL;
	if (!qtResolveConditionRow(pScript, row, &pSelOr, &pSelCond) || pSelCond == NULL)
	{
		return -1;
	}
	Condition *pCopy = pSelCond->duplicate();
	if (WBQtScriptEdit_GetSmartCopy())
	{
		// == applySmartCopyToCondition.
		for (int i = 0; i < pCopy->getNumParameters(); ++i)
		{
			qtApplySmartCopyToParameter(pCopy->getParameter(i));
		}
	}
	pCopy->setNextCondition(pSelCond->getNext());
	pSelCond->setNextCondition(pCopy);
	return qtFindConditionRow(pScript, pSelOr, pCopy);
}

// ---- cross-script clipboard (app lifetime) ----
// Copying a condition/action stashes a duplicate here; pasting duplicates it back into the target
// script. Kept as raw Condition*/ScriptAction* owned by the bridge, freed when replaced. Separate
// holders keep conditions and actions type-safe: a condition can only paste into a conditions list,
// an action only into an actions list.
static Condition   *s_clipCondition = NULL;
static ScriptAction *s_clipAction   = NULL;

extern "C" int WBQtScriptEdit_ConditionHasClipboard(void)
{
	return (s_clipCondition != NULL) ? 1 : 0;
}

extern "C" int WBQtScriptEdit_ConditionCopyToClipboard(void *script, int row)
{
	Script *pScript = static_cast<Script *>(script);
	OrCondition *pSelOr = NULL;
	Condition *pSelCond = NULL;
	if (!qtResolveConditionRow(pScript, row, &pSelOr, &pSelCond) || pSelCond == NULL)
	{
		return -1;	// no condition selected (an OR header row, or nothing)
	}
	if (s_clipCondition != NULL)
	{
		s_clipCondition->deleteInstance();
	}
	s_clipCondition = pSelCond->duplicate();
	return 1;
}

extern "C" int WBQtScriptEdit_ConditionPasteFromClipboard(void *script, int row)
{
	if (s_clipCondition == NULL)
	{
		return -1;
	}
	Script *pScript = static_cast<Script *>(script);
	OrCondition *pSelOr = NULL;
	Condition *pSelCond = NULL;
	qtResolveConditionRow(pScript, row, &pSelOr, &pSelCond);	// pSelOr may be the row's OR group
	if (pSelOr == NULL)
	{
		// No selection resolved: paste into the first OR group (create one if the script has none,
		// matching how New seeds an OrCondition).
		pSelOr = pScript->getOrCondition();
		if (pSelOr == NULL)
		{
			pSelOr = newInstance( OrCondition );
			pScript->setOrCondition(pSelOr);
		}
	}
	Condition *pCopy = s_clipCondition->duplicate();	// keep the clipboard for repeat pastes
	if (pSelCond != NULL)
	{
		// Insert right after the selected condition.
		pCopy->setNextCondition(pSelCond->getNext());
		pSelCond->setNextCondition(pCopy);
	}
	else
	{
		// No condition selected in this group: append to its head.
		pCopy->setNextCondition(pSelOr->getFirstAndCondition());
		pSelOr->setFirstAndCondition(pCopy);
	}
	return qtFindConditionRow(pScript, pSelOr, pCopy);
}

extern "C" int WBQtScriptEdit_ConditionDelete(void *script, int row)
{
	// == ScriptConditionsDlg::OnDelete (select the previous row, clamped to 0).
	Script *pScript = static_cast<Script *>(script);
	OrCondition *pSelOr = NULL;
	Condition *pSelCond = NULL;
	if (!qtResolveConditionRow(pScript, row, &pSelOr, &pSelCond))
	{
		return -1;
	}
	if (pSelCond)
	{
		pSelOr->deleteCondition(pSelCond);
	}
	else
	{
		pScript->deleteOrCondition(pSelOr);
	}
	int newRow = row - 1;
	if (newRow < 0)
	{
		newRow = 0;
	}
	return newRow;
}

extern "C" int WBQtScriptEdit_ConditionMoveDown(void *script, int row)
{
	// == ScriptConditionsDlg::doMoveDown/OnMoveDown.
	Script *pScript = static_cast<Script *>(script);
	OrCondition *pSelOr = NULL;
	Condition *pSelCond = NULL;
	if (!qtResolveConditionRow(pScript, row, &pSelOr, &pSelCond))
	{
		return -1;
	}
	OrCondition *pNowOr = pSelOr;
	if (pSelCond && pSelOr)
	{
		Condition *pNext = pSelCond->getNext();
		if (pNext == NULL)
		{
			OrCondition *pNOr = pSelOr->getNextOrCondition();
			if (!pNOr)
			{
				pNOr = newInstance( OrCondition);
				pSelOr->setNextOrCondition(pNOr);
			}
			Condition *newNext = pNOr->getFirstAndCondition();
			pNOr->setFirstAndCondition(pSelCond);
			pSelOr->removeCondition(pSelCond);
			pSelCond->setNextCondition(newNext);
			pNowOr = pNOr;
		}
		else
		{
			Condition *pCur = pSelOr->getFirstAndCondition();
			Condition *pPrev = NULL;
			while (pCur != pSelCond)
			{
				pPrev = pCur;
				pCur = pCur->getNext();
			}
			DEBUG_ASSERTCRASH(pCur, ("Didn't find condition in list."));
			if (!pCur)
			{
				return -1;
			}
			if (pPrev)
			{
				pPrev->setNextCondition(pNext);
				pCur->setNextCondition(pNext->getNext());
				pNext->setNextCondition(pCur);
			}
			else
			{
				DEBUG_ASSERTCRASH(pSelCond == pSelOr->getFirstAndCondition(), ("Logic error."));
				pCur->setNextCondition(pNext->getNext());
				pNext->setNextCondition(pCur);
				pSelOr->setFirstAndCondition(pNext);
			}
		}
		return qtFindConditionRow(pScript, pNowOr, pSelCond);
	}
	else if (pSelOr)
	{
		OrCondition *pNext = pSelOr->getNextOrCondition();
		if (pNext == NULL)
		{
			return -1;
		}
		OrCondition *pCur = pScript->getOrCondition();
		OrCondition *pPrev = NULL;
		while (pCur != pSelOr)
		{
			pPrev = pCur;
			pCur = pCur->getNextOrCondition();
		}
		DEBUG_ASSERTCRASH(pCur, ("Didn't find Or in list."));
		if (!pCur)
		{
			return -1;
		}
		if (pPrev)
		{
			pPrev->setNextOrCondition(pNext);
			pCur->setNextOrCondition(pNext->getNextOrCondition());
			pNext->setNextOrCondition(pCur);
		}
		else
		{
			DEBUG_ASSERTCRASH(pSelOr == pScript->getOrCondition(), ("Logic error."));
			pCur->setNextOrCondition(pNext->getNextOrCondition());
			pNext->setNextOrCondition(pCur);
			pScript->setOrCondition(pNext);
		}
		return qtFindConditionRow(pScript, pSelOr, NULL);
	}
	return -1;
}

extern "C" int WBQtScriptEdit_ConditionMoveUp(void *script, int row)
{
	// == ScriptConditionsDlg::doMoveUp/OnMoveUp.
	Script *pScript = static_cast<Script *>(script);
	OrCondition *pSelOr = NULL;
	Condition *pSelCond = NULL;
	if (!qtResolveConditionRow(pScript, row, &pSelOr, &pSelCond))
	{
		return -1;
	}
	OrCondition *pNowOr = pSelOr;
	if (pSelCond && pSelOr)
	{
		Condition *pPrev = pSelOr->findPreviousCondition(pSelCond);
		if (pPrev == NULL)
		{
			OrCondition *pNOr = pScript->findPreviousOrCondition(pSelOr);
			if (!pNOr)
			{
				pNOr = newInstance( OrCondition);
				pNOr->setNextOrCondition(pSelOr);
				pScript->setOrCondition(pNOr);
			}
			Condition *previous = pNOr->findPreviousCondition(NULL);
			if (previous)
			{
				pSelOr->removeCondition(pSelCond);
				previous->setNextCondition(pSelCond);
			}
			else
			{
				pSelOr->removeCondition(pSelCond);
				pNOr->setFirstAndCondition(pSelCond);
			}
			pNowOr = pNOr;
		}
		else
		{
			pPrev->setNextCondition(pSelCond->getNext());
			pSelCond->setNextCondition(pPrev);

			Condition *pPrevPrev = pSelOr->findPreviousCondition(pPrev);
			if (pPrevPrev)
			{
				pPrevPrev->setNextCondition(pSelCond);
			}
			else
			{
				pSelOr->setFirstAndCondition(pSelCond);
			}
		}
		return qtFindConditionRow(pScript, pNowOr, pSelCond);
	}
	else if (pSelOr)
	{
		OrCondition *pOrPrev = pScript->findPreviousOrCondition(pSelOr);
		if (!pOrPrev)
		{
			return -1;
		}

		pOrPrev->setNextOrCondition(pSelOr->getNextOrCondition());
		pSelOr->setNextOrCondition(pOrPrev);

		OrCondition *pOrPrevPrev = pScript->findPreviousOrCondition(pOrPrev);
		if (pOrPrevPrev)
		{
			pOrPrevPrev->setNextOrCondition(pSelOr);
		}
		else
		{
			pScript->setOrCondition(pSelOr);
		}
		return qtFindConditionRow(pScript, pSelOr, NULL);
	}
	return -1;
}

// ================= Actions tabs =================
// The true/false pages are structural twins differing only in which list they touch; the
// isFalse flag picks the list, mirroring ScriptActionsTrue vs ScriptActionsFalse.

static ScriptAction *qtGetActionHead(Script *pScript, int isFalse)
{
	return isFalse ? pScript->getFalseAction() : pScript->getAction();
}

static void qtSetActionHead(Script *pScript, int isFalse, ScriptAction *pHead)
{
	if (isFalse)
	{
		pScript->setFalseAction(pHead);
	}
	else
	{
		pScript->setAction(pHead);
	}
}

static void qtDeleteAction(Script *pScript, int isFalse, ScriptAction *pAction)
{
	if (isFalse)
	{
		pScript->deleteFalseAction(pAction);
	}
	else
	{
		pScript->deleteAction(pAction);
	}
}

static ScriptAction *qtActionAt(Script *pScript, int isFalse, int index)
{
	if (index < 0)
	{
		return NULL;
	}
	ScriptAction *pAction = qtGetActionHead(pScript, isFalse);
	while (pAction && index > 0)
	{
		pAction = pAction->getNext();
		index--;
	}
	return pAction;
}

extern "C" int WBQtScriptEditData_GetActionCount(void *script, int isFalse)
{
	Script *pScript = static_cast<Script *>(script);
	// loadList() refreshed the warning state on every rebuild; the Qt tab reloads through here.
	ScriptDialog::updateScriptWarning(pScript);
	int count = 0;
	ScriptAction *pAction = qtGetActionHead(pScript, isFalse);
	while (pAction)
	{
		count++;
		pAction = pAction->getNext();
	}
	return count;
}

extern "C" void WBQtScriptEditData_GetActionLabel(void *script, int isFalse, int index, char *buf, int cap)
{
	Script *pScript = static_cast<Script *>(script);
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	ScriptAction *pAction = qtActionAt(pScript, isFalse, index);
	if (pAction)
	{
		// == loadList()'s label building.
		AsciiString astr = pAction->getUiText();
		if (astr.isEmpty())
		{
			astr = "Invalid Action";
		}
		copyOut(astr, buf, cap);
	}
}

// == loadList()'s index clamp: keep the requested index inside the rebuilt list.
static int qtClampActionIndex(Script *pScript, int isFalse, int index)
{
	int count = 0;
	ScriptAction *pAction = qtGetActionHead(pScript, isFalse);
	while (pAction)
	{
		count++;
		pAction = pAction->getNext();
	}
	if (count == 0)
	{
		return -1;
	}
	if (index < 0)
	{
		index = 0;
	}
	if (index >= count)
	{
		index = count - 1;
	}
	return index;
}

extern "C" int WBQtScriptEdit_ActionNew(void *script, int isFalse, int index)
{
	// == ScriptActionsTrue/False::OnNew.
	Script *pScript = static_cast<Script *>(script);
	ScriptAction *pSel = qtActionAt(pScript, isFalse, index);

	ScriptAction *pAct = newInstance( ScriptAction)(ScriptAction::DEBUG_MESSAGE_BOX);
	if (WBQtCondAct_Run(pAct, 1) != 0)
	{
		if (pSel)
		{
			pAct->setNextAction(pSel->getNext());
			pSel->setNextAction(pAct);
		}
		else
		{
			pAct->setNextAction(qtGetActionHead(pScript, isFalse));
			qtSetActionHead(pScript, isFalse, pAct);
		}
		return qtClampActionIndex(pScript, isFalse, index + 1);
	}
	pAct->deleteInstance();
	return -1;
}

extern "C" int WBQtScriptEdit_ActionEdit(void *script, int isFalse, int index)
{
	// == OnEditAction (modal result intentionally ignored there too).
	Script *pScript = static_cast<Script *>(script);
	ScriptAction *pSel = qtActionAt(pScript, isFalse, index);
	if (pSel == NULL)
	{
		return -1;
	}
	WBQtCondAct_Run(pSel, 1);
	ScriptDialog::updateScriptWarning(pScript);
	return index;
}

extern "C" int WBQtScriptEdit_ActionCopy(void *script, int isFalse, int index)
{
	// == OnCopy.
	Script *pScript = static_cast<Script *>(script);
	ScriptAction *pSel = qtActionAt(pScript, isFalse, index);
	if (pSel == NULL)
	{
		return -1;
	}
	ScriptAction *pCopy = pSel->duplicate();
	if (WBQtScriptEdit_GetSmartCopy())
	{
		// == applySmartCopyToAction.
		for (int i = 0; i < pCopy->getNumParameters(); ++i)
		{
			qtApplySmartCopyToParameter(pCopy->getParameter(i));
		}
	}
	pCopy->setNextAction(pSel->getNext());
	pSel->setNextAction(pCopy);
	return qtClampActionIndex(pScript, isFalse, index + 1);
}

extern "C" int WBQtScriptEdit_ActionHasClipboard(void)
{
	return (s_clipAction != NULL) ? 1 : 0;
}

extern "C" int WBQtScriptEdit_ActionCopyToClipboard(void *script, int isFalse, int index)
{
	Script *pScript = static_cast<Script *>(script);
	ScriptAction *pSel = qtActionAt(pScript, isFalse, index);
	if (pSel == NULL)
	{
		return -1;
	}
	if (s_clipAction != NULL)
	{
		s_clipAction->deleteInstance();
	}
	s_clipAction = pSel->duplicate();
	return 1;
}

extern "C" int WBQtScriptEdit_ActionPasteFromClipboard(void *script, int isFalse, int index)
{
	if (s_clipAction == NULL)
	{
		return -1;
	}
	Script *pScript = static_cast<Script *>(script);
	ScriptAction *pSel = qtActionAt(pScript, isFalse, index);
	ScriptAction *pCopy = s_clipAction->duplicate();	// keep the clipboard for repeat pastes
	if (pSel != NULL)
	{
		pCopy->setNextAction(pSel->getNext());
		pSel->setNextAction(pCopy);
	}
	else
	{
		pCopy->setNextAction(qtGetActionHead(pScript, isFalse));
		qtSetActionHead(pScript, isFalse, pCopy);
	}
	return qtClampActionIndex(pScript, isFalse, (pSel != NULL) ? index + 1 : 0);
}

extern "C" int WBQtScriptEdit_ActionDelete(void *script, int isFalse, int index)
{
	// == OnDelete (loadList clamps the stale index into the rebuilt list).
	Script *pScript = static_cast<Script *>(script);
	ScriptAction *pSel = qtActionAt(pScript, isFalse, index);
	if (pSel == NULL)
	{
		return -1;
	}
	qtDeleteAction(pScript, isFalse, pSel);
	return qtClampActionIndex(pScript, isFalse, index);
}

// == ScriptActionsTrue/False::doMoveDown (swap with the next action).
static Bool qtActionMoveDown(Script *pScript, int isFalse, ScriptAction *pAction)
{
	if (pAction && pAction->getNext())
	{
		ScriptAction *pNext = pAction->getNext();
		ScriptAction *pCur = qtGetActionHead(pScript, isFalse);
		ScriptAction *pPrev = NULL;
		while (pCur != pAction)
		{
			pPrev = pCur;
			pCur = pCur->getNext();
		}
		DEBUG_ASSERTCRASH(pCur, ("Didn't find action in list."));
		if (!pCur)
		{
			return false;
		}
		if (pPrev)
		{
			pPrev->setNextAction(pNext);
			pCur->setNextAction(pNext->getNext());
			pNext->setNextAction(pCur);
		}
		else
		{
			DEBUG_ASSERTCRASH(pAction == qtGetActionHead(pScript, isFalse), ("Logic error."));
			pCur->setNextAction(pNext->getNext());
			pNext->setNextAction(pCur);
			qtSetActionHead(pScript, isFalse, pNext);
		}
		return true;
	}
	return false;
}

extern "C" int WBQtScriptEdit_ActionMoveDown(void *script, int isFalse, int index)
{
	Script *pScript = static_cast<Script *>(script);
	ScriptAction *pSel = qtActionAt(pScript, isFalse, index);
	if (pSel == NULL)
	{
		return -1;
	}
	if (qtActionMoveDown(pScript, isFalse, pSel))
	{
		return qtClampActionIndex(pScript, isFalse, index + 1);
	}
	return -1;
}

extern "C" int WBQtScriptEdit_ActionMoveUp(void *script, int isFalse, int index)
{
	// == OnMoveUp (move the previous action down past the selection).
	Script *pScript = static_cast<Script *>(script);
	if (index <= 0)
	{
		return -1;
	}
	ScriptAction *pPrev = qtActionAt(pScript, isFalse, index - 1);
	if (pPrev == NULL || pPrev->getNext() == NULL)
	{
		return -1;
	}
	if (qtActionMoveDown(pScript, isFalse, pPrev))
	{
		return qtClampActionIndex(pScript, isFalse, index - 1);
	}
	return -1;
}

extern "C" int WBQtScriptEdit_ActionMoveToOther(void *script, int isFalse, int index)
{
	// == OnMoveToFalse / OnMoveToTrue (duplicate the single action, delete from the source
	// list, append to the other list, reset the selection to the top).
	Script *pScript = static_cast<Script *>(script);
	ScriptAction *pSel = qtActionAt(pScript, isFalse, index);
	if (pSel == NULL)
	{
		return -1;
	}
	ScriptAction *pMove = pSel->duplicate();
	pMove->setNextAction(NULL); // important!
	qtDeleteAction(pScript, isFalse, pSel);
	int destIsFalse = isFalse ? 0 : 1;
	ScriptAction *pTail = qtGetActionHead(pScript, destIsFalse);
	if (pTail)
	{
		while (pTail->getNext())
		{
			pTail = pTail->getNext();
		}
		pTail->setNextAction(pMove);
	}
	else
	{
		qtSetActionHead(pScript, destIsFalse, pMove);
	}
	return 0;
}

#endif // RTS_HAS_QT
