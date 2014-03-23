// This file is part of BowPad.
//
// Copyright (C) 2013-2014 - Stefan Kueng
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
#include "BowPad.h"
#include "CmdFiles.h"
#include "MRU.h"
#include "LexStyles.h"
#include "PreserveChdir.h"
#include "PathUtils.h"
#include "StringUtils.h"

bool CCmdOpen::Execute()
{
    PreserveChdir keepCWD;

    std::vector<std::wstring> paths;
    _COM_SMARTPTR_TYPEDEF(IFileOpenDialog, __uuidof(IFileOpenDialog));
    IFileOpenDialogPtr pfd = NULL;

    HRESULT hr = pfd.CreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER);
    if (SUCCEEDED(hr))
    {
        // Set the dialog options
        DWORD dwOptions;
        if (SUCCEEDED(hr = pfd->GetOptions(&dwOptions)))
        {
            hr = pfd->SetOptions(dwOptions | FOS_FILEMUSTEXIST | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_ALLOWMULTISELECT);
        }

        // Set a title
        pfd->SetTitle(L"BowPad");

        // set the default folder to the folder of the current tab
        if (HasActiveDocument())
        {
            CDocument doc = GetActiveDocument();
            if (!doc.m_path.empty())
            {
                std::wstring folder = CPathUtils::GetParentDirectory(doc.m_path);
                _COM_SMARTPTR_TYPEDEF(IShellItem, __uuidof(IShellItem));
                IShellItemPtr psiDefFolder = NULL;
                hr = SHCreateItemFromParsingName(folder.c_str(), NULL, IID_PPV_ARGS(&psiDefFolder));

                if (SUCCEEDED(hr))
                {
                    pfd->SetFolder(psiDefFolder);
                }
            }
        }

        // Show the open file dialog
        if (SUCCEEDED(hr) && SUCCEEDED(hr = pfd->Show(GetHwnd())))
        {
            _COM_SMARTPTR_TYPEDEF(IShellItemArray, __uuidof(IShellItemArray));
            IShellItemArrayPtr psiaResults;
            hr = pfd->GetResults(&psiaResults);
            if (SUCCEEDED(hr))
            {
                DWORD count = 0;
                hr = psiaResults->GetCount(&count);
                for (DWORD i = 0; i < count; ++i)
                {
                    _COM_SMARTPTR_TYPEDEF(IShellItem, __uuidof(IShellItem));
                    IShellItemPtr psiResult = NULL;
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
            OpenFile(it.c_str(), true);
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

void CCmdSave::TabNotify(TBHDR * ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }
}

HRESULT CCmdSave::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue )
{
    if (UI_PKEY_Enabled == key)
    {
        if (HasActiveDocument())
        {
            CDocument doc = GetActiveDocument();
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, doc.m_bIsDirty||doc.m_bNeedsSaving, ppropvarNewValue);
        }
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, false, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

bool CCmdSaveAll::Execute()
{
    for (int i = 0; i < (int)GetDocumentCount(); ++i)
    {
        if (GetDocumentFromID(GetDocIDFromTabIndex(i)).m_bIsDirty)
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

void CCmdSaveAll::TabNotify(TBHDR * ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }
}

HRESULT CCmdSaveAll::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue )
{
    if (UI_PKEY_Enabled == key)
    {
        int dirtycount = 0;
        for (int i = 0; i < GetDocumentCount(); ++i)
        {
            CDocument doc = GetDocumentFromID(GetDocIDFromTabIndex(i));
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

HRESULT CCmdReload::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_Enabled == key)
    {
        if (HasActiveDocument())
        {
            CDocument doc = GetActiveDocument();
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, !doc.m_path.empty(), ppropvarNewValue);
        }
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, false, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

void CCmdReload::TabNotify(TBHDR * ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }
}

HRESULT CCmdFileDelete::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_Enabled == key)
    {
        if (HasActiveDocument())
        {
            CDocument doc = GetActiveDocument();
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, !doc.m_path.empty(), ppropvarNewValue);
        }
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, false, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

void CCmdFileDelete::TabNotify(TBHDR * ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }
}

bool CCmdFileDelete::Execute()
{
    if (HasActiveDocument())
    {
        CDocument doc = GetActiveDocument();
        if (!doc.m_path.empty())
        {
            // ask first
            ResString rTitle(hRes, IDS_FILEDELETE_TITLE);
            ResString rQuestion(hRes, IDS_FILEDELETE_ASK);
            ResString rDelete(hRes, IDS_FILEDELETE_DEL);
            ResString rCancel(hRes, IDS_FILEDELETE_CANCEL);
            std::wstring filename = CPathUtils::GetFileName(doc.m_path);
            std::wstring sQuestion = CStringUtils::Format(rQuestion, filename.c_str());

            TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
            TASKDIALOG_BUTTON aCustomButtons[2];
            aCustomButtons[0].nButtonID = 100;
            aCustomButtons[0].pszButtonText = rDelete;
            aCustomButtons[1].nButtonID = 101;
            aCustomButtons[1].pszButtonText = rCancel;

            tdc.hwndParent = GetHwnd();
            tdc.hInstance = hRes;
            tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
            tdc.dwCommonButtons = TDCBF_CANCEL_BUTTON;
            tdc.pButtons = aCustomButtons;
            tdc.cButtons = _countof(aCustomButtons);
            tdc.pszWindowTitle = MAKEINTRESOURCE(IDS_APP_TITLE);
            tdc.pszMainIcon = TD_WARNING_ICON;
            tdc.pszMainInstruction = rTitle;
            tdc.pszContent = sQuestion.c_str();
            tdc.nDefaultButton = 101;
            int nClickedBtn = 0;
            HRESULT hr = TaskDialogIndirect(&tdc, &nClickedBtn, NULL, NULL);

            if (SUCCEEDED(hr))
            {
                if (nClickedBtn == 100)
                {
                    // Close the tab
                    if (!CloseTab(GetTabIndexFromDocID(GetCurrentTabId()), false))
                        return false;

                    _COM_SMARTPTR_TYPEDEF(IFileOperation, __uuidof(IFileOperation));
                    IFileOperationPtr pfo = NULL;
                    HRESULT hr = pfo.CreateInstance(CLSID_FileOperation, NULL, CLSCTX_ALL);

                    if (SUCCEEDED(hr))
                    {
                        // Set parameters for current operation
                        hr = pfo->SetOperationFlags(FOF_ALLOWUNDO | FOF_NO_CONNECTED_ELEMENTS | FOFX_ADDUNDORECORD | FOF_NOCONFIRMATION | FOF_NORECURSION | FOF_SILENT | FOFX_SHOWELEVATIONPROMPT | FOFX_RECYCLEONDELETE);

                        if (SUCCEEDED(hr))
                        {
                            // Create IShellItem instance associated to file to delete
                            _COM_SMARTPTR_TYPEDEF(IShellItem, __uuidof(IShellItem));
                            IShellItemPtr psiFileToDelete = NULL;
                            hr = SHCreateItemFromParsingName(doc.m_path.c_str(), NULL, IID_PPV_ARGS(&psiFileToDelete));

                            if (SUCCEEDED(hr))
                            {
                                // Declare this shell item (file) to be deleted
                                hr = pfo->DeleteItem(psiFileToDelete, NULL);
                            }
                        }
                        pfo->SetOwnerWindow(GetHwnd());
                        if (SUCCEEDED(hr))
                        {
                            // Perform the deleting operation
                            hr = pfo->PerformOperations();
                            if (SUCCEEDED(hr))
                            {
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}
