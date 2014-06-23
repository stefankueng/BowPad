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

/*

First basic implementation. WIP.

OVERVIEW

When the user clicks on the Corresponding File button, BP responds by displaying
a menu list of (usually relative) filenames.

The list displayed comes from any #include statements in the current document.
The include statements have the form #include "x" or #include <x>.
The former are deemed "user" include files; the later are "system" include files.

Clicking on a relative filename causes BP to open whatever physical file BP thinks
corresponds to it.

Since BP has no project system, BP may not be able to find a corresponding file at all
or it might not be the right one. Usually the user will realise when this happens though.

BP resolves user includes files by searching for them in the folder of the active document.

BP searches for system include files by following a search path definded in
it's settings file %appdata%\bowpad\settings.

The confguration for this is shown below, but is optional:

[cpp]
toolchain=msvc
[msvc]
systemincludepath=c:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\include;c:\Program Files (x86)\Windows Kits\8.1\Include\um
[clang]
systemincludepath=c:\libcxx\include
[mingw]
systemincludepath=c:\mingw\x86_64-w64-mingw32\include\c++

The configuration structure allows multiple "toolchains" to be defined in BP though only
one or none is active at any time.
The "toolchain" variable determines what the active toolchain is and accordingly then which search path is used.
If it is empty no toolchain is used and system files must be found manually.

If BP does not resolve a file to an existing file name on the disk ... is shown next to the file.
This gives the user an indication that file cannot be found. Clicking on an item shown as
... results in a File Open Dialog being shown to the user so they can find it themslves.

Since BP can fill in the filename for the user and assume the file type opened in this manner
is a C/C++ file, this is the prime service this mechanism has over the regular file open method
from the File Menu that the user would otherwise have to use. This mechanism also means the user
doesn't have to go to the top of the file to find the list of possibile includes they might want.

Landing in later patches:

TODO: Tighten the corresponding file list. Sort etc.
Introduce the ability to "switch between header and .cpp file" in one key/click. This will come soon.

TOOD! Fix the override mechanism. Shift doesn't get detected well (at least on my keyboard).

TODO! Restructure the menu inf oobject to be a bit more generic / flexible.

TODO: Add a file file find dialog that remember previous folders and helps more with finding files etc.

TODO: Fix outstanding bug:
1. Press Ctrn-N to create an empty document.
2. Type #include <cstdio>
3. Notice include button is disabled - as it should be.
4. From the Lexer ribbon menu, change the language to C++.
5. Notice the include button is still disabled - it shouldn't be.
6. Type something. Notice include is still disabled.
7. Press enter. Notice include is no longer disabled.

TODO: Convert a few hard coded strings to resource file entries.

TODO: Remember why ComboBox_ShowDropDown might be useful somewhere and test if it is.

TODO! Remember paths where include files have been opened previously.
This could be under the search button as a split, the combo list may not be viable for this purpose.

TODO! Change the logic so the menu rebuilds each time the button is clicked, rather than detecting when
it should be rebuilt.

TODO! The include feature is quite a C++ specific for now.
The feature could be expanded on more for other languages / file types.
*/

#include "stdafx.h"
#include "CmdHeaderSource.h"
#include "PropertySet.h"
#include "BowPad.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "LexStyles.h"
#include "PathUtils.h"
#include "AppUtils.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include "PreserveChdir.h"
#include "Resource.h"
#include "DirFileEnum.h"

// TODO! Move appropriate functions to utilities.
namespace {

// Maximum number of lines to scan for include statements.
// NOTE: This could be a configuration item but have decided that's not neccessary.
const int MAX_INCLUDE_SEARCH_LINES = 100;

const int CORRESPONDING_FILES_CATEGORY = 1;
const int USER_INCLUDE_CATEGORY = 2;
const int SYSTEM_INCLUDE_CATEGORY = 3;

// TODO! Candidate for CAppUtils::
HRESULT AddCategory(const IUICollectionPtr& coll, int catId, int catNameResId)
{
    // TOOD! Use a RAII type to manage life time of the category objects.
    // Note Categories appear in the list in the order of their creation.
    CPropertySet* cat;
    HRESULT hr = CPropertySet::CreateInstance(&cat);
    if (CAppUtils::FailedShowMessage(hr))
        return hr;

    ResString catName(hRes, catNameResId);
    cat->InitializeCategoryProperties(catName, catId);
    coll->Add(cat);
    cat->Release();
    return S_OK;
}

// File Selection API. Should be utility and shared with other code.
// Will leave unshared right now while this include/corresponding file
// feature functionality is settles down.

bool ShowFileSelectionDialog(
    HWND hWndParent,
    const std::wstring& defaultFilename,
    const std::wstring& defaultFolder,
    std::vector<std::wstring>& filesChosen,
    bool multiple)
{
    PreserveChdir keepCWD;
    filesChosen.clear();

    IFileOpenDialogPtr pfd = NULL;

    HRESULT hr = pfd.CreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER);
    if (CAppUtils::FailedShowMessage(hr))
        return false;

