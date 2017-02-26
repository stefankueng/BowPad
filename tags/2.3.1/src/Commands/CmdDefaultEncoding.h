// This file is part of BowPad.
//
// Copyright (C) 2014, 2016-2017 - Stefan Kueng
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

class CDefaultEncodingDlg : public CDialog
{
public:
    CDefaultEncodingDlg() = default;
    ~CDefaultEncodingDlg() = default;

protected:
    LRESULT CALLBACK        DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT                 DoCommand(int id, int msg);
};


class CCmdDefaultEncoding : public ICommand
{
public:

    CCmdDefaultEncoding(void * obj)
        : ICommand(obj)
    {
    }

    ~CCmdDefaultEncoding(void) = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdNewDefault; }
};

