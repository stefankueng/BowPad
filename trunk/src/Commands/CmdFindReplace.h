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
#include "BaseDialog.h"
#include "DlgResizer.h"
#include "ScintillaWnd.h"

class CSearchResult
{
public:
    CSearchResult()
        : docID(-1)
        , pos(0)
        , line(0)
        , posInLineStart(0)
        , posInLineEnd(0)
    {}
    ~CSearchResult() {}

    int             docID;
    std::wstring    lineText;
    size_t          pos;
    size_t          line;
    size_t          posInLineStart;
    size_t          posInLineEnd;
};


class CFindReplaceDlg : public CDialog,  public ICommand
{
public:
    CFindReplaceDlg(void * obj);
    ~CFindReplaceDlg(void);

protected:
    LRESULT CALLBACK        DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT                 DoCommand(int id, int msg);

    void                    SetInfoText(UINT resid);
    void                    DoSearch();
    void                    DoSearchAll(int id);
    void                    DoReplace(int id);
    void                    SearchDocument(int docID, const CDocument& doc, const std::string& searchfor, int searchflags);
    int                     ReplaceDocument(CDocument& doc, const std::string& sFindString, const std::string& sReplaceString, int searchflags);
    void                    CheckRegex();

    void                    ShowResults(bool bShow);
    void                    InitResultList();
    LRESULT                 DoListNotify(LPNMITEMACTIVATE lpNMItemActivate);
    LRESULT                 DrawListItemWithMatches(NMLVCUSTOMDRAW * pLVCD);
    RECT                    DrawListColumnBackground(NMLVCUSTOMDRAW * pLVCD);
    std::string             UnEscape(const std::string& str);
    bool                    ReadBase(const char * str, int * value, int base, int size);

    virtual bool Execute() override { return true; }
    virtual UINT GetCmdId() override { return 0; }
private:
    CDlgResizer                 m_resizer;
    std::list<std::wstring>     m_searchStrings;
    std::list<std::wstring>     m_replaceStrings;
    bool                        m_freeresize;
    CScintillaWnd               m_searchWnd;
    std::deque<CSearchResult>   m_searchResults;
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

    virtual bool Execute() override;

    virtual UINT GetCmdId() override { return cmdFindReplace; }

    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override
    {
        if (UI_PKEY_BooleanValue == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETWRAPMODE) > 0, ppropvarNewValue);
        }
        return E_NOTIMPL;
    }

    virtual void ScintillaNotify(Scintilla::SCNotification * pScn) override;

    virtual void TabNotify(TBHDR * ptbhdr) override;

private:
    CFindReplaceDlg *           m_pFindReplaceDlg;
    std::string                 m_lastSelText;
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

    virtual bool Execute() override;

    virtual UINT GetCmdId() override { return cmdFindNext; }
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

    virtual bool Execute() override;

    virtual UINT GetCmdId() override { return cmdFindPrev; }
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

    virtual bool Execute() override;

    virtual UINT GetCmdId() override { return cmdFindSelectedNext; }
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

    virtual bool Execute() override;

    virtual UINT GetCmdId() override { return cmdFindSelectedPrev; }
};
