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
#include "MRU.h"
#include "ScintillaWnd.h"


class CCmdBookmarks : public ICommand
{
public:

    CCmdBookmarks(void * obj) : ICommand(obj)
    {
        int maxFiles = (int)CIniSettings::Instance().GetInt64(L"bookmarks", L"maxfiles", 30);
        m_bookmarks.clear();
        for (int i = 0; i < maxFiles; ++i)
        {
            std::wstring sKey = CStringUtils::Format(L"file%d", i);
            std::wstring sBmData = CIniSettings::Instance().GetString(L"bookmarks", sKey.c_str(), L"");
            if (!sBmData.empty())
            {
                std::vector<std::wstring> tokens;
                stringtok(tokens, sBmData, true, L"*");
                std::vector<long> lines;
                std::wstring filepath;
                int i = 0;
                for (const auto& t : tokens)
                {
                    if (i == 0)
                        filepath = t;
                    else
                        lines.push_back(_wtol(t.c_str()));
                    ++i;
                }
                if (!filepath.empty() && !lines.empty())
                    m_bookmarks.push_back(std::make_tuple(filepath, lines));
            }
        }
    }

    ~CCmdBookmarks(void)
    {}


    virtual bool Execute() override
    {
        return true;
    }

    virtual UINT GetCmdId() override { return cmdBookmarks; }

    virtual void OnDocumentClose(int index) override
    {
        CDocument doc = GetDocumentFromID(GetDocIDFromTabIndex(index));
        if (doc.m_path.empty())
            return;
        bool bModified = false;
        // look if the path is already in our list
        for (auto it = m_bookmarks.cbegin(); it != m_bookmarks.cend(); ++it)
        {
            if (_wcsicmp(doc.m_path.c_str(), std::get<0>(*it).c_str()) == 0)
            {
                // remove the entry for this path
                m_bookmarks.erase(it);
                bModified = true;
                break;
            }
        }

        // find all bookmarks
        std::vector<long> bookmarklines;
        long line = -1;
        do
        {
            line = (long)ScintillaCall(SCI_MARKERNEXT, line + 1, (1 << MARK_BOOKMARK));
            if (line >= 0)
                bookmarklines.push_back(line);
        } while (line >= 0);

        if (!bookmarklines.empty())
        {
            m_bookmarks.push_front(std::make_tuple(doc.m_path, bookmarklines));
            bModified = true;
        }

        if (bModified)
        {
            // Save the bookmarks to the ini file
            int maxFiles = (int)CIniSettings::Instance().GetInt64(L"bookmarks", L"maxfiles", 30);
            int i = 0;
            for (const auto& bm : m_bookmarks)
            {
                std::wstring sValue = std::get<0>(bm);
                const auto lines = std::get<1>(bm);
                for (const auto& line : lines)
                {
                    sValue += L"*";
                    sValue += CStringUtils::Format(L"%ld", line);
                }
                std::wstring sKey = CStringUtils::Format(L"file%d", i);
                CIniSettings::Instance().SetString(L"bookmarks", sKey.c_str(), sValue.c_str());
                if (i > maxFiles)
                    break;
                ++i;
            }
        }
    }

    virtual void OnDocumentOpen(int index)
    {
        CDocument doc = GetDocumentFromID(GetDocIDFromTabIndex(index));
        if (doc.m_path.empty())
            return;

        // look if the path is already in our list
        for (auto it = m_bookmarks.cbegin(); it != m_bookmarks.cend(); ++it)
        {
            if (_wcsicmp(doc.m_path.c_str(), std::get<0>(*it).c_str()) == 0)
            {
                // apply the bookmarks
                const auto lines = std::get<1>(*it);
                for (const auto& line : lines)
                {
                    ScintillaCall(SCI_MARKERADD, line, MARK_BOOKMARK);
                    DocScrollAddLineColor(DOCSCROLLTYPE_BOOKMARK, line, RGB(255, 0, 0));
                }
                break;
            }
        }
    }

private:
    std::list<std::tuple<std::wstring, std::vector<long>>> m_bookmarks;
};

class CCmdBookmarkToggle : public ICommand
{
public:

    CCmdBookmarkToggle(void * obj) : ICommand(obj)
    {
    }

    ~CCmdBookmarkToggle(void)
    {
    }