    // Set the dialog options, if it fails don't continue as we don't
    // know what options we're blending. Shouldn't ever fail anyway.
    DWORD dwOptions;
    hr = pfd->GetOptions(&dwOptions);
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    // If we can't set our options, we have no idea what's happening
    // so don't continue.
    dwOptions |= FOS_FILEMUSTEXIST | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
    if (multiple)
        dwOptions |= FOS_ALLOWMULTISELECT;
    hr = pfd->SetOptions(dwOptions);
    if (CAppUtils::FailedShowMessage(hr))
        return false;

    if (!defaultFilename.empty())
        pfd->SetFileName(defaultFilename.c_str());

    // Set the standard title.
    ResString rTitle(hRes, IDS_APP_TITLE);
    pfd->SetTitle(rTitle);

    if (!defaultFolder.empty())
    {
        IShellItemPtr psiDefFolder = NULL;
        hr = SHCreateItemFromParsingName(defaultFolder.c_str(), NULL, IID_PPV_ARGS(&psiDefFolder));
        if (!CAppUtils::FailedShowMessage(hr))
        {
            // Assume if we can't set the folder we can at least continue
            // and the user might be able to set another folder or something.
            hr = pfd->SetFolder(psiDefFolder);
            CAppUtils::FailedShowMessage(hr);
        }
    }

    // Show the open file dialog, not much we can do if that doesn't work.
    hr = pfd->Show(hWndParent);
    if(hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) // Expected error
        return false;
    else
    {
        if (CAppUtils::FailedShowMessage(hr))
            return false;
    }

    IShellItemArrayPtr psiaResults;
    hr = pfd->GetResults(&psiaResults);
    if (CAppUtils::FailedShowMessage(hr))
        return false;

    // Fetch the (possibly multiple) results. Some may possibly fail
    // but get as many as we can. We don't report partial failure.
    // We could make it an all or nothing deal but we have chosen not to.

    DWORD count = 0;
    hr = psiaResults->GetCount(&count);
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    for (DWORD i = 0; i < count; ++i)
    {
        IShellItemPtr psiResult = NULL;
        hr = psiaResults->GetItemAt(i, &psiResult);
        if (!CAppUtils::FailedShowMessage(hr))
        {
            PWSTR pszPath = NULL;
            hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
            if (!CAppUtils::FailedShowMessage(hr))
                filesChosen.push_back(pszPath);
        }
        if (multiple)
            break;
    }

    return true;
}

bool ShowSingleFileSelectionDialog(
    HWND hWndParent,
    const std::wstring& defaultFilename,
    const std::wstring& defaultFolder,
    std::wstring& fileChosen)
{
    fileChosen.clear();
    std::vector<std::wstring> filesChosen;
    if (ShowFileSelectionDialog(hWndParent, defaultFilename, defaultFolder, filesChosen, false))
    {
        fileChosen = filesChosen[0];
        return true;
    }
    return false;
}

#if 0 // TOOD. Open Other File can use this overload. Not used just yet.
bool ShowMultipleFileSelectionDialog(
    HWND hWndParent,
    const std::wstring& defaultFilename,
    const std::wstring& defaultFolder,
    std::vector<std::wstring>& filesChosen)
{
    return ShowFileSelectionDialog(hWndParent, defaultFilename, defaultFolder, filesChosen, true);
}
#endif

