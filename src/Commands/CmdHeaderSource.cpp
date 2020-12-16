// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2020 - Stefan Kueng
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

OVERVIEW

Clicking the list part of the Corresponding File button causes BP to display
a menu of filenames.

The list contains "Corresponding" files and (if the language is set to C/C++)
C++ User Include files and C++ System Include files. Filenames may be absolute or relative.

The "Corresponding Files" list contains files that match the
filename of the current document up to but excluding the first ".".
The name after the first "." is termed the "long" extension.
So given test.aspx.cs, "aspx.cs" is the long extension. "test" is the filename
without that.

The include list is filenames parsed from C++ #include statements in the
current document. User Include statements have the form #include "x"
and System Include statements have the form #include <x>.
The list contains the x from these statements.

Clicking a filename in the list causes BP to open the physical file it represents..

As BP has no project system, BP might not always find a physical file match for
every include file it presents and for the same reason it might find the wrong file.
BP signals files that can't be found by showing ellipses "..." after their name.
When a wrong file is found, there is know way to know unless the user realizes,
so it's important the user be mindful that this can happen and why, to not get caught out.

User includes files are found by searching for them in the folder of the active document.
If the document is New (unsaved), that folder will be that of whatever
document the user had active when they created that New document/tab. If that's none,
the final fallback is the current working directory.

When a New document is saved, the active folder may change. This is something to be
aware of to avoid surprises but should cause little problem in practice.

System include files are found by following a search path in BP's settings file
"%appdata%\bowpad\settings" or other paths it detects might be appropriate like
Windows SDK or Visual Studio paths.

If no such configuration is defined BP will fallback to a basic functionality set.

An example configuration for this is shown below.

[cpp]
toolchain=msvc
[msvc]
systemincludepath=c:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\include;c:\Program Files (x86)\Windows Kits\8.1\Include\um
[clang]
systemincludepath=c:\libcxx\include
[mingw]
systemincludepath=c:\mingw\x86_64-w64-mingw32\include\c++

The configuration structure design and the one shown above allows for multiple "toolchains"
to be defined in BP. Only one or none is ever "active" at any time though.

The "toolchain" variable determines which toolchain is the active one and accordingly which
search paths will be used.

If the toolchain value is blank or missing, no toolchain will be used and the system will behave
as if no configuration at all was present. This allows the default/unconfigured/fallback feature
set to be tested without having to completely remove the configuration information first.

The basic (unconfigured) feature set means system files must be found manually.
User file finding will still occur for the active folder.

Showing missing include files in the menu means:
a) the user doesn't have to go the File Open menu to open the file,
as would be the case if it wasn't listed.
b) BP can pre-populate the file name for the File Open Dialog if the user clicks on it
which wouldn't be done otherwise if it wasn't listed.
c) allows BP to assume the language will be C/C++ so the user never has to set it
manually. This is useful as some file types which aren't safe to assume language type
from (e.g. .inc or .inl files) and where there is no type extension
like C++ system include files.

A full list serves as a one click discoverability tool to determine exactly what
names are relevant without forcing the user to manually scan the document to find out.

Clicking the non list part of the Corresponding File Button fires the default action.
The purpose of the default action is to try to load a *preferred file* in *one* click.
If the user doesn't want that, they shouldn't click the list part of the button.

Identifying what the preferred file is consistently, requires configuration settings.
BP supports settings that do this shown below:

The left side represents a file extension that if the current document has that
extension, then BP will look for a corresponding files that has the same name
as the current file but with the right extension.
Multiple files can be listed on the right side. BP will search for each file
in order until it finds a match.
The table below is actually the defaults but the user can override the
defaults by defining the table below with just what they want to additionally define.

[corresponding_files_ext_map]
cpp=hpp;h
cxx=hpp;h
h=c;cpp
hpp=cpp;cxx
aspx=aspx.cs;aspx.vb
aspx.cs=aspx

Provision of such a mapping for C++ ensures that a one click action is possible and
prevents presenting listing of alternative files for a file type when that alternative
list is never desirable.
e.g. If you are editing test.cpp, you would never want test.db, it's a database file.
If you are editing test.hpp, you'd likely never want test.c, only test.cpp
If you're editing test.cpp, test.c is not of interest nor is test.h if test.hpp is present.
If you are editing test.cs, you'd never want test.cpp but you might want test.aspx
and you'd not want to filter that out like an .exe or something.
Mappings solve all these decisions ensure a single click is possible more often.

END OVERVIEW.

It would be preferable for this feature if the menu could populate on demand when clicked.
but I can't make that work.
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
#include "DirFileEnum.h"
#include "Resource.h"
#include "CorrespondingFileDlg.h"
#include "OnOutOfScope.h"

extern void FindReplace_FindFile(void *mainWnd, const std::wstring& fileName);

