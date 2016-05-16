// This file is part of BowPad.
//
// Copyright (C) 2016 - Stefan Kueng
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
#include "BowPad.h"
#include "UnicodeUtils.h"
#include "LexStyles.h"

extern void FindReplace_FindFunction(void *mainWnd, const std::wstring& functionName);

CCmdGotoSymbol::CCmdGotoSymbol(void* obj)
    : ICommand(obj)
{
}

CCmdGotoSymbol::~CCmdGotoSymbol()
{
}

bool CCmdGotoSymbol::Execute()
{
    std::wstring symbolName = CUnicodeUtils::StdGetUnicode(GetSelectedText(true));
    FindReplace_FindFunction(m_Obj, symbolName);
    return true;
}

HRESULT CCmdGotoSymbol::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT * ppropvarCurrentValue, PROPVARIANT * ppropvarNewValue)
{
    if (UI_PKEY_Enabled == key)
    {
        if (HasActiveDocument())
        {
            CDocument doc = GetActiveDocument();
            auto funcRegex = CLexStyles::Instance().GetFunctionRegexForLang(CUnicodeUtils::StdGetUTF8(doc.m_language));
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, !funcRegex.empty(), ppropvarNewValue);
        }
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, false, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

void CCmdGotoSymbol::TabNotify(TBHDR * ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
}

