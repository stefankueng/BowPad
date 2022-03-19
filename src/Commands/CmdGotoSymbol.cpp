// This file is part of BowPad.
//
// Copyright (C) 2016-2017, 2020-2022 - Stefan Kueng
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
#include "CmdGotoSymbol.h"
#include "UnicodeUtils.h"
#include "LexStyles.h"

extern void findReplaceFindFunction(void* mainWnd, const std::wstring& functionName);

CCmdGotoSymbol::CCmdGotoSymbol(void* obj)
    : ICommand(obj)
{
}

bool CCmdGotoSymbol::Execute()
{
    std::wstring symbolName = CUnicodeUtils::StdGetUnicode(GetSelectedText(SelectionHandling::CurrentWordIfSelectionIsEmpty));
    findReplaceFindFunction(m_pMainWindow, symbolName);
    return true;
}

HRESULT CCmdGotoSymbol::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue)
{
    if (UI_PKEY_Enabled == key)
    {
        if (HasActiveDocument())
        {
            const auto& doc       = GetActiveDocument();
            const auto& funcRegex = CLexStyles::Instance().GetFunctionRegexForLang(doc.GetLanguage());
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, !funcRegex.empty(), pPropVarNewValue);
        }
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, false, pPropVarNewValue);
    }
    return E_NOTIMPL;
}

void CCmdGotoSymbol::TabNotify(TBHDR* ptbHdr)
{
    if (ptbHdr->hdr.code == TCN_SELCHANGE)
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
}

void CCmdGotoSymbol::OnLangChanged()
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
}