namespace
{
// We may want to use this elsewhere in the future so draw attention to it, but
// but for now it's only needed in this module.
static constexpr char CPP_INCLUDE_STATEMENT_REGEX[] = { "^\\#include\\s+((\\\"[^\\\"]+\\\")|(<[^>]+>))" };

// Maximum number of lines to scan for include statements.
// NOTE: This could be a configuration item but that doesn't seem necessary for now.
constexpr int MAX_INCLUDE_SEARCH_LINES = 100;

constexpr int CREATE_CORRESPONDING_FILE_CATEGORY = 1;
constexpr int CORRESPONDING_FILES_CATEGORY = 2;
constexpr int USER_INCLUDE_CATEGORY = 3;
constexpr int SYSTEM_INCLUDE_CATEGORY = 4;

};

bool CCmdHeaderSource::UserFindFile(HWND hwndParent, const std::wstring& filename,
                                    const std::wstring& defaultFolder,
                                    std::wstring& selectedFilename) const
{
    std::wstring defFolder;
    if (defaultFolder.empty())
    {
        const auto& doc = GetActiveDocument();
        if (!doc.m_path.empty())
            defFolder = CPathUtils::GetParentDirectory(doc.m_path);
        else
        {
            defFolder = CPathUtils::GetCWD();
            auto modDir = CPathUtils::GetLongPathname(CPathUtils::GetModuleDir());
            if (_wcsicmp(defFolder.c_str(), modDir.c_str()) == 0)
                defFolder.clear();
        }
    }
    else
        defFolder = defaultFolder;

    std::wstring name = CPathUtils::GetFileName(filename);
    if (!ShowSingleFileSelectionDialog(hwndParent, name, defFolder, selectedFilename))
        return false;
    return true;
}

CCmdHeaderSource::CCmdHeaderSource(void* obj)
    : ICommand(obj)
    , m_bSearchedIncludePaths(false)
{
}

void CCmdHeaderSource::AfterInit()
{
    // Because we don't know if this file type supports includes.
    InvalidateMenu();
}

void CCmdHeaderSource::InvalidateMenuEnabled()
{
    // Note SetUICommandProperty(UI_PKEY_Enabled,en) can be useful but probably
    // isn't what we want; and it fails when the ribbon is hidden anyway.
    HRESULT hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
    CAppUtils::FailedShowMessage(hr);
}

void CCmdHeaderSource::InvalidateMenu()
{
    m_menuInfo.clear();
    HRESULT hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
    CAppUtils::FailedShowMessage(hr);
    InvalidateMenuEnabled();
}

