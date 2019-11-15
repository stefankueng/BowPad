// This file is part of BowPad.
//
// Copyright (C) 2013-2019 - Stefan Kueng
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
#include "CmdFindReplace.h"
#include "BowPad.h"
#include "ScintillaWnd.h"
#include "UnicodeUtils.h"
#include "StringUtils.h"
#include "PathUtils.h"
#include "DocumentManager.h"
#include "DirFileEnum.h"
#include "BrowseFolder.h"
#include "LexStyles.h"
#include "OnOutOfScope.h"
#include "ResString.h"

#include <regex>
#include <thread>
#include <algorithm>
#include <utility>
#include <memory>

static std::string  g_findString;
std::string         g_sHighlightString;
static int          g_searchFlags = 0;
int                 g_searchMarkerCount = 0;
static std::string  g_lastSelText;
static int          g_lastSearchFlags = 0;

static std::unique_ptr<CFindReplaceDlg> g_pFindReplaceDlg;

namespace
{
    const int TIMER_INFOSTRING = 100;
    const int TIMER_SUGGESTION = 101;
    const unsigned int SF_SEARCHSUBFOLDERS = 1;
    const unsigned int SF_SEARCHFORFUNCTIONS = 2;
    // Limit the max search results so as not to crash by running out of memory or allowed memory.
    const int MAX_SEARCHRESULTS = 5000;

    // The maximum for these values is set by the user in the config file.
    // These are just the default maximums.
    const int DEFAULT_MAX_SEARCH_STRINGS = 20;
    const int DEFAULT_MAX_REPLACE_STRINGS = 20;
    const int DEFAULT_MAX_SEARCHFOLDER_STRINGS = 20;
    const int DEFAULT_MAX_SEARCHFILE_STRINGS = 20;
    // The batch size and progress interval determine how much data to buffer up and
    // when to flush the buffer - even if it is not full by the time the interval has expired.
    // If the time limit is too high it may appear like nothing is happening
    // for search patterns that take a long time to find results.
    // If the batch size is too high, excessive memory use may occur.
    // If the interval time or batch size is too low, performance may be affected
    // because the message delivery and progress reporting mechanism may
    // become more of a bottle-neck in performance than time taken to find results.
    constexpr auto PROGRESS_UPDATE_INTERVAL = std::chrono::seconds(3);
    const size_t MAX_DATA_BATCH_SIZE = 1000;
    constexpr auto MATCH_COLOR = RGB(0xFF, 0, 0); // Red.

    // A couple of functions here are similar to those in CmdFunctions.cpp.
    // Code sharing is possible but for now the preference is not to do that
    // to keep unrelated modules unconnected.
    
    // Find the name of the function from its definition.
    bool ParseSignature(std::wstring& name, const std::wstring& sig)
    {
        // Look for a ( of perhaps void x::f(whatever)
        auto bracepos = sig.find(L'(');
        if (bracepos != std::wstring::npos)
        {
            auto wpos = sig.find_last_of(L"\t :,.", bracepos - 1, 5);
            size_t spos = (wpos == std::wstring::npos) ? 0 : wpos + 1;

            // Functions returning pointer or reference will feature these symbols
            // before the name. Ignore them. This logic is a bit C language based.
            while (spos < bracepos && (sig[spos] == L'*' || sig[spos] == L'&' || sig[spos] == L'^'))
                ++spos;
            name.assign(sig, spos, bracepos - spos);
            CStringUtils::trim(name);
        }
        else
            name.clear();
        return !name.empty();
    }
 
    void Normalize(CSearchResult& sr)
    {
        std::wstring normalized = sr.lineText;
        bool bLastCharWasSpace = false;
        size_t sLen = 0;
        for (size_t i = 0; i < sr.lineText.size(); ++i)
        {
            switch (sr.lineText[i])
            {
                case ' ':
                {
                    if (!bLastCharWasSpace)
                    {
                        normalized[sLen++] = ' ';
                    }
                    else
                    {
                        if (i < sr.posInLineEnd)
                            --sr.posInLineEnd;
                        if (i < sr.posInLineStart)
                            --sr.posInLineStart;
                    }
                    bLastCharWasSpace = true;
                }
                break;
                // remove carriage return and '{'
                case '\r':
                case '{':
                if (i < sr.posInLineEnd)
                    --sr.posInLineEnd;
                if (i < sr.posInLineStart)
                    --sr.posInLineStart;
                bLastCharWasSpace = false;
                break;
                // replace newlines and tabs with spaces
                case '\n':
                case '\t':
                normalized[sLen++] = ' ';
                bLastCharWasSpace = false;
                break;
                default:
                normalized[sLen++] = sr.lineText[i];
                bLastCharWasSpace = false;
                break;
            }
        }
        sr.lineText.assign(normalized, 0, sLen);
    }

    // Given "a,b" or "a  ,  b"  or "a,b ," or "a,,b" this routine will yield v[0] "a", v[1] "b" for all.
    // In summary: discards leading and trailing spaces and trailing delimiters and 0 length fields.
    void split(std::vector<std::wstring>& v, const std::wstring& s, wchar_t delimiter, bool append = false)
    {
        if (!append)
            v.clear();

        bool delimited = false;
        const size_t sl = s.length();
        for (size_t b = 0, e; b != sl; b = delimited ? e + 1 : e)
        {
            delimited = false;
            for (e = b; e < sl; ++e) // Try to find the delimiter.
            {
                if (s[e] == delimiter)
                {
                    delimited = true;
                    break;
                }
            }
            while (b < e && s[b] == L' ') // Skip leading.
                ++b;
            size_t te = e;
            while (te > b && s[te - 1] == L' ') // Skip trailing.
                --te;
            size_t l = te - b;
            if (l > 0) // Ignore empty fields.
                v.emplace_back(s, b, l);
        }
    }

    void FlashWindow(HWND hwnd)
    {
        FLASHWINFO fi{ sizeof(FLASHWINFO) };
        fi.hwnd = hwnd;
        fi.dwFlags = FLASHW_CAPTION;
        fi.uCount = 5;
        fi.dwTimeout = 40;
        FlashWindowEx(&fi);
    }

    // Will work with vector or deque.
    template<typename T> static inline void move_append(T& dst, T& src)
    {
        if (dst.empty())
            dst = std::move(src);
        else
        {
            dst.insert(dst.end(),
                std::make_move_iterator(src.begin()),
                std::make_move_iterator(src.end()));
            src.clear();
        }
    }

    std::wstring GetHomeFolder()
    {
        std::wstring homeFolder;
        size_t requiredSize;
        wchar_t hd[MAX_PATH + 1];
        if (_wgetenv_s(&requiredSize, hd, L"USERPROFILE") == 0)
            homeFolder = hd;
        return homeFolder;
    }

}; // unnamed namespace


CFindReplaceDlg::CFindReplaceDlg(void* obj)
    : ICommand(obj)
    , m_searchWnd(hRes)
{
}

std::wstring CFindReplaceDlg::GetCurrentDocumentFolder() const
{
    std::wstring currentDocFolder;
    if (this->HasActiveDocument())
    {
        const auto& doc = this->GetActiveDocument();
        if (!doc.m_path.empty())
        {
            currentDocFolder = CPathUtils::GetParentDirectory(doc.m_path);
            if (currentDocFolder.empty())
                currentDocFolder = doc.m_path;
        }
    }
    return currentDocFolder;
}

void CFindReplaceDlg::UpdateMatchCount(bool finished)
{
    ResString rInfo(hRes, IDS_FINDRESULT_COUNT);
    auto sInfo = CStringUtils::Format(rInfo, (int)m_searchResults.size());
    if (!finished) // Indicate more results might come.
        sInfo += L"...";
    SetDlgItemText(*this, IDC_SEARCHINFO, sInfo.c_str());
}

void CFindReplaceDlg::HandleButtonDropDown(const NMBCDROPDOWN* pDropDown)
{
    // Get screen coordinates of the button.
    POINT pt;
    pt.x = pDropDown->rcButton.left;
    pt.y = pDropDown->rcButton.bottom;
    ClientToScreen(pDropDown->hdr.hwndFrom, &pt);
    // Create a menu and add items.
    HMENU hSplitMenu = CreatePopupMenu();
    if (!hSplitMenu)
        return;
    OnOutOfScope(
        DestroyMenu(hSplitMenu);
    );
    if (pDropDown->hdr.hwndFrom == GetDlgItem(*this, IDC_FINDBTN))
    {
        ResString sFindAll(hRes, IDS_FINDALL);
        ResString sFindAllInTabs(hRes, IDS_FINDALLINTABS);
        ResString findAllInDir(hRes, IDS_FINDALLINDIR);
        AppendMenu(hSplitMenu, MF_STRING, IDC_FINDALL, sFindAll);
        AppendMenu(hSplitMenu, MF_STRING, IDC_FINDALLINTABS, sFindAllInTabs);
        AppendMenu(hSplitMenu, MF_STRING, IDC_FINDALLINDIR, findAllInDir);
    }
    else if (pDropDown->hdr.hwndFrom == GetDlgItem(*this, IDC_REPLACEALLBTN))
    {
        size_t selStart = ScintillaCall(SCI_GETSELECTIONSTART);
        size_t selEnd = ScintillaCall(SCI_GETSELECTIONEND);
        size_t linestart = ScintillaCall(SCI_LINEFROMPOSITION, selStart);
        size_t lineend = ScintillaCall(SCI_LINEFROMPOSITION, selEnd);

        int wrapcount = 1;
        if (linestart == lineend)
            wrapcount = (int)ScintillaCall(SCI_WRAPCOUNT, linestart);
        bool bReplaceOnlyInSelection = (linestart != lineend) || ((wrapcount > 1) && (selEnd - selStart > 20));

        ResString sReplaceAll(hRes, IDS_REPLACEALL);
        ResString sReplaceAllInSelection(hRes, IDS_REPLACEALLINSELECTION);
        AppendMenu(hSplitMenu, MF_STRING, IDC_REPLACEALLBTN,
            bReplaceOnlyInSelection ? sReplaceAllInSelection : sReplaceAll);

        ResString sReplaceAllInTabs(hRes, IDS_REPLACEALLINTABS);
        AppendMenu(hSplitMenu, MF_STRING, IDC_REPLACEALLINTABSBTN, sReplaceAllInTabs);
    }
    // Display the menu.
    TrackPopupMenu(hSplitMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, *this, nullptr);
}

// The suggestion mechanism is a *basic* means to *quickly* assist
// completing simple filenames that are just a bit long to type/remember.
// It's useful for simple folder structures that can be searched fast
// but it will not find filenames in deep folder structures that can't
// be searched quickly. For those the user is expected to just type
// as much of the name as they know and use * wildcard for the rest and
// then press enter to let the normal search process find the file exactly.
// It does not support lists or wildcards.
// This routine somewhat mimics what the find files logic does.
// We don't suggest things the user likely does not want as that
// can easily get in the way of suggestions the user may want.
// e.g. The user would likely prefer bowpad.cpp over bowpad.exe.
// We try to find the shortest filename match in the time given
// as the longest match will be found later if the user keeps typing.
// If a longer match was accepted as soon as it was found we'd never
// offer shorter suggestions and they'd appear to be non-existent.

std::wstring CFindReplaceDlg::OfferFileSuggestion(
    const std::wstring& searchFolder, bool searchSubFolders,
    const std::wstring& currentValue) const
{
    std::wstring suggestedFilename;
    // Don't even attempt to hit storage with wildcard or lists.
    if (currentValue.find_first_of(L";*?") != std::wstring::npos)
        return suggestedFilename; // Should be empty.
    
    constexpr auto maxSearchTime = std::chrono::milliseconds(200);
    CDirFileEnum enumerator(searchFolder);
    bool bIsDir = false;
    std::wstring path;
    std::wstring filename;
    bool searchSubFoldersFlag = searchSubFolders;
    auto startTime = std::chrono::steady_clock::now();
    while (enumerator.NextFile(path, &bIsDir, searchSubFoldersFlag))
    {
        // See this functions block comments for details of what's happening here.
        auto ellapsedPeriod = std::chrono::steady_clock::now() - startTime;
        if (ellapsedPeriod > maxSearchTime)
            break;
        if (bIsDir)
        {
            searchSubFoldersFlag = IsExcludedFolder(path) ? false : searchSubFolders;
            continue;
        }
        if (IsExcludedFile(path))
            continue;
        filename = CPathUtils::GetFileName(path);
        if (_wcsnicmp(filename.c_str(), currentValue.c_str(), currentValue.size()) == 0)
        {
            if (suggestedFilename.empty() ||
                filename.length() < suggestedFilename.length())
                suggestedFilename = filename;
        }
    }
    return suggestedFilename;
}

LRESULT CFindReplaceDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SHOWWINDOW:
        m_open = wParam != FALSE;
        break;
    case WM_DESTROY:
        SaveSettings();
        break;
    case WM_INITDIALOG:
        DoInitDialog(hwndDlg);
        break;
    case WM_CTLCOLORSTATIC:
        if (GetDlgCtrlID((HWND)lParam) == IDC_SEARCHINFO)
        {
            SetTextColor((HDC)wParam, RGB(0, 128, 0));
            SetBkMode((HDC)wParam, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }
        break;
    case WM_ACTIVATE:
        if (wParam == WA_INACTIVE)
        {
            SetTransparency(150);
            // This is arguable, but we don't make some seldom used options too "sticky"
            // because any later interaction with other buttons can be unexpected.
            // Only do this for re-activation because with initial activation
            // the values can be set by the app and we don't want to clear them
            // before we have even used them.
            if (m_reactivation)
            {
                SetDlgItemText(*this, IDC_SEARCHFILES, L"");
                Button_SetCheck(GetDlgItem(*this, IDC_FUNCTIONS), BST_UNCHECKED);
                Button_SetCheck(GetDlgItem(*this, IDC_SEARCHSUBFOLDERS), BST_CHECKED);
                Button_SetCheck(GetDlgItem(*this, IDC_MATCHWORD), BST_UNCHECKED);
            }
            m_reactivation = true;
        }
        else
        {
            SetTransparency(255);
        }
        break;
    case WM_ENTERSIZEMOVE:
    {
        RECT resultsRect;
        GetClientRect(*this, &resultsRect);
        m_resultsWidthBefore = resultsRect.right - resultsRect.left;
        break;
    }
    case WM_EXITSIZEMOVE:
    {
        RECT resultsRect;
        GetClientRect(*this, &resultsRect);
        int resultsWidthNow = resultsRect.right - resultsRect.left;
        int difference = resultsWidthNow - m_resultsWidthBefore;

        HWND hListControl = GetDlgItem(*this, IDC_FINDRESULTS);
        int columnWidth = ListView_GetColumnWidth(hListControl, 2); // Line Text column.
        int newColumnWidth = columnWidth + difference;
        ListView_SetColumnWidth(hListControl, 2, newColumnWidth);
        break;
    }
    case WM_SIZE:
    {
        int newWidth = LOWORD(lParam);
        int newHeight = HIWORD(lParam);
        m_resizer.DoResize(newWidth, newHeight);
        break;
    }
    case WM_GETMINMAXINFO:
        if (!m_freeresize)
        {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = m_resizer.GetDlgRectScreen()->right;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRectScreen()->bottom;
            return 0;
        }
        break;
    case WM_COMMAND:
        return DoCommand(LOWORD(wParam), HIWORD(wParam));
    case WM_TIMER:
        if (wParam == TIMER_INFOSTRING)
        {
            KillTimer(*this, TIMER_INFOSTRING);
            Clear(IDC_SEARCHINFO);
        }
        break;
    case WM_THREADRESULTREADY:
        OnSearchResultsReady(wParam != 0);
        break;
    case WM_NOTIFY:
        switch (wParam)
        {
            case IDC_FINDRESULTS:
                return DoListNotify((LPNMITEMACTIVATE)lParam);
            case IDC_FINDBTN:
            case IDC_FINDALLINDIR:
            case IDC_REPLACEALLBTN:
                switch (((LPNMHDR)lParam)->code)
                {
                    case BCN_DROPDOWN:
                    {
                        const NMBCDROPDOWN* pDropDown = (NMBCDROPDOWN*)lParam;
                        HandleButtonDropDown(pDropDown);
                        return TRUE;
                    }
                    break;
                }
                break;
        }
        break;
    }
    return FALSE;
}