// Find all derivatives of a file (the current document) in it's folder.
// i.e. match files that share the same filename, but not extension.
// Directory names are not included.
// Ignore any files that have certain extensions.
// e.g. Given a name like c:\test\test.cpp,
// return {c:\test\test.h and test.h} but ignore c:\test\test.exe.
// Filenames that have no extension are not considered (for not particular reason).
void GetFilesWithSameName(
    const std::wstring& targetPath,
    std::vector<std::wstring>& matchingfiles
)
{
    std::wstring targetExt = CPathUtils::GetFileExtension(targetPath);
    if (targetExt.empty())
        return;

    std::vector<std::wstring> ignoredExts;
    stringtok(ignoredExts, CIniSettings::Instance().GetString(L"HeaderSource", L"IgnoredExts", L"exe*obj*dll*ilk*lib*ncb*ipch*bml*pch*res*pdb*aps"), true, L"*");

    std::wstring targetFolder = CPathUtils::GetParentDirectory(targetPath);
    std::wstring targetFileName = CPathUtils::GetFileName(targetPath);
    std::wstring targetFileNameWithoutExt = CPathUtils::GetFileNameWithoutExtension(targetPath);
    std::wstring matchingPath;
    std::wstring matchingFileName;
    std::wstring matchingFileNameWithoutExt;
    std::wstring matchingExt;

    CDirFileEnum enumerator(targetFolder);
    bool bIsDir = false;
    while (enumerator.NextFile(matchingPath, &bIsDir, false))
    {
        if (bIsDir)
            continue;
        // Don't match ourself.
        if (CPathUtils::PathCompare(matchingPath, targetPath) == 0)
            continue;
        // Don't match files without extensions. REVIEW: Why not?
        matchingExt = CPathUtils::GetFileExtension(matchingPath);
        if (matchingExt.empty())
            continue;
        // TODO! Consider if corresponding file should be based on the first "." in the
        // the name rather than last "." etc.
        // So test.cs can be linked with test.cs.html or whatever.
        matchingFileNameWithoutExt = CPathUtils::GetFileNameWithoutExtension(matchingPath);
        if (CPathUtils::PathCompare(targetFileNameWithoutExt, matchingFileNameWithoutExt) == 0)
        {
            bool useIt = true;
            for (const auto& ignoredExt : ignoredExts)
            {
                if (CPathUtils::PathCompare(matchingExt, ignoredExt) == 0)
                {
                    useIt = false;
                    break;
                }
            }
            if (useIt)
                matchingfiles.push_back(matchingPath);
        }
    }
}

bool FindFile( const std::wstring& fileToFind, const std::vector<std::wstring>& foldersToSearch, std::wstring& foundPath )
{
    foundPath.clear();
    if (::PathIsRelative(fileToFind.c_str()))
    {
        std::wstring possiblePath;
        for (const auto& folderToSearch : foldersToSearch)
        {
            possiblePath = CPathUtils::Append(folderToSearch, fileToFind);
            if (_waccess(possiblePath.c_str(), 0) == 0)
            {
                foundPath = possiblePath;
                return true;
            }
        }
    }
    else
    {
        if (_waccess(fileToFind.c_str(), 0) == 0)
        {
            foundPath = fileToFind;
            return true;
        }
    }
    return false;
}

// Extract the filename from the #include <x> or #include "y" elements.
bool ParseInclude(const std::wstring& raw, std::wstring& filename, IncludeType& incType)
{
    filename.clear();
    incType = IncludeType::Unknown;

    size_t len = raw.length();
    size_t first, last;

    if (len <= 0)
        return false;

    // The regex is supposed to do the heavy lifting, matching wise,
    // this is more about filename extractionand trying
    // not to cash given something vaguely sensible than matching/validation.

    // Match the '#' of include. Basic sanity check.
    if (raw[0] != L'#')
    {
        APPVERIFY(false);
        return false;
    }
    // Looking for a filename in quotes, hopefully follows after #include.
    first = raw.find(L'\"');
    if (first != std::wstring::npos)
    {
        last = raw.find_last_of(L'\"');
        if (last != first)
        {
            incType = IncludeType::User;
            len = last - (first + 1);
            filename = raw.substr(first+1,len);
            return (len> 0);
        }
    }
    else
    {
        // Looking for a filename in angle brackets, hopefully after an include.
        first = raw.find(L'<');
        if (first != std::wstring::npos)
        {
            incType = IncludeType::System;
            last = raw.find_last_of(L'>');
            if (last != first)
            {
                len = last - (first + 1);
                filename = raw.substr(first+1,len);
                return (len > 0);
            }
        }
    }

    return false;
}

