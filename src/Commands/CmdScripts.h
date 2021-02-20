// This file is part of BowPad.
//
// Copyright (C) 2014-2016, 2018, 2020-2021 - Stefan Kueng
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

class BasicScriptObject;
class BasicScriptHost;

class CCmdScript : public ICommand
{
public:
    CCmdScript(void* obj);
    virtual ~CCmdScript();

    bool Create(const std::wstring& path);
    void SetCmdId(UINT cmdId);
    void SetDescription(const std::wstring& desc) { m_description = desc; }

    bool        Execute() override;
    bool        IsEnabled() const;
    bool        IsChecked() const;
    IUIImagePtr getIcon() const;

    UINT GetCmdId() override;

    void ScintillaNotify(SCNotification* pScn) override;

    void TabNotify(TBHDR* ptbHdr) override;

    void OnClose() override;

    void BeforeLoad() override;

    void AfterInit() override;

    void OnDocumentClose(DocID id) override;

    void OnDocumentOpen(DocID id) override;

    void OnDocumentSave(DocID id, bool bSaveAs) override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue) override;

    void OnTimer(UINT id) override;

    void OnThemeChanged(bool bDark) override;

    void OnLangChanged() override;

    int m_version;

private:
    BasicScriptObject* m_appObject;
    BasicScriptHost*   m_host;
    UINT               m_cmdID;
    IUIImagePtr        m_image;
    std::wstring       m_name;
    std::wstring       m_description;
};
