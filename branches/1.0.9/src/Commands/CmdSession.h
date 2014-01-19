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

#pragma once
#include "ICommand.h"
#include "BowPadUI.h"
#include "StringUtils.h"
#include "IniSettings.h"
#include "CmdLineParser.h"

class CCmdSessionLoad : public ICommand
{
public:

    CCmdSessionLoad(void * obj) : ICommand(obj)
    {
    }

    ~CCmdSessionLoad(void)
    {
    }

    virtual bool Execute() override
    {
        RestoreSavedSession();
        return true;
    }

    virtual UINT GetCmdId() override { return cmdSessionLoad; }

    virtual void OnClose() override
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
        for (int i = 0; i < tabcount; ++i)
        {
            CDocument doc = GetDocumentFromID(GetDocIDFromTabIndex(i));
            if (!doc.m_path.empty())
            {
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
                }
            }
            ++saveindex;
        }
    }
protected:
    void    RestoreSavedSession()
    {
        for (int i = 0; i < 100; ++i)
        {
            std::wstring key = CStringUtils::Format(L"path%d", i);
            std::wstring path = CIniSettings::Instance().GetString(L"TabSession", key.c_str(), L"");
            if (path.empty())
                break;
            if (OpenFile(path.c_str()))
            {
                if (HasActiveDocument())
                {
                    CDocument doc = GetActiveDocument();
                    doc.m_position.m_nSelMode           = (size_t)CIniSettings::Instance().GetInt64(L"TabSession", CStringUtils::Format(L"selmode%d", i).c_str(), 0);
                    doc.m_position.m_nStartPos          = (size_t)CIniSettings::Instance().GetInt64(L"TabSession", CStringUtils::Format(L"startpos%d", i).c_str(), 0);
                    doc.m_position.m_nEndPos            = (size_t)CIniSettings::Instance().GetInt64(L"TabSession", CStringUtils::Format(L"endpos%d", i).c_str(), 0);
                    doc.m_position.m_nScrollWidth       = (size_t)CIniSettings::Instance().GetInt64(L"TabSession", CStringUtils::Format(L"scrollwidth%d", i).c_str(), 0);
                    doc.m_position.m_xOffset            = (size_t)CIniSettings::Instance().GetInt64(L"TabSession", CStringUtils::Format(L"xoffset%d", i).c_str(), 0);
                    doc.m_position.m_nFirstVisibleLine  = (size_t)CIniSettings::Instance().GetInt64(L"TabSession", CStringUtils::Format(L"firstvisible%d", i).c_str(), 0);
                    SetDocument(GetCurrentTabId(), doc);

                    ScintillaCall(SCI_GOTOPOS, 0);
                    ScintillaCall(SCI_SETSELECTIONMODE, doc.m_position.m_nSelMode);
                    ScintillaCall(SCI_SETANCHOR, doc.m_position.m_nStartPos);
                    ScintillaCall(SCI_SETCURRENTPOS, doc.m_position.m_nEndPos);
                    ScintillaCall(SCI_CANCEL);
                    if (ScintillaCall(SCI_GETWRAPMODE) != SC_WRAP_WORD)
                    {
                        // only offset if not wrapping, otherwise the offset isn't needed at all
                        ScintillaCall(SCI_SETSCROLLWIDTH, doc.m_position.m_nScrollWidth);
                        ScintillaCall(SCI_SETXOFFSET, doc.m_position.m_xOffset);
                    }
                    ScintillaCall(SCI_CHOOSECARETX);
                    size_t lineToShow = ScintillaCall(SCI_VISIBLEFROMDOCLINE,doc.m_position.m_nFirstVisibleLine);
                    ScintillaCall(SCI_LINESCROLL, 0, lineToShow);
                }
            }
        }
    }
};

class CCmdSessionAutoLoad : public CCmdSessionLoad
{
public:

    CCmdSessionAutoLoad(void * obj) : CCmdSessionLoad(obj)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }

    ~CCmdSessionAutoLoad(void)
    {
    }

    virtual bool Execute() override
    {
        bool bAutoLoad = CIniSettings::Instance().GetInt64(L"TabSession", L"autoload", 0) != 0;
        bAutoLoad = !bAutoLoad;
        CIniSettings::Instance().SetInt64(L"TabSession", L"autoload", bAutoLoad);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        return true;
    }

    virtual UINT GetCmdId() override { return cmdSessionAutoLoad; }

    virtual void OnClose() override {}

    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override
    {
        if (UI_PKEY_BooleanValue == key)
        {
            bool bAutoLoad = CIniSettings::Instance().GetInt64(L"TabSession", L"autoload", 0) != 0;
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, bAutoLoad, ppropvarNewValue);
        }
        return E_NOTIMPL;
    }

    virtual void AfterInit() override
    {
        CCmdLineParser parser(GetCommandLine());

        bool bAutoLoad = CIniSettings::Instance().GetInt64(L"TabSession", L"autoload", 0) != 0;
        if (bAutoLoad && !parser.HasKey(L"multiple"))
            RestoreSavedSession();
    }

};

class CCmdSessionRestoreLast : public ICommand
{
public:

    CCmdSessionRestoreLast(void * obj) : ICommand(obj)
    {
    }

    ~CCmdSessionRestoreLast(void)
    {
    }

    virtual bool Execute() override
    {
        if (m_docstates.empty())
            return false;
        // restore the tab that was closed last
        auto pt = m_docstates.back();
        if (OpenFile(std::get<0>(pt).c_str()))
        {
            ScintillaCall(SCI_GOTOPOS, 0);

            ScintillaCall(SCI_SETSELECTIONMODE, std::get<1>(pt).m_nSelMode);
            ScintillaCall(SCI_SETANCHOR, std::get<1>(pt).m_nStartPos);
            ScintillaCall(SCI_SETCURRENTPOS, std::get<1>(pt).m_nEndPos);
            ScintillaCall(SCI_CANCEL);
            if (ScintillaCall(SCI_GETWRAPMODE) != SC_WRAP_WORD)
            {
                // only offset if not wrapping, otherwise the offset isn't needed at all
                ScintillaCall(SCI_SETSCROLLWIDTH, std::get<1>(pt).m_nScrollWidth);
                ScintillaCall(SCI_SETXOFFSET, std::get<1>(pt).m_xOffset);
            }
            ScintillaCall(SCI_CHOOSECARETX);

            size_t lineToShow = ScintillaCall(SCI_VISIBLEFROMDOCLINE, std::get<1>(pt).m_nFirstVisibleLine);
            ScintillaCall(SCI_LINESCROLL, 0, lineToShow);
        }
        m_docstates.pop_back();
        return true;
    }

    virtual UINT GetCmdId() override { return cmdSessionLast; }

    virtual void OnDocumentClose(int index) override
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

            m_docstates.push_back(std::make_tuple(doc.m_path, pos));
        }
        else
        {
            m_docstates.push_back(std::make_tuple(doc.m_path, doc.m_position));
        }
    }

private:
    std::list<std::tuple<std::wstring,CPosData>> m_docstates;
};