bool FindNext(CScintillaWnd& edit, const Scintilla::Sci_TextToFind& ttf, int flags,
    std::string& found_text, size_t* line_no)
{
    found_text.clear();
    *line_no = 0;

    auto findRet = edit.Call(SCI_FINDTEXT, flags, (sptr_t)&ttf);
    if (findRet < 0)
        return false;
    size_t len = ttf.chrgText.cpMax - ttf.chrgText.cpMin;
    found_text.resize(len+1);
    Scintilla::Sci_TextRange r{};
    r.chrg.cpMin = ttf.chrgText.cpMin;
    r.chrg.cpMax = ttf.chrgText.cpMax;
    r.lpstrText = &found_text[0];
    edit.Call(SCI_GETTEXTRANGE, 0, reinterpret_cast<sptr_t>(&r));
    found_text.resize(len);
    *line_no = edit.Call(SCI_LINEFROMPOSITION, r.chrg.cpMin);
    return true;
}

void AttachDocument(CScintillaWnd& edit, CDocument& doc)
{
    edit.Call(SCI_SETDOCPOINTER, 0, 0);
    edit.Call(SCI_SETSTATUS, SC_STATUS_OK);
    edit.Call(SCI_CLEARALL);
    edit.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
    edit.Call(SCI_SETCODEPAGE, CP_UTF8);
}

bool GetIncludes(const CDocument& doc, CScintillaWnd& edit, std::vector<IncludeInfo>& includes)
{
    includes.clear();

    std::string lang = CUnicodeUtils::StdGetUTF8(doc.m_language);
    if (lang != "C/C++")
        return false;

    // It's compromise of speed vs accurracy. Only scan the first N lines
    // for #include statements. They usually only exist at the start of the file.
    // We could get more creative and improve on this but this seems fine for now.
    long length = (long) edit.Call(SCI_GETLINEENDPOSITION, WPARAM(MAX_INCLUDE_SEARCH_LINES-1)); // 0 based.
    // NOTE: If we want whole file scanned use:
    // long length = (long) edit.Call(SCI_GETLENGTH);

    Scintilla::Sci_TextToFind ttf{}; // Zero initialize.
    ttf.chrg.cpMax = length;
    // NOTE: Intentionaly hard coded for now. See overview for reasons.
    // Match an include statement: #include <x> or #include "x" at start of line.
    ttf.lpstrText = const_cast<char*>("^\\#include\\s+((\\\"[^\\\"]+\\\")|(<[^>]+>))");

    std::string text_found;
    size_t line_no;
    std::wstring name;

    for (;;)
    {
        ttf.chrgText.cpMin = 0;
        ttf.chrgText.cpMax = 0;

        // Note:
        // RegEx searches currently take a long time in debug mode.
        // SCI_GETLINE(int line, char *text) and SCI_GETLINELENGTH is an
        // untried alternative that might be faster it ever becomes neccessary.

        // Use match case because the include keyword is case sensitive and the
        // rest of the regular expression is symbols.
        if (!FindNext(edit, ttf, SCFIND_REGEXP | SCFIND_MATCHCASE, text_found, &line_no))
            break;
        ttf.chrg.cpMin = ttf.chrgText.cpMax + 1;

        std::wstring raw = CUnicodeUtils::StdGetUnicode(text_found);
        std::wstring filename;
        IncludeType includeType;
        bool parsed = ParseInclude(raw, filename, includeType);
        if (parsed)
        {
            CPathUtils::NormalizeFolderSeparators(filename);
            includes.push_back(IncludeInfo(line_no, std::move(filename), includeType));
        }
    }
    // Sort the list putting user includes before system includes. Remove duplcicates.
    // A duplicate is an include with the same name and type as another in the list.
    std::sort(includes.begin(), includes.end(), [](const IncludeInfo& a, const IncludeInfo& b) -> bool
    {
        // Put user before system includes.
        if (a.includeType == IncludeType::System && b.includeType == IncludeType::User)
            return false;
        // Within the same type, sort by name.
        return CPathUtils::PathCompare(a.filename, b.filename) < 0;
    });
    includes.erase( std::unique( includes.begin(), includes.end() ), includes.end() );
    return true;
}

}; // anon namespace

IncludeInfo::IncludeInfo(size_t line, std::wstring&& filename, IncludeType includeType)
    : line(line), filename(std::move(filename)), includeType(includeType)
{
}

bool IncludeInfo::operator == (const IncludeInfo& other) const
{
    return other.includeType == this->includeType &&
        CPathUtils::PathCompare(other.filename.c_str(), this->filename.c_str()) == 0;
}