void CFindReplaceDlg::InitSizing()
{
    HWND hwndDlg = *this;
    AdjustControlSize(IDC_MATCHWORD);
    AdjustControlSize(IDC_MATCHCASE);
    AdjustControlSize(IDC_MATCHREGEX);
    AdjustControlSize(IDC_FUNCTIONS);
    m_resizer.Init(hwndDlg);
    m_resizer.AddControl(hwndDlg, IDC_SEARCHFORLABEL, RESIZER_TOPLEFT);
    m_resizer.AddControl(hwndDlg, IDC_REPLACEWITHLABEL, RESIZER_TOPLEFT);
    m_resizer.AddControl(hwndDlg, IDC_SEARCHCOMBO, RESIZER_TOPLEFTRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_REPLACECOMBO, RESIZER_TOPLEFTRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_MATCHWORD, RESIZER_TOPLEFT);
    m_resizer.AddControl(hwndDlg, IDC_MATCHCASE, RESIZER_TOPLEFT);
    m_resizer.AddControl(hwndDlg, IDC_MATCHREGEX, RESIZER_TOPLEFT);
    m_resizer.AddControl(hwndDlg, IDC_FUNCTIONS, RESIZER_TOPLEFT);
    m_resizer.AddControl(hwndDlg, IDC_SEARCHSUBFOLDERS, RESIZER_TOPRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_FINDBTN, RESIZER_TOPRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_FINDPREVIOUS, RESIZER_TOPRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_FINDFILES, RESIZER_TOPRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_SEARCHFILES, RESIZER_TOPLEFTRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_SETSEARCHFOLDER, RESIZER_TOPRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_SETSEARCHFOLDERCURRENT, RESIZER_TOPRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_SETSEARCHFOLDERTOPARENT, RESIZER_TOPRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_REPLACEBTN, RESIZER_TOPRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_REPLACEALLBTN, RESIZER_TOPRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_FINDRESULTS, RESIZER_TOPLEFTBOTTOMRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_SEARCHFOLDER, RESIZER_TOPLEFTRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_SEARCHFOLDERFOLLOWTAB, RESIZER_TOPRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_SEARCHINFO, RESIZER_TOPLEFT);
    m_freeresize = true;
    ShowResults(false);
    m_freeresize = false;
    m_resizer.AdjustMinMaxSize();
}

void CFindReplaceDlg::DoInitDialog(HWND hwndDlg)
{
    InitDialog(hwndDlg, IDI_BOWPAD, false);

    // Position the find dialog in the top right corner.
    // Make sure we don't obscure the scroll bar though.
    RECT rcScintilla, rcDlg;
    GetWindowRect(GetScintillaWnd(), &rcScintilla);
    GetWindowRect(hwndDlg, &rcDlg);

    int sbVertWidth = GetSystemMetrics(SM_CXVSCROLL);
    // Make a small gap between the dialog and the SB, or edge if
    // there isn't an sb. The gap needs to be a little bit
    // big to account for the shadow dialogs sometimes get.
    // We don't want the shadow obscuring the sb either.
    LONG adjustX = 15;
    if (sbVertWidth >= 0)
        adjustX += sbVertWidth;
    LONG x = rcScintilla.right - ((rcDlg.right - rcDlg.left) + adjustX);
    // Try (unscientifically) to not get to close to the tab bar either.
    LONG y = rcScintilla.top + 15;

    SetWindowPos(hwndDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);

    ResString findPreviousTip(hRes, IDS_TT_FINDPREVIOUS);
    AddToolTip(IDC_FINDPREVIOUS, findPreviousTip);
    ResString setSearchFolderToCurrentTip(hRes, IDS_TT_SETSEARCHFOLDERCURRENT);
    AddToolTip(IDC_SETSEARCHFOLDERCURRENT, setSearchFolderToCurrentTip);
    ResString setSearchFolderToParentTip(hRes, IDS_TT_SETSEARCHFOLDERTOPARENT);
    AddToolTip(IDC_SETSEARCHFOLDERTOPARENT, setSearchFolderToParentTip);
    ResString setSearchFolderTip(hRes, IDS_TT_SETSEARCHFOLDER);
    AddToolTip(IDC_SETSEARCHFOLDER, setSearchFolderTip);

    InitSizing();
    GetWindowRect(hwndDlg, &rcDlg);
    m_originalSize = { rcDlg.right - rcDlg.left, rcDlg.bottom - rcDlg.top };

    m_searchWnd.Init(hRes, *this);

    LoadSearchStrings();
    LoadReplaceStrings();
    LoadSearchFolderStrings();
    LoadSearchFileStrings();

    // We don't make the search sub folder flag persistent as the app
    // wants to set that itself and having the app persist it's own
    // changes is confusing in the circumstances.
    bool searchSubFolders = true;//CIniSettings::Instance().GetInt64(L"searchreplace", L"searchsubfolders", 1LL) != 0LL;
    Button_SetCheck(GetDlgItem(*this, IDC_SEARCHSUBFOLDERS), searchSubFolders ? BST_CHECKED : BST_UNCHECKED);
    bool followTab = CIniSettings::Instance().GetInt64(L"searchreplace", L"searchfolderfollowtab", 1LL) != 0LL;
    Button_SetCheck(GetDlgItem(*this, IDC_SEARCHFOLDERFOLLOWTAB), followTab ? BST_CHECKED : BST_UNCHECKED);
    if (!followTab)
    {
        auto hSearchFolder = GetDlgItem(*this, IDC_SEARCHFOLDER);
        int count = ComboBox_GetCount(hSearchFolder);
        if (count > 0)
            ComboBox_SetCurSel(hSearchFolder, 0);
    }

    // These routines can be made somewhat generic and go in a base class eventually.
    // But things need to settle down and it become clear what the final requirements are.
    EnableComboBoxDeleteEvents(IDC_SEARCHCOMBO, true);
    EnableComboBoxDeleteEvents(IDC_REPLACECOMBO, true);
    EnableComboBoxDeleteEvents(IDC_SEARCHFOLDER, true);
    EnableComboBoxDeleteEvents(IDC_SEARCHFILES, true);
    EnableListEndTracking(IDC_FINDRESULTS, true);
    COMBOBOXINFO cbInfo{ sizeof(cbInfo) };
    GetComboBoxInfo(GetDlgItem(*this, IDC_SEARCHFILES), &cbInfo);
    SetWindowSubclass(cbInfo.hwndItem, EditSubClassProc, 0, reinterpret_cast<DWORD_PTR>(this));
    GetComboBoxInfo(GetDlgItem(*this, IDC_SEARCHFOLDER), &cbInfo);
    SHAutoComplete(cbInfo.hwndItem, SHACF_FILESYS_DIRS);

    // Note: This dialog is initiated by CCmdFindReplace::Execute
    // and the code expects ActivateDialog to be called next after this routine
    // to continue initialization / or perform re-initialization.
}

void CFindReplaceDlg::FocusOn(int id)
{
    HWND hWnd = GetDlgItem(*this, id);
    assert(hWnd != nullptr);
    SendMessage(*this, WM_NEXTDLGCTL, (WPARAM)hWnd, TRUE);
}

void CFindReplaceDlg::SetDefaultButton(int id, bool savePrevious)
{
    if (savePrevious)
        m_previousDefaultButton = GetDefaultButton();
    SendMessage(*this, DM_SETDEFID, id, 0);
}

void CFindReplaceDlg::RestorePreviousDefaultButton()
{
    if (m_previousDefaultButton != 0)
        SetDefaultButton(m_previousDefaultButton);
}

void CFindReplaceDlg::Clear(int id)
{
    SetDlgItemText(*this, id, L"");
}

int CFindReplaceDlg::GetDefaultButton() const
{
    LRESULT result = SendMessage(*this, DM_GETDEFID, WPARAM(0), LPARAM(0));
    return (HIWORD(result) == DC_HASDEFID) ? LOWORD(result) : 0;
}

void CFindReplaceDlg::SaveSettings()
{
    //bool searchSubFolders = IsDlgButtonChecked(*this, IDC_SEARCHSUBFOLDERS) == BST_CHECKED;
    //CIniSettings::Instance().SetInt64(L"searchreplace", L"searchsubfolders", searchSubFolders ? 1LL : 0LL);
    bool followTab = IsDlgButtonChecked(*this, IDC_SEARCHFOLDERFOLLOWTAB) == BST_CHECKED;
    CIniSettings::Instance().SetInt64(L"searchreplace", L"searchfolderfollowtab", followTab ? 1LL: 0LL);
    SaveSearchStrings();
    SaveReplaceStrings();
    SaveSearchFolderStrings();
    SaveSearchFileStrings();
}

void CFindReplaceDlg::CheckSearchFolder()
{
    std::wstring searchFolder = GetDlgItemText(IDC_SEARCHFOLDER).get();
    std::wstring currentDocFolder;
    if (this->HasActiveDocument())
        currentDocFolder = GetCurrentDocumentFolder();

    if (searchFolder.empty())
        SetDlgItemText(*this, IDC_SEARCHFOLDER, currentDocFolder.c_str());
    else
    {
        bool follow = IsDlgButtonChecked(*this, IDC_SEARCHFOLDERFOLLOWTAB) == BST_CHECKED;
        if (follow)
        {
            if (currentDocFolder != searchFolder && ! currentDocFolder.empty())
                SetDlgItemText(*this, IDC_SEARCHFOLDER, currentDocFolder.c_str());
        }
    }
}


void CFindReplaceDlg::FindText()
{
    this->ShowModeless(hRes, IDD_FINDREPLACEDLG, GetHwnd());
    int selStart = (int)ScintillaCall(SCI_GETSELECTIONSTART);
    int selEnd = (int)ScintillaCall(SCI_GETSELECTIONEND);
    int linestart = (int)ScintillaCall(SCI_LINEFROMPOSITION, selStart);
    int lineend = (int)ScintillaCall(SCI_LINEFROMPOSITION, selEnd);

    std::string sSelText;
    if (linestart == lineend)
        sSelText = GetSelectedText(true);

    if (!sSelText.empty() && (sSelText.find_first_of("\r\n") == std::string::npos))
        SetDlgItemText(*this, IDC_SEARCHCOMBO, CUnicodeUtils::StdGetUnicode(sSelText).c_str());
    else
        Clear(IDC_SEARCHCOMBO);

    CheckSearchFolder();
    CheckSearchOptions();
    FocusOn(IDC_SEARCHCOMBO);
    UpdateWindow(*this);
}

void CFindReplaceDlg::FindFunction(const std::wstring& functionToFind)
{
    this->ShowModeless(hRes, IDD_FINDREPLACEDLG, GetHwnd());
    SetDlgItemText(*this, IDC_SEARCHCOMBO, functionToFind.c_str());
    Button_SetCheck(GetDlgItem(*this, IDC_FUNCTIONS), BST_CHECKED);
    Button_SetCheck(GetDlgItem(*this, IDC_MATCHWORD), BST_CHECKED);
    CheckSearchFolder();
    CheckSearchOptions();
    FocusOn(IDC_SEARCHCOMBO);
    UpdateWindow(*this);
    PostMessage(*this, WM_COMMAND, MAKEWPARAM(IDC_FINDALLINTABS, BN_CLICKED),
        LPARAM(GetDlgItem(*this, IDC_FINDALLINTABS)));
}

void CFindReplaceDlg::FindFile(const std::wstring& fileToFind)
{
    this->ShowModeless(hRes, IDD_FINDREPLACEDLG, GetHwnd());
    Clear(IDC_SEARCHCOMBO);
    SetDlgItemText(*this, IDC_SEARCHFILES, CPathUtils::GetFileName(fileToFind).c_str());
    auto parentDir = CPathUtils::GetParentDirectory(fileToFind);
    if (!parentDir.empty())
        SetDlgItemText(*this, IDC_SEARCHFOLDER, parentDir.c_str());
    CheckSearchFolder();
    CheckSearchOptions();
    FocusOn(IDC_SEARCHFILES);
    UpdateWindow(*this);
}

bool CFindReplaceDlg::EnableListEndTracking(int list_id, bool enable)
{
    auto hList = GetDlgItem(*this, list_id);
    APPVERIFY(hList != nullptr);
    if (!hList)
        return false;
    if (enable)
        return SetWindowSubclass(hList, ListViewSubClassProc, 0,
            reinterpret_cast<DWORD_PTR>(this)) != FALSE;
    return RemoveWindowSubclass(hList, ListViewSubClassProc, 0) != FALSE;
}

bool CFindReplaceDlg::EnableComboBoxDeleteEvents(int combo_id, bool enable)
{
    auto hCombo = GetDlgItem(*this, combo_id);
    APPVERIFY(hCombo != nullptr);
    if (!hCombo)
        return false;
    COMBOBOXINFO comboInfo = { sizeof comboInfo };
    APPVERIFY(SendMessage(hCombo, CB_GETCOMBOBOXINFO, 0, reinterpret_cast<LPARAM>(&comboInfo)) != 0);
    if (enable)
        return SetWindowSubclass(comboInfo.hwndItem, ComboBoxListSubClassProc, 0,
            reinterpret_cast<DWORD_PTR>(this)) != FALSE;
    return RemoveWindowSubclass(comboInfo.hwndItem, ComboBoxListSubClassProc, 0) != FALSE;
}


LRESULT CFindReplaceDlg::DoListNotify(LPNMITEMACTIVATE lpNMItemActivate)
{
    switch (lpNMItemActivate->hdr.code)
    {
        case LVN_GETINFOTIP:
        {
            LPNMLVGETINFOTIP tip = (LPNMLVGETINFOTIP)lpNMItemActivate;
            int itemIndex = (size_t)tip->iItem;
            if (itemIndex < 0 || itemIndex >= (int)m_searchResults.size())
            {
                assert(false);
                return 0;
            }
            const auto& sr = m_searchResults[itemIndex];
            if (sr.docID.IsValid())
            {
                const auto& doc = GetDocumentFromID(sr.docID);
                _snwprintf_s(tip->pszText, tip->cchTextMax, _TRUNCATE, L"%s (#%d)",
                    doc.m_path.c_str(), itemIndex);
            }
            else if (sr.hasPath())
            {
                _snwprintf_s(tip->pszText, tip->cchTextMax, _TRUNCATE, L"%s (#%d)",
                    m_foundPaths[sr.pathIndex].c_str(), itemIndex);
            }
            break;
        }
        case LVN_GETDISPINFO:
            // Note: the listview is Owner Draw which means you draw it,
            // but you still need to tell the list what exactly the text is
            // that you intend to draw or things like clicking the header to
            // re-size won't work.
            return GetListItemDispInfo(reinterpret_cast<NMLVDISPINFO*>(lpNMItemActivate));

        case LVN_ITEMCHANGED:
        {
            bool controlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (controlDown)
                OnListItemChanged(reinterpret_cast<LPNMLISTVIEW>(lpNMItemActivate));
            break;
        }
        case NM_CLICK:
        {
            int item = lpNMItemActivate->iItem;
            if (item > 0)
                m_trackingOn = false;
            break;
        }
        case NM_DBLCLK:
            DoListItemAction(lpNMItemActivate->iItem);
            break;
        case NM_SETFOCUS:
            SetDefaultButton(IDC_FINDRESULTSACTION, true);
            break;
        case NM_KILLFOCUS:
            RestorePreviousDefaultButton();
            break;
        case NM_CUSTOMDRAW:
            return DrawListItem(reinterpret_cast<NMLVCUSTOMDRAW*>(lpNMItemActivate));
            break;
    }
    return 0;
}

