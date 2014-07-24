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

#include <vector>
#include <tuple>

namespace Scintilla
{
	struct SCNotification;
};

class CCmdBookmarks : public ICommand
{
public:

    CCmdBookmarks(void * obj);

    ~CCmdBookmarks(void)
    {}

    bool Execute() override
    {
        return true;
    }

    UINT GetCmdId() override { return cmdBookmarks; }

    void OnDocumentClose(int index) override;

    void OnDocumentOpen(int index) override;

private:
	// REVIEW: A case insensitive string map to vector<lomg>
    // is a lot less code, way more readable and gets rid of tuple too!
    std::list<std::tuple<std::wstring, std::vector<long>>> m_bookmarks;
};

class CCmdBookmarkToggle : public ICommand
{
public:

    CCmdBookmarkToggle(void * obj) : ICommand(obj)
    {
    }

    ~CCmdBookmarkToggle(void) { }

    bool Execute() override;

    UINT GetCmdId() override { return cmdBookmarkToggle; }
};

class CCmdBookmarkClearAll : public ICommand
{
public:

    CCmdBookmarkClearAll(void * obj) : ICommand(obj)
    {
    }

    ~CCmdBookmarkClearAll(void) { }

    bool Execute() override;

    UINT GetCmdId() override { return cmdBookmarkClearAll; }
};

class CCmdBookmarkNext : public ICommand
{
public:

    CCmdBookmarkNext(void * obj) : ICommand(obj)
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }

    ~CCmdBookmarkNext(void) { }

    bool Execute() override;

    UINT GetCmdId() override { return cmdBookmarkNext; }

    void ScintillaNotify(Scintilla::SCNotification * pScn) override;

    HRESULT IUICommandHandlerUpdateProperty(
		REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override;
};

class CCmdBookmarkPrev : public ICommand
{
public:

    CCmdBookmarkPrev(void * obj) : ICommand(obj)
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }

    ~CCmdBookmarkPrev(void) { }

    bool Execute() override;

    UINT GetCmdId() override { return cmdBookmarkPrev; }

    void ScintillaNotify(Scintilla::SCNotification * pScn) override;

    HRESULT IUICommandHandlerUpdateProperty(
		REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override;
};