bool CCmdHeaderSource::UserFindFile(
    HWND hwndParent, const std::wstring& filename, const std::wstring& defaultFolder, std::wstring& selectedFilename)
{
    std::wstring name = CPathUtils::GetFileName(filename);
    if (!ShowSingleFileSelectionDialog(hwndParent, name, defaultFolder, selectedFilename))
        return false;
    return true;
}

CCmdHeaderSource::CCmdHeaderSource(void * obj)
    : ICommand(obj)
    , m_edit(hRes)
    , m_bStale(true)

{
    m_edit.InitScratch(hRes);
    // Because we don't know if this file type supports includes.
    InvalidateIncludes();

    //TODO! Review if this is required.
    HRESULT hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    CAppUtils::FailedShowMessage(hr);
}

void CCmdHeaderSource::InvalidateIncludesSource()
{
    HRESULT hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
    CAppUtils::FailedShowMessage(hr);
}

void CCmdHeaderSource::InvalidateIncludesEnabled()
{
    // Note SetUICommandProperty(UI_PKEY_Enabled,en) can be useful but probably
    // isn't what we want; and it fails when the ribbon is hidden anyway.
    HRESULT hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY,&UI_PKEY_Enabled);
    CAppUtils::FailedShowMessage(hr);
}

void CCmdHeaderSource::InvalidateIncludes()
{
    m_bStale = true;
    m_menuInfo.clear();
    InvalidateIncludesSource();
    InvalidateIncludesEnabled();
}

HRESULT CCmdHeaderSource::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue)
{
    HRESULT hr;

    if(key == UI_PKEY_Categories)
    {
        IUICollectionPtr coll;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&coll));
        if (FAILED(hr))
            return hr;

        hr = AddCategory(coll, CORRESPONDING_FILES_CATEGORY, IDS_CORRESPONDING_FILES);
        if (FAILED(hr))
            return hr;
        AddCategory(coll, USER_INCLUDE_CATEGORY, IDS_USER_INCLUDES);
        if (FAILED(hr))
            return hr;
        AddCategory(coll, SYSTEM_INCLUDE_CATEGORY, IDS_SYSTEM_INCLUDES);
        if (FAILED(hr))
            return hr;

        return S_OK;
    }

    if (key == UI_PKEY_ItemsSource)
    {
        // We should have detected all the re-/population points and marked
        // the data stale. If the data isn't marked stale by here it suggests
        // we've missed a point where we should have invalidated the data.
        ASSERT(m_bStale);

        IUICollectionPtr collection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&collection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;

        // The list will retain whatever from last time so clear it.
        collection->Clear();
        // We will rebuild this information so clear it.
        m_menuInfo.clear();

         // We need to know have an active document to continue.
        int docId = GetDocIdOfCurrentTab();
        if (docId < 0)
            return E_FAIL;
        if (!HasActiveDocument())
            return E_FAIL;
        CDocument doc = GetDocumentFromID(docId);
        AttachDocument(m_edit, doc);

        PopulateMenu(doc, collection);

        return S_OK;
    }

    if (key == UI_PKEY_SelectedItem)
    {
        return UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)UI_COLLECTION_INVALIDINDEX, ppropvarNewValue);
    }

    if (key == UI_PKEY_Enabled)
    {
        bool enabled = IsServiceAvailable();

        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, enabled, ppropvarNewValue);
    }

    return E_NOTIMPL;
}

bool CCmdHeaderSource::IsServiceAvailable()
{
    bool available = false;
    if (HasActiveDocument())
        available = true;
    return available;
}