LRESULT CFindReplaceDlg::GetListItemDispInfo(NMLVDISPINFO* pDispInfo)
{
    if ((pDispInfo->item.mask & LVIF_TEXT) != 0)
    {
        if (pDispInfo->item.pszText == nullptr)
            return 0;
        pDispInfo->item.pszText[0] = 0;
        int itemIndex = pDispInfo->item.iItem;
        if (itemIndex >= (int)m_searchResults.size())
            return 0;

        std::wstring sTemp;
        const auto& item = m_searchResults[itemIndex];
        switch (pDispInfo->item.iSubItem)
        {
        case 0:     // file
            if (!item.docID.IsValid())
                sTemp = CPathUtils::GetFileName(m_foundPaths[item.pathIndex]);
            else
                sTemp = GetTitleForDocID(item.docID);
            lstrcpyn(pDispInfo->item.pszText, sTemp.c_str(), pDispInfo->item.cchTextMax);
            break;

        case 1:     // line
            sTemp = std::to_wstring(item.line + 1);
            lstrcpyn(pDispInfo->item.pszText, sTemp.c_str(), pDispInfo->item.cchTextMax);
            break;

        case 2:     // line text
            if (m_searchType == IDC_FINDFILES)
            {
                auto parent = CPathUtils::GetParentDirectory(m_foundPaths[item.pathIndex]);
                lstrcpyn(pDispInfo->item.pszText, parent.c_str(), pDispInfo->item.cchTextMax);
            }
            else
                lstrcpyn(pDispInfo->item.pszText, item.lineText.c_str(), pDispInfo->item.cchTextMax);
            break;
        }
    }
    return 0;
}

LRESULT CFindReplaceDlg::DrawListItem(NMLVCUSTOMDRAW* pLVCD)
{
    switch (pLVCD->nmcd.dwDrawStage)
    {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT:
            // Tell Windows to send draw notifications for each subitem.
            return CDRF_NOTIFYSUBITEMDRAW;
        case CDDS_ITEMPREPAINT | CDDS_ITEM | CDDS_SUBITEM:
        {
            switch (pLVCD->iSubItem)
            {
                case 0:     // file
                case 1:     // line
                    return CDRF_DODEFAULT;

                case 2:     // line text
                    return DrawListItemWithMatches(pLVCD);
            }
            break;
        }
    }
    return CDRF_DODEFAULT;
}

LRESULT CFindReplaceDlg::DrawListItemWithMatches(NMLVCUSTOMDRAW* pLVCD)
{
    HWND hListControl = pLVCD->nmcd.hdr.hwndFrom;
    const int itemIndex = (int)pLVCD->nmcd.dwItemSpec;
    assert(itemIndex >= 0 && itemIndex < (int)m_searchResults.size());
    if (/*m_ThreadsRunning ||*/ itemIndex >= (int)m_searchResults.size())
        return CDRF_DODEFAULT;

    const CSearchResult& searchResult = m_searchResults[itemIndex];
    const std::wstring& text = searchResult.lineText;
    size_t matchStart = searchResult.posInLineStart;
    size_t matchEnd   = searchResult.posInLineEnd;

    constexpr auto mainDrawFlags = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS;

    if (matchStart == matchEnd)
        return CDRF_DODEFAULT;

    // Even though we initialize the 'rect' here with nmcd.rc,
    // we must not use it but use the rects from GetItemRect()
    // and GetSubItemRect(). Because on XP, the nmcd.rc has
    // bogus data in it.
    RECT rect = pLVCD->nmcd.rc;
    // find the margin where the text label starts
    RECT labelRC, boundsRC, iconRC;

    ListView_GetItemRect(hListControl, itemIndex, &labelRC, LVIR_LABEL);
    ListView_GetItemRect(hListControl, itemIndex, &iconRC, LVIR_ICON);
    ListView_GetItemRect(hListControl, itemIndex, &boundsRC, LVIR_BOUNDS);

    DrawListColumnBackground(pLVCD);
    int leftmargin = labelRC.left - boundsRC.left;
    if (pLVCD->iSubItem)
        leftmargin -= (iconRC.right - iconRC.left);

    if (pLVCD->iSubItem != 0)
        ListView_GetSubItemRect(hListControl, itemIndex, pLVCD->iSubItem, LVIR_BOUNDS, &rect);

    int borderWidth = 0;
    if (IsAppThemed())
    {
        HTHEME hTheme = OpenThemeData(*this, L"LISTVIEW");
        GetThemeMetric(hTheme, pLVCD->nmcd.hdc, LVP_LISTITEM, LISS_NORMAL, TMT_BORDERSIZE, &borderWidth);
        CloseThemeData(hTheme);
    }
    else
    {
        borderWidth = GetSystemMetrics(SM_CXBORDER);
    }

    if (ListView_GetExtendedListViewStyle(hListControl) & LVS_EX_CHECKBOXES)
    {
        // I'm not very happy about this fixed margin here but I haven't found a way
        // to ask the system what the margin really is.
        // At least it works on XP/Vista/win7 even with increased font sizes
        leftmargin = 4;
    }

    LVITEM item;
    item.iItem = itemIndex;
    item.iSubItem = 0;
    item.mask = LVIF_IMAGE | LVIF_STATE;
    item.stateMask = (UINT)-1;
    ListView_GetItem(hListControl, &item);
    assert(item.iItem == itemIndex);

    // Draw the icon for the first column.
    if (pLVCD->iSubItem == 0)
    {
        rect = boundsRC;
        rect.right = rect.left + ListView_GetColumnWidth(hListControl, 0) - 2 * borderWidth;
        rect.left = iconRC.left;

        if (item.iImage >= 0)
        {
            POINT pt{ rect.left, rect.top };
            HIMAGELIST hImgList = ListView_GetImageList(hListControl, LVSIL_SMALL);
            ImageList_Draw(hImgList, item.iImage, pLVCD->nmcd.hdc, pt.x, pt.y, ILD_TRANSPARENT);
            leftmargin -= iconRC.left;
        }
        else
        {
            int state = 0;
            if ((item.state & LVIS_SELECTED) != 0)
            {
                if (ListView_GetHotItem(hListControl) == itemIndex)
                    state = CBS_CHECKEDHOT;
                else
                    state = CBS_CHECKEDNORMAL;
            }
            else
            {
                if (ListView_GetHotItem(hListControl) == itemIndex)
                    state = CBS_UNCHECKEDHOT;
            }
            if (state && (ListView_GetExtendedListViewStyle(hListControl) & LVS_EX_CHECKBOXES))
            {
                RECT irc = boundsRC;
                irc.left += borderWidth;
                irc.right = iconRC.left;
                HTHEME hTheme = OpenThemeData(*this, L"BUTTON");
                DrawThemeBackground(hTheme, pLVCD->nmcd.hdc, BP_CHECKBOX, state, &irc, nullptr);
                CloseThemeData(hTheme);
            }
        }
    }
    InflateRect(&rect, -(2 * borderWidth), 0);

    // NOTE: Be aware of the comment about this field in the NM_CUSTOMDRAW event.
    COLORREF textColor = pLVCD->clrText;

    SetTextColor(pLVCD->nmcd.hdc, textColor);
    SetBkMode(pLVCD->nmcd.hdc, TRANSPARENT);

    rect.left += leftmargin;
    RECT rc = rect;

    LVCOLUMN Column;
    Column.mask = LVCF_FMT;
    ListView_GetColumn(hListControl, pLVCD->iSubItem, &Column);
    // Is the column left- or right-aligned? (we don't handle centered yet).
    if (Column.fmt & LVCFMT_RIGHT)
    {
        DrawText(pLVCD->nmcd.hdc, text.c_str(), -1, &rc, mainDrawFlags | DT_CALCRECT);
        rect.left = rect.right - (rc.right - rc.left);
        if (!IsAppThemed())
        {
            rect.left += 2 * borderWidth;
            rect.right += 2 * borderWidth;
        }
    }

    rc = rect;

    if (m_resultsType == ResultsType::Filenames || m_resultsType == ResultsType::FirstLines)
    {
        DrawText(pLVCD->nmcd.hdc, text.c_str(), (int)text.size(), &rc, mainDrawFlags);
        return CDRF_SKIPDEFAULT;
    }

    bool invalid = (matchStart >= text.size() || matchEnd > text.size() || matchEnd < matchStart);
    // Functions are a kind of match but we don't know where exactly.
    // Same is true of anything with invalid data.
    if (m_resultsType == ResultsType::Functions || invalid)
    {
        auto oldColor = SetTextColor(pLVCD->nmcd.hdc, MATCH_COLOR);
        DrawText(pLVCD->nmcd.hdc, text.c_str(), (int)text.size(), &rc, mainDrawFlags);
        SetTextColor(pLVCD->nmcd.hdc, oldColor);
        return CDRF_SKIPDEFAULT;
    }

    // Regex search can match over several lines, but here we only draw one line.
    // Draw the text before the match.
    int beforeMatchLen = int(matchStart);
    int matchLen = int(matchEnd - matchStart);
    int afterMatchLen = int(text.size() - matchEnd);

    if (beforeMatchLen > 0) // Draw the line text before the match.
    {
        DrawText(pLVCD->nmcd.hdc, text.c_str(), beforeMatchLen, &rc, mainDrawFlags);
        DrawText(pLVCD->nmcd.hdc, text.c_str(), beforeMatchLen, &rc, mainDrawFlags | DT_CALCRECT);
        rect.left = rc.right;
    }
    rc = rect;
    // DT_CALCRECT does not account for the last character that's not fully drawn but clipped,
    // it only calculates until the last char that's fully drawn. To avoid drawing
    // beyond the rect, only draw matches and after-match text if we have at least 4 pixels
    // left to draw.
    if ((matchLen > 0) && (rect.left + 4 < rect.right)) // Draw the text that is the match.
    {
        auto oldColor = SetTextColor(pLVCD->nmcd.hdc, MATCH_COLOR);
        DrawText(pLVCD->nmcd.hdc, &text[matchStart], matchLen, &rc, mainDrawFlags);
        DrawText(pLVCD->nmcd.hdc, &text[matchStart], matchLen, &rc, mainDrawFlags | DT_CALCRECT);
        rect.left = rc.right;
        SetTextColor(pLVCD->nmcd.hdc, oldColor);
    }
    rc = rect;
    if ((afterMatchLen > 0) && (rect.left + 4 < rect.right)) // Draw the line text after the match.
        DrawText(pLVCD->nmcd.hdc, &text[matchEnd], afterMatchLen, &rc, mainDrawFlags);

    return CDRF_SKIPDEFAULT;
}

RECT CFindReplaceDlg::DrawListColumnBackground(NMLVCUSTOMDRAW* pLVCD)
{
    HWND hListControl = pLVCD->nmcd.hdr.hwndFrom;
    const int itemIndex = (int)pLVCD->nmcd.dwItemSpec;

    // Get the selected state of the item being drawn.
    LVITEM rItem;
    rItem.mask = LVIF_STATE;
    rItem.iItem = itemIndex;
    rItem.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    ListView_GetItem(hListControl, &rItem);

    RECT rect;
    ListView_GetSubItemRect(hListControl, itemIndex, pLVCD->iSubItem, LVIR_BOUNDS, &rect);

    // The rect we get for column 0 always extends over the whole row instead of just
    // the column itself. Since we must not redraw the background for the whole row (other columns
    // might not be asked to redraw), we have to find the right border of the column
    // another way.
    if (pLVCD->iSubItem == 0)
        rect.right = ListView_GetColumnWidth(hListControl, 0);

    // Fill the background.
    if (IsAppThemed())
    {
        auto brush = ::GetSysColorBrush(COLOR_WINDOW);
        if (brush == nullptr)
            return rect;

        ::FillRect(pLVCD->nmcd.hdc, &rect, brush);
        ::DeleteObject(brush);

        HTHEME hTheme = OpenThemeData(*this, L"Explorer::ListView");
        int state = (pLVCD->nmcd.uItemState & CDIS_HOT) ? LISS_HOT : LISS_NORMAL;
        if ((rItem.state & LVIS_SELECTED) != 0)
        {
            if (::GetFocus() == hListControl)
                state = (pLVCD->nmcd.uItemState & CDIS_HOT) ? LISS_HOTSELECTED : LISS_SELECTED;
            else
                state = LISS_SELECTEDNOTFOCUS;
        }
        if (IsThemeBackgroundPartiallyTransparent(hTheme, LVP_LISTDETAIL, state))
            DrawThemeParentBackground(*this, pLVCD->nmcd.hdc, &rect);
        // don't draw the background if state is normal:
        // it would draw a border around the item/subitem, which is *not* how the
        // item should look!
        if (state != LISS_NORMAL)
        {
            DrawThemeBackground(hTheme, pLVCD->nmcd.hdc, LVP_LISTITEM, state, &rect, nullptr);
        }
        CloseThemeData(hTheme);
    }
    else
    {
        HBRUSH brush = nullptr;
        if ((rItem.state & LVIS_SELECTED) != 0)
        {
            if (::GetFocus() == hListControl)
                brush = ::GetSysColorBrush(COLOR_HIGHLIGHT);
            else
                brush = ::GetSysColorBrush(COLOR_BTNFACE);
        }
        else
            brush = ::GetSysColorBrush(COLOR_WINDOW);
        if (brush == nullptr)
            return rect;

        ::FillRect(pLVCD->nmcd.hdc, &rect, brush);
        ::DeleteObject(brush);
    }

    return rect;
}

void CFindReplaceDlg::ShowResults(bool bShow)
{
    RECT windowRect, infoRect;
    GetWindowRect(*this, &windowRect);
    GetWindowRect(GetDlgItem(*this, IDC_SEARCHINFO), &infoRect);
    int height = infoRect.bottom - windowRect.top;
    HWND hListControl = GetDlgItem(*this, IDC_FINDRESULTS);
    if (bShow)
    {
        MoveWindow(*this, windowRect.left, windowRect.top, windowRect.right - windowRect.left, height + 300, TRUE);
        ShowWindow(hListControl, SW_SHOW);
    }
    else
    {
        MoveWindow(*this, windowRect.left, windowRect.top, windowRect.right - windowRect.left, height + 15, TRUE);
        ShowWindow(hListControl, SW_HIDE);
    }
}

void CFindReplaceDlg::DoListItemAction(int itemIndex)
{
    if (itemIndex < 0 || itemIndex >= (int)m_searchResults.size())
    {
        assert(false);
        return;
    }
    const CSearchResult& item = m_searchResults[itemIndex];

    bool controlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    unsigned int openFlags = OpenFlags::AddToMRU;
    if (controlDown)
        openFlags |= OpenFlags::OpenIntoActiveTab;
    std::wstring path;
    if (HasDocumentID(item.docID))
    {
        const auto& doc = GetDocumentFromID(item.docID);
        path = doc.m_path;
    }
    else if (item.hasPath())
    {
        path = m_foundPaths[item.pathIndex];
    }
    if (OpenFile(path.c_str(), openFlags)<0)
        return;
    ScintillaCall(SCI_GOTOLINE, item.line);
    Center((long)item.pos, (long)item.pos);
    if (m_resultsType == ResultsType::MatchedTerms)
    {
        // Set the color indicators for the doc scroll bar
        if (g_lastSelText.empty() || g_lastSelText.compare(g_sHighlightString) || (g_searchFlags != g_lastSearchFlags))
        {
            DocScrollClear(DOCSCROLLTYPE_SEARCHTEXT);
            g_searchMarkerCount = 0;
        }
        OnOutOfScope(DocScrollUpdate());

        std::wstring findText = GetDlgItemText(IDC_SEARCHCOMBO).get();
        UpdateSearchStrings(findText);
        g_findString = CUnicodeUtils::StdGetUTF8(findText);
        g_sHighlightString = g_findString;
        g_lastSelText = g_sHighlightString;

        for (const auto& sRes : m_searchResults)
        {
            if (sRes.docID == item.docID)
            {
                DocScrollAddLineColor(DOCSCROLLTYPE_SEARCHTEXT, sRes.line, RGB(200, 200, 0));
                ++g_searchMarkerCount;
            }
        }
    }
    FocusOn(IDC_FINDRESULTS);
    // Close the dialog if asked to using the shift key or if there was only
    // one item as it's been actioned.
    if (shiftDown || m_searchResults.size() == 1)
    {
        UpdateWindow(*this);
        PostMessage(*this, WM_CLOSE, WPARAM(0), LPARAM(0));
    }
}

