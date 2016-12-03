// This file is part of BowPad.
//
// Copyright (C) 2014, 2016 - Stefan Kueng
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

#include "stdafx.h"
#include "CmdSummary.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "Resource.h"
#include "BaseDialog.h"
#include "DlgResizer.h"

#include <algorithm>

extern HINSTANCE hRes;


class CSummaryDlg : public CDialog
{
public:
    CSummaryDlg();
    ~CSummaryDlg();

    std::wstring            m_sSummary;

protected:
    LRESULT CALLBACK        DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT                 DoCommand(int id, int msg);
    CDlgResizer             m_resizer;
};


CSummaryDlg::CSummaryDlg()
{}

CSummaryDlg::~CSummaryDlg()
{}

LRESULT CSummaryDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_BOWPAD);
            SetDlgItemText(*this, IDC_SUMMARY, m_sSummary.c_str());

            m_resizer.Init(hwndDlg);
            m_resizer.AddControl(hwndDlg, IDC_SUMMARY, RESIZER_TOPLEFTBOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDOK, RESIZER_BOTTOMRIGHT);
            m_resizer.AdjustMinMaxSize();
            return FALSE;
        }
        case WM_SIZE:
            m_resizer.DoResize(LOWORD(lParam), HIWORD(lParam));
            break;
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO * mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = 200;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRectScreen()->bottom;
            return 0;
        }
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam), HIWORD(wParam));
        default:
            return FALSE;
    }
    return FALSE;
}

LRESULT CSummaryDlg::DoCommand(int id, int /*msg*/)
{
    switch (id)
    {
        case IDOK:
        case IDCANCEL:
            EndDialog(*this, id);
            break;
    }
    return 1;
}

bool CCmdSummary::Execute()
{
    if (!HasActiveDocument())
        return false;

    size_t len = ScintillaCall(SCI_GETLENGTH);
    char * str = (char*)ScintillaCall(SCI_GETCHARACTERPOINTER);
    bool inSpaces = true;
    bool inLine = false;
    bool inParagraph = true;
    long numWords = 0;
    long numParagraphs = 0;
    long numEmptyLines = 0;

    while (len)
    {
        --len;
        switch (*str)
        {
            case '\r':
                if (*(str + 1) == '\n')
                    ++str;
                // intentional fall through
            case '\n':
                inSpaces = true;
                if (!inLine)
                {
                    ++numEmptyLines;
                    if (inParagraph)
                        ++numParagraphs;
                    inParagraph = false;
                }
                inLine = false;
                break;
            case ' ':
            case '\t':
                inSpaces = true;
                break;
            // non-word chars - anything that can split up text into words (tokens)
            case ';':
            case '.':
            case ':':
            case ',':
            case '?':
            case '!':
            case '^':
            case '(':
            case ')':
            case '"':
            case '\'':
            case '{':
            case '}':
            case '[':
            case ']':
            case '&':
            case '%':
            case '$':
            case '/':
            case '*':
            case '-':
            case '+':
            case '=':
            case '|':
            case '\\':
                inSpaces = true;
                inLine = true;
                inParagraph = true;
                break;
            default:
                if (inSpaces)
                    ++numWords;
                inSpaces = false;
                inLine = true;
                inParagraph = true;
                break;
        }
        ++str;
    }
    if (!inLine)
    {
        ++numEmptyLines;
        if (inParagraph)
            ++numParagraphs;
    }


    const auto& doc = GetActiveDocument();

    ResString rSummary(hRes, IDS_SUMMARY);
    std::wstring sSummary = CStringUtils::Format(rSummary,
                                                 doc.m_path.c_str(),
                                                 numWords,
                                                 (long)ScintillaCall(SCI_GETLINECOUNT),
                                                 numEmptyLines,
                                                 numParagraphs);

    CSummaryDlg dlg;
    dlg.m_sSummary = sSummary;
    dlg.DoModal(hRes, IDD_SUMMARY, GetHwnd());

    return true;
}

