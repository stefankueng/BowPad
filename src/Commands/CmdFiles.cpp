// This file is part of BowPad.
//
// Copyright (C) 2013 - Stefan Kueng
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
#include "CmdFiles.h"
#include "MRU.h"
#include "LexStyles.h"
#include "PreserveChdir.h"

bool CCmdOpen::Execute()
{
    PreserveChdir keepCWD;

    std::vector<std::wstring> paths;
    CComPtr<IFileOpenDialog> pfd = NULL;

    HRESULT hr = pfd.CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER);
    if (SUCCEEDED(hr))
    {
        // Set the dialog options
        DWORD dwOptions;
        if (SUCCEEDED(hr = pfd->GetOptions(&dwOptions)))
        {
            hr = pfd->SetOptions(dwOptions | FOS_FILEMUSTEXIST | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_ALLOWMULTISELECT);
        }

        // Set a title
        if (SUCCEEDED(hr))
        {
            pfd->SetTitle(L"BowPad");
        }

        // Show the save/open file dialog
        if (SUCCEEDED(hr) && SUCCEEDED(hr = pfd->Show(GetHwnd())))
        {
            CComPtr<IShellItemArray> psiaResults;
            hr = pfd->GetResults(&psiaResults);
            if (SUCCEEDED(hr))
            {
                DWORD count = 0;
                hr = psiaResults->GetCount(&count);
                for (DWORD i = 0; i < count; ++i)
                {
                    CComPtr<IShellItem> psiResult = NULL;
                    hr = psiaResults->GetItemAt(i, &psiResult);
                    PWSTR pszPath = NULL;
                    hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                    if (SUCCEEDED(hr))
                    {
                        paths.push_back(pszPath);
                    }
                }
            }
        }
    }
    if (SUCCEEDED(hr))
    {
        for (auto it : paths)
            OpenFile(it.c_str());
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////

bool CCmdSave::Execute()
{
    return SaveCurrentTab();
}

void CCmdSave::ScintillaNotify( Scintilla::SCNotification * pScn )
{
    switch (pScn->nmhdr.code)
    {
    case SCN_SAVEPOINTREACHED:
    case SCN_SAVEPOINTLEFT:
        {
            InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
        }
        break;
    }
}

HRESULT CCmdSave::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue )
{
    if (UI_PKEY_Enabled == key)
    {
        int tab = GetCurrentTabIndex();
        if ((tab >= 0) && (tab < GetDocumentCount()))
        {
            CDocument doc = GetDocument(tab);
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, doc.m_bIsDirty||doc.m_bNeedsSaving, ppropvarNewValue);
        }
    }
    return E_NOTIMPL;
}

bool CCmdSaveAll::Execute()
{
    for (int i = 0; i < (int)GetDocumentCount(); ++i)
    {
        if (GetDocument(i).m_bIsDirty)
        {
            TabActivateAt(i);
            SaveCurrentTab();
        }
    }
    return true;
}

void CCmdSaveAll::ScintillaNotify( Scintilla::SCNotification * pScn )
{
    switch (pScn->nmhdr.code)
    {
    case SCN_SAVEPOINTREACHED:
    case SCN_SAVEPOINTLEFT:
        {
            InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
        }
        break;
    }
}

HRESULT CCmdSaveAll::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue )
{
    if (UI_PKEY_Enabled == key)
    {
        int dirtycount = 0;
        for (int i = 0; i < GetDocumentCount(); ++i)
        {
            CDocument doc = GetDocument(i);
            if (doc.m_bIsDirty||doc.m_bNeedsSaving)
                dirtycount++;
        }

        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, dirtycount>0, ppropvarNewValue);
    }
    return E_NOTIMPL;
}


bool CCmdSaveAs::Execute()
{
    return SaveCurrentTab(true);
}

HRESULT CCmdReload::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue )
{
    if (UI_PKEY_Enabled == key)
    {
        int tab = GetCurrentTabIndex();
        if ((tab >= 0) && (tab < GetDocumentCount()))
        {
            CDocument doc = GetDocument(tab);
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, !doc.m_path.empty(), ppropvarNewValue);
        }
    }
    return E_NOTIMPL;
}

void CCmdReload::TabNotify( TBHDR * ptbhdr )
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }
}
