// This file is part of BowPad.
//
// Copyright (C) 2014-2017, 2020-2021 Stefan Kueng
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
#include <string>
#include <map>
#include "ScintillaWnd.h"
#include "editorconfig/editorconfig.h"

class CDocument;

class EditorConfigData
{
public:
    EditorConfigData()
        : enabled(true)
        , handle(nullptr)
    { }

    editorconfig_handle handle;
    bool enabled;
};

class CEditorConfigHandler
{
private:
    CEditorConfigHandler();
    ~CEditorConfigHandler();

public:
    static CEditorConfigHandler& Instance();

    void ApplySettingsForPath(const std::wstring& path, CScintillaWnd * pScintilla, CDocument& doc, bool keepEncoding);
    bool IsEnabled(const std::wstring& path);
    bool HasTabSize(const std::wstring& path);
    bool HasTabSpace(const std::wstring& path);
    bool HasOption(const std::wstring& path, const char* option);
    void EnableForPath(const std::wstring& path, bool enable);

private:
    std::map<std::wstring, EditorConfigData> m_handles;
};
