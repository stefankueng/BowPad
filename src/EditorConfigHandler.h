// This file is part of BowPad.
//
// Copyright (C) 2014 - 2016 Stefan Kueng
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

class CEditorConfigHandler
{
private:
    CEditorConfigHandler();
    ~CEditorConfigHandler();

public:
    static CEditorConfigHandler& Instance();

    void ApplySettingsForPath(const std::wstring& path, CScintillaWnd * pScintilla, CDocument& doc);

private:
    std::map<std::wstring, editorconfig_handle> m_handles;
};
