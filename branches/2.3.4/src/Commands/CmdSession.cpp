// This file is part of BowPad.
//
// Copyright (C) 2014-2017 - Stefan Kueng
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
#include "ICommand.h"
#include "BowPadUI.h"
#include "BowPad.h"

#include "CmdSession.h"

#include "StringUtils.h"
#include "IniSettings.h"
#include "CmdLineParser.h"
#include "OnOutOfScope.h"
#include "ResString.h"
#include "ProgressDlg.h"

namespace
{
    const wchar_t g_sessionSection[] = { L"TabSession" };
    const int BP_DEFAULT_SESSION_SIZE = 500;
};

static void SetAutoLoad(bool bAutoLoad)
{
    CIniSettings::Instance().SetInt64(g_sessionSection, L"autoload", bAutoLoad);
}

static bool GetAutoLoad()
{
    bool bAutoLoad = CIniSettings::Instance().GetInt64(g_sessionSection, L"autoload", 0) != 0;
    return bAutoLoad;
}


bool CCmdSessionLoad::Execute()
{
    RestoreSavedSession();
    return true;
}

void CCmdSessionLoad::OnClose()
{
    // BowPad is closing, save the current session

    auto& settings = CIniSettings::Instance();
    bool bAutoLoad = GetAutoLoad();
    // first remove the whole session section
    settings.Delete(g_sessionSection, nullptr);
    SetAutoLoad(bAutoLoad);
    // now go through all tabs and save their state
    int tabcount = GetTabCount();
    int activetab = GetActiveTabIndex();
    int saveindex = 0;

    auto SavePosSettings = [&](int saveindex, const CPosData& pos)
    {
        settings.SetInt64(g_sessionSection, CStringUtils::Format(L"selmode%d", saveindex).c_str(), pos.m_nSelMode);
        settings.SetInt64(g_sessionSection, CStringUtils::Format(L"startpos%d", saveindex).c_str(), pos.m_nStartPos);
        settings.SetInt64(g_sessionSection, CStringUtils::Format(L"endpos%d", saveindex).c_str(), pos.m_nEndPos);
        settings.SetInt64(g_sessionSection, CStringUtils::Format(L"scrollwidth%d", saveindex).c_str(), pos.m_nScrollWidth);
        settings.SetInt64(g_sessionSection, CStringUtils::Format(L"xoffset%d", saveindex).c_str(), pos.m_xOffset);
        settings.SetInt64(g_sessionSection, CStringUtils::Format(L"firstvisible%d", saveindex).c_str(), pos.m_nFirstVisibleLine);
    };

    int sessionSize = (int)settings.GetInt64(g_sessionSection, L"session_size", BP_DEFAULT_SESSION_SIZE);
    // No point saving more than we are prepared to load.
    int savecount = min(tabcount, sessionSize);
    for (int i = 0; i < savecount; ++i)
    {
        auto docId = GetDocIDFromTabIndex(i);
        const auto& doc = GetDocumentFromID(docId);
        if (doc.m_path.empty())
            continue;
        if (i == activetab)
        {
            CPosData pos;
            SaveCurrentPos(pos);

            settings.SetInt64(g_sessionSection, CStringUtils::Format(L"activetab%d", saveindex).c_str(), 1);
            SavePosSettings(saveindex, pos);
        }
        else
        {
            SavePosSettings(saveindex, doc.m_position);
            settings.SetInt64(g_sessionSection, CStringUtils::Format(L"activetab%d", saveindex).c_str(), 0);
        }
        settings.SetString(g_sessionSection, CStringUtils::Format(L"path%d", saveindex).c_str(), doc.m_path.c_str());
        settings.SetInt64(g_sessionSection, CStringUtils::Format(L"tabspace%d", saveindex).c_str(), doc.m_TabSpace);

        ++saveindex;
    }
}

