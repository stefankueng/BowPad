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
    int                         lexer;
    std::map<int, std::string>  keywordlist;
};

enum FontStyle
{
    FONTSTYLE_NORMAL,
    FONTSTYLE_BOLD,
    FONTSTYLE_ITALIC,
    FONTSTYLE_UNDERLINED
};

class StyleData
{
public:
    StyleData()
        : ForegroundColor(::GetSysColor(COLOR_WINDOWTEXT))
        , BackgroundColor(::GetSysColor(COLOR_WINDOW))
        , FontStyle(FONTSTYLE_NORMAL)
        , FontSize(0)
    {}

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

    const std::map<int, std::string>&   GetKeywordsForExt(const std::string& ext) const;
    const std::map<int, std::string>&   GetKeywordsForLang(const std::string& lang) const;

    const LexerData&                    GetLexerDataForExt(const std::string& ext) const;
    const LexerData&                    GetLexerDataForLang(const std::string& lang) const;

private:
    CLexStyles(void);
    ~CLexStyles(void);

    void                                Load();
    void                                ReplaceVariables(std::wstring& s, const std::map<std::wstring, std::wstring>& vars);
private:
    bool                m_bLoaded;

    std::map<std::string, std::string>  m_extLang;
    std::map<std::string, LanguageData> m_Langdata;
    std::map<int, LexerData>            m_lexerdata;
};
