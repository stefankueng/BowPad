#pragma once

#include <string>
#include <map>

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
    static CLexStyles&          Instance();

    std::map<int, std::string>  GetKeywordsForExt(const std::string& ext) const;
    std::map<int, std::string>  GetKeywordsForLang(const std::string& lang) const;

    LexerData                   GetLexerDataForExt(const std::string& ext) const;
    LexerData                   GetLexerDataForLang(const std::string& lang) const;

private:
    CLexStyles(void);
    ~CLexStyles(void);

    void                Load();
private:
    bool                m_bLoaded;

    std::map<std::string, std::string>  m_extLang;
    std::map<std::string, LanguageData> m_Langdata;
    std::map<int, LexerData>            m_lexerdata;
};