LRESULT CFindReplaceDlg::DoCommand(int id, int msg)
{
    switch (id)
    {
    case IDCANCEL:
        if (m_ThreadsRunning)
        {
            // In async mode opening a large document can mean a wait before
            // cancellation is possible. In non async mode the button request
            // won't be acknowledged even. That's not ideal but not a new problem.
            SetInfoText(IDS_PLEASEWAIT, AlertMode::None);
            m_bStop = true;
            return 1;
        }
        DoClose();
        break;
    case IDC_FINDRESULTSACTION:
    {
        auto hFindResults = GetDlgItem(*this, IDC_FINDRESULTS);
        if (GetFocus() == hFindResults)
        {
            int selIndex = ListView_GetNextItem(hFindResults, -1, LVNI_SELECTED);
            if (selIndex >= 0)
                DoListItemAction(selIndex);
        }
        break;
    }
    case IDC_FINDBTN:
        if (msg == BN_CLICKED)
            DoFind();
        break;
    case IDC_FINDPREVIOUS:
        if (msg == BN_CLICKED)
            DoFindPrevious();
        break;
    case IDC_FINDALL:
        if (msg == BN_CLICKED)
            DoSearchAll(id);
        break;
    case IDC_FINDALLINTABS:
        if (msg == BN_CLICKED)
            DoSearchAll(id);
        break;
    case IDC_FINDALLINDIR:
        if (msg == BN_CLICKED)
            DoSearchAll(id);
        break;
    case IDC_FINDFILES:
        if (msg == CBN_SETFOCUS)
            SetDefaultButton(IDC_FINDFILES, true);
        else if (msg == CBN_KILLFOCUS)
            RestorePreviousDefaultButton();
        else if (msg == BN_CLICKED)
            DoSearchAll(id); // Don't show results now as these commands run in another thread.
        break;
    case IDC_REPLACEALLBTN:
    case IDC_REPLACEBTN:
    case IDC_REPLACEALLINTABSBTN:
        if (msg == BN_CLICKED)
        {
            if (m_ThreadsRunning)
                return 1;
            DoReplace(id);
        }
        break;
    case IDC_MATCHREGEX:
        if (msg == BN_CLICKED)
        {
            ResString sInfo(hRes, IDS_REGEXTOOLTIP);
            bool useRegEx = IsDlgButtonChecked(*this, IDC_MATCHREGEX) == BST_CHECKED;
            COMBOBOXINFO cinfo{ sizeof(COMBOBOXINFO) };
            GetComboBoxInfo(GetDlgItem(*this, IDC_REPLACECOMBO), &cinfo);
            LPCWSTR tipText = useRegEx ? sInfo : L"";
            AddToolTip(cinfo.hwndCombo, tipText);
            AddToolTip(cinfo.hwndItem, tipText);
            AddToolTip(cinfo.hwndList, tipText);
            AddToolTip(IDC_REPLACEWITHLABEL, tipText);
            if (IsDlgButtonChecked(*this, IDC_MATCHREGEX) == BST_CHECKED)
                CheckRegex();
        }
        break;
    case IDC_SEARCHCOMBO:
        {
            if (msg == CBN_SETFOCUS)
                SetDefaultButton(IDC_FINDBTN);
            if (msg == CBN_EDITCHANGE || msg == CBN_CLOSEUP)
            {
                if (IsDlgButtonChecked(*this, IDC_MATCHREGEX) == BST_CHECKED)
                    CheckRegex();
                CheckSearchOptions();
            }
        }
        break;
    case IDC_REPLACECOMBO:
        if (msg == CBN_SETFOCUS)
            SetDefaultButton(IDC_REPLACEBTN, true);
        else if (msg == CBN_KILLFOCUS)
            RestorePreviousDefaultButton();
        break;
    case IDC_SETSEARCHFOLDER:
        if (msg == BN_CLICKED)
            LetUserSelectSearchFolder();
        break;
    case IDC_SETSEARCHFOLDERCURRENT:
        if (msg == BN_CLICKED)
        {
            std::wstring searchFolder = GetCurrentDocumentFolder();
            SetSearchFolder(searchFolder);
        }
        break;
    case IDC_SETSEARCHFOLDERTOPARENT:
        if (msg == BN_CLICKED)
        {
            std::wstring searchFolder = GetDlgItemText(IDC_SEARCHFOLDER).get();
            std::wstring parentFolder = CPathUtils::GetParentDirectory(searchFolder);
            // If parent folder is empty assume we reached the root and do nothing.
            if (!parentFolder.empty())
                SetDlgItemText(*this, IDC_SEARCHFOLDER, parentFolder.c_str());
        }
        else if (msg == BN_SETFOCUS)
            SetDefaultButton(IDC_FINDFILES, true);
        else if (msg == BN_KILLFOCUS)
            RestorePreviousDefaultButton();
        break;
    case IDC_FUNCTIONS:
        if (msg == BN_CLICKED)
        {
            // If the user wants to find functions they probably don't want a partial match.
            // But if they do, they can uncheck it.
            if (Button_GetCheck(GetDlgItem(*this, IDC_FUNCTIONS))==BST_CHECKED)
                Button_SetCheck(GetDlgItem(*this, IDC_MATCHWORD), BST_CHECKED);
            CheckSearchOptions();
        }
        break;
    case IDC_SEARCHFILES:
        if (msg == CBN_SETFOCUS)
            SetDefaultButton(IDC_FINDFILES, true);
        else if (msg == CBN_EDITCHANGE || msg == CBN_CLOSEUP)
            CheckSearchOptions();
        break;
    case IDC_SEARCHFOLDER:
        if (msg == CBN_SETFOCUS)
        {
            // Encourage user to set path if it doesn't exist.
            std::wstring currentFolder = GetDlgItemText(IDC_SEARCHFOLDER).get();
            if (currentFolder.empty() || !PathFileExists(currentFolder.c_str()))
                SetDefaultButton(IDC_SETSEARCHFOLDER, true);
            else
                SetDefaultButton(IDC_FINDFILES, true);
        }
        else if (msg == CBN_KILLFOCUS)
            RestorePreviousDefaultButton();
        else if (msg == CBN_EDITCHANGE || msg == CBN_CLOSEUP)
        {
            // Encourage user to set path if it doesn't exist.
            std::wstring currentFolder = GetDlgItemText(IDC_SEARCHFOLDER).get();
            if (currentFolder.empty() || !PathFileExists(currentFolder.c_str()))
                SetDefaultButton(IDC_SETSEARCHFOLDER);
            else
                SetDefaultButton(IDC_FINDFILES);
        }
        break;
    case IDC_SEARCHFOLDERFOLLOWTAB:
        if (msg == BN_CLICKED)
            CheckSearchFolder();
        break;
    }
    return 1;
}

void CFindReplaceDlg::LetUserSelectSearchFolder()
{
    std::wstring currentFolder = GetDlgItemText(IDC_SEARCHFOLDER).get();
    std::wstring selectedFolder;

    CBrowseFolder bf;
    ResString title(hRes, IDS_APP_TITLE);
    bf.SetInfo(title);
    // Don't use a path that doesn't exist as the dialog will refuse to open.
    // Try to offer something other than blank.
    // If a path doesn't exist, try to back up to the first parent that does exist.
    if (currentFolder.empty())
        currentFolder = GetHomeFolder();
    while (!PathFileExists(currentFolder.c_str()))
    {
        auto parent = CPathUtils::GetParentDirectory(currentFolder);
        if (parent.empty())
            break;
        currentFolder = parent;
    }
    if (!PathFileExists(currentFolder.c_str()))
        currentFolder.clear();
    bf.m_style = BIF_USENEWUI;
    CBrowseFolder::retVal result = bf.Show(*this, selectedFolder, currentFolder);
    if (result == CBrowseFolder::OK)
    {
        SetSearchFolder(selectedFolder);
        // If the user is changing the search folder they must
        // must be intending to click IDC_FINDFILES or IDC_FINDALLINDIR.
        // We'll assume it's find files and set the default button to that,
        // because if we set it to IDC_FINDALLINDIR that button is hidden
        // under the IDC_FINDBTN and they can't see it will be the default
        // button so they will have to click it anyway to be sure.
        SetDefaultButton(IDC_FINDFILES);
    }
}

void CFindReplaceDlg::SetInfoText( UINT resid, AlertMode alertMode )
{
    ResString str(hRes, resid);
    SetDlgItemText(*this, IDC_SEARCHINFO, str);
    if (alertMode == AlertMode::Flash)
        FlashWindow(*this);
    SetTimer(*this, TIMER_INFOSTRING, 5000, nullptr);
}

void CFindReplaceDlg::DoFindPrevious()
{
    // Same as find previous but uses the dialog.
    Clear(IDC_SEARCHINFO);
    Sci_TextToFind ttf = { 0 };

    std::wstring findText = GetDlgItemText(IDC_SEARCHCOMBO).get();
    UpdateSearchStrings(findText);
    g_findString = CUnicodeUtils::StdGetUTF8(findText);
    g_sHighlightString = g_findString;
    ttf.lpstrText = g_findString.c_str();
    g_searchFlags = GetScintillaOptions();

    if (g_findString.empty())
    {
        SearchStringNotFound();
        return;
    }

    ttf.chrg.cpMin = (long)ScintillaCall(SCI_GETCURRENTPOS);
    if (ttf.chrg.cpMin > 0)
        ttf.chrg.cpMin--;
    ttf.chrg.cpMax = 0;
    sptr_t findRet = ScintillaCall(SCI_FINDTEXT, g_searchFlags, (sptr_t)&ttf);
    if (findRet == -1)
    {
        // Retry from the end of the doc.
        ttf.chrg.cpMax = ttf.chrg.cpMin + 1;
        ttf.chrg.cpMin = (long)ScintillaCall(SCI_GETLENGTH);
        findRet = ScintillaCall(SCI_FINDTEXT, g_searchFlags, (sptr_t)&ttf);
        // Only report wrap around if we found something in the other direction.
        // If we didn't find anything we'll prefer to report that no match was found.
        if (findRet >= 0)
            SetInfoText(IDS_FINDRETRYWRAP);
    }
    if (findRet >= 0)
        Center(ttf.chrgText.cpMin, ttf.chrgText.cpMax);
    else
        SearchStringNotFound();
}

void CFindReplaceDlg::SearchStringNotFound()
{
    SetInfoText(IDS_FINDNOTFOUND);
    FocusOn(IDC_SEARCHCOMBO);
}

void CFindReplaceDlg::DoFind()
{
    if (!m_ThreadsRunning)
        DoSearch();
}

void CFindReplaceDlg::DoReplace( int id )
{
    Clear(IDC_SEARCHINFO);
  
    // Don't allow replace when finding functions because we don't support singular find as
    // that would be more complicated than we want to deal with right now.
    if (IsDlgButtonChecked(*this, IDC_FUNCTIONS) == BST_CHECKED)
    {
        SetInfoText(IDS_NOFUNCTIONFINDORREPLACE);
        return;
    }

    size_t selStart = ScintillaCall(SCI_GETSELECTIONSTART);
    size_t selEnd = ScintillaCall(SCI_GETSELECTIONEND);
    size_t linestart = ScintillaCall(SCI_LINEFROMPOSITION, selStart);
    size_t lineend = ScintillaCall(SCI_LINEFROMPOSITION, selEnd);
    int wrapcount = 1;
    if (linestart == lineend)
        wrapcount = (int)ScintillaCall(SCI_WRAPCOUNT, linestart);
    bool bReplaceOnlyInSelection = (linestart != lineend
        || (wrapcount > 1 && selEnd - selStart > 20)) && (id == IDC_REPLACEALLBTN);

    if (bReplaceOnlyInSelection)
    {
        ScintillaCall(SCI_SETTARGETSTART, selStart);
        ScintillaCall(SCI_SETTARGETEND, selEnd);
    }
    else
    {
        ScintillaCall(SCI_SETTARGETSTART, id == IDC_REPLACEBTN ? selStart : 0);
        ScintillaCall(SCI_SETTARGETEND, ScintillaCall(SCI_GETLENGTH));
    }

    std::wstring findText = GetDlgItemText(IDC_SEARCHCOMBO).get();
    std::wstring replaceText = GetDlgItemText(IDC_REPLACECOMBO).get();
    UpdateSearchStrings(findText);
    UpdateReplaceStrings(replaceText);

    g_findString = CUnicodeUtils::StdGetUTF8(findText);
    g_sHighlightString = g_findString;
    g_searchFlags = GetScintillaOptions();

    if (g_findString.empty())
    {
        SearchStringNotFound();
        return; // Empty search term, nothing to find.
    }

    std::string sReplaceString = CUnicodeUtils::StdGetUTF8(replaceText);
    if ((g_searchFlags & SCFIND_REGEXP) != 0)
        sReplaceString = UnEscape(sReplaceString);

    int replaceCount = 0;
    if (id == IDC_REPLACEALLINTABSBTN)
    {
        int tabcount = GetTabCount();
        for (int i = 0; i < tabcount; ++i)
        {
            auto docID = GetDocIDFromTabIndex(i);
            auto& doc = GetModDocumentFromID(docID);
            int rcount = ReplaceDocument(doc, g_findString, sReplaceString, g_searchFlags);
            if (rcount)
            {
                replaceCount += rcount;
                UpdateTab(i);
            }
        }
    }
    else
    {
        ScintillaCall(SCI_SETSEARCHFLAGS, g_searchFlags);
        sptr_t findRet = -1;
        ScintillaCall(SCI_BEGINUNDOACTION);
        do
        {
            findRet = ScintillaCall(SCI_SEARCHINTARGET, g_findString.length(), (sptr_t)g_findString.c_str());
            // note: our regex search implementation returns -2 if the regex is invalid
            if (findRet == -1 && id == IDC_REPLACEBTN)
            {
                SetInfoText(IDS_FINDRETRYWRAP);
                // Retry from the start of the doc.
                ScintillaCall(SCI_SETTARGETSTART, 0);
                ScintillaCall(SCI_SETTARGETEND, ScintillaCall(SCI_GETCURRENTPOS));
                findRet = ScintillaCall(SCI_SEARCHINTARGET, g_findString.length(), (sptr_t)g_findString.c_str());
            }
            if (findRet >= 0)
            {
                auto replaceFunc = ((g_searchFlags & SCFIND_REGEXP) != 0)
                    ? SCI_REPLACETARGETRE : SCI_REPLACETARGET;
                ScintillaCall(replaceFunc, sReplaceString.length(), (sptr_t)sReplaceString.c_str());
                ++replaceCount;
                long tstart = (long)ScintillaCall(SCI_GETTARGETSTART);
                long tend = (long)ScintillaCall(SCI_GETTARGETEND);
                if (id == IDC_REPLACEBTN)
                    Center(tstart, tend);
                if ((tend > tstart || sReplaceString.empty()) && (tstart != tend))
                    ScintillaCall(SCI_SETTARGETSTART, tend);
                else
                    ScintillaCall(SCI_SETTARGETSTART, tend + 1);
                if (bReplaceOnlyInSelection)
                    ScintillaCall(SCI_SETTARGETEND, selEnd);
                else
                    ScintillaCall(SCI_SETTARGETEND, ScintillaCall(SCI_GETLENGTH));
            }
        } while (id == IDC_REPLACEALLBTN && findRet >= 0);
        ScintillaCall(SCI_ENDUNDOACTION);
    }
    if (id == IDC_REPLACEALLBTN || id == IDC_REPLACEALLINTABSBTN)
    {
        if (replaceCount > 0)
        {
            ResString rInfo(hRes, IDS_REPLACEDCOUNT);
            auto sInfo = CStringUtils::Format(rInfo, replaceCount);
            SetDlgItemText(*this, IDC_SEARCHINFO, sInfo.c_str());
            // We can assume replaced everything so there is no point stay focused
            // on the replace all button or the replace with edit box. The user
            // can really only cancel now or search for something else.
            FocusOn(IDC_SEARCHCOMBO);
        }
        else
            SearchStringNotFound();
    }
    else
    {
        if (replaceCount == 0)
            SearchStringNotFound();
        else
        {
            if (!DoSearch(true))
                SetInfoText(IDS_NOMORETOREPLACE);
        }
    }
}

