// This file is part of BowPad.
//
// Copyright (C) 2015-2017, 2020-2022 - Stefan Kueng
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
#include "LexStyles.h"

#include <vector>
#include <set>

class CCmdSpellCheck : public ICommand
{
public:
    CCmdSpellCheck(void* obj);

    ~CCmdSpellCheck() override;

    bool    Execute() override;

    UINT    GetCmdId() override { return cmdSpellCheck; }

    void    ScintillaNotify(SCNotification* pScn) override;
    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override;

    void    OnTimer(UINT id) override;

protected:
    void Check();

private:
    bool                    m_enabled;
    std::wstring            m_lang;
    std::set<std::string>   m_keywords;
    int                     m_activeLexer;
    bool                    m_useComprehensiveCheck;
    LexerData               m_lexerData;
    std::unique_ptr<char[]> m_textBuffer;
    Sci_Position            m_textBufLen;
    sptr_t                  m_lastCheckedPos;
};

class CCmdSpellCheckLang : public ICommand
{
public:
    CCmdSpellCheckLang(void* obj);
    ~CCmdSpellCheckLang() override = default;

    bool    Execute() override { return false; }
    UINT    GetCmdId() override { return cmdSpellCheckLang; }
    bool    IsItemsSourceCommand() override { return true; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue) override;

    HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* pPropVarValue, IUISimplePropertySet* pCommandExecutionProperties) override;
};

class CCmdSpellCheckCorrect : public ICommand
{
public:
    CCmdSpellCheckCorrect(void* obj);
    ~CCmdSpellCheckCorrect() override = default;

    bool    Execute() override { return false; }
    UINT    GetCmdId() override { return cmdSpellCheckCorrect; }
    bool    IsItemsSourceCommand() override { return true; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue) override;

    HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* pPropVarValue, IUISimplePropertySet* pCommandExecutionProperties) override;

    void    ScintillaNotify(SCNotification* pScn) override;

private:
    std::vector<std::wstring> m_suggestions;
};

class CCmdSpellCheckAll : public ICommand
{
public:
    CCmdSpellCheckAll(void* obj);

    ~CCmdSpellCheckAll() override = default;

    bool    Execute() override;

    UINT    GetCmdId() override { return cmdSpellCheckAll; }
    void    AfterInit() override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override;
};

class CCmdSpellCheckUpper : public ICommand
{
public:
    CCmdSpellCheckUpper(void* obj);

    ~CCmdSpellCheckUpper() override = default;

    bool    Execute() override;

    UINT    GetCmdId() override { return cmdSpellCheckUpper; }
    void    AfterInit() override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override;
};
