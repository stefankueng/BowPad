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

#include "ScintillaWnd.h"
#include "BowPadUI.h"

enum class IncludeType
{
    Unknown,
    User,
    System
};

struct IncludeInfo
{
    size_t line; // line number where the include was found.
    std::wstring filename; // filename parsed from raw line. e.g. <x> or "y"
    IncludeType includeType; // type of include i.e. <x> or "x"

    IncludeInfo(size_t line, std::wstring&& filename, IncludeType includeType);
    bool operator == (const IncludeInfo& other) const;
};

struct IncludeMenuItemInfo
{
    IncludeMenuItemInfo(IncludeInfo&& definition, std::wstring&& filename)
        : definition(std::move(definition)), filename(std::move(filename))
    {
    }
    IncludeInfo definition;
    std::wstring filename;
};

class CCmdHeaderSource : public ICommand
{
public:
    CCmdHeaderSource(void * obj);
    ~CCmdHeaderSource(void)
    {
    }

    virtual bool Execute() override { return false; }
    virtual UINT GetCmdId() override { return cmdHeaderSource; }

    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;

    virtual HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties) override;

    virtual void TabNotify(TBHDR * ptbhdr) override;

    virtual void ScintillaNotify(Scintilla::SCNotification * pScn) override;

    virtual void OnDocumentOpen(int id) override;

    virtual void OnDocumentSave(int index, bool bSaveAs) override;

private:

    bool PopulateMenu(const CDocument& doc, IUICollectionPtr& collection);
    void InvalidateIncludes();
    void InvalidateIncludesSource();
    void InvalidateIncludesEnabled();
    bool HandleSelectedMenuItem(size_t selected);
    bool IsValidMenuItem(size_t item);
    bool UserFindFile( HWND hwndParent,
        const std::wstring& filename, const std::wstring& defaultFolder, std::wstring& selectedFilename);
    bool IsServiceAvailable();

private:
    std::vector<IncludeMenuItemInfo> m_menuInfo;
    std::string m_includeRegEx;
    bool m_bStale;
    // TODO! Find out what requires this to be a memeber variable.
    // It crashes if on the stack etc. Might reveal bugs.
    CScintillaWnd m_edit;
};
