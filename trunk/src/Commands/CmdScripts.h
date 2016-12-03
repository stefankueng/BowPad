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

#pragma once
#include "ICommand.h"
#include "BowPadUI.h"

class BasicScriptObject;
class BasicScriptHost;

class CCmdScript : public ICommand
{
public:
    CCmdScript(void * obj);
    virtual ~CCmdScript();

    bool Create(const std::wstring& path);
    void SetCmdId(UINT cmdId) { m_cmdID = cmdId; }

    bool Execute() override;

    UINT GetCmdId() override;

    void ScintillaNotify(SCNotification * pScn) override;

    void TabNotify(TBHDR * ptbhdr) override;

    void OnClose() override;

    void AfterInit() override;

    void OnDocumentClose(DocID id) override;

    void OnDocumentOpen(DocID id) override;

    void OnDocumentSave(DocID id, bool bSaveAs) override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;

    void OnTimer(UINT id) override;

    void OnThemeChanged(bool bDark) override;

    void OnLexerChanged(int lexer) override;

    int m_version;

private:
    BasicScriptObject * m_appObject;
    BasicScriptHost *   m_host;
    UINT                m_cmdID;
};

