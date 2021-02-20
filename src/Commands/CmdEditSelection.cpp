// This file is part of BowPad.
//
// Copyright (C) 2020-2021 - Stefan Kueng
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// See <http://www.gnu.org/licenses/> for a copy of the full license text
//
#include "stdafx.h"
#include "CmdEditSelection.h"
#include "LexStyles.h"

bool CCmdEditSelection::Execute()
{
    MarkSelectedWord(false, true);
    return true;
}

void CCmdEditSelection::ScintillaNotify(SCNotification* pScn)
{
    if (pScn->nmhdr.code == SCN_UPDATEUI)
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, nullptr);
    }
}

HRESULT CCmdEditSelection::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue)
{
    if (UI_PKEY_Enabled == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, (ScintillaCall(SCI_GETSELECTIONEMPTY) == 0), pPropVarNewValue);
    }
    return E_NOTIMPL;
}
