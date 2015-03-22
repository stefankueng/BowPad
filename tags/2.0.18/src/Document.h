// This file is part of BowPad.
//
// Copyright (C) 2013-2015 - Stefan Kueng
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
#include "Scintilla.h"

typedef uptr_t Document;

enum FormatType { UNKNOWN_FORMAT, WIN_FORMAT, MAC_FORMAT, UNIX_FORMAT };

std::wstring FormatTypeToString(FormatType ft);

class CPosData
{
public:
    CPosData()
        : m_nFirstVisibleLine(0)
        , m_nStartPos(0)
        , m_nEndPos(0)
        , m_xOffset(0)
        , m_nSelMode(0)
        , m_nScrollWidth(1)
    {};
    ~CPosData(){}

    size_t m_nFirstVisibleLine;
    size_t m_nStartPos;
    size_t m_nEndPos;
    size_t m_xOffset;
    size_t m_nSelMode;
    size_t m_nScrollWidth;
};

class CDocument
{
public:
    CDocument()
        : m_document(NULL)
        , m_format(UNKNOWN_FORMAT)
        , m_bHasBOM(false)
        , m_bTrimBeforeSave(false)
        , m_bEnsureNewlineAtEnd(false)
        , m_encoding(-1)
        , m_bIsDirty(false)
        , m_bNeedsSaving(false)
        , m_bIsReadonly(false)
        , m_bDoSaveAs(false)
    {
        m_lastWriteTime.dwHighDateTime = 0;
        m_lastWriteTime.dwLowDateTime  = 0;
    }

    std::wstring GetEncodingString();

    Document                m_document;
    FormatType              m_format;
    bool                    m_bHasBOM;
    bool                    m_bTrimBeforeSave;
    bool                    m_bEnsureNewlineAtEnd;
    int                     m_encoding;
    std::wstring            m_path;
    FILETIME                m_lastWriteTime;
    std::wstring            m_language;
    CPosData                m_position;
    bool                    m_bIsDirty;
    bool                    m_bNeedsSaving;
    bool                    m_bIsReadonly;
    bool                    m_bDoSaveAs;        ///< even if m_path is set, always ask where to save
};
