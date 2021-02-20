// This file is part of BowPad.
//
// Copyright (C) 2020-2021 - Stefan Kueng
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
#include "DlgResizer.h"
#include "ScintillaWnd.h"
#include "BPBaseDialog.h"

#include <mutex>

void regexCaptureFinish();

class CRegexCaptureDlg : public CBPBaseDialog
    , public ICommand
{
public:
    CRegexCaptureDlg(void* obj);

    void Show();

protected: // override
    bool Execute() override { return true; }
    UINT GetCmdId() override { return 0; }
    //void                    OnClose() override;
    LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

protected:
    enum class AlertMode
    {
        None,
        Flash
    };

    LRESULT DoCommand(int id, int msg);
    void    DoInitDialog(HWND hwndDlg);
    void    SetTheme(bool bDark);
    void    DoCapture();
    void    SetInfoText(UINT resid, AlertMode alertMode);

private:
    CDlgResizer   m_resizer;
    CScintillaWnd m_captureWnd;
    int           m_themeCallbackId;
    int           m_maxRegexStrings;
    int           m_maxCaptureStrings;
};

class CCmdRegexCapture : public ICommand
{
public:
    CCmdRegexCapture(void* obj);
    ~CCmdRegexCapture() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdRegexCapture; }

    HRESULT IUICommandHandlerUpdateProperty(
        REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/,
        PROPVARIANT*   pPropVarNewValue) override
    {
        if (UI_PKEY_BooleanValue == key)
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue,
                                             ScintillaCall(SCI_GETWRAPMODE) > 0, pPropVarNewValue);
        return E_NOTIMPL;
    }
};
