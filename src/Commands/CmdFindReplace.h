// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2019-2021 - Stefan Kueng
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
#include "InfoRtfDialog.h"

#include <chrono>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <string>

class CSearchResult
{
public:
    DocID        docID;
    std::wstring lineText;
    sptr_t       pathIndex      = -1;
    sptr_t       posBegin       = 0;
    sptr_t       posEnd         = 0;
    sptr_t       line           = 0;
    sptr_t       posInLineStart = 0;
    sptr_t       posInLineEnd   = 0;

    inline bool  hasPath() const
    {
        return pathIndex != -1;
    }
};

enum class ResultsType
{
    Unknown,
    MatchedTerms,
    Functions,
    FileNames,
    FirstLines
};

enum class FindMode
{
    FindText,
    FindFile,
    FindFunction
};

void findReplaceFinish();
void findReplaceFindText(void* mainWnd);
void findReplaceFindFile(void* mainWnd, const std::wstring& fileName);
void findReplaceFindFunction(void* mainWnd, const std::wstring& functionName);

class CFindReplaceDlg : public CBPBaseDialog
    , public ICommand
{
    // vector or deque should work here. Usage pattern suggests deque
    // might be better but simple tests didn't reveal much difference.
    using SearchResults = std::deque<CSearchResult>;
    using SearchPaths   = std::deque<std::wstring>;

public:
    CFindReplaceDlg(void* obj);

    void FindText();
    void FindFunction(const std::wstring& functionToFind);
    void FindFile(const std::wstring& fileToFind);

    void SetSearchFolder(const std::wstring& folder);
    void NotifyOnDocumentClose(DocID id);
    void NotifyOnDocumentSave(DocID id, bool saveAs);

protected: // override
    bool             Execute() override { return true; }
    UINT             GetCmdId() override { return 0; }
    void             OnClose() override;
    LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    enum class AlertMode
    {
        None,
        Flash
    };

protected:
    LRESULT                 DoCommand(int id, int msg);
    void                    SetInfoText(UINT resid, AlertMode alertMode = AlertMode::Flash);
    bool                    DoSearch(bool replaceMode = false);
    void                    DoSearchAll(int id);
    void                    DoFind();
    void                    DoFindPrevious();
    void                    DoReplace(int id);

    void                    SearchDocument(CScintillaWnd& searchWnd, DocID docID, const CDocument& doc,
                                           const std::string& searchFor, Scintilla::FindOption searchFlags, unsigned int exSearchFlags,
                                           SearchResults& searchResults,
                                           SearchPaths&   foundPaths);

    int                     ReplaceDocument(CDocument& doc, const std::string& sFindString,
                                            const std::string& sReplaceString, Scintilla::FindOption searchFlags) const;

    void                    SearchThread(int id, const std::wstring& searchPath, const std::string& searchFor,
                                         Scintilla::FindOption flags, unsigned int exSearchFlags, const std::vector<std::wstring>& filesToFind);

    void                    SortResults();
    void                    CheckRegex(bool flash);
    void                    ShowResults(bool bShow);
    void                    InitResultsList();
    LRESULT                 DoListNotify(LPNMITEMACTIVATE lpNMItemActivate);
    LRESULT                 DrawListItemWithMatches(NMLVCUSTOMDRAW* pLVCD);
    RECT                    DrawListColumnBackground(NMLVCUSTOMDRAW* pLVCD);
    LRESULT                 DrawListItem(NMLVCUSTOMDRAW* pLVCD);
    LRESULT                 GetListItemDispInfo(NMLVDISPINFO* pDispInfo) const;
    void                    HandleButtonDropDown(const NMBCDROPDOWN* pDropDown);

    bool                    IsMatchingFile(const std::wstring& path, const std::vector<std::wstring>& filesToFind) const;

    bool                    IsExcludedFile(const std::wstring& path) const;
    bool                    IsExcludedFolder(const std::wstring& path) const;

    void                    EnableControls(bool bEnable);
    void                    SearchStringNotFound();
    std::wstring            GetCurrentDocumentFolder() const;

    void                    FocusOn(int id);
    void                    SetDefaultButton(int id, bool savePrevious = false);
    void                    RestorePreviousDefaultButton();
    int                     GetDefaultButton() const;
    void                    Clear(int id);

    void                    LoadSearchStrings();
    void                    SaveSearchStrings() const;
    void                    UpdateSearchStrings(const std::wstring& item);

    void                    LoadReplaceStrings();
    void                    SaveReplaceStrings() const;
    void                    UpdateReplaceStrings(const std::wstring& item);

    void                    LoadSearchFolderStrings();
    void                    SaveSearchFolderStrings() const;
    void                    UpdateSearchFolderStrings(const std::wstring& target);

    void                    LoadSearchFileStrings();
    void                    SaveSearchFileStrings() const;
    void                    UpdateSearchFilesStrings(const std::wstring& target);

    std::wstring            OfferFileSuggestion(const std::wstring& searchFolder,
                                                bool                searchSubFolders,
                                                const std::wstring& currentValue) const;

    void                    AcceptData();
    void                    NewData(std::chrono::steady_clock::time_point& timeOfLastProgressUpdate, bool finished);
    void                    UpdateMatchCount(bool finished = true);
    void                    DoListItemAction(int itemIndex);
    void                    DoInitDialog(HWND hwndDlg);
    void                    DoClose();
    void                    InitSizing();
    void                    OnSearchResultsReady(bool finished);
    void                    FocusOnFirstListItem(bool keepAnyExistingSelection = false);
    Scintilla::FindOption   GetScintillaOptions() const;
    void                    CheckSearchOptions();
    void                    CheckSearchFolder();
    void                    SaveSettings();
    void                    SelectItem(int item);
    void                    OnListItemChanged(LPNMLISTVIEW pListView);
    void                    LetUserSelectSearchFolder();

    bool                    EnableListEndTracking(int listID, bool enable);

    void                    SetTheme(bool bDark);

    static LRESULT CALLBACK ListViewSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                                 UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    static LRESULT CALLBACK EditSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                             UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

private:
    CDlgResizer                     m_resizer;
    bool                            m_freeResize = false;
    CScintillaWnd                   m_searchWnd;
    SearchResults                   m_searchResults;
    SearchPaths                     m_foundPaths;
    SearchResults                   m_pendingSearchResults;
    SearchPaths                     m_pendingFoundPaths;
    bool                            m_dataAccepted = false;
    bool                            m_dataReady    = false;
    std::mutex                      m_waitingDataMutex;
    std::condition_variable         m_dataExchangeCondition;
    volatile bool                   m_bStop                  = false;
    volatile LONG                   m_threadsRunning         = false;
    int                             m_searchType             = 0;
    ResultsType                     m_resultsType            = ResultsType::Unknown;
    bool                            m_trackingOn             = true;
    int                             m_previousDefaultButton  = 0;
    int                             m_resultsWidthBefore     = 0;
    int                             m_maxSearchStrings       = 0;
    int                             m_maxReplaceStrings      = 0;
    int                             m_maxSearchFolderStrings = 0;
    int                             m_maxSearchFileStrings   = 0;
    size_t                          m_maxSearchResults       = 10000;
    SIZE                            m_originalSize           = {0};
    bool                            m_open                   = false;
    volatile size_t                 m_foundSize              = 0;
    int                             m_themeCallbackId        = 0;
    bool                            m_resultsListInitialized = false;
    std::unique_ptr<CInfoRtfDialog> m_regexHelpDialog        = nullptr;

    // Some types usually best avoided while searching.
    // The user can explicitly override these if they want them though.
    // REVIEW: consider making this list configurable?
    // NOTE: Keep filenames lower case as code assumes that.
    const std::vector<std::wstring> m_excludedExtensions     = {
        // Binary types.
        L"exe", L"dll", L"obj", L"lib", L"ilk", L"iobj", L"ipdb", L"idb",
        L"pch", L"ipch", L"sdf", L"pdb", L"res", L"sdf", L"db", L"iso",
        // Common temporary VC project types.
        L"tlog",
        // svn types.
        L"svn-base",
        // Image types.
        L"bmp", L"png", L"jpg", L"ico", L"cur"};
    const std::vector<std::wstring> m_excludedFolders = {
        L".svn", L".git"};
};