// Populate the dropdown with the details.
bool CCmdHeaderSource::PopulateMenu(const CDocument& doc, IUICollectionPtr& collection)
{
    // If the user can't find the file they want, offer a shortcut to find it themselves
    // and open it. Using this mechanism has two advantages over regular file open:
    // 1. The user doesn't have to move the cursor off to the file open menu to find the file.
    // 2. The file opened can be assumed to be an include file if it has no file extension.

    IncludeInfo inc(0, std::wstring(), IncludeType::Unknown);
    IncludeMenuItemInfo item(std::move(inc), std::wstring());
    m_menuInfo.push_back(std::move(item));
    HRESULT hr = CAppUtils::AddResStringItem(collection, IDS_OPEN_OTHER_INCLUDE, CORRESPONDING_FILES_CATEGORY);
    CAppUtils::FailedShowMessage(hr);

    std::vector<std::wstring> matchingFiles;

    std::wstring targetPath = doc.m_path;
    GetFilesWithSameName(targetPath, matchingFiles);

    std::sort(matchingFiles.begin(), matchingFiles.end(),
        [](const std::wstring& a, const std::wstring& b) -> bool
    {
        return CPathUtils::PathCompare(a, b) < 0;
    });

    std::wstring matchingFileName;
    for (size_t m = 0; m < matchingFiles.size(); ++m)
    {
        matchingFileName = CPathUtils::GetFileName(matchingFiles[m]);
        IncludeInfo inc(0, std::wstring(), IncludeType::Unknown);
        IncludeMenuItemInfo item(std::move(inc), std::move(matchingFiles[m]));
        m_menuInfo.push_back(std::move(item));
        HRESULT hr = CAppUtils::AddStringItem(collection, matchingFileName.c_str(), CORRESPONDING_FILES_CATEGORY);
        if (CAppUtils::FailedShowMessage(hr))
        {
            m_menuInfo.pop_back();
            return false;
        }
    }

    std::vector<IncludeInfo> includes;
    GetIncludes(doc, m_edit, includes);

    std::wstring mainFolder;
    if (!doc.m_path.empty())
        mainFolder = CPathUtils::GetParentDirectory(doc.m_path);
    else
        mainFolder = CPathUtils::GetCWD();

    std::vector<std::wstring> systemFoldersToSearch;

    std::wstring cpptoolchain;
    std::wstring systemIncludePath;
    LPCWSTR configItem;
    configItem = CIniSettings::Instance().GetString(
        L"cpp", L"toolchain", 0);
    if (configItem != nullptr && *configItem != L'\0')
    {
        cpptoolchain = configItem;
        configItem = CIniSettings::Instance().GetString(
            cpptoolchain.c_str(), L"systemincludepath", 0);
        if (configItem != nullptr)
        {
            systemIncludePath = configItem;
            stringtok(systemFoldersToSearch, systemIncludePath, true, L";");
        }
    }

    std::vector<std::wstring> regularFoldersToSearch;
    regularFoldersToSearch.push_back(mainFolder);

    std::wstring foundFile;
    bool hasCppToolChain = !cpptoolchain.empty();
    for ( auto& inc : includes )
    {
        HRESULT hr;
        std::vector<std::wstring>& searchPath =
            (inc.includeType == IncludeType::System)
                ? systemFoldersToSearch
                : regularFoldersToSearch;
        int includeCategory = (inc.includeType == IncludeType::System)
                ? SYSTEM_INCLUDE_CATEGORY
                : USER_INCLUDE_CATEGORY;

        bool found = false;
        if (hasCppToolChain)
            found = FindFile(inc.filename, searchPath, foundFile);

        std::wstring menuText = inc.filename;
        if (!found)
            menuText += L" ...";
        IncludeMenuItemInfo m(std::move(inc), std::move(foundFile));

        m_menuInfo.push_back(std::move(m));
        hr = CAppUtils::AddStringItem(collection, menuText.c_str(), includeCategory);
        if (CAppUtils::FailedShowMessage(hr))
        {
            m_menuInfo.pop_back();
            return false;
        }
    }

    return true;
}

bool CCmdHeaderSource::IsValidMenuItem(size_t item)
{
    return item >= 0 && item < m_menuInfo.size();
}

