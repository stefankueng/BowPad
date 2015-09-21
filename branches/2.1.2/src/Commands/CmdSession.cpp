// This file is part of BowPad.
//
// Copyright (C) 2014-2015 - Stefan Kueng
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
};

bool CCmdSessionLoad::Execute()
{
    RestoreSavedSession();
    return true;
}

void CCmdSessionLoad::OnClose()
{
    // BowPad is closing, save the current session

    bool bAutoLoad = CIniSettings::Instance().GetInt64(L"TabSession", L"autoload", 0) != 0;
    // first remove the whole session section
    CIniSettings::Instance().Delete(L"TabSession", nullptr);
    CIniSettings::Instance().SetInt64(L"TabSession", L"autoload", bAutoLoad);
    // now go through all tabs and save their state
    int tabcount = GetTabCount();
    int activetab = GetActiveTabIndex();
    int saveindex = 0;
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
            pos.m_nFirstVisibleLine   = ScintillaCall(SCI_GETFIRSTVISIBLELINE);
            pos.m_nFirstVisibleLine   = ScintillaCall(SCI_DOCLINEFROMVISIBLE, pos.m_nFirstVisibleLine);

            pos.m_nStartPos           = ScintillaCall(SCI_GETANCHOR);
            pos.m_nEndPos             = ScintillaCall(SCI_GETCURRENTPOS);
            pos.m_xOffset             = ScintillaCall(SCI_GETXOFFSET);
            pos.m_nSelMode            = ScintillaCall(SCI_GETSELECTIONMODE);
            pos.m_nScrollWidth        = ScintillaCall(SCI_GETSCROLLWIDTH);

            CIniSettings::Instance().SetString(L"TabSession", CStringUtils::Format(L"path%d", saveindex).c_str(), doc.m_path.c_str());
            CIniSettings::Instance().SetInt64 (L"TabSession", CStringUtils::Format(L"selmode%d", saveindex).c_str(), pos.m_nSelMode);
            CIniSettings::Instance().SetInt64 (L"TabSession", CStringUtils::Format(L"startpos%d", saveindex).c_str(), pos.m_nStartPos);
            CIniSettings::Instance().SetInt64 (L"TabSession", CStringUtils::Format(L"endpos%d", saveindex).c_str(), pos.m_nEndPos);
            CIniSettings::Instance().SetInt64 (L"TabSession", CStringUtils::Format(L"scrollwidth%d", saveindex).c_str(), pos.m_nScrollWidth);
            CIniSettings::Instance().SetInt64 (L"TabSession", CStringUtils::Format(L"xoffset%d", saveindex).c_str(), pos.m_xOffset);
            CIniSettings::Instance().SetInt64 (L"TabSession", CStringUtils::Format(L"firstvisible%d", saveindex).c_str(), pos.m_nFirstVisibleLine);
            CIniSettings::Instance().SetInt64 (L"TabSession", CStringUtils::Format(L"activetab%d", saveindex).c_str(), 1);
        }
        else
        {
            CIniSettings::Instance().SetString(L"TabSession", CStringUtils::Format(L"path%d", saveindex).c_str(), doc.m_path.c_str());
            CIniSettings::Instance().SetInt64 (L"TabSession", CStringUtils::Format(L"selmode%d", saveindex).c_str(), doc.m_position.m_nSelMode);
            CIniSettings::Instance().SetInt64 (L"TabSession", CStringUtils::Format(L"startpos%d", saveindex).c_str(), doc.m_position.m_nStartPos);
            CIniSettings::Instance().SetInt64 (L"TabSession", CStringUtils::Format(L"endpos%d", saveindex).c_str(), doc.m_position.m_nEndPos);
            CIniSettings::Instance().SetInt64 (L"TabSession", CStringUtils::Format(L"scrollwidth%d", saveindex).c_str(), doc.m_position.m_nScrollWidth);
            CIniSettings::Instance().SetInt64 (L"TabSession", CStringUtils::Format(L"xoffset%d", saveindex).c_str(), doc.m_position.m_xOffset);
            CIniSettings::Instance().SetInt64 (L"TabSession", CStringUtils::Format(L"firstvisible%d", saveindex).c_str(), doc.m_position.m_nFirstVisibleLine);
            CIniSettings::Instance().SetInt64 (L"TabSession", CStringUtils::Format(L"activetab%d", saveindex).c_str(), 0);
        }
        ++saveindex;
    }
}

