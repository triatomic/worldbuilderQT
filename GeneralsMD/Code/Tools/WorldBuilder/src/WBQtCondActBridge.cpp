// WBQtCondActBridge.cpp -- MFC side of the Qt condition/action editor seam (Tier 2b). Plain MFC
// TU (no Qt include). Serves both Condition and ScriptAction (selected by isAction), mirroring
// how EditCondition / EditAction are twins: the template catalog comes from TheScriptEngine,
// the sentence model from getUiStrings()/getParameter(i)->getUiText(), warnings from
// EditParameter::getWarningText/getInfoText, and parameter editing pops the (still MFC)
// EditParameter modal exactly like the rich-edit link click did. Whole body guarded by
// RTS_HAS_QT so the OFF build compiles it to an empty object.
#include "StdAfx.h"
#include "resource.h"				// IDD_EDIT_PARAMETER (used in EditParameter.h's IDD enum)
#include "Lib/BaseType.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/ScriptEngine.h"
#include "EditParameter.h"
#include "qt/panels/WBQtCondActBridge.h"

#define SCRIPT_DIALOG_SECTION "ScriptDialog"

#ifdef RTS_HAS_QT

static void copyOut(const AsciiString &str, char *buf, int cap)
{
	if (buf == NULL || cap <= 0)
	{
		return;
	}
	strncpy(buf, str.str(), cap - 1);
	buf[cap - 1] = 0;
}

// ================= the template catalog =================

extern "C" int WBQtCondActData_GetTemplateCount(int isAction)
{
	return isAction ? ScriptAction::NUM_ITEMS : Condition::NUM_ITEMS;
}

extern "C" void WBQtCondActData_GetTemplateName(int isAction, int i, char *buf, int cap)
{
	if (isAction)
	{
		copyOut(TheScriptEngine->getActionTemplate(i)->getName(), buf, cap);
	}
	else
	{
		copyOut(TheScriptEngine->getConditionTemplate(i)->getName(), buf, cap);
	}
}

extern "C" void WBQtCondActData_GetTemplateName2(int isAction, int i, char *buf, int cap)
{
	if (isAction)
	{
		copyOut(TheScriptEngine->getActionTemplate(i)->getName2(), buf, cap);
	}
	else
	{
		copyOut(TheScriptEngine->getConditionTemplate(i)->getName2(), buf, cap);
	}
}

extern "C" void WBQtCondActData_GetTemplateHelp(int isAction, int i, char *buf, int cap)
{
	if (isAction)
	{
		copyOut(TheScriptEngine->getActionTemplate(i)->getHelpText(), buf, cap);
	}
	else
	{
		copyOut(TheScriptEngine->getConditionTemplate(i)->getHelpText(), buf, cap);
	}
}

// ================= the edited item =================

extern "C" int WBQtCondActData_GetType(void *item, int isAction)
{
	if (isAction)
	{
		return static_cast<ScriptAction *>(item)->getActionType();
	}
	return static_cast<Condition *>(item)->getConditionType();
}

extern "C" void WBQtCondActData_SetType(void *item, int isAction, int type)
{
	if (isAction)
	{
		static_cast<ScriptAction *>(item)->setActionType((enum ScriptAction::ScriptActionType)type);
	}
	else
	{
		static_cast<Condition *>(item)->setConditionType((enum Condition::ConditionType)type);
	}
}

extern "C" void WBQtCondActData_ClearWarningFlag(void *item, int isAction)
{
	if (isAction)
	{
		static_cast<ScriptAction *>(item)->setWarnings(false);
	}
	else
	{
		static_cast<Condition *>(item)->setWarnings(false);
	}
}

// ================= the parameter sentence =================

extern "C" int WBQtCondActData_GetUiStringCount(void *item, int isAction)
{
	AsciiString strings[MAX_PARMS];
	if (isAction)
	{
		return static_cast<ScriptAction *>(item)->getUiStrings(strings);
	}
	return static_cast<Condition *>(item)->getUiStrings(strings);
}

extern "C" void WBQtCondActData_GetUiString(void *item, int isAction, int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	AsciiString strings[MAX_PARMS];
	int count;
	if (isAction)
	{
		count = static_cast<ScriptAction *>(item)->getUiStrings(strings);
	}
	else
	{
		count = static_cast<Condition *>(item)->getUiStrings(strings);
	}
	if (i >= 0 && i < count)
	{
		copyOut(strings[i], buf, cap);
	}
}

static Parameter *qtParameterAt(void *item, int isAction, int i)
{
	if (isAction)
	{
		ScriptAction *pAction = static_cast<ScriptAction *>(item);
		if (i < 0 || i >= pAction->getNumParameters())
		{
			return NULL;
		}
		return pAction->getParameter(i);
	}
	Condition *pCondition = static_cast<Condition *>(item);
	if (i < 0 || i >= pCondition->getNumParameters())
	{
		return NULL;
	}
	return pCondition->getParameter(i);
}

extern "C" int WBQtCondActData_GetParameterCount(void *item, int isAction)
{
	if (isAction)
	{
		return static_cast<ScriptAction *>(item)->getNumParameters();
	}
	return static_cast<Condition *>(item)->getNumParameters();
}

extern "C" void WBQtCondActData_GetParameterText(void *item, int isAction, int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	Parameter *param = qtParameterAt(item, isAction, i);
	if (param != NULL)
	{
		copyOut(param->getUiText(), buf, cap);
	}
}

extern "C" void WBQtCondAct_EditParameter(void *item, int isAction, int i)
{
	Parameter *param = qtParameterAt(item, isAction, i);
	if (param != NULL)
	{
		// == the EN_LINK WM_LBUTTONDOWN handler. EditParameter's modals resolve their owner
		// via GetActiveWindow, which is the Qt dialog while it runs.
		if (isAction && param->getParameterType() == Parameter::COMMANDBUTTON_ABILITY)
		{
			// == EditAction's COMMANDBUTTON_ABILITY special case: the ability list is built
			// from the unit this action targets -- the action's FIRST parameter -- so pass
			// its name through, or the picker can't resolve the unit's command set.
			Parameter *unitParam = qtParameterAt(item, isAction, 0);
			if (unitParam != NULL)
			{
				EditParameter::edit(param, 0, unitParam->getString());
				return;
			}
		}
		EditParameter::edit(param, 0);
	}
}

extern "C" void WBQtCondActData_GetWarnings(void *item, int isAction, char *warnBuf, int warnCap, char *infoBuf, int infoCap)
{
	// == formatConditionText/formatActionText's warning sweep (both pass isAction=false to
	// getWarningText -- kept identical).
	AsciiString warningText;
	AsciiString informationText;
	int count = WBQtCondActData_GetParameterCount(item, isAction);
	for (int i = 0; i < count; i++)
	{
		Parameter *param = qtParameterAt(item, isAction, i);
		if (param != NULL)
		{
			warningText.concat(EditParameter::getWarningText(param, false));
			informationText.concat(EditParameter::getInfoText(param));
		}
	}
	copyOut(warningText, warnBuf, warnCap);
	copyOut(informationText, infoBuf, infoCap);
}

// ================= the Compress Script setting =================

extern "C" int WBQtCondAct_GetCompress(void)
{
	return ::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "CompressScripts", 1) ? 1 : 0;
}

extern "C" void WBQtCondAct_SetCompress(int enabled)
{
	::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "CompressScripts", enabled ? 1 : 0);
}

#endif // RTS_HAS_QT
