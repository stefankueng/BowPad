// This file is part of BowPad.
//
// Copyright (C) 2014-2016 - Stefan Kueng
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
#include "CmdMisc.h"
#include "BowPad.h"
#include "AppUtils.h"
#include "Theme.h"
#include "SysInfo.h"

CCmdToggleTheme::CCmdToggleTheme(void * obj)
    : ICommand(obj)
{
    int dark = (int)CIniSettings::Instance().GetInt64(L"View", L"darktheme", 0);
    if (dark)
    {
        CTheme::Instance().SetDarkTheme(dark != 0);
    }
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

CCmdToggleTheme::~CCmdToggleTheme()
{
}

bool CCmdToggleTheme::Execute()
{
    if (!HasActiveDocument())
        return false;
    CTheme::Instance().SetDarkTheme(!CTheme::Instance().IsDarkTheme());

    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    return true;
}

HRESULT CCmdToggleTheme::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, CTheme::Instance().IsDarkTheme(), ppropvarNewValue);
    }
    return E_NOTIMPL;
}

bool CCmdConfigShortcuts::Execute()
{
    std::wstring userFile = CAppUtils::GetDataPath() + L"\\shortcuts.ini";
    if (!PathFileExists(userFile.c_str()))
    {
        HRSRC hResource = FindResource(NULL, MAKEINTRESOURCE(IDR_SHORTCUTS), L"config");
        if (hResource)
        {
            HGLOBAL hResourceLoaded = LoadResource(NULL, hResource);
            if (hResourceLoaded)
            {
                const char* lpResLock = (const char *) LockResource(hResourceLoaded);
                if (lpResLock)
                {
                    const char* lpStart = strstr(lpResLock, "#--");
                    if (lpStart)
                    {
                        const char* lpEnd = strstr(lpStart + 3, "#--");
                        if (lpEnd)
                        {
                            HANDLE hFile = CreateFile(userFile.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                            if (hFile != INVALID_HANDLE_VALUE)
                            {
                                DWORD dwWritten = 0;
                                WriteFile(hFile, lpStart, (DWORD)(lpEnd - lpStart), &dwWritten, nullptr);
                                CloseHandle(hFile);
                            }
                        }
                    }
                }
            }
        }
    }
    return OpenFile(userFile.c_str(), 0) >= 0;
}


CCmdAutoBraces::CCmdAutoBraces(void * obj) : ICommand(obj)
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

CCmdAutoBraces::~CCmdAutoBraces()
{
}

bool CCmdAutoBraces::Execute()
{
    CIniSettings::Instance().SetInt64(L"View", L"autobrace", CIniSettings::Instance().GetInt64(L"View", L"autobrace", 1) ? 0 : 1);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    return true;
}

HRESULT CCmdAutoBraces::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, (BOOL)CIniSettings::Instance().GetInt64(L"View", L"autobrace", 1), ppropvarNewValue);
    }
    return E_NOTIMPL;
}

CCmdViewFileTree::CCmdViewFileTree(void * obj) : ICommand(obj)
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

CCmdViewFileTree::~CCmdViewFileTree()
{
}

bool CCmdViewFileTree::Execute()
{
    ShowFileTree(!IsFileTreeShown());
    return true;
}

HRESULT CCmdViewFileTree::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, IsFileTreeShown(), ppropvarNewValue);
    }
    if (UI_PKEY_Enabled == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, !GetFileTreePath().empty(), ppropvarNewValue);
    }
    return E_NOTIMPL;
}

CCmdWriteProtect::CCmdWriteProtect(void * obj)
    : ICommand(obj)
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
}

CCmdWriteProtect::~CCmdWriteProtect(void)
{
}

bool CCmdWriteProtect::Execute()
{
    if (!HasActiveDocument())
        return false;

    auto doc = GetActiveDocument();
    doc.m_bIsWriteProtected = !(doc.m_bIsWriteProtected || doc.m_bIsReadonly);
    if (!doc.m_bIsWriteProtected && doc.m_bIsReadonly)
        doc.m_bIsReadonly = false;
    ScintillaCall(SCI_SETREADONLY, doc.m_bIsWriteProtected);
    SetDocument(GetDocIdOfCurrentTab(), doc);
    UpdateTab(GetDocIdOfCurrentTab());

    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    return true;
}

HRESULT CCmdWriteProtect::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        bool bWriteProtected = false;
        if (HasActiveDocument())
        {
            auto doc = GetActiveDocument();
            bWriteProtected = doc.m_bIsReadonly || doc.m_bIsWriteProtected;
        }
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, bWriteProtected, ppropvarNewValue);
    }

    if (UI_PKEY_Enabled == key)
    {
        bool bHasPath = false;
        if (HasActiveDocument())
        {
            auto doc = GetActiveDocument();
            bHasPath = !doc.m_path.empty();
        }

        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, bHasPath, ppropvarNewValue);
    }

    return E_NOTIMPL;
}

void CCmdWriteProtect::TabNotify(TBHDR * ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }
}

void CCmdWriteProtect::ScintillaNotify(Scintilla::SCNotification * pScn)
{
    if (pScn->nmhdr.code == SCN_SAVEPOINTREACHED)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }
}