HRESULT CCmdHeaderSource::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue)
{
    HRESULT hr;

    if (key == UI_PKEY_Categories)
    {
        IUICollectionPtr coll;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&coll));
        if (FAILED(hr))
            return hr;

        hr = CAppUtils::AddCategory(coll, CREATE_CORRESPONDING_FILE_CATEGORY,
            IDS_CREATE_CORRESPONDING_FILE_CATEGORY);
        if (FAILED(hr))
            return hr;

        hr = CAppUtils::AddCategory(coll, CORRESPONDING_FILES_CATEGORY, IDS_CORRESPONDING_FILES);
        if (FAILED(hr))
            return hr;
        hr = CAppUtils::AddCategory(coll, USER_INCLUDE_CATEGORY, IDS_USER_INCLUDES);
        if (FAILED(hr))
            return hr;
        hr = CAppUtils::AddCategory(coll, SYSTEM_INCLUDE_CATEGORY, IDS_SYSTEM_INCLUDES);
        if (FAILED(hr))
            return hr;

        return S_OK;
    }

    if (key == UI_PKEY_ItemsSource)
    {
        IUICollectionPtr collection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&collection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;

        // The list will retain whatever from last time so clear it.
        collection->Clear();
        // We will rebuild this information so clear it.
        m_menuInfo.clear();

        // We need to know have an active document to continue.
        auto docId = GetDocIdOfCurrentTab();
        if (!docId.IsValid())
            return E_FAIL;
        if (!HasActiveDocument())
            return E_FAIL;
        auto& doc = GetModDocumentFromID(docId);

        CScintillaWnd edit(g_hRes);
        edit.InitScratch(g_hRes);

        AttachDocument(edit, doc);
        OnOutOfScope(
            edit.Call(SCI_SETDOCPOINTER, 0, 0); // Detach document
        );

        PopulateMenu(doc, edit, collection);

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

// Populate the dropdown with the details. Returns false if
// any options couldn't be added but for some reason.
// Not a good enough reason not to show the menu though.
bool CCmdHeaderSource::PopulateMenu(const CDocument& doc, CScintillaWnd& edit, IUICollectionPtr& collection)
{
    std::vector<std::wstring> correspondingFiles;
    GetCorrespondingFileMappings(CPathUtils::GetFileName(doc.m_path), correspondingFiles);

    std::wstring basePath = CPathUtils::GetParentDirectory(doc.m_path);
    std::wstring correspondingFile;

    bool showCreate = true;

    // If one of the corresponding files exists, don't offer to create
    // any of the others.
    for (const std::wstring& filename : correspondingFiles)
    {
        correspondingFile = CPathUtils::Append(basePath, filename);
        if (_waccess(correspondingFile.c_str(), 0) == 0)
        {
            showCreate = false;
            break;
        }
    }

    m_menuInfo.push_back(RelatedFileItem(correspondingFile,
        RelatedType::CreateCorrespondingFiles));
    ResString createCorrespondingFiles(g_hRes, IDS_NEWCORRESPONDINGFILES);

    auto hr = CAppUtils::AddStringItem(collection, createCorrespondingFiles.c_str(), CREATE_CORRESPONDING_FILE_CATEGORY, nullptr);
    if (FAILED(hr))
    {
        m_menuInfo.pop_back();
        return false;
    }

    if (showCreate)
    {
        for (const std::wstring& filename : correspondingFiles)
        {
            correspondingFile = CPathUtils::Append(basePath, filename);
            if (_waccess(correspondingFile.c_str(), 0) != 0)
            {
                m_menuInfo.push_back(RelatedFileItem(correspondingFile,
                    RelatedType::CreateCorrespondingFile));

                ResString createMenuItem(g_hRes, IDS_CREATE_CORRESPONDING_FILE);
                std::wstring menuText = CStringUtils::Format(createMenuItem, filename.c_str());

                hr = CAppUtils::AddStringItem(collection, menuText.c_str(), CREATE_CORRESPONDING_FILE_CATEGORY, nullptr);
                if (FAILED(hr))
                {
                    m_menuInfo.pop_back();
                    return false;
                }
            }
        }
    }

    // Open File Option

    // If the user can't find the file they want, offer a shortcut to find it themselves
    // and open it. Using this mechanism has two advantages over regular file open:
    // 1. The user doesn't have to move the cursor off to the file open menu to find the file.
    // 2. The file opened can be assumed to be an include file if it has no file extension.

    // Corresponding File Options
    std::vector<std::wstring> matchingFiles;

    std::wstring targetPath = doc.m_path;
    // Can only get matching files for a saved document.
    if (!targetPath.empty())
    {
        GetFilesWithSameName(targetPath, matchingFiles);

        std::sort(matchingFiles.begin(), matchingFiles.end(),
                  [](const std::wstring& a, const std::wstring& b) -> bool
        {
            return StrCmpLogicalW(a.c_str(), b.c_str()) < 0;
        });

        std::wstring matchingFileName;
        for (const auto& matchingFile : matchingFiles)
        {
            matchingFileName = CPathUtils::GetFileName(matchingFile);
            m_menuInfo.push_back(RelatedFileItem(matchingFile, RelatedType::Corresponding));
            hr = CAppUtils::AddStringItem(collection, matchingFileName.c_str(), CORRESPONDING_FILES_CATEGORY, nullptr);
            if (FAILED(hr))
            {
                m_menuInfo.pop_back();
                return false;
            }
        }
    }

    if (doc.GetLanguage() == "C/C++")
    {
        // Include Files

        std::vector<RelatedFileItem> includes;
        GetIncludes(doc, edit, includes);

        std::wstring mainFolder;
        if (!doc.m_path.empty())
            mainFolder = CPathUtils::GetParentDirectory(doc.m_path);
        else
            mainFolder = CPathUtils::GetCWD();

        std::vector<std::wstring> systemFoldersToSearch;

        std::wstring cpptoolchain;
        LPCWSTR configItem = CIniSettings::Instance().GetString(L"cpp", L"toolchain");
        if (configItem != nullptr)
            cpptoolchain = configItem;

        std::wstring foundFile;
        std::wstring menuText;
        bool found;

        // Handle System Include Files

        // Only look for system include files if there's a tool chain defined that
        // says where they can be found.
        configItem = nullptr;
        if (!cpptoolchain.empty())
            configItem = CIniSettings::Instance().GetString(cpptoolchain.c_str(), L"systemincludepath");
        if (configItem != nullptr)
            stringtok(systemFoldersToSearch, configItem, true, L";");
        else
        {
            if (!m_bSearchedIncludePaths)
            {
                GetCPPIncludePathsForMS(m_systemIncludePaths);
                m_bSearchedIncludePaths = true;
            }
            if (!m_systemIncludePaths.empty())
                stringtok(systemFoldersToSearch, m_systemIncludePaths, true, L";");
        }

        for (const auto& inc : includes)
        {
            if (inc.Type == RelatedType::SystemInclude)
            {
                found = FindFile(inc.Path, systemFoldersToSearch, foundFile);
                m_menuInfo.push_back(RelatedFileItem(found ? foundFile : inc.Path, inc.Type));

                menuText = inc.Path;
                if (!found)
                    menuText += L" ...";
                hr = CAppUtils::AddStringItem(collection, menuText.c_str(), SYSTEM_INCLUDE_CATEGORY, nullptr);
                if (FAILED(hr))
                {
                    m_menuInfo.pop_back();
                    return false;
                }
            }
        }

        // Handle User Includes Files

        // Look for user files in the directory of the current document no matter
        // if there is no tool chain.
        std::vector<std::wstring> regularFoldersToSearch;
        regularFoldersToSearch.push_back(mainFolder);

        for (const auto& inc : includes)
        {
            if (inc.Type == RelatedType::UserInclude)
            {
                found = FindFile(inc.Path, regularFoldersToSearch, foundFile);
                m_menuInfo.push_back(RelatedFileItem(found ? foundFile : inc.Path, inc.Type));

                menuText = inc.Path;
                if (!found)
                    menuText += L" ...";
                hr = CAppUtils::AddStringItem(collection, menuText.c_str(), USER_INCLUDE_CATEGORY, nullptr);
                if (FAILED(hr))
                {
                    m_menuInfo.pop_back();
                    return false;
                }
            }
        }
    }

    // Stop the drop down list from ever being blank by putting a no files found
    // indicator in, but it's also useful.
    if (m_menuInfo.empty())
    {
        m_menuInfo.push_back(RelatedFileItem()); // No action.
        // Using CORRESPONDING_FILES_CATEGORY, but might possibly prefer no category,
        // but that doesn't appear to be possible it seems.
        hr = CAppUtils::AddResStringItem(collection, IDS_NO_FILES_FOUND, CORRESPONDING_FILES_CATEGORY, nullptr);
        CAppUtils::FailedShowMessage(hr);
    }

    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);

    return true;
}

