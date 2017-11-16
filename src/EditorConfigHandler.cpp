// This file is part of BowPad.
//
// Copyright (C) 2014, 2016-2017 - Stefan Kueng
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
#include "Document.h"


CEditorConfigHandler::CEditorConfigHandler()
{
}

CEditorConfigHandler::~CEditorConfigHandler()
{
    for (const auto& handle : m_handles)
    {
        editorconfig_handle_destroy(handle.second);
    }
}

CEditorConfigHandler& CEditorConfigHandler::Instance()
{
    static CEditorConfigHandler instance;
    return instance;
}

void CEditorConfigHandler::ApplySettingsForPath(const std::wstring& path, CScintillaWnd * pScintilla, CDocument& doc, bool keepEncoding)
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

            if ((doc.m_TabSpace == Default) && (strcmp(name, "indent_style") == 0))
            {
                // tab / space
                pScintilla->Call(SCI_SETUSETABS, strcmp(value, "tab") == 0);
            }
            else if ((doc.m_TabSpace == Default) && (strcmp(name, "indent_size") == 0))
            {
                // tab / number
                pScintilla->Call(SCI_SETINDENT, strcmp(value, "tab") == 0 ? 0 : atoi(value));
            }
            else if ((doc.m_TabSpace == Default) && (strcmp(name, "tab_width") == 0))
            {
                // number
                pScintilla->Call(SCI_SETTABWIDTH, atoi(value));
            }
            else if (strcmp(name, "end_of_line") == 0)
            {
                // lf / cr / crlf
                if (strcmp(value, "lf") == 0)
                {
                    pScintilla->Call(SCI_SETEOLMODE, SC_EOL_LF);
                    if (doc.m_format != UNIX_FORMAT)
                    {
                        doc.m_format = UNIX_FORMAT;
                        if (!doc.m_bIsReadonly && !doc.m_bIsWriteProtected)
                            pScintilla->Call(SCI_CONVERTEOLS, SC_EOL_LF);
                    }
                }
                else if (strcmp(value, "cr") == 0)
                {
                    pScintilla->Call(SCI_SETEOLMODE, SC_EOL_CR);
                    if (doc.m_format != MAC_FORMAT)
                    {
                        doc.m_format = MAC_FORMAT;
                        if (!doc.m_bIsReadonly && !doc.m_bIsWriteProtected)
                            pScintilla->Call(SCI_CONVERTEOLS, SC_EOL_CR);
                    }
                }
                else if (strcmp(value, "crlf") == 0)
                {
                    pScintilla->Call(SCI_SETEOLMODE, SC_EOL_CRLF);
                    if (doc.m_format != WIN_FORMAT)
                    {
                        doc.m_format = WIN_FORMAT;
                        if (!doc.m_bIsReadonly && !doc.m_bIsWriteProtected)
                            pScintilla->Call(SCI_CONVERTEOLS, SC_EOL_CRLF);
                    }
                }

                //pScintilla->Call(SCI_CONVERTEOLS, eol);
            }
            else if ((strcmp(name, "charset") == 0) && (!keepEncoding))
            {
                // latin1 / utf-8 / utf-16be / utf-16le
                if (strcmp(value, "latin1") == 0)
                {
                    doc.m_encodingSaving = CP_ACP;
                    doc.m_bHasBOMSaving = false;
                }
                else if (strcmp(value, "utf-8") == 0)
                {
                    doc.m_encodingSaving = CP_UTF8;
                    doc.m_bHasBOMSaving = false;
                }
                else if (strcmp(value, "utf-16le") == 0)
                {
                    doc.m_encodingSaving = 1200;
                    doc.m_bHasBOMSaving = false;
                }
                else if (strcmp(value, "utf-16le-bom") == 0)
                {
                    doc.m_encodingSaving = 1200;
                    doc.m_bHasBOMSaving = true;
                }
                else if (strcmp(value, "utf-16be") == 0)
                {
                    doc.m_encodingSaving = 1201;
                    doc.m_bHasBOMSaving = false;
                }
                else if (strcmp(value, "utf-16be-bom") == 0)
                {
                    doc.m_encodingSaving = 1201;
                    doc.m_bHasBOMSaving = true;
                }
            }
            else if (strcmp(name, "trim_trailing_whitespace") == 0)
            {
                // true / false
                doc.m_bTrimBeforeSave = strcmp(value, "true") == 0;
            }
            else if (strcmp(name, "insert_final_newline") == 0)
            {
                // true / false
                doc.m_bEnsureNewlineAtEnd = strcmp(value, "true") == 0;
            }
        }
    }
}
