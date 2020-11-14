// This file is part of BowPad.
//
// Copyright (C) 2013-2016, 2019-2020 - Stefan Kueng
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
#include "Document.h"
#include "ScintillaWnd.h"

#include <string>
#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <vector>

class LanguageData
{
public:
    LanguageData()
        : commentlineatstart(false)
        , functionregexsort(0)
        , userfunctions(0)
        , lexer(0)
    {
    }

    int                                  lexer;
    std::unordered_map<int, std::string> keywordlist;
    std::string                          commentline;
    bool                                 commentlineatstart;
    std::string                          commentstreamstart;
    std::string                          commentstreamend;
    std::string                          functionregex;
    std::vector<std::string>             functionregextrim;
    int                                  functionregexsort;
    int                                  userfunctions;
    std::unordered_set<std::string>      userkeywords;
    bool                                 userkeywordsupdated = false;
};

enum FontStyle
{
    FONTSTYLE_NORMAL     = 0,
    FONTSTYLE_BOLD       = 1,
    FONTSTYLE_ITALIC     = 2,
    FONTSTYLE_UNDERLINED = 4
};

class StyleData
{
public:
    StyleData();

    std::wstring Name;
    COLORREF     ForegroundColor;
    COLORREF     BackgroundColor;
    std::wstring FontName;
    FontStyle    FontStyle;
    int          FontSize;
    bool         eolfilled;
};

class LexerData
{
public:
    LexerData()
        : ID(0)
    {
    }
    int                                ID;
    std::unordered_map<int, StyleData> Styles;
    std::map<std::string, std::string> Properties;
};

class CLexStyles
{
public:
    static CLexStyles& Instance();

    std::vector<std::wstring> GetLanguages() const;
    std::string               GetLanguageForDocument(const CDocument& doc, CScintillaWnd& edit);
    std::wstring              GetUserExtensionsForLanguage(const std::wstring& lang) const;
    bool                      GetDefaultExtensionForLanguage(const std::string& lang, std::wstring& ext, UINT& index) const;

    size_t                   GetFilterSpceCount() const;
    const COMDLG_FILTERSPEC* GetFilterSpceData() const;

    const std::unordered_map<int, std::string>& GetKeywordsForLang(const std::string& lang);
    const std::unordered_map<int, std::string>& GetKeywordsForLexer(int lexer);
    LanguageData*                               GetLanguageData(const std::string& lang);
    const std::string&                          GetCommentLineForLang(const std::string& lang) const;
    const std::string&                          GetCommentStreamStartForLang(const std::string& lang) const;
    const std::string&                          GetCommentStreamEndForLang(const std::string& lang) const;
    bool                                        GetCommentLineAtStartForLang(const std::string& lang) const;
    const std::string&                          GetFunctionRegexForLang(const std::string& lang) const;
    int                                         GetFunctionRegexSortForLang(const std::string& lang) const;
    const std::vector<std::string>&             GetFunctionRegexTrimForLang(const std::string& lang) const;

    const LexerData&   GetLexerDataForLang(const std::string& lang) const;
    const LexerData&   GetLexerDataForLexer(int lexer) const;
    const std::string& GetLanguageForLexer(int lexer) const;

    void SetLangForPath(const std::wstring& path, const std::string& language);

    void        SetUserForeground(int ID, int style, COLORREF clr);
    void        SetUserBackground(int ID, int style, COLORREF clr);
    void        SetUserFont(int ID, int style, const std::wstring& font);
    void        SetUserFontSize(int ID, int style, int size);
    void        SetUserFontStyle(int ID, int style, FontStyle fontstyle);
    void        SetUserExt(const std::wstring& ext, const std::string& lang);
    void        ResetUserData();
    void        SaveUserData();
    bool        AddUserFunctionForLang(const std::string& lang, const std::string& fnc);
    std::string GetLanguageForPath(const std::wstring& path);
    void        GenerateUserKeywords(LanguageData& ld);
    void        Reload();

private:
    CLexStyles();
    ~CLexStyles();

    void Load();
    void ReplaceVariables(std::wstring& s, const std::unordered_map<std::wstring, std::wstring>& vars) const;
    void ParseStyle(LPCWSTR                                               styleName,
                    LPCWSTR                                               styleString,
                    const std::unordered_map<std::wstring, std::wstring>& variables,
                    StyleData&                                            style) const;

private:
    bool m_bLoaded;

    // Different languages may have the same file extension
    std::multimap<std::string, std::string> m_extLang;
    std::map<std::string, std::string>      m_fileLang;
    std::map<std::string, LanguageData>     m_Langdata;
    std::unordered_map<int, LexerData>      m_lexerdata;
    std::unordered_map<int, std::wstring>   m_lexerSection;

    std::unordered_map<int, LexerData>  m_userlexerdata;
    std::map<std::string, std::string>  m_userextLang;
    std::map<std::string, std::string>  m_autoextLang;
    std::map<std::wstring, std::string> m_pathsLang;
    std::list<std::wstring>             m_pathsForLang;
    // Used by the Save File Dialog filter.
    std::list<std::pair<std::wstring, std::wstring>> m_fileTypes;
    std::vector<COMDLG_FILTERSPEC>                   m_filterSpec;
};
