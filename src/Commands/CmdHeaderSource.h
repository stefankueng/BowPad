// This file is part of BowPad.
//
// Copyright (C) 2013-2017 - Stefan Kueng
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

#include <string>
#include <vector>

#include "ScintillaWnd.h"
#include "BowPadUI.h"

enum class RelatedType
{
    Unknown,
    Corresponding,
    UserInclude,
    SystemInclude,
    CreateCorrespondingFile,
    CreateCorrespondingFiles
};

class RelatedFileItem
{
public:
    RelatedFileItem() : Type(RelatedType::Unknown) {};
    RelatedFileItem(const std::wstring& path, RelatedType type)
        : Path(path)
        , Type(type)
    {}
    ~RelatedFileItem() {};

    std::wstring    Path;
    RelatedType     Type;
};


class CCmdHeaderSource : public ICommand
{
public:
    CCmdHeaderSource(void* obj);
    ~CCmdHeaderSource() = default;

    // Overrides
    bool Execute() override;
    UINT GetCmdId() override { return cmdHeaderSource; }
    void AfterInit() override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;

    HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties) override;

    void TabNotify(TBHDR* ptbhdr) override;

    void ScintillaNotify(SCNotification* pScn) override;

    void OnDocumentOpen(DocID id) override;

    void OnDocumentSave(DocID id, bool bSaveAs) override;

    void OnLangChanged() override;

private:
    void HandleIncludeFileMenuItem(const RelatedFileItem& item);
    void HandleCorrespondingFileMenuItem(const RelatedFileItem& item);
    bool PopulateMenu(const CDocument& doc, CScintillaWnd& edit, IUICollectionPtr& collection);
    void InvalidateMenu();
    void InvalidateMenuEnabled();
    bool HandleSelectedMenuItem(size_t selected);
    bool IsValidMenuItem(size_t item) const;
    bool UserFindFile(HWND hwndParent, const std::wstring& filename, const std::wstring& defaultFolder, std::wstring& selectedFilename) const;
    bool IsServiceAvailable();
    bool OpenFileAsLanguage(const std::wstring& filename);
    bool ShowSingleFileSelectionDialog(HWND hWndParent, const std::wstring& defaultFilename, const std::wstring& defaultFolder, std::wstring& fileChosen) const;
    void GetFilesWithSameName(const std::wstring& targetPath, std::vector<std::wstring>& matchingfiles) const;
    bool FindFile(const std::wstring& fileToFind, const std::vector<std::wstring>& foldersToSearch, std::wstring& foundPath) const;
    bool ParseInclude(const std::wstring& raw, std::wstring& filename, RelatedType& incType) const;
    bool FindNext(CScintillaWnd& edit, const Sci_TextToFind& ttf, int flags, std::string& found_text, size_t* line_no) const;
    void AttachDocument(CScintillaWnd& edit, CDocument& doc);
    bool GetIncludes(const CDocument& doc, CScintillaWnd& edit, std::vector<RelatedFileItem>& includes) const;
    bool GetDefaultCorrespondingFileExtMappings(const std::wstring& from, std::wstring& to) const;
    void GetCorrespondingFileMappings(const std::wstring& input_filename, std::vector<std::wstring>& corresponding_filenames) const;
    bool GetCPPIncludePathsForMS(std::wstring& systemIncludePaths) const;
private:
    std::vector<RelatedFileItem>    m_menuInfo;
    std::wstring                    m_systemIncludePaths;
    bool                            m_bSearchedIncludePaths;
};


