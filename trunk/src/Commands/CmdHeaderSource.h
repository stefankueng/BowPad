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

class OpenFileItem; class IncludeFileItem; class CorrespondingFileItem;

class CCmdHeaderSource : public ICommand
{
public:
    CCmdHeaderSource(void * obj);
    ~CCmdHeaderSource(void) { }

    // Overrides
    bool Execute() override { return false; }
    UINT GetCmdId() override { return cmdHeaderSource; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;

    HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties) override;

    void TabNotify(TBHDR * ptbhdr) override;

    void ScintillaNotify(Scintilla::SCNotification * pScn) override;

    void OnDocumentOpen(int id) override;

    void OnDocumentSave(int index, bool bSaveAs) override;

private:
    void HandleIncludeFileMenuItem(IncludeFileItem& item);
    void HandleCorrespondingFileMenuItem(CorrespondingFileItem & item);
    void HandleOpenFileMenuItem(OpenFileItem& item);
    bool PopulateMenu(const CDocument& doc, IUICollectionPtr& collection);
    void InvalidateIncludes();
    void InvalidateIncludesSource();
    void InvalidateIncludesEnabled();
    bool HandleSelectedMenuItem(size_t selected);
    bool IsValidMenuItem(size_t item) const;
    bool UserFindFile( HWND hwndParent,
        const std::wstring& filename, const std::wstring& defaultFolder, std::wstring& selectedFilename) const;
    bool IsServiceAvailable();
    bool OpenFileAsLanguage(const std::wstring& filename);

private:
    std::vector<std::function<void()>> m_menuInfo;
    bool m_bStale;
    // TODO! Find out what requires this to be a member variable.
    // It crashes if on the stack etc. Might reveal bugs.
    CScintillaWnd m_edit;
};

enum class IncludeType
{
    Unknown,
    User,
    System
};

struct IncludeInfo
{
    size_t line;             // Line number where the include was found.
    std::wstring filename;   // Filename parsed from raw line. e.g. <x> or "y"
    IncludeType includeType; // Type of include i.e. <x> or "x" or unknown (e.g. parse error)

    inline IncludeInfo::IncludeInfo(size_t line, std::wstring& filename, IncludeType includeType)
        : line(line), filename(filename), includeType(includeType) { }
};

class CorrespondingFileItem
{
public:
    using CorrespondingFileItemOwner = std::function<void(CorrespondingFileItem&)>;
    CorrespondingFileItemOwner owner;
    std::wstring filename;
    CorrespondingFileItem(CorrespondingFileItemOwner& owner, const std::wstring& filename)
        : owner(owner), filename(filename) { }
    void operator()()
    {
        owner(*this);
    }
};

class IncludeFileItem
{
public:
    IncludeInfo inc;
    std::wstring realFile;
    using IncludeFileItemOwner = std::function<void(IncludeFileItem&)>;
    IncludeFileItemOwner owner;
    IncludeFileItem( IncludeFileItemOwner& owner,
        const IncludeInfo& inc, const std::wstring& realFile) 
        : owner(owner), inc(inc), realFile(realFile) { }
    void operator()()
    {
        owner(*this);
    }
};

