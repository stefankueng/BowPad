// This file is part of BowPad.
//
// Copyright (C) 2024 - Stefan Kueng
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
#include "CmdTail.h"
#include "ScintillaWnd.h"
#include "StringUtils.h"

namespace
{
}

CCmdTail::CCmdTail(void* obj)
    : ICommand(obj)
    , m_timerId(0)
{
    m_timerId = GetTimerID();
}

bool CCmdTail::Execute()
{
    auto& doc = GetModActiveDocument();

    if ((doc.m_bIsDirty || doc.m_bNeedsSaving) && !doc.m_bTailing)
        return false;

    doc.m_bTailing = !doc.m_bTailing;
    if (doc.m_bTailing)
    {
        SetTimer(GetHwnd(), m_timerId, 1000, nullptr);
        Scintilla().SetReadOnly(true);
    }
    else
    {
        KillTimer(GetHwnd(), m_timerId);
        Scintilla().SetReadOnly(doc.m_bIsReadonly || doc.m_bIsWriteProtected);
    }

    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);

    return true;
}

void CCmdTail::TabNotify(TBHDR* ptbHdr)
{
    if (ptbHdr->hdr.code == TCN_SELCHANGE)
    {
        const auto& doc = GetActiveDocument();
        if (doc.m_bTailing)
        {
            SetTimer(GetHwnd(), m_timerId, 1000, nullptr);
            Scintilla().SetReadOnly(true);
        }
        else
        {
            KillTimer(GetHwnd(), m_timerId);
        }
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
    }
}

HRESULT CCmdTail::IUICommandHandlerUpdateProperty(const PROPERTYKEY& key, const PROPVARIANT* /*tagPropvariant*/, PROPVARIANT* pPropVarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue,
                                         GetActiveDocument().m_bTailing, pPropVarNewValue);
    }
    else if (UI_PKEY_Enabled == key)
    {
        auto& doc     = GetModActiveDocument();
        auto  canTail = !doc.m_bIsDirty && !doc.m_bNeedsSaving && !doc.m_path.empty();

        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, canTail, pPropVarNewValue);
    }
    return E_NOTIMPL;
}

void CCmdTail::OnTimer(UINT id)
{
    if (id == m_timerId)
    {
        auto& doc = GetModActiveDocument();
        if (!doc.m_bTailing)
        {
            KillTimer(GetHwnd(), m_timerId);
            InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
            InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
            InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
        }
        else
        {
            auto data = ReadNewData(doc);
            if (!data.empty())
            {
                Scintilla().AppendText(data.size(), data.data());
                Scintilla().ScrollToEnd();
                Scintilla().EmptyUndoBuffer();
                doc.m_bIsDirty = false;
                doc.m_bNeedsSaving = false;
            }
        }
    }
}
