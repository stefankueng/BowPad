// This file is part of BowPad.
//
// Copyright (C) 2013 - Stefan Kueng
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
#include "BaseDialog.h"
#include "DlgResizer.h"

class CFindReplaceDlg : public CDialog,  public ICommand
{
public:
    CFindReplaceDlg(void * obj);
    ~CFindReplaceDlg(void);

protected:
    LRESULT CALLBACK        DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT                 DoCommand(int id, int msg);

    void                    SetInfoText(UINT resid);

    virtual bool Execute() { return true; }
    virtual UINT GetCmdId() { return 0; }

private:
    CDlgResizer             m_resizer;
    std::list<std::wstring> m_searchStrings;
    std::list<std::wstring> m_replaceStrings;
};


class CCmdFindReplace : public ICommand
{
public:

    CCmdFindReplace(void * obj)
        : ICommand(obj)
        , m_pFindReplaceDlg(nullptr)
    {
    }

    ~CCmdFindReplace(void)
    {
        delete m_pFindReplaceDlg;
    }

    virtual bool Execute();

    virtual UINT GetCmdId() { return cmdFindReplace; }

    virtual HRESULT IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue )
    {
        if (UI_PKEY_BooleanValue == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETWRAPMODE) > 0, ppropvarNewValue);
        }
        return E_NOTIMPL;
    }

    virtual void ScintillaNotify( Scintilla::SCNotification * pScn );

private:
    CFindReplaceDlg *           m_pFindReplaceDlg;
};

class CCmdFindNext : public ICommand
{
public:

    CCmdFindNext(void * obj)
        : ICommand(obj)
    {
    }

    ~CCmdFindNext(void)
    {
    }

    virtual bool Execute();

    virtual UINT GetCmdId() { return cmdFindNext; }
};

class CCmdFindPrev : public ICommand
{
public:

    CCmdFindPrev(void * obj)
        : ICommand(obj)
    {
    }

    ~CCmdFindPrev(void)
    {
    }

    virtual bool Execute();

    virtual UINT GetCmdId() { return cmdFindPrev; }
};

class CCmdFindSelectedNext : public ICommand
{
public:

    CCmdFindSelectedNext(void * obj)
        : ICommand(obj)
    {
    }

    ~CCmdFindSelectedNext(void)
    {
    }

    virtual bool Execute();

    virtual UINT GetCmdId() { return cmdFindSelectedNext; }
};

class CCmdFindSelectedPrev : public ICommand
{
public:

    CCmdFindSelectedPrev(void * obj)
        : ICommand(obj)
    {
    }

    ~CCmdFindSelectedPrev(void)
    {
    }

    virtual bool Execute();

    virtual UINT GetCmdId() { return cmdFindSelectedPrev; }
};
