// This file is part of BowPad.
//
// Copyright (C) 2015-2017, 2020-2021 - Stefan Kueng
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
#include "COMPtrs.h"
#include "LexStyles.h"

#include <vector>
#include <set>


class CCmdSpellcheck : public ICommand
{
public:

    CCmdSpellcheck(void * obj);

    ~CCmdSpellcheck();

    bool Execute() override;

    UINT GetCmdId() override { return cmdSpellCheck; }

    void ScintillaNotify(SCNotification * pScn) override;
    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override;

    void OnTimer(UINT id) override;
protected:
    void        Check();


private:
    bool                        m_enabled;
    std::wstring                m_lang;
    std::set<std::string>       m_keywords;
    int                         m_activeLexer;
    LexerData                   m_lexerData;
    std::unique_ptr<char[]>     m_textbuffer;
    int                         m_textbuflen;
    sptr_t                      m_lastcheckedpos;
};

class CCmdSpellcheckLang : public ICommand
{
public:
    CCmdSpellcheckLang(void * obj);
    ~CCmdSpellcheckLang() = default;

    bool Execute() override { return false; }
    UINT GetCmdId() override { return cmdSpellCheckLang; }
    bool IsItemsSourceCommand() override { return true; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;

    HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties) override;

private:

};

class CCmdSpellcheckCorrect : public ICommand
{
public:
    CCmdSpellcheckCorrect(void * obj);
    ~CCmdSpellcheckCorrect() = default;

    bool Execute() override { return false; }
    UINT GetCmdId() override { return cmdSpellCheckCorrect; }
    bool IsItemsSourceCommand() override { return true; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;

    HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties) override;

    void ScintillaNotify(SCNotification * pScn) override;

private:
    std::vector<std::wstring>   m_suggestions;
};

class CCmdSpellcheckAll : public ICommand
{
public:

    CCmdSpellcheckAll(void * obj);

    ~CCmdSpellcheckAll(void) = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdSpellCheckAll; }
    void AfterInit() override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override;
};

class CCmdSpellcheckUpper : public ICommand
{
public:

    CCmdSpellcheckUpper(void * obj);

    ~CCmdSpellcheckUpper(void) = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdSpellCheckUpper; }
    void AfterInit() override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override;
};
