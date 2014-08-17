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

#include <string>
#include <vector>


struct TabInfo
{
    TabInfo(int docId) : docId(docId)
    {
    }
    int docId;
};

class CCmdTabList : public ICommand
{
public:

    CCmdTabList(void * obj) : ICommand(obj)
    {
    }

    ~CCmdTabList(void)
    {
    }

    bool Execute() override;
    UINT GetCmdId() override { return cmdTabList; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;

    HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties) override;

    void TabNotify(TBHDR * ptbhdr) override;

    void ScintillaNotify(Scintilla::SCNotification * pScn) override;

    void OnDocumentOpen(int tab) override;
    void OnDocumentClose(int tab) override;
    void OnDocumentSave(int tab, bool bSaveAs) override;

private:
    void InvalidateTabList();
    bool IsValidMenuItem(size_t item) const;
    bool IsServiceAvailable() const;
    bool PopulateMenu(const CDocument& doc, IUICollectionPtr& collection);
    bool HandleSelectedMenuItem(size_t selected);
    std::vector<TabInfo> m_menuInfo;
};

