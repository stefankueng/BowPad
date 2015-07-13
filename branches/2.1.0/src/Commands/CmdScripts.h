// This file is part of BowPad.
//
// Copyright (C) 2014 - Stefan Kueng
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

    virtual bool Execute() override;

    virtual UINT GetCmdId() override;

    virtual void ScintillaNotify(Scintilla::SCNotification * pScn) override;

    virtual void TabNotify(TBHDR * ptbhdr) override;

    virtual void OnClose() override;

    virtual void AfterInit() override;

    virtual void OnDocumentClose(int index) override;

    virtual void OnDocumentOpen(int index) override;

    virtual void OnDocumentSave(int index, bool bSaveAs) override;

    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;

    virtual void OnTimer(UINT id) override;

    virtual void OnThemeChanged(bool bDark) override;

    int m_version;

private:
    BasicScriptObject * m_appObject;
    BasicScriptHost *   m_host;
    UINT                m_cmdID;
};

