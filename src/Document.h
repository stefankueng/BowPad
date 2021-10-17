// This file is part of BowPad.
//
// Copyright (C) 2013-2018, 2020-2021 - Stefan Kueng
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
#include "ScintillaTypes.h"
#include <functional>

using Document = void*;

enum class EOLFormat : int
{
    Unknown_Format,
    Win_Format,
    Mac_Format,
    Unix_Format
};
enum class TabSpace : int
{
    Default,
    Tabs,
    Spaces
};

std::wstring         getEolFormatDescription(EOLFormat ft);
EOLFormat            toEolFormat(Scintilla::EndOfLine eolMode);
Scintilla::EndOfLine toEolMode(EOLFormat eolFormat);

class CPosData
{
public:
    CPosData()
        : m_nFirstVisibleLine(0)
        , m_nWrapLineOffset(0)
        , m_nStartPos(0)
        , m_nEndPos(0)
        , m_xOffset(0)
        , m_nSelMode(Scintilla::SelectionMode::Stream)
        , m_nScrollWidth(1)
        , m_lastStyleLine(0){};
    ~CPosData()
    {
    }

    sptr_t                   m_nFirstVisibleLine;
    sptr_t                   m_nWrapLineOffset;
    sptr_t                   m_nStartPos;
    sptr_t                   m_nEndPos;
    sptr_t                   m_xOffset;
    Scintilla::SelectionMode m_nSelMode;
    sptr_t                   m_nScrollWidth;
    std::vector<sptr_t>      m_lineStateVector;
    sptr_t                   m_lastStyleLine;
};

class CDocument
{
public:
    CDocument()
        : m_document(nullptr)
        , m_encoding(-1)
        , m_encodingSaving(-1)
        , m_format(EOLFormat::Win_Format)
        , m_bHasBOM(false)
        , m_bHasBOMSaving(false)
        , m_bTrimBeforeSave(false)
        , m_bEnsureNewlineAtEnd(false)
        , m_bIsDirty(false)
        , m_bNeedsSaving(false)
        , m_bIsReadonly(false)
        , m_bIsWriteProtected(false)
        , m_bDoSaveAs(false)
        , m_tabSpace(TabSpace::Default)
        , m_readDir(Scintilla::Bidirectional::Disabled)
    {
        m_lastWriteTime.dwHighDateTime = 0;
        m_lastWriteTime.dwLowDateTime  = 0;
    }

    std::wstring       GetEncodingString() const;
    const std::string& GetLanguage() const
    {
        return m_language;
    }
    void SetLanguage(const std::string& lang);

    Document                 m_document;
    std::wstring             m_path;
    int                      m_encoding;
    int                      m_encodingSaving;
    EOLFormat                m_format;
    bool                     m_bHasBOM;
    bool                     m_bHasBOMSaving;
    bool                     m_bTrimBeforeSave;
    bool                     m_bEnsureNewlineAtEnd;
    bool                     m_bIsDirty;
    bool                     m_bNeedsSaving;
    bool                     m_bIsReadonly;
    bool                     m_bIsWriteProtected;
    bool                     m_bDoSaveAs; ///< even if m_path is set, always ask where to save
    FILETIME                 m_lastWriteTime;
    CPosData                 m_position;
    TabSpace                 m_tabSpace;
    Scintilla::Bidirectional m_readDir;
    std::function<void()>    m_saveCallback;

private:
    std::string m_language;
};