bool CCmdHeaderSource::IsValidMenuItem(size_t item) const
{
    return item < m_menuInfo.size();
}

bool CCmdHeaderSource::OpenFileAsLanguage(const std::wstring& filename)
{
    SetInsertionIndex(GetActiveTabIndex());
    if (OpenFile(filename.c_str(), OpenFlags::AddToMRU)<0)
        return false;

    auto desiredLang = "C/C++";
    if (HasActiveDocument())
    {
        auto& doc = GetModActiveDocument();
        if (doc.GetLanguage() != desiredLang)
        {
            SetupLexerForLang(desiredLang);
            doc.SetLanguage(desiredLang);
            CLexStyles::Instance().SetLangForPath(doc.m_path, desiredLang);
            UpdateStatusBar(true);
        }
    }
    return true;
}

void CCmdHeaderSource::HandleIncludeFileMenuItem(const RelatedFileItem& item)
{
    // If the system has found a file, the filename will be non blank.
    // In that case, proceed to try and open the file but
    // let the user override that file choice first which they do by
    // holding the shift key down.
    // If they didn't chose to override, open the file and be done
    // no matter if we succeed in opening or not.
    // If the user chose to override, fall through and let them try to find it.
    // Which is what happens if the System couldn't find the file to begin with
    // and it was blank.
    if (!item.Path.empty() && PathFileExists(item.Path.c_str()))
    {
        if ((GetKeyState(VK_SHIFT) & 0x8000) == 0)
        {
            OpenFileAsLanguage(item.Path);
            return;
        }
    }
    //std::wstring defaultFolder;
    //std::wstring fileToOpen;

    //if (!UserFindFile(GetHwnd(), CPathUtils::GetFileName(item.Path), defaultFolder, fileToOpen))
    //    return;
    //if (!OpenFileAsLanguage(fileToOpen))
    //    return;

    auto filename = CPathUtils::GetFileName(item.Path);
    FindReplace_FindFile(m_pMainWindow, filename);
}

void CCmdHeaderSource::HandleCorrespondingFileMenuItem(const RelatedFileItem& item)
{
    SetInsertionIndex(GetActiveTabIndex());
    OpenFile(item.Path.c_str(), OpenFlags::AddToMRU);
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

    const auto& item = m_menuInfo[selected];
    switch (item.Type)
    {
        case RelatedType::Corresponding:
            HandleCorrespondingFileMenuItem(item);
            break;
        case RelatedType::SystemInclude:
        case RelatedType::UserInclude:
            HandleIncludeFileMenuItem(item);
            break;
        case RelatedType::CreateCorrespondingFile:
            return OpenFile(item.Path.c_str(), OpenFlags::AddToMRU | OpenFlags::AskToCreateIfMissing) >= 0;
            break;
        case RelatedType::CreateCorrespondingFiles:
            CCorrespondingFileDlg dlg(m_pMainWindow);

            std::wstring initialFolder;
            if (HasActiveDocument())
            {
                const auto& doc = GetActiveDocument();
                initialFolder = CPathUtils::GetParentDirectory(doc.m_path);
            }
            dlg.Show(GetHwnd(), initialFolder);
            break;
    }

    return true;
}

HRESULT CCmdHeaderSource::IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    HRESULT hr;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if (key && *key == UI_PKEY_SelectedItem)
        {
            // Happens when a highlighted item is selected from the drop down
            // and clicked.
            UINT uSelected;
            hr = UIPropertyToUInt32(*key, *ppropvarValue, &uSelected);
            if (CAppUtils::FailedShowMessage(hr))
                return hr;
            size_t selected = static_cast<size_t>(uSelected);

            // The user selected a file to open, we don't want that file
            // to remain selected because the user is supposed to
            // reselect a new one each time,so clear the selection status.
            hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
            if (CAppUtils::FailedShowMessage(hr))
                return hr;

            return HandleSelectedMenuItem(selected) ? S_OK : E_FAIL;
        }
    }
    return E_NOTIMPL;
}

