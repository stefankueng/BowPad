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
#include "BowPad.h"
#include "ScintillaWnd.h"

#include <string>
#include <vector>


class CCmdFunctions : public ICommand
{
public:

    CCmdFunctions(void * obj)
        : ICommand(obj)
        , m_ScratchScintilla(hRes)
    {
        m_timerID = GetTimerID();
        m_ScratchScintilla.InitScratch(hRes);
    }

    ~CCmdFunctions(void)
    {
    }

    virtual bool Execute() override { return false; }
    virtual UINT GetCmdId() override { return cmdFunctions; }


    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;

    virtual HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties) override;

    virtual void TabNotify(TBHDR * ptbhdr) override;

    virtual void ScintillaNotify(Scintilla::SCNotification * pScn) override;

    virtual void OnTimer(UINT id) override;

    virtual void OnDocumentOpen(int id) override;

    virtual void OnDocumentSave(int index, bool bSaveAs) override;

private:
    void            FindFunctions(int docID, bool bBackground);

    UINT            m_timerID;
    std::set<int>   m_docIDs;
    CScintillaWnd   m_ScratchScintilla;
};

