// This file is part of BowPad.
//
// Copyright (C) 2013-2016, 2019-2021 - Stefan Kueng
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
        : lexer(0)
        , commentLineAtStart(false)
        , functionRegexSort(0)
        , userFunctions(0)
    {
    }

    int                                  lexer;
    std::unordered_map<int, std::string> keywordList;
    std::string                          commentLine;
    bool                                 commentLineAtStart;
    std::string                          commentStreamStart;
    std::string                          commentStreamEnd;
    std::string                          functionRegex;
    std::vector<std::string>             functionRegexTrim;
    int                                  functionRegexSort;
    std::string                          autoCompleteRegex;
    int                                  userFunctions;
    std::unordered_set<std::string>      userKeyWords;
    bool                                 userKeyWordsUpdated = false;
    std::string                          autocompletionWords;
};

enum FontStyle
{
    Fontstyle_Normal     = 0,
    Fontstyle_Bold       = 1,
    Fontstyle_Italic     = 2,
    Fontstyle_Underlined = 4
};

class StyleData
{
public:
    StyleData();
    bool operator==(const StyleData& data) const = default;
    bool operator!=(const StyleData& data) const = default;

    std::wstring name;
    COLORREF     foregroundColor;
    COLORREF     backgroundColor;
    std::wstring fontName;
    FontStyle    fontStyle;
    int          fontSize;
    bool         eolFilled;
};

class LexerData
{
public:
    LexerData()
        : id(0)
    {
    }
    int                                id;
    std::string                        name;
    std::unordered_map<int, StyleData> styles;
    std::map<std::string, std::string> properties;
    std::map<std::string, std::string> annotations;
};

class CLexStyles
{
public:
    static CLexStyles& Instance();

    std::vector<std::wstring>            GetLanguages() const;
    std::map<std::string, LanguageData>& GetLanguageDataMap();
    std::string                          GetLanguageForDocument(const CDocument& doc, CScintillaWnd& edit);
    std::wstring                         GetUserExtensionsForLanguage(const std::wstring& lang) const;
    bool                                 GetDefaultExtensionForLanguage(const std::string& lang, std::wstring& ext, UINT& index) const;
    bool                                 IsLanguageHidden(const std::wstring& lang) const;
    size_t                               GetFilterSpecCount() const;
    const COMDLG_FILTERSPEC*             GetFilterSpecData() const;

    const std::unordered_map<int, std::string>&  GetKeywordsForLang(const std::string& lang);
    const std::unordered_map<int, std::string>&  GetKeywordsForLexer(int lexer);
    LanguageData*                                GetLanguageData(const std::string& lang);
    const std::string&                           GetCommentLineForLang(const std::string& lang) const;
    const std::string&                           GetCommentStreamStartForLang(const std::string& lang) const;
    const std::string&                           GetCommentStreamEndForLang(const std::string& lang) const;
    bool                                         GetCommentLineAtStartForLang(const std::string& lang) const;
    const std::string&                           GetFunctionRegexForLang(const std::string& lang) const;
    int                                          GetFunctionRegexSortForLang(const std::string& lang) const;
    const std::vector<std::string>&              GetFunctionRegexTrimForLang(const std::string& lang) const;
    const std::string&                           GetAutoCompleteRegexForLang(const std::string& lang) const;
    std::unordered_map<std::string, std::string> GetAutoCompleteWords() const;

    const LexerData&   GetLexerDataForLang(const std::string& lang) const;
    const LexerData&   GetLexerDataForLexer(int lexer) const;
    const std::string& GetLanguageForLexer(int lexer) const;

    void SetLangForPath(const std::wstring& path, const std::string& language);

    void        SetUserForeground(int id, int style, COLORREF clr);
    void        SetUserBackground(int id, int style, COLORREF clr);
    void        SetUserFont(int id, int style, const std::wstring& font);
    void        SetUserFontSize(int id, int style, int size);
    void        SetUserFontStyle(int id, int style, FontStyle fontstyle);
    void        SetUserExt(const std::wstring& ext, const std::string& lang);
    void        SetLanguageHidden(const std::wstring& lang, bool hidden);
    void        ResetUserData();
    void        SaveUserData();
    bool        AddUserFunctionForLang(const std::string& lang, const std::string& fnc);
    std::string GetLanguageForPath(const std::wstring& path);
    void        GenerateUserKeywords(LanguageData& ld) const;
    void        Reload();

private:
    CLexStyles();
    ~CLexStyles();

    void        Load();
    static void ReplaceVariables(std::wstring& s, const std::unordered_map<std::wstring, std::wstring>& vars);
    void        ParseStyle(LPCWSTR                                               styleName,
                           LPCWSTR                                               styleString,
                           const std::unordered_map<std::wstring, std::wstring>& variables,
                           StyleData&                                            style) const;

private:
    bool m_bLoaded;

    // Different languages may have the same file extension
    std::multimap<std::string, std::string> m_extLang;
    std::map<std::string, std::string>      m_fileLang;
    std::map<std::string, LanguageData>     m_langData;
    std::unordered_map<int, LexerData>      m_lexerData;
    std::unordered_map<int, std::wstring>   m_lexerSection;
    std::set<std::wstring>                  m_hiddenLangs;

    std::unordered_map<int, LexerData>  m_userLexerData;
    std::map<std::string, std::string>  m_userExtLang;
    std::map<std::string, std::string>  m_autoExtLang;
    std::map<std::wstring, std::string> m_pathsLang;
    std::list<std::wstring>             m_pathsForLang;
    // Used by the Save File Dialog filter.
    std::list<std::pair<std::wstring, std::wstring>> m_fileTypes;
    std::vector<COMDLG_FILTERSPEC>                   m_filterSpec;
};