void CCmdHeaderSource::TabNotify(TBHDR* ptbhdr)
{
    // Include list will be stale now.
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
        InvalidateMenu();
}

void CCmdHeaderSource::OnLangChanged()
{
    InvalidateMenu();
}


void CCmdHeaderSource::ScintillaNotify(SCNotification * pScn)
{
    // TODO! Need to trap the language change too.
    // We get style needed events but that's not the same.
    switch (pScn->nmhdr.code)
    {
        case SCN_MODIFIED:
            if (pScn->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
            {
                // Links may have changed.
                InvalidateMenu();
            }
            break;
    }
}

void CCmdHeaderSource::OnDocumentOpen(DocID /*id*/)
{
    // All new links to find.
    InvalidateMenu();
}

void CCmdHeaderSource::OnDocumentSave(DocID /*id*/, bool /*bSaveAs*/)
{
    // Language might have changed.
    InvalidateMenu();
}

bool CCmdHeaderSource::Execute()
{
    // This is executed when the user clicks the default button or
    // through the hotkey.
    // The default action is to pick the most appropriate file if
    // one is known through configuration (or default) settings.

    if (HasActiveDocument())
    {
        const auto& doc = GetActiveDocument();
        std::vector<std::wstring> matchingFiles;
        GetFilesWithSameName(doc.m_path, matchingFiles);

        std::vector<std::wstring> correspondingFiles;
        GetCorrespondingFileMappings(CPathUtils::GetFileName(doc.m_path), correspondingFiles);

        std::wstring basePath = CPathUtils::GetParentDirectory(doc.m_path);
        std::wstring correspondingFile;

        // Map to any file present that's deemed to be appropriate.
        // If we aren't able map to any file, fall through to present a list.
        for (const std::wstring& filename : correspondingFiles)
        {
            correspondingFile = CPathUtils::Append(basePath, filename);
            if (_waccess(correspondingFile.c_str(), 0) == 0)
            {
                SetInsertionIndex(GetActiveTabIndex());
                return OpenFile(correspondingFile.c_str(), OpenFlags::AddToMRU) >=0;
            }
        }

        // Automatically loading a file just because it is the only file
        // there and it happens to have a similar filename to the file
        // currently being edited can be bad.
        // For example, if you're editing test.cpp and there is a test.db
        // you don't want to load a database ever.
        //
        // You can filter files out but you might not want to, for example:
        // If you are editing test.cpp, you are unlikely to
        // want test.aspx.cs just because it's the only other file there
        // and you definitely don't want to filter that file out.
        // You want to see it and switch to it you are editing test.aspx.
        // For all these reasons, autoload should be switched off by default,
        // If the user wants to load files, they should add a file mapping.
        //
        // Explicit mappings help reduce that problems of file clashings and provide
        // priorities where otherwise there might be a conflict or a wrong choice;
        // and it keep things to one click that should be one click even in the presence
        // of similar files. (see OVERVIEW)
        // The user needs a way of preventing/allowing files to be automatically opened
        // though.
        bool autoload = CIniSettings::Instance().GetInt64(L"corresponding_files", L"autoload", 0) != 0; // See comment above why default is 0.
        // Don't try to autoload files that have mappings, if the file
        // wasn't there, it doesn't mean we want just any old file instead.
        if (correspondingFile.size() > 0)
            autoload = false;

        if (matchingFiles.size() == 1 && autoload)
        {
            SetInsertionIndex(GetActiveTabIndex());
            return OpenFile(matchingFiles[0].c_str(), OpenFlags::AddToMRU) >= 0;
        }

        // To force a menu to be shown if all other paths have been exhausted so
        // that the user doesn't think the button didn't work, do this:
        // ResString ctrlName(hRes, cmdHeaderSource_LabelTitle_RESID);
        // return CAppUtils::ShowDropDownList(GetHwnd(), ctrlName);

        std::wstring fileNames;
        for (size_t i = 0; i < correspondingFiles.size(); ++i)
        {
            fileNames += correspondingFiles[i];
            if (i + 1 < correspondingFiles.size())
                fileNames += ';';
        }
        FindReplace_FindFile(m_pMainWindow, fileNames);
    }

    return false;
}

bool CCmdHeaderSource::ShowSingleFileSelectionDialog(HWND hWndParent, const std::wstring& defaultFilename, const std::wstring& defaultFolder, std::wstring& fileChosen) const
{
    PreserveChdir keepCWD;

    IFileOpenDialogPtr pfd;

    HRESULT hr = pfd.CreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER);
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
    hr = pfd->SetOptions(dwOptions);
    if (CAppUtils::FailedShowMessage(hr))
        return false;

    if (!defaultFilename.empty())
    {
        pfd->SetFileName(defaultFilename.c_str());
        // set a filter for this as well
        COMDLG_FILTERSPEC filters[2];
        filters[0].pszName = defaultFilename.c_str();
        filters[0].pszSpec = defaultFilename.c_str();
        filters[1].pszName = L"All files";
        filters[1].pszSpec = L"*.*";
        pfd->SetFileTypes(2, filters);
        pfd->SetFileTypeIndex(1);
    }

    // Set the standard title.
    ResString rTitle(g_hRes, IDS_APP_TITLE);
    pfd->SetTitle(rTitle);

    if (!defaultFolder.empty())
    {
        IShellItemPtr psiDefFolder;
        hr = SHCreateItemFromParsingName(defaultFolder.c_str(), nullptr, IID_PPV_ARGS(&psiDefFolder));
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
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) // Expected error
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
    if (count)
    {
        IShellItemPtr psiResult;
        hr = psiaResults->GetItemAt(0, &psiResult);
        if (!CAppUtils::FailedShowMessage(hr))
        {
            PWSTR pszPath = nullptr;
            hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
            if (!CAppUtils::FailedShowMessage(hr))
            {
                fileChosen = pszPath;
                CoTaskMemFree(pszPath);
            }
        }
    }

    return true;
}

