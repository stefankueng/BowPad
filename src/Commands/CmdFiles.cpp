// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2020 - Stefan Kueng
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
#include "AppUtils.h"
#include "ResString.h"

#include <VersionHelpers.h>
#include <Shobjidl.h>

bool CCmdOpen::Execute()
{
    PreserveChdir keepCWD;

    IFileOpenDialogPtr pfd;

    HRESULT hr = pfd.CreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER);
    if (CAppUtils::FailedShowMessage(hr))
        return false;

    // Set the dialog options, if it fails don't continue as we don't
    // know what options we're blending. Shouldn't ever fail anyway.
    DWORD dwOptions;
    hr = pfd->GetOptions(&dwOptions);
    if (CAppUtils::FailedShowMessage(hr))
        return false;

    // allow the user to enter a filename that does not exist yet
    dwOptions &= ~(FOS_FILEMUSTEXIST);

    // If we can't set our options, we have no idea what's happening
    // so don't continue.
    hr = pfd->SetOptions(dwOptions | FOS_ALLOWMULTISELECT);
    if (CAppUtils::FailedShowMessage(hr))
        return false;

    // Set the standard title.
    ResString rTitle(g_hRes, IDS_APP_TITLE);
    pfd->SetTitle(rTitle);

    // Set the default folder to the folder of the current tab.
    if (HasActiveDocument())
    {
        const auto& doc = GetActiveDocument();
        if (!doc.m_path.empty())
        {
            auto folder = CPathUtils::GetParentDirectory(doc.m_path);
            IShellItemPtr psiDefFolder;
            hr = SHCreateItemFromParsingName(folder.c_str(), nullptr, IID_PPV_ARGS(&psiDefFolder));
            if (!CAppUtils::FailedShowMessage(hr))
            {
                // Assume if we can't set the folder we can at least continue
                // and the user might be able to set another folder or something.
                hr = pfd->SetFolder(psiDefFolder);
                CAppUtils::FailedShowMessage(hr);
            }
        }
    }

    // Show the open file dialog, not much we can do if that doesn't work.
    hr = pfd->Show(GetHwnd());
    if(hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) // Expected error
        return false;
    else
    {
        if (CAppUtils::FailedShowMessage(hr))
            return false;
    }

    IShellItemArrayPtr psiaResults;
    hr = pfd->GetResults(&psiaResults);
    if (CAppUtils::FailedShowMessage(hr))
        return false;

    // Fetch the (possibly multiple) results. Some may possibly fail
    // but get as many as we can. We don't report partial failure.
    // We could make it an all or nothing deal but we have chosen not to.

    DWORD count = 0;
    hr = psiaResults->GetCount(&count);
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    std::vector<std::wstring> paths;
    paths.reserve(count);
    for (decltype(count) i = 0; i < count; ++i)
    {
        IShellItemPtr psiResult;
        hr = psiaResults->GetItemAt(i, &psiResult);
        if (!CAppUtils::FailedShowMessage(hr))
        {
            PWSTR pszPath = nullptr;
            hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
            if (!CAppUtils::FailedShowMessage(hr))
            {
                paths.push_back(pszPath);
                CoTaskMemFree(pszPath);
            }
        }
    }
    std::sort(paths.begin(), paths.end(), [](const auto& lhs, const auto& rhs)
    {
        return CPathUtils::PathCompare(lhs, rhs) < 0;
    });
    // treat opening a single file differently so we can pass OpenFlags.
    if (paths.size() == 1)
        OpenFile(paths[0].c_str(), OpenFlags::AddToMRU | OpenFlags::AskToCreateIfMissing);
    else
        OpenFiles(paths);

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool CCmdSave::Execute()
{
    return SaveCurrentTab();
}

void CCmdSave::ScintillaNotify( SCNotification * pScn )
{
    switch (pScn->nmhdr.code)
    {
    case SCN_SAVEPOINTREACHED:
    case SCN_SAVEPOINTLEFT:
        {
            InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
        }
        break;
    }
}

void CCmdSave::TabNotify(TBHDR * ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
    }
}

HRESULT CCmdSave::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue )
{
    if (UI_PKEY_Enabled == key)
    {
        if (HasActiveDocument())
        {
            const auto& doc = GetActiveDocument();
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, doc.m_bIsDirty||doc.m_bNeedsSaving, ppropvarNewValue);
        }
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, false, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

bool CCmdSaveAll::Execute()
{
    auto docCount = GetDocumentCount();
    for (decltype(docCount) i = 0; i < docCount; ++i)
    {
        auto docID = GetDocIDFromTabIndex(i);
        if (GetDocumentFromID(docID).m_bIsDirty)
        {
            SaveDoc(docID);
        }
    }
    return true;
}

void CCmdSaveAll::ScintillaNotify( SCNotification * pScn )
{
    switch (pScn->nmhdr.code)
    {
    case SCN_SAVEPOINTREACHED:
    case SCN_SAVEPOINTLEFT:
        {
            InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
        }
        break;
    }
}

void CCmdSaveAll::TabNotify(TBHDR * ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
    }
}