    virtual bool Execute() override
    {
        long line = long(ScintillaCall(SCI_LINEFROMPOSITION, ScintillaCall(SCI_GETCURRENTPOS)));

        LRESULT state = ScintillaCall(SCI_MARKERGET, line);
        if ((state & (1 << MARK_BOOKMARK)) != 0)
        {
            ScintillaCall(SCI_MARKERDELETE, line, MARK_BOOKMARK);
            DocScrollRemoveLine(DOCSCROLLTYPE_BOOKMARK, line);
        }
        else
        {
            ScintillaCall(SCI_MARKERADD, line, MARK_BOOKMARK);
            DocScrollAddLineColor(DOCSCROLLTYPE_BOOKMARK, line, RGB(255,0,0));
        }
        InvalidateUICommand(cmdBookmarkNext, UI_INVALIDATIONS_STATE, NULL);
        InvalidateUICommand(cmdBookmarkPrev, UI_INVALIDATIONS_STATE, NULL);
        InvalidateUICommand(cmdBookmarkClearAll, UI_INVALIDATIONS_STATE, NULL);

        return true;
    }

    virtual UINT GetCmdId() override { return cmdBookmarkToggle; }
};

class CCmdBookmarkClearAll : public ICommand
{
public:

    CCmdBookmarkClearAll(void * obj) : ICommand(obj)
    {
    }

    ~CCmdBookmarkClearAll(void)
    {
    }

    virtual bool Execute() override
    {
        ScintillaCall(SCI_MARKERDELETEALL, MARK_BOOKMARK);
        DocScrollClear(DOCSCROLLTYPE_BOOKMARK);
        return true;
    }

    virtual UINT GetCmdId() override { return cmdBookmarkClearAll; }
};

class CCmdBookmarkNext : public ICommand
{
public:

    CCmdBookmarkNext(void * obj) : ICommand(obj)
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }

    ~CCmdBookmarkNext(void)
    {
    }

    virtual bool Execute() override
    {
        long line = (long)ScintillaCall(SCI_LINEFROMPOSITION, ScintillaCall(SCI_GETCURRENTPOS));
        line = (long)ScintillaCall(SCI_MARKERNEXT, line+1, (1 << MARK_BOOKMARK));
        if (line >= 0)
            ScintillaCall(SCI_GOTOLINE, line);
        else
        {
            // retry from the start of the document
            line = (long)ScintillaCall(SCI_MARKERNEXT, 0, (1 << MARK_BOOKMARK));
            if (line >= 0)
                ScintillaCall(SCI_GOTOLINE, line);
        }
        return true;
    }

    virtual UINT GetCmdId() override { return cmdBookmarkNext; }

    virtual void ScintillaNotify(Scintilla::SCNotification * pScn) override
    {
        if (pScn->nmhdr.code == SCN_MODIFIED)
            InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
        else if ((pScn->nmhdr.code == SCN_MARGINCLICK) && (pScn->margin == SC_MARGE_SYBOLE) && !pScn->modifiers)
            InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }

    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override
    {
        if (UI_PKEY_Enabled == key)
        {
            long nextpos = (long)ScintillaCall(SCI_MARKERNEXT, 0, (1 << MARK_BOOKMARK));
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, nextpos >= 0, ppropvarNewValue);
        }
        return E_NOTIMPL;
    }

};

class CCmdBookmarkPrev : public ICommand
{
public:

    CCmdBookmarkPrev(void * obj) : ICommand(obj)
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }

    ~CCmdBookmarkPrev(void)
    {
    }

    virtual bool Execute() override
    {
        long line = (long)ScintillaCall(SCI_LINEFROMPOSITION, ScintillaCall(SCI_GETCURRENTPOS));
        line = (long)ScintillaCall(SCI_MARKERPREVIOUS, line-1, (1 << MARK_BOOKMARK));
        if (line >= 0)
            ScintillaCall(SCI_GOTOLINE, line);
        else
        {
            // retry from the start of the document
            line = (long)ScintillaCall(SCI_MARKERPREVIOUS, ScintillaCall(SCI_GETLINECOUNT), (1 << MARK_BOOKMARK));
            if (line >= 0)
                ScintillaCall(SCI_GOTOLINE, line);
        }
        return true;
    }

    virtual UINT GetCmdId() override { return cmdBookmarkPrev; }

    virtual void ScintillaNotify(Scintilla::SCNotification * pScn) override
    {
        if (pScn->nmhdr.code == SCN_MODIFIED)
            InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
        else if ((pScn->nmhdr.code == SCN_MARGINCLICK) && (pScn->margin == SC_MARGE_SYBOLE) && !pScn->modifiers)
            InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }

    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override
    {
        if (UI_PKEY_Enabled == key)
        {
            long nextpos = (long)ScintillaCall(SCI_MARKERNEXT, 0, (1 << MARK_BOOKMARK));
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, nextpos >= 0, ppropvarNewValue);
        }
        return E_NOTIMPL;
    }

};
