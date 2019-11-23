// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2018 - Stefan Kueng
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

class ClipboardBase : public ICommand
{
public:
    ClipboardBase(void * obj) : ICommand(obj)
    {
    }

    virtual ~ClipboardBase() = default;

protected:
    std::string GetHtmlSelection();
    void AddHtmlStringToClipboard(const std::string& sHtml);
    void AddLexerToClipboard();
    void SetLexerFromClipboard();
};

class CCmdCut : public ClipboardBase
{
public:

    CCmdCut(void * obj) : ClipboardBase(obj)
    {
    }

    ~CCmdCut() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdCut; }

    void ScintillaNotify(SCNotification * pScn) override;
};

class CCmdCutPlain : public ClipboardBase
{
public:

    CCmdCutPlain(void * obj) : ClipboardBase(obj)
    {
    }

    ~CCmdCutPlain() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdCutPlain; }

    void ScintillaNotify(SCNotification * pScn) override;
};


class CCmdCopy : public ClipboardBase
{
public:

    CCmdCopy(void * obj) : ClipboardBase(obj)
    {
    }

    ~CCmdCopy() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdCopy; }
};

class CCmdCopyPlain : public ClipboardBase
{
public:

    CCmdCopyPlain(void * obj) : ClipboardBase(obj)
    {
    }

    ~CCmdCopyPlain() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdCopyPlain; }
};

class CCmdPaste : public ClipboardBase
{
public:

    CCmdPaste(void * obj) : ClipboardBase(obj)
    {
    }

    ~CCmdPaste() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdPaste; }

    void ScintillaNotify(SCNotification * pScn) override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override;
};

class CCmdPasteHtml : public ClipboardBase
{
public:

    CCmdPasteHtml(void * obj) : ClipboardBase(obj)
    {
    }

    ~CCmdPasteHtml() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdPasteHtml; }

    void ScintillaNotify(SCNotification * pScn) override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override;
private:
    void HtmlExtractMetadata(const std::string& cf_html, std::string* base_url, size_t* html_start, size_t* fragment_start, size_t* fragment_end);
};
