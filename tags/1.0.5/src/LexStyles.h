// This file is part of BowPad.
//
// Copyright (C) 2013 - Stefan Kueng
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
#include <vector>

class LanguageData
{
public:
    LanguageData()
        : commentlineatstart(false)
        , functionregexsort(0)
    {
    }

    int                         lexer;
    std::map<int, std::string>  keywordlist;
    std::string                 commentline;
    bool                        commentlineatstart;
    std::string                 commentstreamstart;
    std::string                 commentstreamend;
    std::string                 functionregex;
    std::vector<std::string>    functionregextrim;
    int                         functionregexsort;
};

enum FontStyle
{
    FONTSTYLE_NORMAL            = 0,
    FONTSTYLE_BOLD              = 1,
    FONTSTYLE_ITALIC            = 2,
    FONTSTYLE_UNDERLINED        = 4
};

class StyleData
{
public:
    StyleData();

    std::wstring        Name;
    COLORREF            ForegroundColor;
    COLORREF            BackgroundColor;
    std::wstring        FontName;
    FontStyle           FontStyle;
    int                 FontSize;
};

class LexerData
{
public:
    LexerData()
        : ID(0)
    {}
    int                                 ID;
    std::map<int, StyleData>            Styles;
    std::map<std::string, std::string>  Properties;
};

class CLexStyles
{
public:
    static CLexStyles&                  Instance();

    std::vector<std::wstring>           GetLanguages() const;
    std::wstring                        GetLanguageForExt(const std::wstring& ext) const;
    std::wstring                        GetUserExtensionsForLanguage(const std::wstring& lang) const;

    const std::map<int, std::string>&   GetKeywordsForExt(const std::string& ext) const;
    const std::map<int, std::string>&   GetKeywordsForLang(const std::string& lang) const;
    const std::string&                  GetCommentLineForLang(const std::string& lang) const;
    const std::string&                  GetCommentStreamStartForLang(const std::string& lang) const;
    const std::string&                  GetCommentStreamEndForLang(const std::string& lang) const;
    bool                                GetCommentLineAtStartForLang(const std::string& lang) const;
    const std::string&                  GetFunctionRegexForLang(const std::string& lang) const;
    int                                 GetFunctionRegexSortForLang(const std::string& lang) const;
    const std::vector<std::string>&     GetFunctionRegexTrimForLang(const std::string& lang) const;

    const LexerData&                    GetLexerDataForExt(const std::string& ext) const;
    const LexerData&                    GetLexerDataForLang(const std::string& lang) const;

    void                                SetUserForeground(int ID, int style, COLORREF clr);
    void                                SetUserBackground(int ID, int style, COLORREF clr);
    void                                SetUserFont(int ID, int style, const std::wstring& font);
    void                                SetUserFontSize(int ID, int style, int size);
    void                                SetUserFontStyle(int ID, int style, FontStyle fontstyle);
    void                                SetUserExt(const std::wstring& ext, const std::wstring& lang);
    void                                ResetUserData();
    void                                SaveUserData();
private:
    CLexStyles(void);
    ~CLexStyles(void);

    void                                Load();
    void                                ReplaceVariables(std::wstring& s, const std::map<std::wstring, std::wstring>& vars);
private:
    bool                                m_bLoaded;

    std::map<std::string, std::string>  m_extLang;
    std::map<std::string, LanguageData> m_Langdata;
    std::map<int, LexerData>            m_lexerdata;
    std::map<int, std::wstring>         m_lexerSection;

    std::map<int, LexerData>            m_userlexerdata;
    std::map<std::string, std::string>  m_userextLang;
};
