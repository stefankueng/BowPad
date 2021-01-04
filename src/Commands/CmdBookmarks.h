// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2020-2021 - Stefan Kueng
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

#include <vector>
#include <map>

namespace Scintilla
{
struct SCNotification;
};

class CCmdBookmarks : public ICommand
{
    struct CaseInsensitiveLess
    {
        bool operator()(const std::wstring& s1, const std::wstring& s2) const
        {
            return _wcsicmp(s1.c_str(), s2.c_str()) < 0;
        }
    };

    using BookmarkContainer = std::map<std::wstring, std::vector<sptr_t>, CaseInsensitiveLess>;

public:
    CCmdBookmarks(void* obj);

    ~CCmdBookmarks() = default;

    bool Execute() override
    {
        return true;
    }

    UINT GetCmdId() override { return cmdBookmarks; }

    void OnDocumentClose(DocID id) override;

    void OnDocumentOpen(DocID id) override;

private:
    BookmarkContainer m_bookmarks;
};

class CCmdBookmarkToggle : public ICommand
{
public:
    CCmdBookmarkToggle(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdBookmarkToggle() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdBookmarkToggle; }
};

class CCmdBookmarkClearAll : public ICommand
{
public:
    CCmdBookmarkClearAll(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdBookmarkClearAll() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdBookmarkClearAll; }
};

class CCmdBookmarkNext : public ICommand
{
public:
    CCmdBookmarkNext(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdBookmarkNext() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdBookmarkNext; }

    void AfterInit() override
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
    }
    void ScintillaNotify(SCNotification* pScn) override;

    HRESULT IUICommandHandlerUpdateProperty(
        REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override;
    void TabNotify(TBHDR* ptbhdr) override
    {
        if (ptbhdr->hdr.code == TCN_SELCHANGE)
        {
            InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
        }
    }
};

class CCmdBookmarkPrev : public ICommand
{
public:
    CCmdBookmarkPrev(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdBookmarkPrev() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdBookmarkPrev; }
    void AfterInit() override
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
    }

    void ScintillaNotify(SCNotification* pScn) override;

    HRESULT IUICommandHandlerUpdateProperty(
        REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override;
    void TabNotify(TBHDR* ptbhdr) override
    {
        if (ptbhdr->hdr.code == TCN_SELCHANGE)
        {
            InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
        }
    }
};