class CCmdFindReplace : public ICommand
{
public:
    CCmdFindReplace(void* obj);
    ~CCmdFindReplace() override = default;

    bool    Execute() override;

    UINT    GetCmdId() override { return cmdFindReplace; }

    HRESULT IUICommandHandlerUpdateProperty(
        REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/,
        PROPVARIANT*   pPropVarNewValue) override
    {
        if (UI_PKEY_BooleanValue == key)
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue,
                                             Scintilla().WrapMode() > Scintilla::Wrap::None, pPropVarNewValue);
        return E_NOTIMPL;
    }

    void ScintillaNotify(SCNotification* pScn) override;

    void TabNotify(TBHDR* ptbHdr) override;

    void OnDocumentClose(DocID id) override;
    void OnDocumentSave(DocID id, bool saveAs) override;

private:
    void SetSearchFolderToCurrentDocument() const;
};

class CCmdFindNext : public ICommand
{
public:
    CCmdFindNext(void* obj)
        : ICommand(obj)
    {
    }

    bool Execute() override;

    UINT GetCmdId() override { return cmdFindNext; }
};

class CCmdFindPrev : public ICommand
{
public:
    CCmdFindPrev(void* obj)
        : ICommand(obj)
    {
    }

    bool Execute() override;

    UINT GetCmdId() override { return cmdFindPrev; }
};

class CCmdFindSelectedNext : public ICommand
{
public:
    CCmdFindSelectedNext(void* obj)
        : ICommand(obj)
    {
    }

    bool Execute() override;

    UINT GetCmdId() override { return cmdFindSelectedNext; }
};

class CCmdFindSelectedPrev : public ICommand
{
public:
    CCmdFindSelectedPrev(void* obj)
        : ICommand(obj)
    {
    }

    bool Execute() override;

    UINT GetCmdId() override { return cmdFindSelectedPrev; }
};

class CCmdFindFile : public ICommand
{
public:
    CCmdFindFile(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdFindFile() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdFindFile; }
};