bool CFindReplaceDlg::DoSearch(bool replaceMode)
{
    Clear(IDC_SEARCHINFO);

    if (g_lastSelText.empty() || g_lastSelText.compare(g_sHighlightString) || (g_searchFlags != g_lastSearchFlags))
    {
        DocScrollClear(DOCSCROLLTYPE_SEARCHTEXT);
        g_searchMarkerCount = 0;
    }
    DocScrollClear(DOCSCROLLTYPE_SELTEXT);
    OnOutOfScope(DocScrollUpdate());

    std::wstring findText = GetDlgItemText(IDC_SEARCHCOMBO).get();
    UpdateSearchStrings(findText);
    g_findString = CUnicodeUtils::StdGetUTF8(findText);
    g_sHighlightString = g_findString;
    g_searchFlags = GetScintillaOptions();
    if (g_findString.empty())
    {
        SearchStringNotFound();
        return false;
    }

    Sci_TextToFind ttf = { 0 };
    ttf.chrg.cpMin = (long)ScintillaCall(SCI_GETCURRENTPOS);
    ttf.chrg.cpMax = (long)ScintillaCall(SCI_GETLENGTH);
    ttf.lpstrText = g_findString.c_str();

    sptr_t findRet = ScintillaCall(SCI_FINDTEXT, g_searchFlags, (sptr_t)&ttf);
    if (findRet == -1)
    {
        // Retry from the start of the doc.
        ttf.chrg.cpMax = ttf.chrg.cpMin;
        ttf.chrg.cpMin = 0;
        findRet = ScintillaCall(SCI_FINDTEXT, g_searchFlags, (sptr_t)&ttf);
        // Only report wrap around if we found something in the other direction.
        // If we didn't find anything we'll prefer to report that no match was found.
        if (findRet >= 0)
            SetInfoText(IDS_FINDRETRYWRAP);
    }
    if (findRet >= 0)
        Center(ttf.chrgText.cpMin, ttf.chrgText.cpMax);
    else if (!replaceMode)
        SearchStringNotFound();
    return (findRet >= 0);
}

int CFindReplaceDlg::GetScintillaOptions() const
{
    int flags = 0;
    if (IsDlgButtonChecked(*this, IDC_MATCHCASE) == BST_CHECKED)
        flags |= SCFIND_MATCHCASE;
    if (IsDlgButtonChecked(*this, IDC_MATCHWORD) == BST_CHECKED)
        flags |= SCFIND_WHOLEWORD;
    if (IsDlgButtonChecked(*this, IDC_MATCHREGEX) == BST_CHECKED)
        flags |= SCFIND_REGEXP | SCFIND_CXX11REGEX;
    return flags;
}

void CFindReplaceDlg::SortResults()
{
    // Where both are on disk files, sort by filename then folder.
    // Otherwise just order by the name.
    // Lastly after each compare, fallthrough to also sort by line number.
    std::sort(m_searchResults.begin(), m_searchResults.end(),
        [&](const CSearchResult& lhs, const CSearchResult& rhs)
    {
        int result = -1;
        if (lhs.docID.IsValid() && rhs.docID.IsValid())
        {
            const auto& ldoc = GetDocumentFromID(lhs.docID);
            const auto& rdoc = GetDocumentFromID(rhs.docID);
            // If both results are associated with files...
            if (!ldoc.m_path.empty() && !rdoc.m_path.empty())
            {
                result = CPathUtils::PathCompare(
                    CPathUtils::GetFileName(ldoc.m_path),
                    CPathUtils::GetFileName(rdoc.m_path));
                if (result == 0)
                    result = CPathUtils::PathCompare(
                        CPathUtils::GetParentDirectory(ldoc.m_path),
                        CPathUtils::GetParentDirectory(rdoc.m_path));
            }
            // If left result is associated with a file and (implied)
            // the other path is not associated with a file...
            else if (!ldoc.m_path.empty())
                result = CPathUtils::PathCompare(CPathUtils::GetFileName(ldoc.m_path),
                    GetTitleForDocID(rhs.docID));
            // else if right result is associated with a file and (implied)
            // the left path is not associated with a file...
            else if (!rdoc.m_path.empty())
                result = CPathUtils::PathCompare(GetTitleForDocID(lhs.docID),
                    CPathUtils::GetFileName(rdoc.m_path));
            else
            {
                // Otherwise (implied) neither file is associated with a file,
                // i.e. new documents.
                result = CPathUtils::PathCompare(GetTitleForDocID(lhs.docID),
                    GetTitleForDocID(rhs.docID));
            }
        }
        else if (lhs.docID.IsValid())
        {
            assert(rhs.hasPath());
            result = CPathUtils::PathCompare(GetTitleForDocID(lhs.docID),
                CPathUtils::GetFileName(m_foundPaths[rhs.pathIndex]));
        }
        else if (rhs.docID.IsValid())
        {
            assert(lhs.hasPath());
            result = CPathUtils::PathCompare(
                CPathUtils::GetFileName(m_foundPaths[lhs.pathIndex]), GetTitleForDocID(rhs.docID));
        }
        else if (lhs.hasPath() && rhs.hasPath())
            result = CPathUtils::PathCompare(m_foundPaths[lhs.pathIndex], m_foundPaths[rhs.pathIndex]);
        else
            assert(false);
        if (result == 0)
            return lhs.line < rhs.line;
        else
            return result < 0;
    });
}

void CFindReplaceDlg::DoSearchAll(int id)
{
    // Should have stopped before searching again.
    APPVERIFY(m_ThreadsRunning == 0);
    if (m_ThreadsRunning)
        return;
    // To prevent having each document activate the tab while searching,
    // we use a dummy scintilla control and do the search in there
    // instead of using the active control.
    Clear(IDC_SEARCHINFO);
    m_searchType = id;
    m_searchResults.clear();
    m_foundPaths.clear();
    m_bStop = false;
    m_foundsize = 0;

    std::wstring findText = GetDlgItemText(IDC_SEARCHCOMBO).get();
    if (id != IDC_FINDFILES)
        UpdateSearchStrings(findText);
    std::string searchfor = CUnicodeUtils::StdGetUTF8(findText);

    int searchflags = GetScintillaOptions();
    unsigned int exSearchFlags = 0;
    bool searchForFunctions = IsDlgButtonChecked(*this, IDC_FUNCTIONS) == BST_CHECKED;
    if (searchForFunctions)
        exSearchFlags |= SF_SEARCHFORFUNCTIONS;
    bool searchSubFolders = IsDlgButtonChecked(*this, IDC_SEARCHSUBFOLDERS) == BST_CHECKED;
    if (searchSubFolders)
        exSearchFlags |= SF_SEARCHSUBFOLDERS;

    if (id == IDC_FINDFILES)
        m_resultsType = ResultsType::Filenames;
    else
    {
        if (searchForFunctions)
            m_resultsType = ResultsType::Functions;
        else if (id == IDC_FINDALLINDIR && searchfor.empty())
            m_resultsType = ResultsType::FirstLines;
        else
            m_resultsType = ResultsType::MatchedTerms;
    }

    InitResultsList();

    auto hListControl = GetDlgItem(*this, IDC_FINDRESULTS);

    if (id == IDC_FINDALL || id == IDC_FINDALLINTABS)
    {
        if (searchfor.empty() && !searchForFunctions)
        {
            SetInfoText(IDS_FINDNOTFOUND);
            return;
        }

        if (id == IDC_FINDALL && HasActiveDocument())
        {
            auto docId = GetDocIDFromTabIndex(GetActiveTabIndex());
            const auto& doc = GetActiveDocument();
            ResString rInfo(hRes, IDS_SEARCHING_FILE);
            auto sInfo = CStringUtils::Format(rInfo, CPathUtils::GetFileName(doc.m_path).c_str());
            SetDlgItemText(*this, IDC_SEARCHINFO, sInfo.c_str());
            SearchDocument(m_searchWnd, docId, doc, searchfor, searchflags, exSearchFlags,
                m_searchResults, m_foundPaths);
            SortResults();
        }
        else if (id == IDC_FINDALLINTABS)
        {
            ResString rInfo(hRes, IDS_SEARCHING_FILE);
            int tabcount = GetTabCount();
            for (int i = 0; i < tabcount; ++i)
            {
                auto docID = GetDocIDFromTabIndex(i);
                const auto& doc = GetDocumentFromID(docID);
                auto sInfo = CStringUtils::Format(rInfo, CPathUtils::GetFileName(doc.m_path).c_str());
                SetDlgItemText(*this, IDC_SEARCHINFO, sInfo.c_str());
                UpdateWindow(*this);
                SearchDocument(m_searchWnd, docID, doc, searchfor, searchflags, exSearchFlags,
                    m_searchResults, m_foundPaths);
                if (m_foundsize >= MAX_SEARCHRESULTS)
                {
                    ResString rInfoMax(hRes, IDS_SEARCHING_FILE_MAX);
                    auto sInfoMax = CStringUtils::Format(rInfoMax, MAX_SEARCHRESULTS);
                    SetDlgItemText(*this, IDC_SEARCHINFO, sInfoMax.c_str());
                    break;
                }
            }
            SortResults();
        }
        ListView_SetItemCountEx(hListControl, m_searchResults.size(), 0);
        ShowResults(true);
        UpdateWindow(*this);
        std::wstring sInfo;
        if (id == IDC_FINDALLINTABS)
        {
            ResString rInfo(hRes, IDS_FINDRESULT_COUNTALL);
            sInfo = CStringUtils::Format(rInfo, (int)m_searchResults.size(), GetTabCount());
        }
        else
        {
            ResString rInfo(hRes, IDS_FINDRESULT_COUNT);
            sInfo = CStringUtils::Format(rInfo, (int)m_searchResults.size());
        }
        // We don't action the first result because we can't consistently do that.
        // For multi-tab going to the item might involve opening another
        // document which we're not sure the user wants to do.
        // With async searches, we don't know when the first result will
        // arrive. So better to just be consistent and let the user always
        // make the selection choice. They only need to press ENTER anyway.
        SetDlgItemText(*this, IDC_SEARCHINFO, sInfo.c_str());
        FocusOn(IDC_FINDRESULTS);
        FocusOnFirstListItem(false);
    }
    else if (id == IDC_FINDFILES || id == IDC_FINDALLINDIR)
    {
        std::wstring searchFolder = GetDlgItemText(IDC_SEARCHFOLDER).get();
        if (searchFolder.empty()) // Can't search with no folder.
        {
            SetInfoText(IDS_NOSEARCHFOLDER);
            FocusOn(IDC_SEARCHFOLDER);
            return;
        }
        if (!PathFileExists(searchFolder.c_str()))
        {
            SetInfoText(IDS_SEARCHFOLDERNOTFOUND);
            return;
        }
        UpdateSearchFolderStrings(searchFolder);
        std::wstring filesString = GetDlgItemText(IDC_SEARCHFILES).get();
        std::vector<std::wstring> filesToFind;
        // If searching for specific files or terms in those files, we need the file list.
        // ';' is perhaps not as natural as ',', but Windows Common Dialog Box Controls use ';'
        // as separator so we do too.
        split(filesToFind, filesString, L';');
        // Don't save invalid or empty file sets.
        if (filesToFind.size() > 0)
            UpdateSearchFilesStrings(filesString); 
        EnableControls(false);
        ShowResults(true);

        UpdateMatchCount(false);
        FocusOn(IDC_FINDRESULTS);
        InterlockedIncrement(&m_ThreadsRunning);
        // Start a new thread to search all files.
        std::thread(&CFindReplaceDlg::SearchThread,
            this, id, searchFolder, searchfor, searchflags, exSearchFlags, filesToFind).detach();
        // Operation will be completed in OnSearchResultsReady which will be
        // called in the UI thread so screens results can be updated etc.
    }
    else
    {
        APPVERIFY(false);
    }
}

void CFindReplaceDlg::FocusOnFirstListItem(bool keepAnyExistingSelection)
{
    HWND hListControl = GetDlgItem(*this, IDC_FINDRESULTS);
    int count = ListView_GetItemCount(hListControl);
    if (count > 0)
    {
        int index = ListView_GetNextItem(hListControl, -1, LVNI_SELECTED);
        if (index == -1 || (index != 0 && ! keepAnyExistingSelection))
        {
            ListView_EnsureVisible(hListControl, 0, TRUE);
            ListView_SetItemState(hListControl, 0, LVIS_SELECTED, LVIS_SELECTED);
        }
    }
}

bool CFindReplaceDlg::IsMatchingFile(const std::wstring& path, const std::vector<std::wstring>& filesToFind) const
{
    bool matched = false;
    if (filesToFind.size() > 0)
    {
        auto targetName = CPathUtils::GetFileName(path);
        auto whereAt = std::find_if(filesToFind.begin(), filesToFind.end(),
            [&](const std::wstring& fileToFind)
        {
            bool match = !!PathMatchSpec(targetName.c_str(), fileToFind.c_str());
            return match;
        });
        matched = (whereAt != filesToFind.end());
    }
    return matched;
}

bool CFindReplaceDlg::IsExcludedFile(const std::wstring& path) const
{
    auto ext = CPathUtils::GetFileExtension(path);
    for (const auto& e : excludedExtensions)
    {
        if (_wcsicmp(ext.c_str(), e.c_str()) == 0)
            return true;
    }
    return false;
}

bool CFindReplaceDlg::IsExcludedFolder(const std::wstring& path) const
{
    auto targetName = CPathUtils::GetFileName(path);
    for (const auto& e : excludedFolders)
    {
        if (_wcsicmp(targetName.c_str(), e.c_str()) == 0)
            return true;
    }
    return false;
}

void CFindReplaceDlg::SearchThread(
    int id, const std::wstring& searchpath, const std::string& searchfor,
    int flags, unsigned int exSearchFlags, const std::vector<std::wstring>& filesToFind)
{
    // NOTE: all parameter validation should be done before getting here.
    auto timeOfLastProgressUpdate = std::chrono::steady_clock::now();
    assert(id == IDC_FINDFILES || id == IDC_FINDALLINDIR);

    bool searchSubFolders = (exSearchFlags & SF_SEARCHSUBFOLDERS) != 0;

    m_pendingSearchResults.clear();
    m_pendingFoundPaths.clear();

    // We need a Scintilla object created on the same thread as it will be used,
    // that's why we can't use the m_searchWnd object.
    CScintillaWnd searchWnd(hRes);
    searchWnd.InitScratch(hRes);

    CDirFileEnum enumerator(searchpath);
    bool bIsDir = false;
    std::wstring path;
    CDocumentManager manager;
    bool searchSubFoldersFlag = searchSubFolders;

    // Note that on some versions of Windows, e.g. Window 7, paths like "*.cpp" will
    // actually match "*.cpp*" which is strange but it's seems a quirk of the OS not CDirFileEnum.
    while (enumerator.NextFile(path, &bIsDir, searchSubFoldersFlag) && !m_bStop)
    {
        if (bIsDir)
        {
            searchSubFoldersFlag = IsExcludedFolder(path) ? false : searchSubFolders;
            continue;
        }
        searchSubFoldersFlag = searchSubFolders;

        // We must continue to the top of the loop and onto the next file
        // if we don't want the current file.
        
        bool match = false;
        if (filesToFind.size() == 0) // If we using implicit matching....
        {
            if (!IsExcludedFile(path))
                match = true; // If we implicitly don't want this, reject it.
        }
        else // Using explicit matching.
        {
            match = IsMatchingFile(path, filesToFind);
        }
        if (! match)
            continue; // Not a match.

        // If we reach here, the file is of interest to the user.
        if (id == IDC_FINDFILES)
        {
            // If finding OF files, only the name is of interest so our job is done.
            CSearchResult result;
            result.pathIndex = m_pendingFoundPaths.size();
            m_pendingFoundPaths.push_back(std::move(path));
            m_pendingSearchResults.push_back(std::move(result));
            NewData(timeOfLastProgressUpdate, false);
            if (++m_foundsize >= MAX_SEARCHRESULTS)
                break;

            continue;
        }
        // Else if finding IN files... search for matches in the file of interest.
        assert(id == IDC_FINDALLINDIR);

        // TODO! Ideally we need a means to check for cancellation during load so that
        // BowPad doesn't appear hung while loading large files.
        CDocument doc = manager.LoadFile(nullptr, path, -1, false);
        // Don't crash if the document cannot be loaded. .e.g. if it is locked.
        if (doc.m_document != Document(0))
        {
            DocID did(1);
            manager.AddDocumentAtEnd(doc, did);
            OnOutOfScope( manager.RemoveDocument(did); );
            if (!m_bStop)
            {
                size_t resultSizeBefore = m_pendingSearchResults.size();
                SearchDocument(searchWnd, DocID(), doc, searchfor, flags, exSearchFlags,
                    m_pendingSearchResults, m_pendingFoundPaths);
                if (m_pendingSearchResults.size() - resultSizeBefore > 0)
                    m_pendingFoundPaths.push_back(std::move(path));
                NewData(timeOfLastProgressUpdate, false);
                if (m_foundsize >= MAX_SEARCHRESULTS)
                    break;
            }
        }
    }
    NewData(timeOfLastProgressUpdate, true);

    InterlockedDecrement(&m_ThreadsRunning);
}