void CCmdSessionLoad::RestoreSavedSession()
{
    int activeDoc = -1;
    FileTreeBlockRefresh(true);
    OnOutOfScope(FileTreeBlockRefresh(false));
    for (int i = 0; i < BP_MAX_SESSION_SIZE; ++i)
    {
        std::wstring key = CStringUtils::Format(L"path%d", i);
        std::wstring path = CIniSettings::Instance().GetString(L"TabSession", key.c_str(), L"");
        if (path.empty())
            break;
        if (OpenFile(path.c_str(), OpenFlags::IgnoreIfMissing))
        {
            if (HasActiveDocument())
            {
                CDocument doc = GetActiveDocument();
                auto& pos = doc.m_position;
                pos.m_nSelMode           = (size_t)CIniSettings::Instance().GetInt64(L"TabSession", CStringUtils::Format(L"selmode%d", i).c_str(), 0);
                pos.m_nStartPos          = (size_t)CIniSettings::Instance().GetInt64(L"TabSession", CStringUtils::Format(L"startpos%d", i).c_str(), 0);
                pos.m_nEndPos            = (size_t)CIniSettings::Instance().GetInt64(L"TabSession", CStringUtils::Format(L"endpos%d", i).c_str(), 0);
                pos.m_nScrollWidth       = (size_t)CIniSettings::Instance().GetInt64(L"TabSession", CStringUtils::Format(L"scrollwidth%d", i).c_str(), 0);
                pos.m_xOffset            = (size_t)CIniSettings::Instance().GetInt64(L"TabSession", CStringUtils::Format(L"xoffset%d", i).c_str(), 0);
                pos.m_nFirstVisibleLine  = (size_t)CIniSettings::Instance().GetInt64(L"TabSession", CStringUtils::Format(L"firstvisible%d", i).c_str(), 0);
                int docId = GetDocIdOfCurrentTab();
                APPVERIFY(docId >= 0);
                SetDocument(docId, doc);

                ScintillaCall(SCI_GOTOPOS, 0);
                ScintillaCall(SCI_SETSELECTIONMODE, pos.m_nSelMode);
                ScintillaCall(SCI_SETANCHOR, pos.m_nStartPos);
                ScintillaCall(SCI_SETCURRENTPOS, pos.m_nEndPos);
                ScintillaCall(SCI_CANCEL);
                if (ScintillaCall(SCI_GETWRAPMODE) != SC_WRAP_WORD)
                {
                    // only offset if not wrapping, otherwise the offset isn't needed at all
                    ScintillaCall(SCI_SETSCROLLWIDTH, pos.m_nScrollWidth);
                    ScintillaCall(SCI_SETXOFFSET, pos.m_xOffset);
                }
                ScintillaCall(SCI_CHOOSECARETX);
                size_t lineToShow = ScintillaCall(SCI_VISIBLEFROMDOCLINE,pos.m_nFirstVisibleLine);
                ScintillaCall(SCI_LINESCROLL, 0, lineToShow);
                if ((int)CIniSettings::Instance().GetInt64(L"TabSession", CStringUtils::Format(L"activetab%d", i).c_str(), 0))
                {
                    // Don't use the index to track the active tab, as it's probably
                    // not safe long term to assume the index where a tab was loaded
                    // remains the same after other files load.
                    activeDoc = docId;
                }
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
    bool bAutoLoad = CIniSettings::Instance().GetInt64(L"TabSession", L"autoload", 0) != 0;
    bAutoLoad = !bAutoLoad;
    CIniSettings::Instance().SetInt64(L"TabSession", L"autoload", bAutoLoad);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    return true;
}

HRESULT CCmdSessionAutoLoad::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key,
    const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        bool bAutoLoad = CIniSettings::Instance().GetInt64(L"TabSession", L"autoload", 0) != 0;
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, bAutoLoad, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

void CCmdSessionAutoLoad::AfterInit()
{
    CCmdLineParser parser(GetCommandLine());

    bool bAutoLoad = CIniSettings::Instance().GetInt64(L"TabSession", L"autoload", 0) != 0;
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
    {
        ScintillaCall(SCI_GOTOPOS, 0);

        ScintillaCall(SCI_SETSELECTIONMODE, pos.m_nSelMode);
        ScintillaCall(SCI_SETANCHOR, pos.m_nStartPos);
        ScintillaCall(SCI_SETCURRENTPOS, pos.m_nEndPos);
        ScintillaCall(SCI_CANCEL);
        if (ScintillaCall(SCI_GETWRAPMODE) != SC_WRAP_WORD)
        {
            // only offset if not wrapping, otherwise the offset isn't needed at all
            ScintillaCall(SCI_SETSCROLLWIDTH, pos.m_nScrollWidth);
            ScintillaCall(SCI_SETXOFFSET, pos.m_xOffset);
        }
        ScintillaCall(SCI_CHOOSECARETX);

        size_t lineToShow = ScintillaCall(SCI_VISIBLEFROMDOCLINE, pos.m_nFirstVisibleLine);
        ScintillaCall(SCI_LINESCROLL, 0, lineToShow);
    }
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
        pos.m_nFirstVisibleLine   = ScintillaCall(SCI_GETFIRSTVISIBLELINE);
        pos.m_nFirstVisibleLine   = ScintillaCall(SCI_DOCLINEFROMVISIBLE, pos.m_nFirstVisibleLine);

        pos.m_nStartPos           = ScintillaCall(SCI_GETANCHOR);
        pos.m_nEndPos             = ScintillaCall(SCI_GETCURRENTPOS);
        pos.m_xOffset             = ScintillaCall(SCI_GETXOFFSET);
        pos.m_nSelMode            = ScintillaCall(SCI_GETSELECTIONMODE);
        pos.m_nScrollWidth        = ScintillaCall(SCI_GETSCROLLWIDTH);

        m_docstates.push_back(SessionItem(doc.m_path, pos));
    }
    else
    {
        m_docstates.push_back(SessionItem(doc.m_path, doc.m_position));
    }
}