// Find all derivatives of a file (the current document) in it's folder.
// i.e. match files that share the same filename, but not extension.
// Directory names are not included.
// Ignore any files that have certain extensions.
// e.g. Given a name like c:\test\test.cpp,
// return {c:\test\test.h and test.h} but ignore c:\test\test.exe.
void CCmdHeaderSource::GetFilesWithSameName(const std::wstring& targetPath, std::vector<std::wstring>& matchingfiles) const
{
    std::vector<std::wstring> ignoredExts;
    stringtok(ignoredExts, CIniSettings::Instance().GetString(L"HeaderSource", L"IgnoredExts", L"exe*obj*dll*ilk*lib*ncb*ipch*bml*pch*res*pdb*aps"), true, L"*");

    std::wstring targetFolder = CPathUtils::GetParentDirectory(targetPath);
    std::wstring targetFileNameWithoutExt = CPathUtils::GetFileNameWithoutLongExtension(targetPath);
    std::wstring matchingPath;
    std::wstring matchingExt;
    std::wstring matchingFileNameWithoutExt;

    CDirFileEnum enumerator(targetFolder);
    bool bIsDir = false;
    while (enumerator.NextFile(matchingPath, &bIsDir, false))
    {
        if (bIsDir)
            continue;
        // Don't match ourself.
        if (CPathUtils::PathCompare(matchingPath, targetPath) == 0)
            continue;

        matchingFileNameWithoutExt = CPathUtils::GetFileNameWithoutLongExtension(matchingPath);

        if (CPathUtils::PathCompare(targetFileNameWithoutExt, matchingFileNameWithoutExt) == 0)
        {
            matchingExt = CPathUtils::GetLongFileExtension(matchingPath);
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
                matchingfiles.push_back(std::move(matchingPath));
        }
    }
}

bool CCmdHeaderSource::FindFile(const std::wstring& fileToFind, const std::vector<std::wstring>& foldersToSearch, std::wstring& foundPath) const
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
bool CCmdHeaderSource::ParseInclude(const std::wstring& raw, std::wstring& filename, RelatedType& incType) const
{
    filename.clear();
    incType = RelatedType::Unknown;

    size_t len = raw.length();
    size_t first, last;

    if (len == 0)
        return false;

    // The regex is supposed to do the heavy lifting, matching wise.
    // The code here is more about filename extraction and trying
    // not to crash given something vaguely sensible than
    // precise matching/validation.

    // Match the '#' of include. Basic sanity check.
    if (raw[0] != L'#')
    {
        APPVERIFY(false);
        return false;
    }
    // Look for the filename which hopefully follows after the literal #include.
    // Look for the quote enclosed filename first, then the angle bracket kind.
    first = raw.find(L'\"');
    if (first != std::wstring::npos)
    {
        last = raw.find_last_of(L'\"');
        if (last != first)
        {
            incType = RelatedType::UserInclude;
            len = last - (first + 1);
            filename = raw.substr(first + 1, len);
            return (len > 0);
        }
    }
    else
    {
        // Looking for a filename in angle brackets, hopefully after an include.
        first = raw.find(L'<');
        if (first != std::wstring::npos)
        {
            incType = RelatedType::SystemInclude;
            last = raw.find_last_of(L'>');
            if (last != first)
            {
                len = last - (first + 1);
                filename = raw.substr(first + 1, len);
                return (len > 0);
            }
        }
    }

    return false;
}

