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
#include "ICommand.h"
#include "BowPadUI.h"

#include "CmdSession.h"

#include "StringUtils.h"
#include "IniSettings.h"
#include "CmdLineParser.h"
#include "OnOutOfScope.h"

namespace
{
    const int BP_MAX_SESSION_SIZE = 100;
    const wchar_t g_sessionSection[] = { L"TabSession" };
};

bool CCmdSessionLoad::Execute()
{
    RestoreSavedSession();
    return true;
}

void CCmdSessionLoad::OnClose()
{
    // BowPad is closing, save the current session

    auto& settings = CIniSettings::Instance();
    bool bAutoLoad = settings.GetInt64(g_sessionSection, L"autoload", 0) != 0;
    // first remove the whole session section
    settings.Delete(g_sessionSection, nullptr);
    settings.SetInt64(g_sessionSection, L"autoload", bAutoLoad);
    // now go through all tabs and save their state
    int tabcount = GetTabCount();
    int activetab = GetActiveTabIndex();
    int saveindex = 0;

    auto SavePosSettings = [&](int saveindex, CPosData& pos)
    {
        settings.SetInt64(g_sessionSection, CStringUtils::Format(L"selmode%d", saveindex).c_str(), pos.m_nSelMode);
        settings.SetInt64(g_sessionSection, CStringUtils::Format(L"startpos%d", saveindex).c_str(), pos.m_nStartPos);
        settings.SetInt64(g_sessionSection, CStringUtils::Format(L"endpos%d", saveindex).c_str(), pos.m_nEndPos);
        settings.SetInt64(g_sessionSection, CStringUtils::Format(L"scrollwidth%d", saveindex).c_str(), pos.m_nScrollWidth);
        settings.SetInt64(g_sessionSection, CStringUtils::Format(L"xoffset%d", saveindex).c_str(), pos.m_xOffset);
        settings.SetInt64(g_sessionSection, CStringUtils::Format(L"firstvisible%d", saveindex).c_str(), pos.m_nFirstVisibleLine);
    };
    // No point saving more than we are prepared to load.
    int savecount = min(tabcount, BP_MAX_SESSION_SIZE);
    for (int i = 0; i < savecount; ++i)
    {
        int docId = GetDocIDFromTabIndex(i);
        CDocument doc = GetDocumentFromID(docId);
        if (doc.m_path.empty())
            continue;
        if (i == activetab)
        {
            CPosData pos;
            SaveCurrentPos(pos);

            settings.SetInt64(g_sessionSection, CStringUtils::Format(L"activetab%d", saveindex).c_str(), 1);
            SavePosSettings(saveindex, pos);
            settings.SetString(g_sessionSection, CStringUtils::Format(L"path%d", saveindex).c_str(), doc.m_path.c_str());
        }
        else
        {
            settings.SetString(g_sessionSection, CStringUtils::Format(L"path%d", saveindex).c_str(), doc.m_path.c_str());
            SavePosSettings(saveindex, doc.m_position);
            settings.SetInt64 (g_sessionSection, CStringUtils::Format(L"activetab%d", saveindex).c_str(), 0);
        }
        ++saveindex;
    }
}

void CCmdSessionLoad::RestoreSavedSession()
{
    int activeDoc = -1;
    FileTreeBlockRefresh(true);
    OnOutOfScope(FileTreeBlockRefresh(false));
    auto& settings = CIniSettings::Instance();
    int openflags = OpenFlags::IgnoreIfMissing | OpenFlags::NoActivate;
    for (int i = 0; i < BP_MAX_SESSION_SIZE; ++i)
    {
        std::wstring key = CStringUtils::Format(L"path%d", i);
        std::wstring path = settings.GetString(g_sessionSection, key.c_str(), L"");
        if (path.empty())
            break;
        if (OpenFile(path.c_str(), openflags))
        {
            auto docId = GetDocIDFromPath(path.c_str());
            if (docId >= 0)
            {
                CDocument doc = GetDocumentFromID(docId);
                auto& pos = doc.m_position;
                pos.m_nSelMode           = (size_t)settings.GetInt64(g_sessionSection, CStringUtils::Format(L"selmode%d", i).c_str(), 0);
                pos.m_nStartPos          = (size_t)settings.GetInt64(g_sessionSection, CStringUtils::Format(L"startpos%d", i).c_str(), 0);
                pos.m_nEndPos            = (size_t)settings.GetInt64(g_sessionSection, CStringUtils::Format(L"endpos%d", i).c_str(), 0);
                pos.m_nScrollWidth       = (size_t)settings.GetInt64(g_sessionSection, CStringUtils::Format(L"scrollwidth%d", i).c_str(), 0);
                pos.m_xOffset            = (size_t)settings.GetInt64(g_sessionSection, CStringUtils::Format(L"xoffset%d", i).c_str(), 0);
                pos.m_nFirstVisibleLine  = (size_t)settings.GetInt64(g_sessionSection, CStringUtils::Format(L"firstvisible%d", i).c_str(), 0);
                if ((int)settings.GetInt64(g_sessionSection, CStringUtils::Format(L"activetab%d", i).c_str(), 0))
                {
                    // Don't use the index to track the active tab, as it's probably
                    // not safe long term to assume the index where a tab was loaded
                    // remains the same after other files load.
                    activeDoc = docId;
                    if (docId == GetDocIDFromTabIndex(GetActiveTabIndex()))
                        RestoreCurrentPos(pos);
                }
                SetDocument(docId, doc);
            }
        }
    }
    if (activeDoc >= 0)
    {
        int activetab = GetTabIndexFromDocID(activeDoc);
        TabActivateAt(activetab);
    }
}


CCmdSessionAutoLoad::CCmdSessionAutoLoad(void * obj) : CCmdSessionLoad(obj)
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}


bool CCmdSessionAutoLoad::Execute()
{
    bool bAutoLoad = CIniSettings::Instance().GetInt64(g_sessionSection, L"autoload", 0) != 0;
    bAutoLoad = !bAutoLoad;
    CIniSettings::Instance().SetInt64(g_sessionSection, L"autoload", bAutoLoad);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    return true;
}

HRESULT CCmdSessionAutoLoad::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key,
    const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        bool bAutoLoad = CIniSettings::Instance().GetInt64(g_sessionSection, L"autoload", 0) != 0;
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, bAutoLoad, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

void CCmdSessionAutoLoad::AfterInit()
{
    CCmdLineParser parser(GetCommandLine());

    bool bAutoLoad = CIniSettings::Instance().GetInt64(g_sessionSection, L"autoload", 0) != 0;
    if (bAutoLoad && !parser.HasKey(L"multiple"))
        RestoreSavedSession();
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

void CCmdSessionRestoreLast::OnDocumentClose(int index)
{
    CDocument doc = GetDocumentFromID(GetDocIDFromTabIndex(index));
    if (doc.m_path.empty())
        return;

    if (index == GetActiveTabIndex())
    {
        CPosData pos;
        SaveCurrentPos(pos);
        m_docstates.push_back(SessionItem(doc.m_path, pos));
    }
    else
    {
        m_docstates.push_back(SessionItem(doc.m_path, doc.m_position));
    }
}