void CCmdSessionLoad::RestoreSavedSession()
{
    // REIVEW: note the user can add text to a document and then exit without saving.
    // This means the saved state could refer to a selection or cursorlocation that
    // does not exist or refers to something else. This can be a bit confusing to the user.
    // However if we don't save the state on quitting without saving, a user may also scroll
    // to a new location then exit without saving but expect to return to where they were.
    // It's not clear how to best satisfy these competing needs. Currently we save state
    // regardless of exit status and if it's "wrong" on restore then user beware.

    ProfileTimer profile(L"RestoreSavedSession");
    auto& settings = CIniSettings::Instance();
    int numFilesToRestore = 0;

    int sessionSize = (int)settings.GetInt64(g_sessionSection, L"session_size", BP_DEFAULT_SESSION_SIZE);
    for (int fileNum = 0; fileNum < sessionSize; ++fileNum)
    {
        std::wstring key = CStringUtils::Format(L"path%d", fileNum);
        std::wstring path = settings.GetString(g_sessionSection, key.c_str(), L"");
        if (path.empty())
            break;
        ++numFilesToRestore;
    }
    if (numFilesToRestore == 0)
        return;

    // Block the UI to avoid excessive drawing/painting if loading more than
    // one file. Don't block the UI when loading one or less files as that
    // itself would lead to excessive repainting.
    OnOutOfScope(
        if (numFilesToRestore > 1)
        {
            BlockAllUIUpdates(false);
            HideProgressCtrl();
        }
    );
    if (numFilesToRestore > 1)
    {
        BlockAllUIUpdates(true);
        ShowProgressCtrl((UINT)settings.GetInt64(L"View", L"progressdelay", 1000));
    }


    DocID activeDoc;
    const unsigned int openflags = OpenFlags::IgnoreIfMissing | OpenFlags::NoActivate;
    int filecount = 0;
    for (int fileNum = 0; fileNum < sessionSize; ++fileNum)
    {
        std::wstring key = CStringUtils::Format(L"path%d", fileNum);
        std::wstring path = settings.GetString(g_sessionSection, key.c_str(), L"");
        if (path.empty())
            break;
        ++filecount;
        SetProgress(filecount, numFilesToRestore);
        int tabIndex = OpenFile(path.c_str(), openflags);
        if (tabIndex < 0)
            continue;
        // Don't use the index to track the active tab, as it's probably
        // not safe long term to assume the index where a tab was loaded
        // remains the same after other files load.
        auto docId = GetDocIDFromTabIndex(tabIndex);
        if (!docId.IsValid())
            continue;
        auto& doc = GetModDocumentFromID(docId);
        auto& pos = doc.m_position;
        pos.m_nSelMode           = (size_t)settings.GetInt64(g_sessionSection, CStringUtils::Format(L"selmode%d", fileNum).c_str(), 0);
        pos.m_nStartPos          = (size_t)settings.GetInt64(g_sessionSection, CStringUtils::Format(L"startpos%d", fileNum).c_str(), 0);
        pos.m_nEndPos            = (size_t)settings.GetInt64(g_sessionSection, CStringUtils::Format(L"endpos%d", fileNum).c_str(), 0);
        pos.m_nScrollWidth       = (size_t)settings.GetInt64(g_sessionSection, CStringUtils::Format(L"scrollwidth%d", fileNum).c_str(), 0);
        pos.m_xOffset            = (size_t)settings.GetInt64(g_sessionSection, CStringUtils::Format(L"xoffset%d", fileNum).c_str(), 0);
        pos.m_nFirstVisibleLine  = (size_t)settings.GetInt64(g_sessionSection, CStringUtils::Format(L"firstvisible%d", fileNum).c_str(), 0);
        doc.m_TabSpace           = (TabSpace)settings.GetInt64(g_sessionSection, CStringUtils::Format(L"tabspace%d", fileNum).c_str(), 0);
        if ((int)settings.GetInt64(g_sessionSection, CStringUtils::Format(L"activetab%d", fileNum).c_str(), 0))
            activeDoc = docId;
        RestoreCurrentPos(doc.m_position);
    }
    if (activeDoc.IsValid())
    {
        int activeTabIndex = GetTabIndexFromDocID(activeDoc);
        TabActivateAt(activeTabIndex);
    }
}


CCmdSessionAutoLoad::CCmdSessionAutoLoad(void * obj) : CCmdSessionLoad(obj)
{
}


bool CCmdSessionAutoLoad::Execute()
{
    SetAutoLoad(!GetAutoLoad());
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    return true;
}

HRESULT CCmdSessionAutoLoad::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key,
    const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, GetAutoLoad(), ppropvarNewValue);
    }
    return E_NOTIMPL;
}

void CCmdSessionAutoLoad::AfterInit()
{
    if (GetAutoLoad()) // Don't parse the command line unless we are autoloading.
    {
        CCmdLineParser parser(GetCommandLine());
        if (!parser.HasKey(L"multiple"))
            RestoreSavedSession();
    }
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

bool CCmdSessionRestoreLast::Execute()
{
    if (m_docstates.empty())
        return false;
    const SessionItem si = m_docstates.back();
    const auto& pos = si.posData;
    if (OpenFile(si.path.c_str(), OpenFlags::AddToMRU))
        RestoreCurrentPos(pos);
    m_docstates.pop_back();
    return true;
}

void CCmdSessionRestoreLast::OnDocumentClose(DocID id)
{
    const auto& doc = GetDocumentFromID(id);
    if (doc.m_path.empty())
        return;

    auto activeTabIndex = GetActiveTabIndex();
    auto index = GetTabIndexFromDocID(id);
    if (index == activeTabIndex)
    {
        CPosData pos;
        SaveCurrentPos(pos);
        m_docstates.emplace_back(doc.m_path, pos);
    }
    else
    {
        m_docstates.emplace_back(doc.m_path, doc.m_position);
    }
}
