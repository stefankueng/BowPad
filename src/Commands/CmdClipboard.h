// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2018, 2020-2022 - Stefan Kueng
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
    ClipboardBase(void* obj)
        : ICommand(obj)
    {
    }

    ~ClipboardBase() override = default;

protected:
    std::string GetHtmlSelection() const;
    void        AddHtmlStringToClipboard(const std::string& sHtml) const;
    void        AddLexerToClipboard() const;
    void        SetLexerFromClipboard() const;
};

class CCmdCut : public ClipboardBase
{
public:
    CCmdCut(void* obj)
        : ClipboardBase(obj)
    {
    }

    ~CCmdCut() override = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdCut; }

    void ScintillaNotify(SCNotification* pScn) override;
};

class CCmdCutPlain : public ClipboardBase
{
public:
    CCmdCutPlain(void* obj)
        : ClipboardBase(obj)
    {
    }

    ~CCmdCutPlain() override = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdCutPlain; }

    void ScintillaNotify(SCNotification* pScn) override;
};

class CCmdCopy : public ClipboardBase
{
public:
    CCmdCopy(void* obj)
        : ClipboardBase(obj)
    {
    }

    ~CCmdCopy() override = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdCopy; }
};

class CCmdCopyPlain : public ClipboardBase
{
public:
    CCmdCopyPlain(void* obj)
        : ClipboardBase(obj)
    {
    }

    ~CCmdCopyPlain() override = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdCopyPlain; }
};

class CCmdPaste : public ClipboardBase
{
public:
    CCmdPaste(void* obj)
        : ClipboardBase(obj)
    {
    }

    ~CCmdPaste() override = default;

    bool    Execute() override;

    UINT    GetCmdId() override { return cmdPaste; }

    void    OnClipboardChanged() override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override;
};

class CCmdPasteHtml : public ClipboardBase
{
public:
    CCmdPasteHtml(void* obj)
        : ClipboardBase(obj)
    {
    }

    ~CCmdPasteHtml() override = default;

    bool    Execute() override;

    UINT    GetCmdId() override { return cmdPasteHtml; }

    void    OnClipboardChanged() override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override;

private:
    void HtmlExtractMetadata(const std::string& cfHtml, std::string* baseURL, size_t* htmlStart, size_t* fragmentStart, size_t* fragmentEnd) const;
};