bool CCmdHeaderSource::FindNext(CScintillaWnd& edit, const Sci_TextToFind& ttf, int flags, std::string& found_text, size_t* line_no) const
{
    found_text.clear();
    *line_no = 0;

    auto findRet = edit.Call(SCI_FINDTEXT, flags, (sptr_t)&ttf);
    if (findRet < 0)
        return false;
    found_text = GetTextRange(ttf.chrgText.cpMin, ttf.chrgText.cpMax);
    *line_no = edit.Call(SCI_LINEFROMPOSITION, ttf.chrgText.cpMin);
    return true;
}

void CCmdHeaderSource::AttachDocument(CScintillaWnd& edit, CDocument& doc)
{
    edit.Call(SCI_SETDOCPOINTER, 0, 0);
    edit.Call(SCI_SETSTATUS, SC_STATUS_OK);
    edit.Call(SCI_CLEARALL);
    edit.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
}

bool CCmdHeaderSource::GetIncludes(const CDocument& doc, CScintillaWnd& edit, std::vector<RelatedFileItem>& includes) const
{
    includes.clear();

    const auto& lang = doc.GetLanguage();
    if (lang != "C/C++")
        return false;

    // It is a compromise of speed vs accuracy to only scan the first N lines
    // for #include statements. They mostly exist at the start of the file
    // but can appear anywhere. But searching the whole file using regex is slow
    // especially in debug mode. So this isn't a good idea especially when this isn't
    // the only activity that might be occurring which is eating CPU time like Function scanning.
    // As we don't want to compete with that, for now just scan the first N lines.
    // We could get more creative by not using regex and just fetching and parsing
    // each line ourself but that is unproven and this seems fine for now.
    long length = (long)edit.Call(SCI_GETLINEENDPOSITION, WPARAM(MAX_INCLUDE_SEARCH_LINES - 1)); // 0 based.
    // NOTE: If we want whole file scanned use:
    // long length = (long) edit.Call(SCI_GETLENGTH);

    Sci_TextToFind ttf{}; // Zero initialize.
    ttf.chrg.cpMax = length;
    // NOTE: Intentionally hard coded for now. See overview for reasons.
    // Match an include statement: #include <x> or #include "x" at start of line.
    ttf.lpstrText = const_cast<char*>(CPP_INCLUDE_STATEMENT_REGEX);

    std::string text_found;
    size_t line_no;

    std::wstring raw;
    std::wstring filename;
    RelatedType includeType;

    for (;;)
    {
        ttf.chrgText.cpMin = 0;
        ttf.chrgText.cpMax = 0;

        // Note:
        // RegEx searches currently take a long time in debug mode.
        // SCI_GETLINE(int line, char *text) and SCI_GETLINELENGTH is an
        // untried alternative that might be faster it ever becomes necessary.

        // Use match case because the include keyword is case sensitive and the
        // rest of the regular expression is symbols.
        if (!FindNext(edit, ttf, SCFIND_REGEXP | SCFIND_CXX11REGEX | SCFIND_MATCHCASE, text_found, &line_no))
            break;
        ttf.chrg.cpMin = ttf.chrgText.cpMax + 1;

        raw = CUnicodeUtils::StdGetUnicode(text_found);
        bool parsed = ParseInclude(raw, filename, includeType);
        if (parsed)
        {
            CPathUtils::NormalizeFolderSeparators(filename);
            includes.push_back(RelatedFileItem(filename, includeType));
        }
    }
    // Sort the list putting user includes before system includes. Remove duplicates.
    // A duplicate is an include with the same name and type as another in the list.
    std::sort(includes.begin(), includes.end(), [](const RelatedFileItem& a, const RelatedFileItem& b) -> bool
    {
        // Put user before system includes.
        if (a.Type == RelatedType::SystemInclude && b.Type == RelatedType::UserInclude)
            return false;
        // Within the same type, sort by name.
        // don't use PathCompare since we're sorting to show in an UI
        // and PathCompare should not use StrCmpLogicalW because it's not
        // necessary and stricmp is faster.
        return StrCmpLogicalW(a.Path.c_str(), b.Path.c_str()) < 0;
    });

    // Remove any includes that have the same path and include type, they
    // have to result in the same path found.
    auto newEnd = std::unique(includes.begin(), includes.end(),
                              [](const RelatedFileItem& a, const RelatedFileItem& b)
    {
        return (a.Type == b.Type &&
                CPathUtils::PathCompare(a.Path, b.Path) == 0);
    }
    );
    // Really remove them.
    includes.erase(newEnd, includes.end());
    return true;
}

bool CCmdHeaderSource::GetDefaultCorrespondingFileExtMappings(const std::wstring& from, std::wstring& to) const
{
    to.clear();

    // Find an entry on the left that matches the given name
    // and return the list on the right.
    const struct
    {
        LPCWSTR from;
        LPCWSTR to;
    } default_ext_map[] = {
        // Order is important. Multiple matches expected.
        // Caller will try files in order of generated results.
            { L"cpp", L"hpp;h" },
            { L"cxx", L"hpp;h" },
            { L"c", L"h" },
            { L"cc", L"hpp;h" },
            { L"h", L"c;cpp;cxx;cc" },
            { L"hpp", L"cpp;cxx" },
            { L"aspx.cs", L"aspx" },
            { L"aspx", L"aspx.cs;aspx.vb" }
    };

    for (const auto& me : default_ext_map)
    {
        if (CPathUtils::PathCompare(from, me.from) == 0)
        {
            to = me.to;
            return true;
        }
    }
    return false;
}

