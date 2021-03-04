// This file is part of BowPad.
//
// Copyright (C) 2021 - Stefan Kueng
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
#include "StringUtils.h"
#include "../ext/scintilla/include/Sci_Position.h"
#include <mutex>


class CMainWindow;
class CScintillaWnd;
class DocID;
struct SCNotification;

enum class AutoCompleteType : int
{
    None = -1,
    Code = 0,
    Path,
    Snippet,
};

class CAutoComplete
{
public:
    CAutoComplete(CMainWindow* main, CScintillaWnd* scintilla);
    virtual ~CAutoComplete();

    void Init();
    void HandleScintillaEvents(const SCNotification* scn);
    bool HandleChar(WPARAM wParam, LPARAM lParam);
    void AddWords(const std::string& lang, std::map<std::string, AutoCompleteType>&& words);
    void AddWords(const std::string& lang, const std::map<std::string, AutoCompleteType>& words);
    void AddWords(const DocID& docID, std::map<std::string, AutoCompleteType>&& words);
    void AddWords(const DocID& docID, const std::map<std::string, AutoCompleteType>& words);

private:
    void HandleAutoComplete(const SCNotification* scn);
    void ExitSnippetMode();
    void MarkSnippetPositions(bool clearOnly);
    std::string SanitizeSnippetText(const std::string& text) const;

private:
    CScintillaWnd* m_editor;
    CMainWindow*   m_main;
    // map of [language, [word, AutoCompleteType]]
    std::map<std::string, std::map<std::string, AutoCompleteType, ci_less>> m_langWordList;
    std::map<std::string, std::map<std::string, std::string>>               m_langSnippetList;
    std::map<DocID, std::map<std::string, AutoCompleteType, ci_less>>       m_docWordList;
    std::mutex                                                              m_mutex;
    bool                                                                    m_insertingSnippet;
    std::string                                                             m_stringToSelect;
    std::map<int, std::vector<Sci_Position>>                                m_snippetPositions;
    int                                                                     m_currentSnippetPos;
};
