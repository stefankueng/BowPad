﻿// This file is part of BowPad.
//
// Copyright (C) 2014, 2016-2017, 2020-2021 - Stefan Kueng
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
    for (const auto& [path, data] : m_handles)
    {
        editorconfig_handle_destroy(data.handle);
    }
}

CEditorConfigHandler& CEditorConfigHandler::Instance()
{
    static CEditorConfigHandler instance;
    return instance;
}

void CEditorConfigHandler::ApplySettingsForPath(const std::wstring& path, CScintillaWnd* pScintilla, CDocument& doc, bool keepEncoding)
{
    if (path.empty())
        return;
    auto it = m_handles.find(path);
    if (it == m_handles.end())
    {
        editorconfig_handle eh = editorconfig_handle_init();

        if (editorconfig_parse(CUnicodeUtils::StdGetANSI(path).c_str(), eh) == 0)
        {
            EditorConfigData data{};
            data.handle     = eh;
            data.enabled    = true;
            m_handles[path] = data;
            it              = m_handles.find(path);
        }
        else
            editorconfig_handle_destroy(eh);
    }
    if (it != m_handles.end())
    {
        if (!it->second.enabled)
            return;
        int nameValueCount = editorconfig_handle_get_name_value_count(it->second.handle);
        for (int j = 0; j < nameValueCount; ++j)
        {
            const char* name  = nullptr;
            const char* value = nullptr;
            editorconfig_handle_get_name_value(it->second.handle, j, &name, &value);

            if ((doc.m_tabSpace == TabSpace::Default) && (strcmp(name, "indent_style") == 0))
            {
                // tab / space
                pScintilla->Scintilla().SetUseTabs(strcmp(value, "tab") == 0);
            }
            else if ((doc.m_tabSpace == TabSpace::Default) && (strcmp(name, "indent_size") == 0))
            {
                // tab / number
                pScintilla->Scintilla().SetIndent(strcmp(value, "tab") == 0 ? 0 : atoi(value));
            }
            else if ((doc.m_tabSpace == TabSpace::Default) && (strcmp(name, "tab_width") == 0))
            {
                // number
                pScintilla->Scintilla().SetTabWidth(atoi(value));
            }
            else if (strcmp(name, "end_of_line") == 0)
            {
                // lf / cr / crlf
                if (strcmp(value, "lf") == 0)
                {
                    pScintilla->Scintilla().SetEOLMode(Scintilla::EndOfLine::Lf);
                    if (doc.m_format != EOLFormat::Unix_Format)
                    {
                        doc.m_format = EOLFormat::Unix_Format;
                        if (!doc.m_bIsReadonly && !doc.m_bIsWriteProtected)
                            pScintilla->Scintilla().ConvertEOLs(Scintilla::EndOfLine::Lf);
                    }
                }
                else if (strcmp(value, "cr") == 0)
                {
                    pScintilla->Scintilla().SetEOLMode(Scintilla::EndOfLine::Cr);
                    if (doc.m_format != EOLFormat::Mac_Format)
                    {
                        doc.m_format = EOLFormat::Mac_Format;
                        if (!doc.m_bIsReadonly && !doc.m_bIsWriteProtected)
                            pScintilla->Scintilla().ConvertEOLs(Scintilla::EndOfLine::Cr);
                    }
                }
                else if (strcmp(value, "crlf") == 0)
                {
                    pScintilla->Scintilla().SetEOLMode(Scintilla::EndOfLine::CrLf);
                    if (doc.m_format != EOLFormat::Win_Format)
                    {
                        doc.m_format = EOLFormat::Win_Format;
                        if (!doc.m_bIsReadonly && !doc.m_bIsWriteProtected)
                            pScintilla->Scintilla().ConvertEOLs(Scintilla::EndOfLine::CrLf);
                    }
                }

                //pScintilla->Scintilla().ConvertEOLs(eol);
            }
            else if ((strcmp(name, "charset") == 0) && (!keepEncoding))
            {
                // latin1 / utf-8 / utf-16be / utf-16le
                if (strcmp(value, "latin1") == 0)
                {
                    doc.m_encodingSaving = CP_ACP;
                    doc.m_bHasBOMSaving  = false;
                }
                else if (strcmp(value, "utf-8") == 0)
                {
                    doc.m_encodingSaving = CP_UTF8;
                    doc.m_bHasBOMSaving  = false;
                }
                else if (strcmp(value, "utf-16le") == 0)
                {
                    doc.m_encodingSaving = 1200;
                    doc.m_bHasBOMSaving  = false;
                }
                else if (strcmp(value, "utf-16le-bom") == 0)
                {
                    doc.m_encodingSaving = 1200;
                    doc.m_bHasBOMSaving  = true;
                }
                else if (strcmp(value, "utf-16be") == 0)
                {
                    doc.m_encodingSaving = 1201;
                    doc.m_bHasBOMSaving  = false;
                }
                else if (strcmp(value, "utf-16be-bom") == 0)
                {
                    doc.m_encodingSaving = 1201;
                    doc.m_bHasBOMSaving  = true;
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

bool CEditorConfigHandler::IsEnabled(const std::wstring& path)
{
    if (path.empty())
        return false;
    auto it = m_handles.find(path);

    if (it != m_handles.end())
    {
        int nameValueCount = editorconfig_handle_get_name_value_count(it->second.handle);
        return (nameValueCount != 0) && it->second.enabled;
    }
    return false;
}

bool CEditorConfigHandler::HasTabSize(const std::wstring& path)
{
    return HasOption(path, "tab_width");
}

bool CEditorConfigHandler::HasTabSpace(const std::wstring& path)
{
    return HasOption(path, "indent_style");
}

bool CEditorConfigHandler::HasOption(const std::wstring& path, const char* option)
{
    if (path.empty())
        return false;
    auto it = m_handles.find(path);

    if (it != m_handles.end())
    {
        int nameValueCount = editorconfig_handle_get_name_value_count(it->second.handle);
        if (it->second.enabled)
        {
            for (int j = 0; j < nameValueCount; ++j)
            {
                const char* name  = nullptr;
                const char* value = nullptr;
                editorconfig_handle_get_name_value(it->second.handle, j, &name, &value);

                if (strcmp(name, option) == 0)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

void CEditorConfigHandler::EnableForPath(const std::wstring& path, bool enable)
{
    auto it = m_handles.find(path);

    if (it != m_handles.end())
    {
        it->second.enabled = enable;
    }
}