void CCmdHeaderSource::GetCorrespondingFileMappings(const std::wstring& input_filename, std::vector<std::wstring>& corresponding_filenames) const
{
    // Let the user override our defaults for an extension.
    std::wstring from_ext = CPathUtils::GetLongFileExtension(input_filename);
    std::wstring to_list;
    LPCWSTR item = CIniSettings::Instance().GetString(L"corresponding_files_extension_map", from_ext.c_str());
    if (item != nullptr)
        to_list = item;
    else
        GetDefaultCorrespondingFileExtMappings(from_ext, to_list);

    std::vector<std::wstring> to_exts;
    stringtok(to_exts, to_list, false, L";");
    std::wstring base_filename = CPathUtils::RemoveLongExtension(input_filename);
    for (const std::wstring& to_ext : to_exts)
    {
        std::wstring corresponding_file;
        corresponding_file = base_filename;
        corresponding_file += L".";
        corresponding_file += to_ext;
        corresponding_filenames.push_back(std::move(corresponding_file));
    }
}

bool CCmdHeaderSource::GetCPPIncludePathsForMS(std::wstring& systemIncludePaths) const
{
    systemIncludePaths.clear();
    // try to find sensible default paths
    std::wstring programfiles = CAppUtils::GetProgramFilesX86Folder();
    if (programfiles.empty())
        return false;

    // newer SDKs are stored under %programfilesx86%\Windows Kits\(sdkver)\Include\(version)
    std::vector<std::wstring> sdkversnew = { L"10",L"8.1", L"8.0" };
    for (const auto& sdkver : sdkversnew)
    {
        std::wstring sTestPath = CStringUtils::Format(L"%s\\Windows Kits\\%s\\Include",
                                                      programfiles.c_str(), sdkver.c_str());
        if (PathFileExists(sTestPath.c_str()))
        {
            CDirFileEnum enumerator(sTestPath);
            bool bIsDir = false;
            std::wstring enumpath;
            // first store the direct subfolders in a set: the set is ordered
            // so that we then can enumerate those in reverse order, i.e. to
            // get the latest version first
            std::set<std::wstring> sdkpaths;
            while (enumerator.NextFile(enumpath, &bIsDir, false))
            {
                if (bIsDir)
                {
                    sdkpaths.insert(enumpath);
                    systemIncludePaths += enumpath;
                    systemIncludePaths += L";";
                }
            }
            for (auto it = sdkpaths.crbegin(); it != sdkpaths.crend(); ++it)
            {
                CDirFileEnum enumerator2(*it);
                while (enumerator2.NextFile(enumpath, &bIsDir, true))
                {
                    if (bIsDir)
                    {
                        systemIncludePaths += enumpath;
                        systemIncludePaths += L";";
                    }
                }
            }
            // The user shouldn't be mixing sdks, paths shouldn't be accumulative.
            break;
        }
    }
    // now try the older SDK paths
    std::vector<std::wstring> sdkvers = {
        L"v10.0A",
        L"v8.1A", L"v8.1", L"v8.0A", L"v8.0",
        L"v7.1A", L"v7.1", L"v7.0A", L"v7.0" };
    for (const auto& sdkver : sdkvers)
    {
        std::wstring sTestPath = CStringUtils::Format(L"%s\\Microsoft SDKs\\Windows\\%s\\Include",
                                                      programfiles.c_str(), sdkver.c_str());
        if (PathFileExists(sTestPath.c_str()))
        {
            systemIncludePaths += sTestPath;
            systemIncludePaths += L";";
            // The user shouldn't be mixing sdks, paths shouldn't be accumulative.
            break;
        }
    }

    // now go through the visual studio paths
    std::vector<std::wstring> vsvers = {
        L"Microsoft Visual Studio 14.0",
        L"Microsoft Visual Studio 13.0", L"Microsoft Visual Studio 12.0",
        L"Microsoft Visual Studio 11.0", L"Microsoft Visual Studio 10.0" };
    for (const auto& vsver : vsvers)
    {
        std::wstring sTestPath = CStringUtils::Format(L"%s\\%s\\VC\\include",
            programfiles.c_str(), vsver.c_str());
        if (PathFileExists(sTestPath.c_str()))
        {
            systemIncludePaths += sTestPath;
            systemIncludePaths += L";";
            sTestPath = CStringUtils::Format(L"%s\\%s\\VC\\atlmfc\\include", programfiles.c_str(), vsver.c_str());
            if (PathFileExists(sTestPath.c_str()))
            {
                systemIncludePaths += sTestPath;
                systemIncludePaths += L";";
            }
            // The user shouldn't be mixing VC versions, path shouldn't be accumulative.
            break;
        }
    }
    return true;
}

