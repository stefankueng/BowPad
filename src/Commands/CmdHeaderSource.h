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

#pragma once
#include "ICommand.h"

#include <string>
#include <vector>
#include <functional>

#include "ScintillaWnd.h"
#include "BowPadUI.h"

enum class RelatedType
{
    Unknown,
    Corresponding,
    UserInclude,
    SystemInclude
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
    CCmdHeaderSource(void * obj);
    ~CCmdHeaderSource(void) { }

    // Overrides
    bool Execute() override;
    UINT GetCmdId() override { return cmdHeaderSource; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;

    HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties) override;

    void TabNotify(TBHDR * ptbhdr) override;

    void ScintillaNotify(Scintilla::SCNotification * pScn) override;

    void OnDocumentOpen(int id) override;

    void OnDocumentSave(int index, bool bSaveAs) override;

private:
    void HandleIncludeFileMenuItem(const RelatedFileItem& item);
    void HandleCorrespondingFileMenuItem(const RelatedFileItem& item);
    void HandleOpenFileMenuItem();
    bool PopulateMenu(const CDocument& doc, IUICollectionPtr& collection);
    void InvalidateIncludes();
    void InvalidateIncludesSource();
    void InvalidateIncludesEnabled();
    bool HandleSelectedMenuItem(size_t selected);
    bool IsValidMenuItem(size_t item) const;
    bool UserFindFile(HWND hwndParent, const std::wstring& filename, const std::wstring& defaultFolder, std::wstring& selectedFilename) const;
    bool IsServiceAvailable();
    bool OpenFileAsLanguage(const std::wstring& filename);

private:
    std::vector<RelatedFileItem> m_menuInfo;
    bool m_bStale;
    // TODO! Find out what requires this to be a member variable.
    // It crashes if on the stack etc. Might reveal bugs.
    CScintillaWnd m_edit;
};


