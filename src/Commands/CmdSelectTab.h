// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2021 - Stefan Kueng
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
#include <deque>

class TabListDialog : public CDialog
{
public:
    TabListDialog(HWND hParent, std::function<void(DocID)>&& execFunc);
    virtual ~TabListDialog();

    SIZE SetTabList(std::deque<std::tuple<std::wstring, DocID>>&& list);

protected:
    LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

private:
    HWND                                        m_hParent;
    std::deque<std::tuple<std::wstring, DocID>> m_tabList;
    long                                        m_textHeight;
    HFONT                                       m_hFont;
    size_t                                      m_currentItem;
    std::function<void(DocID)>                  m_execFunction;
};

class CCmdSelectTab : public ICommand
{
public:
    CCmdSelectTab(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdSelectTab() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdSelectTab; }

    void TabNotify(TBHDR* ptbHdr) override;

    std::deque<DocID>              m_docIds;
    std::unique_ptr<TabListDialog> m_dlg;
};