void CFindReplaceDlg::AcceptData()
{
    std::unique_lock<std::mutex> lk(m_waitingDataMutex);
    auto dataReadyPred = [&]() { return m_dataReady; };
    m_dataExchangeCondition.wait(lk, dataReadyPred);
    // Patch up the index so it's makes sense in the list it is
    // appending into rather than the list it moving from.
    for (auto& item : m_pendingSearchResults)
        item.pathIndex += m_foundPaths.size();
    move_append(m_searchResults, m_pendingSearchResults);
    // Enable this if something suspect occurs.
    //for (const auto& item : m_pendingSearchResults)
        //if (item.pathIndex < 0 || item.pathIndex >= m_foundPaths.size())
            //APPVERIFY(false);
    move_append(m_foundPaths, m_pendingFoundPaths);

    m_dataAccepted = true;
    lk.unlock();
    m_dataExchangeCondition.notify_one();
}

void CFindReplaceDlg::SearchDocument(
    CScintillaWnd& searchWnd, DocID docID, const CDocument& doc,
    const std::string& searchfor, int searchflags, unsigned int exSearchFlags,
    SearchResults& searchResults, SearchPaths& foundPaths
    )
{
    bool searchForFunctions = (exSearchFlags & SF_SEARCHFORFUNCTIONS) != 0;

    auto wsearchfor = CUnicodeUtils::StdGetUnicode(searchfor);
    if (searchForFunctions)
        searchflags |= SCFIND_REGEXP | SCFIND_CXX11REGEX;
    bool wholeWord = (searchflags & SCFIND_WHOLEWORD) != 0;
    if (searchForFunctions && !wholeWord)
        wsearchfor = L"*" + wsearchfor + L"*";

    searchWnd.Call(SCI_SETSTATUS, SC_STATUS_OK);   // reset error status
    searchWnd.Call(SCI_CLEARALL);
    searchWnd.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
    bool previousReadOnlyFlag = searchWnd.Call(SCI_GETREADONLY) != sptr_t{};
    if (!previousReadOnlyFlag)
        searchWnd.Call(SCI_SETREADONLY, true);

    OnOutOfScope(
        if (!previousReadOnlyFlag)
            searchWnd.Call(SCI_SETREADONLY, false);
        searchWnd.Call(SCI_SETDOCPOINTER, 0, 0);
    );

    Sci_TextToFind ttf = { 0 };
    ttf.chrg.cpMin = 0;
    ttf.chrg.cpMax = (long)searchWnd.Call(SCI_GETLENGTH);
    std::string funcregex;
    if (searchForFunctions)
    {
        auto lang = doc.GetLanguage();
        if (lang.empty())
            lang = CLexStyles::Instance().GetLanguageForDocument(doc, searchWnd);
        if (lang.empty())
            return;
        funcregex = CLexStyles::Instance().GetFunctionRegexForLang(lang);
        if (funcregex.empty())
            return;
        ttf.lpstrText = funcregex.c_str();
    }
    else
        ttf.lpstrText = searchfor.c_str();

    std::wstring funcName;
    sptr_t findRet = -1;
    std::string line; // Reduce memory reallocations by keeping this out of the loop.
    do
    {
        findRet = searchWnd.Call(SCI_FINDTEXT, searchflags, (sptr_t)&ttf);
        if (findRet >= 0)
        {
            CSearchResult result;
            // Don't use the document id as a reference to this file unless we have to,
            // use the path. The document might close, or be saved to another path,
            // but by not using the document id, the result can just stick consistently
            // to wherever it originally referred to.
            if (docID.IsValid())
                result.docID = docID;
            result.pos = ttf.chrgText.cpMin;
            char c = (char)searchWnd.Call(SCI_GETCHARAT, result.pos);
            while (c == '\n' || c == '\r')
            {
                ++result.pos;
                c = (char)searchWnd.Call(SCI_GETCHARAT, result.pos);
            }
            result.line = searchWnd.Call(SCI_LINEFROMPOSITION, result.pos);
            auto linepos = searchWnd.Call(SCI_POSITIONFROMLINE, result.line);
            if (searchForFunctions)
            {
                result.lineText = CUnicodeUtils::StdGetUnicode(searchWnd.GetTextRange(ttf.chrgText.cpMin, ttf.chrgText.cpMax));
                size_t linesize = result.lineText.length();
                while (linesize > 0 && (result.lineText[linesize - 1] == L'\n' || result.lineText[linesize - 1] == L'\r'))
                    --linesize;
                result.lineText.resize(linesize);
                result.posInLineStart = 0;
                result.posInLineEnd = 0;
            }
            else
            {
                result.posInLineStart = linepos >= 0 ? result.pos - linepos : 0;
                result.posInLineEnd = linepos >= 0 ? ttf.chrgText.cpMax - linepos : 0;

                size_t linesize = (size_t)searchWnd.Call(SCI_LINELENGTH, result.line);
                line.resize(linesize);
                searchWnd.Call(SCI_GETLINE, result.line, reinterpret_cast<sptr_t>(line.data()));
                // remove EOLs
                while (linesize > 0 && (line[linesize - 1] == '\n' || line[linesize - 1] == '\r'))
                    --linesize;
                line.resize(linesize);
                result.lineText = CUnicodeUtils::StdGetUnicode(line, false);
                // adjust the line positions: Scintilla uses utf8, but utf8 converted to
                // utf16 can have different char sizes so the positions won't match anymore
                result.posInLineStart = UTF8Helper::UTF16PosFromUTF8Pos(line.c_str(), result.posInLineStart);
                result.posInLineEnd = UTF8Helper::UTF16PosFromUTF8Pos(line.c_str(), result.posInLineEnd);
            }

            // When searching for functions, we have to narrow the match down by name ourself.
            bool matched = false;
            if (!searchForFunctions)
                matched = true;
            else
            {
                Normalize(result);
                // The set of regexp expressions we use to find functions 
                // don't allow us to identify a specifically named function.
                // They just find any function definitions.
                // To narrow down to a particular function name means doing that ourself.
                // We allow the user to use guess roughly the name by supporting
                // * and ? regular expressions but offering any other options seems excessive.
                // And by sticking to just * and ? means we can enable the use of these
                // by default without requiring the "use regexp" checkbox to be checked
                // because a function name can't include * or ? so the meaning
                // of those two characters is never ambiguous.
                if (searchfor.empty())
                    matched = true;
                else
                {
                    if (ParseSignature(funcName, result.lineText))
                    {
                        matched = wcswildicmp(wsearchfor.c_str(), funcName.c_str()) != 0;
                    }
                }
            }
            if (matched)
            {
                if (!docID.IsValid())
                    result.pathIndex = foundPaths.size();
                searchResults.push_back(std::move(result));
                if (++m_foundsize >= MAX_SEARCHRESULTS)
                    break;
            }

            if (ttf.chrg.cpMin >= ttf.chrgText.cpMax)
                break;
            ttf.chrg.cpMin = ttf.chrgText.cpMax + 1;
        }
    } while (findRet >= 0 && !m_bStop);
}

void CFindReplaceDlg::NewData(
    std::chrono::steady_clock::time_point& timeOfLastProgressUpdate,
    bool finished)
{
    // Only async functions that should be doing this.
    assert(m_searchType == IDC_FINDALLINDIR || m_searchType == IDC_FINDFILES);
    std::chrono::steady_clock::time_point timeNow = std::chrono::steady_clock::now();
    std::chrono::steady_clock::duration durationSinceLastDataUpdate = timeNow - timeOfLastProgressUpdate;
    // Exchange data if we are finished or we have batched enough data
    // or it has been a while since we reported any data and we have some to report.
    if (finished ||
        m_pendingSearchResults.size() >= MAX_DATA_BATCH_SIZE ||
        (durationSinceLastDataUpdate >= PROGRESS_UPDATE_INTERVAL
        && (m_pendingFoundPaths.size() > 0 || m_pendingSearchResults.size() > 0)))
    {
        // Hopefully safe to touch the size field.
        if (m_searchResults.size() >= MAX_SEARCHRESULTS)
            finished = true;
        // NOTE!! It's essential that data isn't sent to the client until
        // it's a full contained unit, i.e. search results must be complete
        // for a given file. Otherwise the file indexes won't match/fixup properly
        // when they are moved from the pending results to the real results.
        timeOfLastProgressUpdate = timeNow;
        {
            std::lock_guard<std::mutex> lk(m_waitingDataMutex);
            m_dataReady = true;
        }
        m_dataExchangeCondition.notify_one();
        SendMessage(*this, WM_THREADRESULTREADY, finished ? WPARAM(1) : WPARAM(0), LPARAM(0));

        {
            std::unique_lock<std::mutex> lk(m_waitingDataMutex);
            auto dataAcceptedPred = [&]() { return this->m_dataAccepted; };
            m_dataExchangeCondition.wait(lk, dataAcceptedPred);
            m_dataAccepted = false;
            m_dataReady = false;
        }
        assert(m_pendingSearchResults.empty());
        assert(m_pendingFoundPaths.empty());
    }
}

int CFindReplaceDlg::ReplaceDocument(CDocument& doc, const std::string& sFindstring, const std::string& sReplaceString, int searchflags)
{
    m_searchWnd.Call(SCI_SETSTATUS, SC_STATUS_OK);   // reset error status
    m_searchWnd.Call(SCI_CLEARALL);
    m_searchWnd.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
    m_searchWnd.Call(SCI_SETTARGETSTART, 0);
    m_searchWnd.Call(SCI_SETSEARCHFLAGS, searchflags);
    m_searchWnd.Call(SCI_SETTARGETEND, m_searchWnd.Call(SCI_GETLENGTH));
    m_searchWnd.Call(SCI_BEGINUNDOACTION);
    OnOutOfScope(
        m_searchWnd.Call(SCI_ENDUNDOACTION);
        m_searchWnd.Call(SCI_SETDOCPOINTER, 0, 0);
    );

    sptr_t findRet = -1;
    int replaceCount = 0;
    do
    {
        findRet = m_searchWnd.Call(SCI_SEARCHINTARGET, sFindstring.length(), (sptr_t)sFindstring.c_str());
        if (findRet >= 0)
        {
            auto replaceFunc = ((searchflags & SCFIND_REGEXP) != 0)
                ? SCI_REPLACETARGETRE : SCI_REPLACETARGET;
            m_searchWnd.Call(replaceFunc, sReplaceString.length(), (sptr_t)sReplaceString.c_str());
            ++replaceCount;
            long tstart = (long)m_searchWnd.Call(SCI_GETTARGETSTART);
            long tend = (long)m_searchWnd.Call(SCI_GETTARGETEND);
            if ((tend > tstart) || (sReplaceString.empty()))
                m_searchWnd.Call(SCI_SETTARGETSTART, tend);
            else
                m_searchWnd.Call(SCI_SETTARGETSTART, tend + 1);

            m_searchWnd.Call(SCI_SETTARGETEND, m_searchWnd.Call(SCI_GETLENGTH));
            doc.m_bIsDirty = true;
        }
    } while (findRet >= 0);
    return replaceCount;
}

void CFindReplaceDlg::InitResultsList()
{
    m_trackingOn = true;

    HWND hListControl = GetDlgItem(*this, IDC_FINDRESULTS);
    SetWindowTheme(hListControl, L"Explorer", nullptr);
    ListView_SetItemCountEx(hListControl, 0, 0);

    auto hListHeader = ListView_GetHeader(hListControl);
    int c = Header_GetItemCount(hListHeader) - 1;
    while (c >= 0)
        ListView_DeleteColumn(hListControl, c--);

    DWORD exStyle = LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_FULLROWSELECT;
    ListView_SetExtendedListViewStyle(hListControl, exStyle);

    // NOTE: m_searchType or m_resultsType could be set here to adjust the titles if need be.
    ResString sFile(hRes, IDS_FINDRESULT_HEADERFILE);
    ResString sLine(hRes, IDS_FINDRESULT_HEADERLINE);
    ResString sLineText(hRes, IDS_FINDRESULT_HEADERLINETEXT);

    LVCOLUMN lvc;
    lvc.mask = LVCF_TEXT | LVCF_FMT;
    lvc.fmt = LVCFMT_LEFT;
    // Note: pszText does not change what it points to here despite not being const.
    lvc.pszText = const_cast<LPWSTR>(sFile.c_str());
    ListView_InsertColumn(hListControl, 0, &lvc);
    lvc.pszText = const_cast<LPWSTR>(sLine.c_str());
    lvc.fmt = LVCFMT_RIGHT;
    ListView_InsertColumn(hListControl, 1, &lvc);
    lvc.fmt = LVCFMT_LEFT;
    lvc.pszText = const_cast<LPWSTR>(sLineText.c_str());
    ListView_InsertColumn(hListControl, 2, &lvc);

    ListView_SetColumnWidth(hListControl, 0, 150);
    ListView_SetColumnWidth(hListControl, 1, 50);
    ListView_SetColumnWidth(hListControl, 2, LVSCW_AUTOSIZE_USEHEADER);
}

void CFindReplaceDlg::CheckRegex()
{
    try
    {
        auto findText = GetDlgItemText(IDC_SEARCHCOMBO);
        const std::wregex ignex(findText.get(),
            std::regex_constants::icase | std::regex_constants::ECMAScript);
        Clear(IDC_SEARCHINFO);
    }
    catch (const std::exception&)
    {
        SetInfoText(IDS_REGEX_NOTOK);
    }
}

static inline size_t GetBase(char current, size_t& size) noexcept
{
    switch (current)
    {
        case 'b': // 11111111
            size = 8;
            return 2;
        case 'o': // 377
            size = 3;
            return 8;
        case 'd': // 255
            size = 3;
            return 10;
        case 'x': // 0xFF
            size = 2;
            return 16;
        case 'u': // 0xCDCD
            size = 4;
            return 16;
    }
    size = 0;
    return 0;
}

std::string CFindReplaceDlg::UnEscape(const std::string& str)
{
    size_t charLeft = str.length();
    std::string result;
    result.reserve(charLeft);
    for (size_t i = 0; i < str.length(); ++i)
    {
        char current = str[i];
        --charLeft;
        if (current == '\\' && charLeft > 0)
        {
            // possible escape sequence
            ++i;
            --charLeft;
            current = str[i];
            switch (current)
            {
                case 'r':
                    result.push_back('\r');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case '0':
                    result.push_back('\0');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                case '\\':
                    result.push_back('\\');
                    break;
                default:
                {
                    size_t size;
                    auto base = GetBase(current, size);
                    if (base != 0 && charLeft >= size)
                    {
                        size_t res = 0;
                        if (ReadBase(&str[i + 1], &res, base, size))
                        {
                            result.push_back((char)res);
                            i += size;
                            break;
                        }
                    }
                    // Unknown/invalid sequence, treat as regular text.
                    result.push_back('\\');
                    result.push_back(current);
                }
            }
        }
        else
        {
            result.push_back(current);
        }
    }
    return result;
}

bool CFindReplaceDlg::ReadBase(const char* str, size_t* value, size_t base, size_t size)
{
    size_t i = 0, temp = 0;
    *value = 0;
    char max = '0' + (char)base - 1;
    char current;
    while (i < size)
    {
        current = str[i];
        if (current >= 'A')
        {
            current &= 0xdf;
            current -= ('A' - '0' - 10);
        }
        else if (current > '9')
            return false;

        if (current >= '0' && current <= max)
        {
            temp *= base;
            temp += (current - '0');
        }
        else
        {
            return false;
        }
        ++i;
    }
    *value = temp;
    return true;
}