bool CCmdHeaderSource::HandleSelectedMenuItem(size_t selected)
{
    if (!HasActiveDocument())
    {
        APPVERIFY(false); // Shouldn't happen.
        return false;
    }
    if (!IsValidMenuItem(selected))
    {
        APPVERIFY(false); // Shouldn't happen.
        return false;
    }

    std::wstring targetFile;

    bool choose = false;
    const IncludeMenuItemInfo& info = m_menuInfo[selected];
    // Assume user chose Open Other Include...
    if (info.filename.empty() && info.definition.filename.empty())
        choose = true;

    // If the system didn't manage to automatically associate the include with a
    // file, use the filename from the include and let the user find it.
    else if (info.filename.empty())
    {
        // If the system didn't find a file, the user has to choose.
        targetFile = CPathUtils::GetFileName(info.definition.filename);
        choose = true;
    }
    else
    {
        // The system found a file. This will be the file to open unless
        // the user wants to override that by pressing the shift key down.
        targetFile = info.filename;

        // TODO! Tighten the key logic so we don't act if other keys are down, like control.
        // This also doesn't seem to yield accurate detection. Need to investigate.
        auto keyState = GetKeyState(VK_SHIFT);
        if (keyState == -127)
            choose = true;
    }

    if (choose)
    {
        std::wstring defaultFolder;
        std::wstring selectedFile;
        // Set the default folder to the folder of the current tab.
        CDocument doc = GetActiveDocument();
        if (!doc.m_path.empty())
            defaultFolder = CPathUtils::GetParentDirectory(doc.m_path);
        if (!UserFindFile(GetHwnd(), targetFile, defaultFolder, selectedFile))
            return false;
        targetFile = selectedFile;
    }

    SetInsertionIndex(GetActiveTabIndex());
    if (!OpenFile(targetFile.c_str(), true))
        return false;

    // Assume not an include.
    if (info.definition.includeType == IncludeType::Unknown)
        return true;

    // If the include file has no extension, we're pretty safe to assume
    // it's a system header like <cstdio> so set the language to C/C++
    // as the system won't do it. This is one of the benefits of using this
    // include facility in the first place over straight File Open.
    std::wstring fileExt = CPathUtils::GetFileExtension(targetFile);
    if (fileExt.empty())
    {
        // REVIEW: It would be more efficient to have an OpenFile overload where
        // we could specify the language at it's opened.
        CDocument doc = GetActiveDocument();
        doc.m_language = L"C/C++";
        SetDocument(GetDocIdOfCurrentTab(), doc);
        SetupLexerForLang(doc.m_language);
        CLexStyles::Instance().SetLangForPath(doc.m_path, doc.m_language);
        UpdateStatusBar(true);
    }

    return true;
}

HRESULT CCmdHeaderSource::IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    HRESULT hr;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if (key && (*key == UI_PKEY_SelectedItem))
        {
            // Happens when a highlighted item is selected from the drop down
            // and clicked.
            UINT uSelected;
            hr = UIPropertyToUInt32(*key, *ppropvarValue, &uSelected);
            if (CAppUtils::FailedShowMessage(hr))
                return hr;
            size_t selected = static_cast<size_t>(uSelected);

            // The user selected a function to goto, we don't want that function
            // to remain selected because the user is supposed to
            // reselect a new one each time,so clear the selection status.
            hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
            if (CAppUtils::FailedShowMessage(hr))
                return hr;

            return HandleSelectedMenuItem(selected) ? S_OK : E_FAIL;
        }
    }
#if 0
        // TODO: Display the file's full path in preview mode.
    if (verb == UI_EXECUTIONVERB_CANCELPREVIEW)
    {
        return S_OK;
    }
    if (verb == UI_EXECUTIONVERB_PREVIEW)
    {
        if ( key && *key == UI_PKEY_SelectedItem)
        {
            UINT selected;
            HRESULT hr = UIPropertyToUInt32(*key, *ppropvarValue, &selected);
            const IncludeMenuItemInfo& info = m_menuInfo[selected];
            // Assume user chose Open Other File....
            //if (!(info.filename.empty() && info.definition.filename.empty()))

            ///InitPropVariantFromString(L"hello",ppropvarNewValue
            UIPrUI_PKEY_TooltipDescriptio
            pCommandExecutionProperties->
            //return UIInitPropertyFromString(UI_PKEY_SelectedItem, selected, ppropvarNewValue);
        }
        return S_OK;
    }
#endif
    return E_NOTIMPL;
}

void CCmdHeaderSource::TabNotify(TBHDR * ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        // Switching to this document.
        APPVERIFY( GetDocIdOfCurrentTab() == GetDocIDFromTabIndex(ptbhdr->tabOrigin) );
        // Include list will be stale now.
        InvalidateIncludes();
    }
}

void CCmdHeaderSource::ScintillaNotify(Scintilla::SCNotification * pScn)
{
    switch (pScn->nmhdr.code)
    {
        case SCN_MODIFIED:
            if (pScn->modificationType & (SC_MOD_INSERTTEXT|SC_MOD_DELETETEXT))
            {
                // Links may have changed.
                InvalidateIncludes();
            }
            break;
    }
}

void CCmdHeaderSource::OnDocumentOpen(int /*index*/)
{
    // All new links to find.
    InvalidateIncludes();
}

void CCmdHeaderSource::OnDocumentSave(int /*index*/, bool /*bSaveAs*/)
{
    // Language might have changed.
    InvalidateIncludes();
}
