// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017 - Stefan Kueng
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


class CCmdSelectTab : public ICommand
{
public:

    CCmdSelectTab(void * obj) : ICommand(obj)
    {
    }

    ~CCmdSelectTab() = default;

    bool Execute() override
    {
        // since this is a 'dummy' command, only executed via keyboard shortcuts
        // Ctrl+1 ... Ctrl+9
        // we have to find out here which key was pressed and activate the
        // corresponding tab
        auto curTab = GetActiveTabIndex();
        for (int key = 1; key < 9; ++key)
        {
            if ((GetKeyState(key+'0')&0x8000) || (GetKeyState(key + VK_NUMPAD0)&0x8000))
            {
                if (GetTabCount() >= key)
                {
                    if (key - 1 != curTab)
                        TabActivateAt(key - 1);
                    return true;
                }
            }
        }

        if ((GetKeyState('9')&0x8000) || (GetKeyState(VK_NUMPAD9)&0x8000))
        {
            auto lastTab = GetTabCount() - 1;
            if (curTab != lastTab)
                TabActivateAt(lastTab);
            return true;
        }
        if (GetKeyState(VK_TAB)&0x8000)
        {
            int tab = GetActiveTabIndex();
            if (GetKeyState(VK_SHIFT)&0x8000)
            {
                // previous tab
                if (tab == 0)
                    tab = GetTabCount() - 1;
                else
                    --tab;
            }
            else
            {
                // next tab
                if (tab == GetTabCount()-1)
                    tab = 0;
                else
                    ++tab;
            }
            if (tab != curTab)
                TabActivateAt(tab);
            return true;
        }

        return true;
    }

    UINT GetCmdId() override { return cmdSelectTab; }
};
