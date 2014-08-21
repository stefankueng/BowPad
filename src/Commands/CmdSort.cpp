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

#include "stdafx.h"
#include "CmdSort.h"
#include "StringUtils.h"
#include "Resource.h"
#include "BaseDialog.h"

#include <algorithm>

extern HINSTANCE hRes;

// OVERVIEW:
//
// Sort Current Selection with Ascending | Descending
// and case Sensitive | Insensitive and  Numeric options.
// Handles regular or rectangular selections.
// Does not handle multiple selections but support is possible.
// May wish to handle multiple selections and custom delimiters as
// future improvements.
//
// FIXME: Test/Support more intersting character encodings,
// but basic functionality options and structure is inplace
// for simple encodings. Use of string/wstring needs addressing.
//
// May wan't to consider user's language as set in BP etc
// and/or provide further UI options.
//

// No need to expose these types. Internal only.
namespace
{
enum class SortAction { None, Sort };
enum class SortOrder { Ascending, Descending };
enum class SortCase { Sensitive, Insensitive };
enum class SortDigits { AsDigits, AsNumbers };

struct SortOptions
{
    SortOptions(
        SortAction sortAction,
        SortOrder sortOrder,
        SortCase sortCase,
        SortDigits sortDigits
        ) :
        sortAction(sortAction),
        sortOrder(sortOrder),
        sortCase(sortCase),
        sortDigits(sortDigits)
    {
    }
    // Default constructor sets sensible default options
    // for sorting, but defaults to not actually sorting.
    // Set sort action explicitly to Sort invoke any sorting.
    SortOptions() :
        sortAction(SortAction::None),
        sortOrder(SortOrder::Ascending),
        sortCase(SortCase::Sensitive),
        sortDigits(SortDigits::AsDigits)
    {
    }
    SortAction sortAction;
    SortOrder sortOrder;
    SortCase sortCase;
    SortDigits sortDigits;
};

class CSortDlg : public CDialog // No need to be ICommand connected as yet.
{
public:
    CSortDlg();
    ~CSortDlg(void);

protected:
    LRESULT CALLBACK        DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT                 DoCommand(int id, int msg);

public:
    bool sortDescending;
    bool sortCaseInsensitive;
    bool sortDigitsAsNumbers;
};

}; // anonymous namespace


CSortDlg::CSortDlg() :
    sortDescending(false),
    sortCaseInsensitive(false),
    sortDigitsAsNumbers(false)
{
}

CSortDlg::~CSortDlg(void)
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
            auto sortOrderList = GetDlgItem(*this,IDC_SORTDLG_ORDER);
            ResString ascending(hRes, IDS_ASCENDING );
            ResString descending(hRes, IDS_DESCENDING );
            int defSel = ListBox_AddString(sortOrderList, ascending);
            ListBox_AddString(sortOrderList, descending);
            ListBox_SetCurSel(sortOrderList, defSel); 
        }
        return FALSE;
    case WM_COMMAND:
        return DoCommand(LOWORD(wParam), HIWORD(wParam));
    default:
        return FALSE;
    }
    return FALSE;
}