HRESULT CCmdSaveAll::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue )
{
    if (UI_PKEY_Enabled == key)
    {
        auto docCount = GetDocumentCount();
        int dirtycount = 0;
        for (decltype(docCount) i = 0; i < docCount; ++i)
        {
            const auto& doc = GetDocumentFromID(GetDocIDFromTabIndex(i));
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
            const auto& doc = GetActiveDocument();
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
        InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
    }
}

HRESULT CCmdFileDelete::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_Enabled == key)
    {
        if (HasActiveDocument())
        {
            const auto& doc = GetActiveDocument();
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
        InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
    }
}

bool CCmdFileDelete::Execute()
{
    if (HasActiveDocument())
    {
        const auto& doc = GetActiveDocument();
        if (!doc.m_path.empty())
        {
            // ask first
            ResString rTitle(g_hRes, IDS_FILEDELETE_TITLE);
            ResString rQuestion(g_hRes, IDS_FILEDELETE_ASK);
            ResString rDelete(g_hRes, IDS_FILEDELETE_DEL);
            ResString rCancel(g_hRes, IDS_FILEDELETE_CANCEL);
            std::wstring filename = CPathUtils::GetFileName(doc.m_path);
            std::wstring sQuestion = CStringUtils::Format(rQuestion, filename.c_str());

            TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
            TASKDIALOG_BUTTON aCustomButtons[2];
            aCustomButtons[0].nButtonID = 100;
            aCustomButtons[0].pszButtonText = rDelete;
            aCustomButtons[1].nButtonID = 101;
            aCustomButtons[1].pszButtonText = rCancel;

            tdc.hwndParent = GetHwnd();
            tdc.hInstance = g_hRes;
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
            HRESULT hr = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);

            if (!CAppUtils::FailedShowMessage(hr))
            {
                if (nClickedBtn == 100)
                {
                    // copy the path, doc and doc.m_path will be invalid after closing the tab
                    auto path = doc.m_path;
                    // Close the tab
                    if (!CloseTab(GetTabIndexFromDocID(GetDocIdOfCurrentTab()), false))
                        return false;

                    IFileOperationPtr pfo = nullptr;
                    hr = pfo.CreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL);

                    if (!CAppUtils::FailedShowMessage(hr))
                    {
                        // Set parameters for current operation
                        DWORD opFlags = FOF_ALLOWUNDO | FOF_NO_CONNECTED_ELEMENTS | FOF_NOCONFIRMATION | FOF_NORECURSION | FOF_SILENT | FOFX_SHOWELEVATIONPROMPT;
                        if (IsWindows8OrGreater())
                            opFlags |= FOFX_RECYCLEONDELETE | FOFX_ADDUNDORECORD;
                        hr = pfo->SetOperationFlags(opFlags);

                        if (!CAppUtils::FailedShowMessage(hr))
                        {
                            // Create IShellItem instance associated to file to delete
                            IShellItemPtr psiFileToDelete = nullptr;
                            hr = SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&psiFileToDelete));

                            if (!CAppUtils::FailedShowMessage(hr))
                            {
                                // Declare this shell item (file) to be deleted
                                hr = pfo->DeleteItem(psiFileToDelete, nullptr);
                            }
                        }
                        pfo->SetOwnerWindow(GetHwnd());
                        if (!CAppUtils::FailedShowMessage(hr))
                        {
                            // Perform the deleting operation
                            hr = pfo->PerformOperations();
                            if (!CAppUtils::FailedShowMessage(hr))
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

bool CCmdSaveAuto::Execute()
{
    int autosave = (int)CIniSettings::Instance().GetInt64(L"View", L"autosave", 0);
    CIniSettings::Instance().SetInt64(L"View", L"autosave", autosave ? 0 : 1);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    return true;
}

HRESULT CCmdSaveAuto::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        int autosave = (int)CIniSettings::Instance().GetInt64(L"View", L"autosave", 0);
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, autosave != 0, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

void CCmdSaveAuto::TabNotify(TBHDR * ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        Save();
    }
}

void CCmdSaveAuto::ScintillaNotify(SCNotification * pScn)
{
    if (pScn->nmhdr.code == SCN_FOCUSOUT)
    {
        Save();
    }
}

void CCmdSaveAuto::Save()
{
    int autosave = (int)CIniSettings::Instance().GetInt64(L"View", L"autosave", 0);
    if (autosave)
    {
        const auto& doc = GetActiveDocument();
        if ((doc.m_bIsDirty || doc.m_bNeedsSaving) && !doc.m_path.empty())
            SaveCurrentTab();
    }
}
