// This file is part of BowPad.
//
// Copyright (C) 2014, 2016, 2020-2021 - Stefan Kueng
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
#include "CmdSort.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "Resource.h"
#include "BaseDialog.h"
#include "Theme.h"
#include "ResString.h"

#include <algorithm>

extern HINSTANCE g_hRes;

// OVERVIEW:
//
// Sort Current Selection with Ascending | Descending
// and case Sensitive | Insensitive and  Numeric options.
// Handles regular or rectangular selections.
// Does not handle multiple selections but support is possible.
// May wish to handle multiple selections and custom delimiters as
// future improvements.

// No need to expose these types. Internal only.
namespace
{
enum class SortAction
{
    None,
    Sort
};
enum class SortOrder
{
    Ascending,
    Descending
};
enum class SortCase
{
    Sensitive,
    Insensitive
};
enum class SortDigits
{
    AsDigits,
    AsNumbers
};

struct SortOptions
{
    SortOptions(
        SortAction sortAction,
        SortOrder  sortOrder,
        SortCase   sortCase,
        SortDigits sortDigits,
        bool       removeDuplicateLines)
        : sortAction(sortAction)
        , sortOrder(sortOrder)
        , sortCase(sortCase)
        , sortDigits(sortDigits)
        , removeDuplicateLines(removeDuplicateLines)
    {
    }
    // Default constructor sets sensible default options
    // for sorting, but defaults to not actually sorting.
    // Set sort action explicitly to Sort invoke any sorting.
    SortOptions()
        : sortAction(SortAction::None)
        , sortOrder(SortOrder::Ascending)
        , sortCase(SortCase::Sensitive)
        , sortDigits(SortDigits::AsDigits)
        , removeDuplicateLines(false)
    {
    }
    SortAction sortAction;
    SortOrder  sortOrder;
    SortCase   sortCase;
    SortDigits sortDigits;
    bool       removeDuplicateLines;
};

class CSortDlg : public CDialog // No need to be ICommand connected as yet.
{
public:
    CSortDlg();
    virtual ~CSortDlg();

protected:
    LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    LRESULT          DoCommand(int id, int msg);

public:
    bool sortDescending;
    bool sortCaseInsensitive;
    bool sortDigitsAsNumbers;
    bool removeDuplicateLines;
};

}; // anonymous namespace

CSortDlg::CSortDlg()
    : sortDescending(false)
    , sortCaseInsensitive(false)
    , sortDigitsAsNumbers(false)
    , removeDuplicateLines(false)
{
}

CSortDlg::~CSortDlg()
{
}

LRESULT CSortDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_BOWPAD);
            CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
            auto      sortOrderList = GetDlgItem(*this, IDC_SORTDLG_ORDER);
            ResString ascending(g_hRes, IDS_ASCENDING);
            ResString descending(g_hRes, IDS_DESCENDING);
            int       defSel = ListBox_AddString(sortOrderList, ascending);
            ListBox_AddString(sortOrderList, descending);
            ListBox_SetCurSel(sortOrderList, defSel);
        }
            return FALSE;
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam), HIWORD(wParam));
    }
    return FALSE;
}

LRESULT CSortDlg::DoCommand(int id, int /*msg*/)
{
    switch (id)
    {
        case IDOK:
            this->sortDescending       = ListBox_GetCurSel(GetDlgItem(*this, IDC_SORTDLG_ORDER)) == 1;
            this->sortCaseInsensitive  = IsDlgButtonChecked(*this, IDC_SORTDLG_CASE_CHECK) != FALSE;
            this->sortDigitsAsNumbers  = IsDlgButtonChecked(*this, IDC_SORTDLG_NUM_CHECK) != FALSE;
            this->removeDuplicateLines = IsDlgButtonChecked(*this, IDC_REMOVEDUPLICATES) != FALSE;
            EndDialog(*this, id);
            break;
        case IDCANCEL:
            EndDialog(*this, id);
            break;
    }
    return 1;
}