void CFindReplaceDlg::EnableControls(bool bEnable)
{
    DialogEnableWindow(IDC_FINDBTN, bEnable);
    DialogEnableWindow(IDC_FINDPREVIOUS, bEnable);
    DialogEnableWindow(IDC_FINDALL, bEnable);
    DialogEnableWindow(IDC_FINDALLINTABS, bEnable);
    DialogEnableWindow(IDC_REPLACEBTN, bEnable);
    DialogEnableWindow(IDC_REPLACEALLBTN, bEnable);
    DialogEnableWindow(IDC_FINDALLINDIR, bEnable);
    DialogEnableWindow(IDC_SEARCHCOMBO, bEnable);
    DialogEnableWindow(IDC_REPLACECOMBO, bEnable);
    DialogEnableWindow(IDC_MATCHWORD, bEnable);
    DialogEnableWindow(IDC_MATCHREGEX, bEnable);
    DialogEnableWindow(IDC_MATCHCASE, bEnable);
    DialogEnableWindow(IDC_FUNCTIONS, bEnable);
    DialogEnableWindow(IDC_SETSEARCHFOLDER, bEnable);
    DialogEnableWindow(IDC_SETSEARCHFOLDERCURRENT, bEnable);
    DialogEnableWindow(IDC_SEARCHFOLDERFOLLOWTAB, bEnable);
    DialogEnableWindow(IDC_SETSEARCHFOLDERTOPARENT, bEnable);
    DialogEnableWindow(IDC_SEARCHSUBFOLDERS, bEnable);
    DialogEnableWindow(IDC_FINDFILES, bEnable);
    DialogEnableWindow(IDC_SEARCHFOLDER, bEnable);
    DialogEnableWindow(IDC_SEARCHFILES, bEnable);
}

void CFindReplaceDlg::DoClose()
{
    // If the user closes the window soon after an error,
    // then re-opens, the previous error will be there which will
    // will make it look like they just did a new search or something.
    KillTimer(*this, TIMER_INFOSTRING);
    Clear(IDC_SEARCHINFO);

    ShowResults(false);
    ShowWindow(*this, SW_HIDE);
    g_sHighlightString.clear();
    DocScrollUpdate();
    // Search results can get quite large (especially for find in files with sub-folders)
    // so ensure the results are discarded when the dialog is closed.
    // Modeless dialogs are hidden and not destroyed on close so we don't want results
    // still taking up memory.
    // Note: at this time shrink_to_fit doesn't appear to be as effective as swap.
    
    // Intentional use of scope is to encourage early destruction.
    {
        SearchResults esr;
        std::swap(m_searchResults, esr);
        SearchPaths esp;
        std::swap(m_foundPaths, esp);
    }
    {
        SearchResults esr;
        std::swap(m_pendingSearchResults, esr);
        SearchPaths esp;
        std::swap(m_pendingFoundPaths, esp);
    }
    // Reset the window to it's starting size.
    // Most users will probably find resetting the size helpful.
    // Resetting the position is more subjective and could be irritating so we don't do that.
    // Some experimentation may be required to find the best default. For now just do size.
    SetWindowPos(*this, nullptr, 0, 0, m_originalSize.cx, m_originalSize.cy,
        SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
}

void CFindReplaceDlg::OnClose()
{
    m_bStop = true;
    auto start = GetTickCount64();
    while (m_ThreadsRunning && (GetTickCount64() - start < 5000))
        Sleep(10);
}

void CFindReplaceDlg::CheckSearchOptions()
{
    HWND hSearchFolder = GetDlgItem(*this, IDC_SEARCHFOLDER);
    bool haveSearchFolder = GetDlgItemTextLength(IDC_SEARCHFOLDER) > 0
        || ComboBox_GetCurSel(hSearchFolder) != CB_ERR;
    // Don't offer to find files or in files if there are no folder and files specified.
    EnableWindow(GetDlgItem(*this, IDC_FINDALLINDIR), haveSearchFolder);
}

void CFindReplaceDlg::SetSearchFolder(const std::wstring& folder)
{
    SetDlgItemText(*this, IDC_SEARCHFOLDER, folder.c_str());
    CheckSearchOptions();
}

LRESULT CALLBACK CFindReplaceDlg::ListViewSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
    //CFindReplaceDlg* pThis = reinterpret_cast<CFindReplaceDlg*>(dwRefData);
    switch (uMsg)
    {
        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, ListViewSubClassProc, uIdSubclass);
            break;
    }
    auto result = DefSubclassProc(hWnd, uMsg, wParam, lParam);
    return result;
}