LRESULT CSortDlg::DoCommand(int id, int /*msg*/)
{
    switch (id)
    {
    case IDOK:
        this->sortDescending = ListBox_GetCurSel(GetDlgItem(*this, IDC_SORTDLG_ORDER)) == 1;
        this->sortCaseInsensitive = IsDlgButtonChecked(*this, IDC_SORTDLG_CASE_CHECK) != FALSE;
        this->sortDigitsAsNumbers = IsDlgButtonChecked(*this, IDC_SORTDLG_NUM_CHECK) != FALSE;
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

    // We only handle single selections or retangular ones.
    bool isRectangular = ScintillaCall(SCI_SELECTIONISRECTANGLE) != 0;
    // Don't handle multiple selections unless it's rectangular.
    if (ScintillaCall(SCI_GETSELECTIONS) > 1 && ! isRectangular)
        return false;

    // Determine the line endings used/to use.
    std::string eol;
    switch (ScintillaCall(SCI_GETEOLMODE))
    {
    case SC_EOL_CRLF:
        eol = "\r\n";
        break;
    case SC_EOL_LF:
        eol = "\n";
        break;
    case SC_EOL_CR:
        eol ="\r";
        break;
    default:
        eol = "\r\n";
        APPVERIFY(false); // Shouldn't happen.
    }

    long selStart         = (long)ScintillaCall(SCI_GETSELECTIONSTART);
    long selEnd           = (long)ScintillaCall(SCI_GETSELECTIONEND);
    long selEndOriginal   = selEnd;
    long lineStart        = (long)ScintillaCall(SCI_LINEFROMPOSITION, selStart);
    long lineEnd          = (long)ScintillaCall(SCI_LINEFROMPOSITION, selEnd);

    long lineCount = lineEnd - lineStart;
    if (lineCount <= 1)
        return true;
    
    if (isRectangular) // Find and sort lines in retangular selections.
    {
        std::vector<std::string> lines;
        std::vector<long> positions;
        for (long lineNum = lineStart; lineNum <= lineEnd; ++lineNum)
        {
            long lineSelStart = (long)ScintillaCall(SCI_GETLINESELSTARTPOSITION, lineNum);
            long lineSelEnd = (long)ScintillaCall(SCI_GETLINESELENDPOSITION, lineNum);
            std::string line = GetTextRange(lineSelStart, lineSelEnd);
            lines.push_back(std::move(line));
            positions.push_back(lineSelStart);
        }

        // The all important sort bit. Doesn't effect the document yet.
        Sort(lines);

        // Use may want to undo this so make it possible
        // We're changing the document from here on in.

        ScintillaCall(SCI_BEGINUNDOACTION);
        size_t ln = 0;
        for (long line = lineStart; line <= lineEnd; ++line, ++ln)
        {
            const auto& lineText = lines[ln];
            ScintillaCall(SCI_DELETERANGE, positions[ln], lineText.length());
            ScintillaCall(SCI_INSERTTEXT, positions[ln], sptr_t(lineText.c_str()));
        }
        ScintillaCall(SCI_ENDUNDOACTION);
    }    
    else // Find an sort lines for regular (non retangular selections).
    {
        // Avoid any trailing blank line in the users selection.
        if (ScintillaCall(SCI_POSITIONFROMLINE, lineEnd) == (sptr_t)selEnd)
        {
            --lineEnd;
            selEnd = (long)ScintillaCall(SCI_GETLINEENDPOSITION, lineEnd);
        }
        // Use may want to undo this so make it possible
        // We're changing the document from here on in.

        ScintillaCall(SCI_BEGINUNDOACTION);
        ScintillaCall(SCI_SETSEL, selStart, selEnd);
        std::string selText = GetSelectedText();
        // Whatever the line breaks type is current, standardsize on "\n".
        // When re-inserting the lines later we'll use whatever line
        // break type is approprirate for the document.
        // REVIEW: check this is reasonable behavior.
        SearchReplace(selText, "\r\n", "\n");
        SearchReplace(selText, "\r", "\n");
        std::vector<std::string> lines;
        stringtok(lines, selText, false, "\n");

        // The all important sort bit. Doesn't effect the document yet.
        Sort(lines);

        selText.clear();
        for (size_t lineNum = 0; lineNum < lines.size(); ++lineNum)
        {
            selText += lines[lineNum];
            if (lineNum < lines.size() - 1) // No new line on the last line.
                selText += eol;
        }
        ScintillaCall(SCI_REPLACESEL, 0, sptr_t(selText.c_str()));
        // Put the selection back where it was so the user
        // can see still see what they sorted and possibly do more with it.
        ScintillaCall(SCI_SETSEL, selStart, selEndOriginal);
        ScintillaCall(SCI_ENDUNDOACTION);
    }

    return true;
}

SortOptions CCmdSort::GetSortOptions() const
{
    CSortDlg sortDlg;

    auto result = sortDlg.DoModal(hRes, IDD_SORTDLG, GetHwnd() );

    if (result == IDOK)
        return SortOptions(SortAction::Sort,
            sortDlg.sortDescending ? SortOrder::Descending : SortOrder::Ascending,
            sortDlg.sortCaseInsensitive ? SortCase::Insensitive : SortCase::Sensitive,
            sortDlg.sortDigitsAsNumbers ? SortDigits::AsNumbers : SortDigits::AsDigits);

    return SortOptions(); // Means don't sort.
}

void CCmdSort::Sort(std::vector<std::string>& lines) const
{
    SortOptions so = GetSortOptions();
    if (so.sortAction == SortAction::None)
        return;

    // TOOD: REVIEW compare options used here. See OVERVIEW.

    DWORD cmpFlags = 0;
    if (so.sortCase == SortCase::Insensitive)
        cmpFlags |= LINGUISTIC_IGNORECASE;
    if (so.sortDigits == SortDigits::AsNumbers)
        cmpFlags |= SORT_DIGITSASNUMBERS;

    if (so.sortOrder == SortOrder::Ascending)
    {
        std::sort(std::begin(lines), std::end(lines),
            [&](const std::string& lhs, const std::string& rhs)->bool
            {
                // TODO! This conversion is neither effecient nor
                // accurate. See OVERVIEW.
                auto wlhs = std::wstring(lhs.begin(), lhs.end());
                auto wrhs = std::wstring(rhs.begin(), rhs.end());
                auto res = CompareStringEx(nullptr, cmpFlags,
                    wlhs.data(), (int) wlhs.length(),
                    wrhs.data(), (int) wrhs.length(),
                    nullptr, nullptr, 0);
                return res == CSTR_LESS_THAN;
            });
    }
    else
    {
        std::sort(std::begin(lines), std::end(lines),
            [&](const std::string& lhs, const std::string& rhs)->bool
            {
                auto wlhs = std::wstring(lhs.begin(), lhs.end());
                auto wrhs = std::wstring(rhs.begin(), rhs.end());
                auto res = CompareStringEx(nullptr, cmpFlags,
                    wlhs.data(), (int) wlhs.length(),
                    wrhs.data(), (int) wrhs.length(),
                    nullptr, nullptr, 0);
                return res == CSTR_GREATER_THAN;
            });
    }
}