bool CCmdSort::Execute()
{
    // Could provide error messages here but have chosen
    // not to bother.
    if (!HasActiveDocument())
        return false; // Need a document.
    bool bSelEmpty = !!ScintillaCall(SCI_GETSELECTIONEMPTY);
    if (bSelEmpty) // Need a non empty selection.
        return false;

    // We only handle single selections or rectangular ones.
    bool isRectangular = ScintillaCall(SCI_SELECTIONISRECTANGLE) != 0;
    // Don't handle multiple selections unless it's rectangular.
    if (ScintillaCall(SCI_GETSELECTIONS) > 1 && !isRectangular)
        return false;

    // Determine the line endings used/to use.
    std::wstring eol;
    switch (ScintillaCall(SCI_GETEOLMODE))
    {
        case SC_EOL_CRLF:
            eol = L"\r\n";
            break;
        case SC_EOL_LF:
            eol = L"\n";
            break;
        case SC_EOL_CR:
            eol = L"\r";
            break;
        default:
            eol = L"\r\n";
            APPVERIFY(false); // Shouldn't happen.
    }

    auto selStart       = ScintillaCall(SCI_GETSELECTIONSTART);
    auto selEnd         = ScintillaCall(SCI_GETSELECTIONEND);
    auto selEndOriginal = selEnd;
    auto lineStart      = ScintillaCall(SCI_LINEFROMPOSITION, selStart);
    auto lineEnd        = ScintillaCall(SCI_LINEFROMPOSITION, selEnd);

    auto lineCount = lineEnd - lineStart;
    if (lineCount <= 1)
        return true;

    if (isRectangular) // Find and sort lines in rectangular selections.
    {
        std::vector<std::wstring> lines;
        std::vector<sptr_t>       positions;
        for (sptr_t lineNum = lineStart; lineNum <= lineEnd; ++lineNum)
        {
            auto         lineSelStart = ScintillaCall(SCI_GETLINESELSTARTPOSITION, lineNum);
            auto         lineSelEnd   = ScintillaCall(SCI_GETLINESELENDPOSITION, lineNum);
            std::wstring line         = CUnicodeUtils::StdGetUnicode(GetTextRange(lineSelStart, lineSelEnd));
            lines.push_back(std::move(line));
            positions.push_back(lineSelStart);
        }

        // The all important sort bit. Doesn't effect the document yet.
        if (Sort(lines))
        {
            // Use may want to undo this so make it possible
            // We're changing the document from here on in.

            ScintillaCall(SCI_BEGINUNDOACTION);
            size_t ln = 0;
            for (sptr_t line = lineStart; line <= lineEnd; ++line, ++ln)
            {
                const auto& lineText = CUnicodeUtils::StdGetUTF8(lines[ln]);
                ScintillaCall(SCI_DELETERANGE, positions[ln], lineText.length());
                ScintillaCall(SCI_INSERTTEXT, positions[ln], reinterpret_cast<sptr_t>(lineText.c_str()));
            }
            ScintillaCall(SCI_ENDUNDOACTION);
        }
    }
    else // Find an sort lines for regular (non rectangular selections).
    {
        // Avoid any trailing blank line in the users selection.
        if (ScintillaCall(SCI_POSITIONFROMLINE, lineEnd) == selEnd)
        {
            --lineEnd;
            selEnd = ScintillaCall(SCI_GETLINEENDPOSITION, lineEnd);
        }
        std::wstring selText = CUnicodeUtils::StdGetUnicode(GetSelectedText());
        // Whatever the line breaks type is current, standardize on "\n".
        // When re-inserting the lines later we'll use whatever line
        // break type is appropriate for the document.
        SearchReplace(selText, L"\r\n", L"\n");
        SearchReplace(selText, L"\r", L"\n");
        std::vector<std::wstring> lines;
        stringtok(lines, selText, false, L"\n");

        // The all important sort bit. Doesn't effect the document yet.
        if (Sort(lines))
        {
            selText.clear();
            for (size_t lineNum = 0; lineNum < lines.size(); ++lineNum)
            {
                selText += lines[lineNum];
                if (lineNum < lines.size() - 1) // No new line on the last line.
                    selText += eol;
            }

            // Use may want to undo this so make it possible
            // We're changing the document from here on in.

            ScintillaCall(SCI_BEGINUNDOACTION);
            ScintillaCall(SCI_SETSEL, selStart, selEnd);
            std::string sSortedText = CUnicodeUtils::StdGetUTF8(selText);
            ScintillaCall(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(sSortedText.c_str()));
            // Put the selection back where it was so the user
            // can see still see what they sorted and possibly do more with it.
            ScintillaCall(SCI_SETSEL, selStart, selEndOriginal);
            ScintillaCall(SCI_ENDUNDOACTION);
        }
    }

    return true;
}

void CCmdSort::ScintillaNotify(SCNotification* pScn)
{
    if (pScn->nmhdr.code == SCN_UPDATEUI)
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, nullptr);
    }
}

HRESULT CCmdSort::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue)
{
    // Enabled if there's something to go back to.
    if (UI_PKEY_Enabled == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, (ScintillaCall(SCI_GETSELECTIONEMPTY) == 0), pPropVarNewValue);
    }
    return E_NOTIMPL;
}

static SortOptions GetSortOptions(HWND hWnd)
{
    CSortDlg sortDlg;

    auto result = sortDlg.DoModal(g_hRes, IDD_SORTDLG, hWnd);

    if (result == IDOK)
        return SortOptions(SortAction::Sort,
                           sortDlg.sortDescending ? SortOrder::Descending : SortOrder::Ascending,
                           sortDlg.sortCaseInsensitive ? SortCase::Insensitive : SortCase::Sensitive,
                           sortDlg.sortDigitsAsNumbers ? SortDigits::AsNumbers : SortDigits::AsDigits,
                           sortDlg.removeDuplicateLines);

    return SortOptions(); // Means don't sort.
}

bool CCmdSort::Sort(std::vector<std::wstring>& lines) const
{
    SortOptions so = GetSortOptions(GetHwnd());
    if (so.sortAction == SortAction::None)
        return false;

    DWORD cmpFlags = 0;
    if (so.sortCase == SortCase::Insensitive)
        cmpFlags |= LINGUISTIC_IGNORECASE;
    if (so.sortDigits == SortDigits::AsNumbers)
        cmpFlags |= SORT_DIGITSASNUMBERS;

    std::sort(std::begin(lines), std::end(lines),
              [&](const std::wstring& lhs, const std::wstring& rhs) -> bool {
                  auto res = CompareStringEx(nullptr, cmpFlags,
                                             lhs.data(), static_cast<int>(lhs.length()),
                                             rhs.data(), static_cast<int>(rhs.length()),
                                             nullptr, nullptr, 0);
                  return so.sortOrder == SortOrder::Ascending ? res == CSTR_LESS_THAN : res == CSTR_GREATER_THAN;
              });
    if (so.removeDuplicateLines)
        lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
    return true;
}
