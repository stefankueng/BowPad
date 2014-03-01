// This file is part of BowPad.
//
// Copyright (C) 2014 - Stefan Kueng
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
#include "EditorConfigHandler.h"
#include "UnicodeUtils.h"



CEditorConfigHandler::CEditorConfigHandler(void)
{
}

CEditorConfigHandler::~CEditorConfigHandler(void)
{
    for (auto handle : m_handles)
    {
        editorconfig_handle_destroy(handle.second);
    }
}

CEditorConfigHandler& CEditorConfigHandler::Instance()
{
    static CEditorConfigHandler instance;
    return instance;
}

void CEditorConfigHandler::ApplySettingsForPath(const std::wstring& path, CScintillaWnd * pScintilla, CDocument& doc)
{
    if (path.empty())
        return;
    auto it = m_handles.find(path);
    if (it == m_handles.end())
    {
        editorconfig_handle eh = editorconfig_handle_init();

        if (editorconfig_parse(CUnicodeUtils::StdGetANSI(path).c_str(), eh) == 0)
        {
            m_handles[path] = eh;
            it = m_handles.find(path);
        }
        else
            editorconfig_handle_destroy(eh);
    }
    if (it != m_handles.end())
    {
        int name_value_count = editorconfig_handle_get_name_value_count(it->second);
        for (int j = 0; j < name_value_count; ++j)
        {
            const char*         name;
            const char*         value;
            editorconfig_handle_get_name_value(it->second, j, &name, &value);

            if (strcmp(name, "indent_style") == 0)
            {
                // tab / space
                pScintilla->Call(SCI_SETUSETABS, strcmp(value, "tab") == 0);
            }
            if (strcmp(name, "indent_size") == 0)
            {
                // tab / number
                pScintilla->Call(SCI_SETINDENT, strcmp(value, "tab") == 0 ? 0 : atoi(value));
            }
            if (strcmp(name, "tab_width") == 0)
            {
                // number
                pScintilla->Call(SCI_SETTABWIDTH, atoi(value));
            }
            if (strcmp(name, "end_of_line") == 0)
            {
                // lf / cr / crlf
                int eol = (int)pScintilla->Call(SCI_GETEOLMODE);
                if (strcmp(value, "lf") == 0)
                    eol =  SC_EOL_LF;
                if (strcmp(value, "cr") == 0)
                    eol = SC_EOL_CR;
                if (strcmp(value, "crlf") == 0)
                    eol = SC_EOL_CRLF;

                pScintilla->Call(SCI_SETEOLMODE, eol);
                //pScintilla->Call(SCI_CONVERTEOLS, eol);
            }
            if (strcmp(name, "charset") == 0)
            {
                // latin1 / utf-8 / utf-16be / utf-16le
                if (strcmp(value, "latin1") == 0)
                {
                    doc.m_encoding = CP_ACP;
                    doc.m_bHasBOM = false;
                }
                if (strcmp(value, "utf-8") == 0)
                {
                    doc.m_encoding = CP_UTF8;
                    doc.m_bHasBOM = false;
                }
                if (strcmp(value, "utf-16le") == 0)
                {
                    doc.m_encoding = 1200;
                    doc.m_bHasBOM = false;
                }
                if (strcmp(value, "utf-16be") == 0)
                {
                    doc.m_encoding = 1201;
                    doc.m_bHasBOM = false;
                }
            }
            if (strcmp(name, "trim_trailing_whitespace") == 0)
            {
                // true / false
            }
            if (strcmp(name, "insert_final_newline") == 0)
            {
                // true / false
            }
        }
    }
}