LRESULT CALLBACK CFindReplaceDlg::ComboBoxListSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
    //CFindReplaceDlg* pThis = reinterpret_cast<CFindReplaceDlg*>(dwRefData);
    switch (uMsg)
    {
        case WM_SYSKEYDOWN:
        {
            HWND hCombo = GetParent(hWnd);
            auto isDroppedDown = ComboBox_GetDroppedState(hCombo) != FALSE;
            if (isDroppedDown)
            {
                auto shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                auto controlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                auto altDown = (GetKeyState(VK_MENU) & 0x8000) != 0;
                auto delDown = (GetKeyState(VK_DELETE) & 0x8000) != 0;
                if (altDown && delDown && !controlDown && !shiftDown)
                {
                    // We don't use ResetContent because that clears
                    // the edit control as well.
                    int count = ComboBox_GetCount(hCombo);
                    while (count > 0)
                    {
                        --count;
                        ComboBox_DeleteString(hCombo, count);
                    }
                }
            }
            break;
        }
        case WM_KEYDOWN:
        {
            // When the dropdown list of the combobox is showing:
            // Let control+delete clear the combobox is the listbox is currently dropped.
            // Let delete do a delete of the currently selected item.
            if (wParam == VK_DELETE)
            {
                HWND hCombo = GetParent(hWnd);
                auto isDroppedDown = ComboBox_GetDroppedState(hCombo);
                if (isDroppedDown)
                {
                    auto shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    auto controlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                    if (!shiftDown && !controlDown)
                    {
                        int curSel = ComboBox_GetCurSel(hCombo);
                        if (curSel >= 0)
                            ComboBox_DeleteString(hCombo, curSel);
                    }
                }
            }
            break;
        }
        case WM_NCDESTROY:
        {
            RemoveWindowSubclass(hWnd, ComboBoxListSubClassProc, uIdSubclass);
            break;
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK CFindReplaceDlg::EditSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    CFindReplaceDlg* pThis = reinterpret_cast<CFindReplaceDlg*>(dwRefData);
    switch (uMsg)
    {
        case WM_NCDESTROY:
        {
            KillTimer(hWnd, TIMER_SUGGESTION);
            RemoveWindowSubclass(hWnd, EditSubClassProc, uIdSubclass);
            break;
        }
    }
    auto result = DefSubclassProc(hWnd, uMsg, wParam, lParam);
    switch (uMsg)
    {
        case WM_CHAR:
            if (wParam != VK_DELETE && wParam != VK_BACK)
                SetTimer(hWnd, TIMER_SUGGESTION, 100, nullptr);
            break;
        case WM_TIMER:
            if (wParam == TIMER_SUGGESTION)
            {
                KillTimer(hWnd, TIMER_SUGGESTION);
                std::wstring currentName = pThis->GetDlgItemText(IDC_SEARCHFILES).get();
                std::wstring searchFolder = pThis->GetDlgItemText(IDC_SEARCHFOLDER).get();
                bool searchSubFolders = IsDlgButtonChecked(*pThis, IDC_SEARCHSUBFOLDERS) == BST_CHECKED;
                auto suggestion = pThis->OfferFileSuggestion(searchFolder, searchSubFolders, currentName);
                if (!suggestion.empty())
                {
                    Edit_SetText(hWnd, suggestion.c_str());
                    Edit_SetSel(hWnd, currentName.size(), -1);
                }
            }
            break;
    }
    return result;
}

int CFindReplaceDlg::GetMaxCount(const std::wstring& section, const std::wstring& countKey, int defaultMaxCount) const
{
    int maxCount = (int)CIniSettings::Instance().GetInt64(section.c_str(), countKey.c_str(), defaultMaxCount);
    if (maxCount <= 0)
        maxCount = defaultMaxCount;
    return maxCount;
}

// The return value is the maximum count allowed, not the default count or count loaded.
// For the loaded count, check data.size().
int CFindReplaceDlg::LoadData(
    std::vector<std::wstring>& data, int defaultMaxCount,
    const std::wstring& section, const std::wstring& countKey, const std::wstring& itemKeyFmt) const
{
    data.clear();
    int maxCount = GetMaxCount(section, countKey, defaultMaxCount);
    std::wstring itemKey;
    for (int i = 0; i < maxCount; ++i)
    {
        itemKey = CStringUtils::Format(itemKeyFmt.c_str(), i);
        std::wstring value = CIniSettings::Instance().GetString(section.c_str(), itemKey.c_str(), L"");
        if (!value.empty())
            data.push_back(std::move(value));
    }
    return maxCount;
}

void CFindReplaceDlg::SaveData(
    const std::vector<std::wstring>& data,
    const std::wstring& section, const std::wstring& /*countKey*/, const std::wstring& itemKeyFmt
    )
{
    int i = 0;
    for (const auto& item : data)
    {
        std::wstring itemKey = CStringUtils::Format(itemKeyFmt.c_str(), i);
        if (!item.empty())
            CIniSettings::Instance().SetString(section.c_str(), itemKey.c_str(), item.c_str());
        ++i;
    }
    // Terminate list with an empty key.
    std::wstring itemKey = CStringUtils::Format(itemKeyFmt.c_str(), i);
    CIniSettings::Instance().SetString(section.c_str(), itemKey.c_str(), L"");
}

void CFindReplaceDlg::LoadCombo(
    int combo_id,
    const std::vector<std::wstring>& data
    )
{

    HWND hCombo = GetDlgItem(*this, combo_id);
    for (const auto& item : data)
    {
        if (!item.empty())
            ComboBox_InsertString(hCombo, -1, item.c_str());
    }
}

void CFindReplaceDlg::SaveCombo(
    int combo_id,
    std::vector<std::wstring>& data) const
{
    data.clear();
    HWND hCombo = GetDlgItem(*this, combo_id);
    int count = ComboBox_GetCount(hCombo);
    for (int i = 0; i < count; ++i)
    {
        std::wstring item;
        auto item_size = ComboBox_GetLBTextLen(hCombo, i);
        item.resize(item_size + 1);
        ComboBox_GetLBText(hCombo, i, item.data());
        item.resize(item_size);
        data.emplace_back(std::move(item));
    }
}

void CFindReplaceDlg::UpdateCombo( int comboId,const std::wstring& item, int maxCount )
{
    if (item.empty())
        return;
    HWND hCombo = GetDlgItem(*this, comboId);
    int count = ComboBox_GetCount(hCombo);
    // Remove any excess items to ensure we our within the maximum,
    // and never exceed it.
    while (count > maxCount)
    {
        --count;
        ComboBox_DeleteString(hCombo, count);
    }
    int pos = ComboBox_FindStringExact(hCombo, -1, item.c_str());
    bool itemExists = (pos != CB_ERR);
    // If the item exists, make sure it's selected and at the top.
    if (itemExists)
    {
        if (pos == 0) // Right position, make sure it's selected too.
            ComboBox_SetCurSel(hCombo, 0);
        else // Wrong position, bring it to the top by removing it and re-adding it.
        {
            ComboBox_DeleteString(hCombo, pos);
            int whereAt = ComboBox_InsertString(hCombo, 0, item.c_str());
            if (whereAt >= 0)
                ComboBox_SetCurSel(hCombo, whereAt);
        }
    }
    else
    {
        // Item does not exist, prepare to add it by purging the oldest
        // item if there is one and we need to in order to stay within the limit.
        if (count > 0 && count >= maxCount)
        {
            ComboBox_DeleteString(hCombo, count - 1);
            --count;
        }
        if (count < maxCount)
        {
            int whereAt = ComboBox_InsertString(hCombo, 0, item.c_str());
            if (whereAt >= 0)
                ComboBox_SetCurSel(hCombo, whereAt);
        }
    }
}

void CFindReplaceDlg::LoadSearchStrings()
{
    std::vector<std::wstring> searchStrings;
    m_maxSearchStrings = LoadData(searchStrings, DEFAULT_MAX_SEARCH_STRINGS,
        L"searchreplace", L"maxsearch", L"search%d");
    LoadCombo(IDC_SEARCHCOMBO, searchStrings);
}

void CFindReplaceDlg::SaveSearchStrings()
{
    std::vector<std::wstring> searchStrings;
    SaveCombo(IDC_SEARCHCOMBO, searchStrings);
    SaveData(searchStrings, L"searchreplace", L"maxsearch", L"search%d");
}

void CFindReplaceDlg::UpdateSearchStrings(const std::wstring& item)
{
    UpdateCombo(IDC_SEARCHCOMBO, item, m_maxSearchStrings);
}

void CFindReplaceDlg::LoadReplaceStrings()
{
    std::vector<std::wstring> replaceStrings;
    m_maxReplaceStrings = LoadData(replaceStrings, DEFAULT_MAX_REPLACE_STRINGS,
        L"searchreplace", L"maxreplace", L"replace%d");
    LoadCombo(IDC_REPLACECOMBO, replaceStrings);
}

void CFindReplaceDlg::SaveReplaceStrings()
{
    std::vector<std::wstring> replaceStrings;
    SaveCombo(IDC_REPLACECOMBO, replaceStrings);
    SaveData(replaceStrings, L"searchreplace", L"maxreplace", L"replace%d");
}

void CFindReplaceDlg::UpdateReplaceStrings(const std::wstring& item)
{
    UpdateCombo(IDC_REPLACECOMBO, item, m_maxReplaceStrings);
}

void CFindReplaceDlg::LoadSearchFolderStrings()
{
    std::vector<std::wstring> searchFolderStrings;
    m_maxSearchFolderStrings = LoadData(searchFolderStrings, DEFAULT_MAX_SEARCHFOLDER_STRINGS,
        L"searchreplace", L"maxsearchfolders", L"searchfolder%d");
    LoadCombo(IDC_SEARCHFOLDER, searchFolderStrings);
}

void CFindReplaceDlg::SaveSearchFolderStrings()
{
    std::vector<std::wstring> searchFolderStrings;
    SaveCombo(IDC_SEARCHFOLDER, searchFolderStrings);
    SaveData(searchFolderStrings, L"searchreplace", L"maxsearchfolders", L"searchfolder%d");
}

void CFindReplaceDlg::UpdateSearchFolderStrings(const std::wstring& item)
{
    UpdateCombo(IDC_SEARCHFOLDER, item, m_maxSearchFolderStrings);
}

void CFindReplaceDlg::LoadSearchFileStrings()
{
    std::vector<std::wstring> searchFileStrings;
    m_maxSearchFileStrings = LoadData(searchFileStrings, DEFAULT_MAX_SEARCHFILE_STRINGS,
        L"searchreplace", L"maxsearchfiles", L"searchfile%d");
    LoadCombo(IDC_SEARCHFILES, searchFileStrings);
}

void CFindReplaceDlg::SaveSearchFileStrings()
{
    std::vector<std::wstring> searchFileStrings;
    SaveCombo(IDC_SEARCHFILES, searchFileStrings);
    SaveData(searchFileStrings, L"searchreplace", L"maxsearchfiles", L"searchfile%d");
}

void CFindReplaceDlg::UpdateSearchFilesStrings(const std::wstring& item)
{
    UpdateCombo( IDC_SEARCHFILES, item, m_maxSearchFileStrings);
}

void CFindReplaceDlg::NotifyOnDocumentClose(DocID id)
{
    if (!m_open)
        return;
    
    // Search Results can have negative document id's.
    // If the closing document should ever be negative too we would match them.
    // That might be bad so check to avoid that.
    if (!id.IsValid())
    {
        assert(false);
        return;
    }

    auto hFindResults = GetDlgItem(*this, IDC_FINDRESULTS);

    // Results are created by searching documents currently loaded into tabs
    // or searching documents on disk.
    // The former result types initially obtain their path via their associated document.
    // The later result types initially obtain their path via a path variable.

    // Closing a document means it's document object is about to be deleted,
    // so we must make sure any results referring to that document stop doing so.
    // We do this by migrating those results to refer to a path via a path variable
    // instead of any document.
    // If we don't do this, at best the results will refer to a detached document and
    // display blankly and be unusable; and at worst the app may crash or be hard to comprehend.
    // But by "fixing up" the results, the document can be re-opened again from the results
    // list after it's closed.
    // For "new" documents that closing that has never been saved,
    // we must/ delete the results for it, because we can't ever re-open a document
    // via it's result if it never persisted and is now gone.

    const auto& doc = GetDocumentFromID(id);
    if (doc.m_path.empty())
    {
        // This is a "new" document that has never been saved, delete it's results
        // as it's closing.
        auto newEnd = std::remove_if(
            m_searchResults.begin(), m_searchResults.end(), [&](const CSearchResult& item)
        {
            return (item.docID == id);
        });
        auto deletedCount = m_searchResults.end() - newEnd;
        m_searchResults.erase(newEnd, m_searchResults.end());
        if (deletedCount > 0)
            ListView_SetItemCountEx(hFindResults, int(m_searchResults.size()), 0);
    }
    else
    {
        // This is a persisted document that is closing. Fix up the results.
        // to they are remain usable and the original document they result to can be
        // re-opened. This logic looks little strange, but is simple really.
        // 1. Find the first result relating to the document closing.
        // 2. On the first result, fix up a path for it which will be relevant
        //    to all the other related results.
        // 3. For all results of the closing document id, set the id to -1
        // 4. We know we have found all results relating to the closing document id,
        //    after we've found one and then failed to match another.
        //    All document id's relating to the same document are contiguous.
        bool found = false;
        int firstToRedraw = -1;
        int redrawCount = 0;
        size_t newPathIndex = (size_t) -1;
        for (int itemIndex = 0; itemIndex < (int)m_searchResults.size(); ++itemIndex)
        {
            auto& item = m_searchResults[itemIndex];
            if (item.docID == id)
            {
                if (!found) // Found first.
                {
                    firstToRedraw = itemIndex;
                    if (item.hasPath())
                        m_foundPaths[item.pathIndex] = doc.m_path; // Fixes all.
                    else
                    {
                        m_foundPaths.push_back(doc.m_path);
                        newPathIndex = m_foundPaths.size() - 1; // Set so we go on to fix all others.
                    }
                    found = true;
                }
                item.docID = DocID();
                if (newPathIndex != size_t(-1))
                    item.pathIndex = newPathIndex;
                ++redrawCount;
            }
            else if (found) // Didn't find a match, but did earlier. We are done.
                break;
        }
        if (redrawCount > 0)
            ListView_RedrawItems(hFindResults, firstToRedraw, firstToRedraw + redrawCount -1 );
    }
}

void CFindReplaceDlg::NotifyOnDocumentSave(DocID id, bool /*saveAs*/)
{
    if (!m_open)
        return;
    // When the user saves the document the path can change either by getting
    // a path or changing it. Force the search results to update to reflect that.
    int firstItemIndex = -1;
    int itemCount = 0;
    bool found = false;
    for (int itemIndex = 0; itemIndex < (int)m_searchResults.size(); ++itemIndex)
    {
        auto& item = m_searchResults[itemIndex];
        if (item.docID == id)
        {
            if (!found)
            {
                firstItemIndex = itemIndex;
                found = true;
            }
            ++itemCount;
        }
        else if (found)
            break;
    }
    if (itemCount > 0)
    {
        auto hFindResults = GetDlgItem(*this, IDC_FINDRESULTS);
        ListView_RedrawItems(hFindResults, firstItemIndex, firstItemIndex + itemCount - 1);
    }
}

void CFindReplaceDlg::SelectItem(int item)
{
    HWND hFindResults = GetDlgItem(*this, IDC_FINDRESULTS);
    ListView_SetItemState(hFindResults, item,
        LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
}

void CFindReplaceDlg::OnListItemChanged(LPNMLISTVIEW pListView)
{
    int itemIndex = pListView->iItem;
    HWND hFindResults = GetDlgItem(*this, IDC_FINDRESULTS);
    if (itemIndex == -1)
        return;
    int count = ListView_GetItemCount(hFindResults);
    if ((pListView->uNewState & LVIS_SELECTED) != 0)
    {
        if (count > 0 && itemIndex == count - 1)
            m_trackingOn = true;
        DoListItemAction(itemIndex);
    }
}

void CFindReplaceDlg::OnSearchResultsReady(bool finished)
{
    // If the user was at the end of the list before the items arrived,
    // ensure they remain at the end of the list after the new items have been added.
    HWND hFindResults = GetDlgItem(*this, IDC_FINDRESULTS);
    bool hadSome = (m_searchResults.size() > 0);

    AcceptData();

    ListView_SetItemCountEx(hFindResults, m_searchResults.size(), 0);
    if (m_trackingOn)
    {
        int count = int(m_searchResults.size());
        if (count > 0)
        {
            SelectItem(count - 1);
            ListView_EnsureVisible(hFindResults, count - 1, FALSE);
        }
    }
    UpdateMatchCount(finished);
    if (finished)
        EnableControls(true);
    // The first time data arrives focus on the first item.
    // If no results were found make it easy for the user change the search criteria,
    // but don't drag the focus away though from anywhere important to do so.
    if (!hadSome && m_searchResults.size() > 0)
        FocusOnFirstListItem(true);
    if (finished)
    {
        if (m_searchResults.size() == 0)
        {
            auto focusWnd = GetFocus();
            if (focusWnd == hFindResults)
            {
                if (m_searchType == IDC_FINDFILES)
                    FocusOn(IDC_SEARCHFILES);
                else
                    FocusOn(IDC_SEARCHCOMBO);
            }
        }
    }
}

CCmdFindReplace::CCmdFindReplace(void* obj)
    : ICommand(obj)
{
}

bool CCmdFindReplace::Execute()
{
    FindReplace_FindText(m_pMainWindow);

    return true;
}

void CCmdFindReplace::ScintillaNotify( SCNotification* pScn )
{
    if (pScn->nmhdr.code == SCN_UPDATEUI)
    {
        LRESULT firstline = ScintillaCall(SCI_GETFIRSTVISIBLELINE);
        LRESULT lastline = firstline + ScintillaCall(SCI_LINESONSCREEN);
        long startstylepos = (long)ScintillaCall(SCI_POSITIONFROMLINE, firstline);
        long endstylepos = (long)ScintillaCall(SCI_POSITIONFROMLINE, lastline) + (long)ScintillaCall(SCI_LINELENGTH, lastline);
        if (endstylepos < 0)
            endstylepos = (long)ScintillaCall(SCI_GETLENGTH)-startstylepos;

        int len = endstylepos - startstylepos + 1;
        // Reset indicators.
        ScintillaCall(SCI_SETINDICATORCURRENT, INDIC_FINDTEXT_MARK);
        ScintillaCall(SCI_INDICATORCLEARRANGE, startstylepos, len);
        ScintillaCall(SCI_INDICATORCLEARRANGE, startstylepos, len - 1);

        if (g_sHighlightString.empty())
        {
            g_lastSelText.clear();
            g_searchMarkerCount = 0;
            DocScrollClear(DOCSCROLLTYPE_SEARCHTEXT);
            return;
        }

        Sci_TextToFind FindText = { 0 };
        FindText.chrg.cpMin = startstylepos;
        FindText.chrg.cpMax = endstylepos;
        FindText.lpstrText = g_sHighlightString.c_str();
        while (ScintillaCall(SCI_FINDTEXT, g_searchFlags, (LPARAM)&FindText) >= 0)
        {
            ScintillaCall(SCI_INDICATORFILLRANGE, FindText.chrgText.cpMin, FindText.chrgText.cpMax-FindText.chrgText.cpMin);
            if (FindText.chrg.cpMin >= FindText.chrgText.cpMax)
                break;
            FindText.chrg.cpMin = FindText.chrgText.cpMax;
        }

        if (g_lastSelText.empty() || g_lastSelText.compare(g_sHighlightString) || (g_searchFlags != g_lastSearchFlags))
        {
            DocScrollClear(DOCSCROLLTYPE_SEARCHTEXT);
            g_searchMarkerCount = 0;
            FindText.chrg.cpMin = 0;
            FindText.chrg.cpMax = (long)ScintillaCall(SCI_GETLENGTH);
            FindText.lpstrText = g_sHighlightString.c_str();
            while (ScintillaCall(SCI_FINDTEXT, g_searchFlags, (LPARAM)&FindText) >= 0)
            {
                size_t line = ScintillaCall(SCI_LINEFROMPOSITION, FindText.chrgText.cpMin);
                DocScrollAddLineColor(DOCSCROLLTYPE_SEARCHTEXT, line, RGB(200,200,0));
                ++g_searchMarkerCount;
                if (FindText.chrg.cpMin >= FindText.chrgText.cpMax)
                    break;
                FindText.chrg.cpMin = FindText.chrgText.cpMax;
            }
            g_lastSelText = g_sHighlightString;
            g_lastSearchFlags = g_searchFlags;
            DocScrollUpdate();
        }
        g_lastSelText = g_sHighlightString;
        g_lastSearchFlags = g_searchFlags;
    }
}

void CCmdFindReplace::TabNotify(TBHDR* ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        g_lastSelText.clear();
        if (g_pFindReplaceDlg != nullptr)
        {
            bool followTab = IsDlgButtonChecked(*g_pFindReplaceDlg, IDC_SEARCHFOLDERFOLLOWTAB) == BST_CHECKED;
            if (followTab)
                SetSearchFolderToCurrentDocument();
        }
    }
}

void CCmdFindReplace::OnDocumentClose(DocID id)
{
    if (g_pFindReplaceDlg != nullptr)
        g_pFindReplaceDlg->NotifyOnDocumentClose(id);
}

void CCmdFindReplace::OnDocumentSave(DocID id, bool saveAs)
{
    if (g_pFindReplaceDlg != nullptr)
        g_pFindReplaceDlg->NotifyOnDocumentSave(id, saveAs);
}

void CCmdFindReplace::SetSearchFolderToCurrentDocument()
{
    if (g_pFindReplaceDlg != nullptr)
    {
        HWND hFindReplaceDlg = *g_pFindReplaceDlg;
        if (hFindReplaceDlg != nullptr)
        {
            if (this->HasActiveDocument())
            {
                const auto& doc = this->GetActiveDocument();
                std::wstring folder = CPathUtils::GetParentDirectory(doc.m_path);
                g_pFindReplaceDlg->SetSearchFolder(folder);
            }
        }
    }
}

bool CCmdFindNext::Execute()
{
    if (g_findString.empty())
    {
        FlashWindow(GetHwnd());
        return true;
    }
    Sci_TextToFind ttf = {0};
    ttf.chrg.cpMin = (long)ScintillaCall(SCI_GETCURRENTPOS);
    ttf.chrg.cpMax = (long)ScintillaCall(SCI_GETLENGTH);
    ttf.lpstrText = g_findString.c_str();
    sptr_t findRet = ScintillaCall(SCI_FINDTEXT, g_searchFlags, (sptr_t)&ttf);
    if (findRet == -1)
    {
        // Retry from the start of the doc.
        ttf.chrg.cpMax = ttf.chrg.cpMin;
        ttf.chrg.cpMin = 0;
        findRet = ScintillaCall(SCI_FINDTEXT, g_searchFlags, (sptr_t)&ttf);
    }
    if (findRet >= 0)
        Center(ttf.chrgText.cpMin, ttf.chrgText.cpMax);
    else
        FlashWindow(GetHwnd());
    return true;
}

bool CCmdFindPrev::Execute()
{
    if (g_findString.empty())
    {
        FlashWindow(GetHwnd());
        return true;
    }

    Sci_TextToFind ttf = {0};
    ttf.chrg.cpMin = (long)ScintillaCall(SCI_GETCURRENTPOS);
    if (ttf.chrg.cpMin > 0)
        ttf.chrg.cpMin--;
    ttf.chrg.cpMax = 0;
    ttf.lpstrText = g_findString.c_str();
    sptr_t findRet = ScintillaCall(SCI_FINDTEXT, g_searchFlags, (sptr_t)&ttf);
    if (findRet == -1)
    {
        // Retry from the end of the doc.
        ttf.chrg.cpMax = ttf.chrg.cpMin+1;
        ttf.chrg.cpMin = (long)ScintillaCall(SCI_GETLENGTH);
        findRet = ScintillaCall(SCI_FINDTEXT, g_searchFlags, (sptr_t)&ttf);
    }
    if (findRet >= 0)
        Center(ttf.chrgText.cpMin, ttf.chrgText.cpMax);
    else
        FlashWindow(GetHwnd());
    return true;
}

bool CCmdFindSelectedNext::Execute()
{
    g_sHighlightString.clear();
    int selTextLen = (int)ScintillaCall(SCI_GETSELTEXT);
    if (selTextLen <= 1) // Includes zero terminator so 1 means none.
    {
        DocScrollUpdate();
        return false;
    }
    std::string seltext = GetSelectedText();
    if (seltext.empty())
    {
        DocScrollUpdate();
        return false;
    }
    g_findString = seltext;
    g_sHighlightString = g_findString;

    Sci_TextToFind ttf = {0};
    ttf.chrg.cpMin = (long)ScintillaCall(SCI_GETCURRENTPOS);
    ttf.chrg.cpMax = (long)ScintillaCall(SCI_GETLENGTH);
    ttf.lpstrText = g_findString.c_str();
    sptr_t findRet = ScintillaCall(SCI_FINDTEXT, g_searchFlags, (sptr_t)&ttf);
    if (findRet == -1)
    {
        // Retry from the start of the doc.
        ttf.chrg.cpMax = ttf.chrg.cpMin;
        ttf.chrg.cpMin = 0;
        findRet = ScintillaCall(SCI_FINDTEXT, g_searchFlags, (sptr_t)&ttf);
    }
    if (findRet >= 0)
        Center(ttf.chrgText.cpMin, ttf.chrgText.cpMax);
    else
    {
        FlashWindow(GetHwnd());
    }
    DocScrollUpdate();
    return true;
}

bool CCmdFindSelectedPrev::Execute()
{
    g_sHighlightString.clear();
    int selTextLen = (int)ScintillaCall(SCI_GETSELTEXT);
    if (selTextLen <= 1) // Includes zero terminator so 1 means none.
    {
        DocScrollUpdate();
        return false;
    }

    std::string seltext = GetSelectedText();
    if (seltext.empty())
    {
        DocScrollUpdate();
        return false;
    }
    g_findString = seltext;
    g_sHighlightString = g_findString;

    Sci_TextToFind ttf = {0};
    ttf.chrg.cpMin = (long)ScintillaCall(SCI_GETCURRENTPOS);
    if (ttf.chrg.cpMin > 0)
        ttf.chrg.cpMin--;
    ttf.chrg.cpMax = 0;
    ttf.lpstrText = g_findString.c_str();
    sptr_t findRet = ScintillaCall(SCI_FINDTEXT, g_searchFlags, (sptr_t)&ttf);
    if (findRet == -1)
    {
        // retry from the end of the doc
        ttf.chrg.cpMax = ttf.chrg.cpMin+1;
        ttf.chrg.cpMin = (long)ScintillaCall(SCI_GETLENGTH);
        findRet = ScintillaCall(SCI_FINDTEXT, g_searchFlags, (sptr_t)&ttf);
    }
    if (findRet >= 0)
        Center(ttf.chrgText.cpMin, ttf.chrgText.cpMax);
    else
        FlashWindow(GetHwnd());
    DocScrollUpdate();
    return true;
}

bool CCmdFindFile::Execute()
{
    FindReplace_FindFile(m_pMainWindow, L"");

    return true;
}

void FindReplace_Finish()
{
    g_pFindReplaceDlg.reset();
}

void FindReplace_FindText(void* mainWnd)
{
    if (!g_pFindReplaceDlg)
        g_pFindReplaceDlg = std::make_unique<CFindReplaceDlg>(mainWnd);
    g_pFindReplaceDlg->FindText();
}

void FindReplace_FindFile(void* mainWnd, const std::wstring& fileName)
{
    if (!g_pFindReplaceDlg)
        g_pFindReplaceDlg = std::make_unique<CFindReplaceDlg>(mainWnd);
    g_pFindReplaceDlg->FindFile(fileName);
}

void FindReplace_FindFunction(void* mainWnd, const std::wstring& functionName)
{
    if (!g_pFindReplaceDlg)
        g_pFindReplaceDlg = std::make_unique<CFindReplaceDlg>(mainWnd);
    g_pFindReplaceDlg->FindFunction(functionName);
}

