// This file is part of BowPad.
//
// Copyright (C) 2013-2014 - Stefan Kueng
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
#include "LexStyles.h"
#include "SimpleIni.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "BowPad.h"
#include "AppUtils.h"

#include <vector>

LexerData emptyLexData;
std::map<int, std::string> emptyIntStrVec;
std::string emptyString;
std::vector<std::string> emptyStringVector;

static COLORREF fgColor = ::GetSysColor(COLOR_WINDOWTEXT);
static COLORREF bgColor = ::GetSysColor(COLOR_WINDOW);

StyleData::StyleData()
    : ForegroundColor(fgColor)
    , BackgroundColor(bgColor)
    , FontStyle(FONTSTYLE_NORMAL)
    , FontSize(0)
{

}

CLexStyles::CLexStyles(void)
    : m_bLoaded(false)
{
}

CLexStyles::~CLexStyles(void)
{
}

CLexStyles& CLexStyles::Instance()
{
    static CLexStyles instance;
    if (!instance.m_bLoaded)
        instance.Load();
    return instance;
}

void CLexStyles::Load()
{
    CSimpleIni ini[2];
    std::wstring userStyleFile = CAppUtils::GetDataPath() + L"\\userconfig";

    if (PathFileExists(userStyleFile.c_str()))
    {
        ini[1].LoadFile(userStyleFile.c_str());
    }
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(IDR_LEXSTYLES), L"config");
    if (hRes)
    {
        HGLOBAL hResourceLoaded = LoadResource(NULL, hRes);
        if (hResourceLoaded)
        {
            char * lpResLock = (char *) LockResource(hResourceLoaded);
            DWORD dwSizeRes = SizeofResource(NULL, hRes);
            if (lpResLock)
            {
                ini[0].LoadFile(lpResLock, dwSizeRes);



                std::map<std::wstring, std::wstring> variables;
                for (int iniind = 0; iniind < _countof(ini); ++iniind)
                {
                    CSimpleIni::TNamesDepend lexvars;
                    ini[iniind].GetAllKeys(L"variables", lexvars);
                    for (const auto& l : lexvars)
                    {
                        std::wstring v = ini[iniind].GetValue(L"variables", l);
                        std::wstring key = l;
                        key = L"$(" + key + L")";
                        variables[key] = v;
                    }
                }

                std::map<std::wstring, int> lexers;
                for (int iniind = 0; iniind < _countof(ini); ++iniind)
                {
                    CSimpleIni::TNamesDepend lexkeys;
                    ini[iniind].GetAllKeys(L"lexers", lexkeys);
                    for (const auto& l : lexkeys)
                    {
                        int lex = _wtoi(ini[iniind].GetValue(l, L"Lexer", L""));
                        m_lexerSection[lex] = l;
                    }
                    for (const auto& l : m_lexerSection)
                    {
                        std::wstring v = ini[iniind].GetValue(L"lexers", l.second.c_str(), L"");
                        int lex = _wtoi(ini[iniind].GetValue(l.second.c_str(), L"Lexer", L""));
                        if (!v.empty())
                        {
                            ReplaceVariables(v, variables);
                            std::vector<std::wstring> langs;
                            stringtok(langs, v, true, L";");
                            for (const auto& x : langs)
                            {
                                lexers[x] = lex;
                            }
                        }

                        // now parse all the data for this lexer
                        LexerData lexerdata = m_lexerdata[lex];
                        LexerData userlexerdata;
                        lexerdata.ID = lex;
                        userlexerdata.ID = lex;
                        CSimpleIni::TNamesDepend lexdatakeys;
                        ini[iniind].GetAllKeys(l.second.c_str(), lexdatakeys);
                        for (const auto& it : lexdatakeys)
                        {
                            if (_wcsnicmp(L"Style", it, 5) == 0)
                            {
                                StyleData style;
                                std::wstring v = ini[iniind].GetValue(l.second.c_str(), it);
                                ReplaceVariables(v, variables);
                                std::vector<std::wstring> vec;
                                stringtok(vec, v, false, L";");
                                int i = 0;
                                wchar_t * endptr;
                                unsigned long hexval;
                                for (const auto& s : vec)
                                {
                                    switch (i)
                                    {
                                    case 0: // Name
                                        style.Name = s;
                                        break;
                                    case 1: // Foreground color
                                        hexval = wcstol(s.c_str(), &endptr, 16);
                                        style.ForegroundColor = (RGB((hexval >> 16) & 0xFF, (hexval >> 8) & 0xFF, hexval & 0xFF)) | (hexval & 0xFF000000);
                                        break;
                                    case 2: // Background color
                                        hexval = wcstol(s.c_str(), &endptr, 16);
                                        style.BackgroundColor = (RGB((hexval >> 16) & 0xFF, (hexval >> 8) & 0xFF, hexval & 0xFF)) | (hexval & 0xFF000000);
                                        break;
                                    case 3: // Font name
                                        style.FontName = s;
                                        break;
                                    case 4: // Font style
                                        style.FontStyle = (FontStyle)_wtoi(s.c_str());
                                        break;
                                    case 5: // Font size
                                        style.FontSize = _wtoi(s.c_str());
                                        break;
                                    case 6: // Override default background color in case the style was set with a variable
                                        hexval = wcstol(s.c_str(), &endptr, 16);
                                        style.BackgroundColor = (RGB((hexval >> 16) & 0xFF, (hexval >> 8) & 0xFF, hexval & 0xFF)) | (hexval & 0xFF000000);
                                        break;
                                    }
                                    ++i;
                                }

                                lexerdata.Styles[_wtoi(it+5)] = style;
                                userlexerdata.Styles[_wtoi(it+5)] = style;
                            }
                            if (_wcsnicmp(L"Prop_", it, 5) == 0)
                            {
                                std::string n = CUnicodeUtils::StdGetUTF8(it+5);
                                std::string v = CUnicodeUtils::StdGetUTF8(ini[iniind].GetValue(l.second.c_str(), it, L""));
                                lexerdata.Properties[n] = v;
                                userlexerdata.Properties[n] = v;
                            }
                        }
                        m_lexerdata[lexerdata.ID] = std::move(lexerdata);
                        if (iniind == 1)
                            m_userlexerdata[lexerdata.ID] = std::move(userlexerdata);
                    }
                }

                for (int iniind = 0; iniind < _countof(ini); ++iniind)
                {
                    CSimpleIni::TNamesDepend autolangkeys;
                    ini[iniind].GetAllKeys(L"autolanguage", autolangkeys);
                    for (const auto& k : autolangkeys)
                    {
                        std::wstring v = ini[iniind].GetValue(L"autolanguage", k);
                        ReplaceVariables(v, variables);
                        std::vector<std::wstring> exts;
                        stringtok(exts, v, true, L";");
                        for (const auto& e : exts)
                        {
                            m_extLang[CUnicodeUtils::StdGetUTF8(e)] = CUnicodeUtils::StdGetUTF8(k);
                            if (iniind == 1)
                                m_autoextLang[CUnicodeUtils::StdGetUTF8(e)] = CUnicodeUtils::StdGetUTF8(k);
                        }
                    }
                    if (iniind == 1)
                    {
                        CSimpleIni::TNamesDepend autopathkeys;
                        ini[iniind].GetAllKeys(L"pathlanguage", autopathkeys);
                        for (const auto& k : autopathkeys)
                        {
                            std::wstring v = ini[iniind].GetValue(L"pathlanguage", k);
                            auto f = v.find(L"*");
                            std::wstring p = v.substr(0, f);
                            std::string l = CUnicodeUtils::StdGetUTF8(v.substr(f + 1));
                            m_pathsLang[p] = l;
                            m_pathsForLang.push_back(p);
                        }
                    }

                    CSimpleIni::TNamesDepend langkeys;
                    ini[iniind].GetAllKeys(L"language", langkeys);
                    for (const auto& k : langkeys)
                    {
                        std::wstring v = ini[iniind].GetValue(L"language", k);
                        ReplaceVariables(v, variables);
                        std::vector<std::wstring> exts;
                        stringtok(exts, v, true, L";");
                        for (const auto& e : exts)
                        {
                            m_extLang[CUnicodeUtils::StdGetUTF8(e)] = CUnicodeUtils::StdGetUTF8(k);
                            if (iniind==1)
                                m_userextLang[CUnicodeUtils::StdGetUTF8(e)] = CUnicodeUtils::StdGetUTF8(k);
                        }

                        std::wstring langsect = L"lang_";
                        langsect += k;
                        CSimpleIni::TNamesDepend specLangKeys;
                        ini[iniind].GetAllKeys(langsect.c_str(), specLangKeys);
                        LanguageData ld;
                        if (iniind > 0)
                        {
                            ld = m_Langdata[CUnicodeUtils::StdGetUTF8(k)];
                        }
                        ld.lexer = lexers[k];
                        for (const auto& sk : specLangKeys)
                        {
                            if (_wcsnicmp(L"keywords", sk, 8) == 0)
                            {
                                ld.keywordlist[_wtol(sk+8)] = CUnicodeUtils::StdGetUTF8(ini[iniind].GetValue(langsect.c_str(), sk));
                            }
                            if (_wcsicmp(L"CommentLine", sk) == 0)
                            {
                                ld.commentline = CUnicodeUtils::StdGetUTF8(ini[iniind].GetValue(langsect.c_str(), sk));
                                ld.commentline.erase(ld.commentline.find_last_not_of("~")+1); // Erase '~'
                            }
                            if (_wcsicmp(L"CommentStreamStart", sk) == 0)
                            {
                                ld.commentstreamstart = CUnicodeUtils::StdGetUTF8(ini[iniind].GetValue(langsect.c_str(), sk));
                                ld.commentstreamstart.erase(ld.commentstreamstart.find_last_not_of("~")+1); // Erase '~'
                            }
                            if (_wcsicmp(L"CommentStreamEnd", sk) == 0)
                            {
                                ld.commentstreamend = CUnicodeUtils::StdGetUTF8(ini[iniind].GetValue(langsect.c_str(), sk));
                                ld.commentstreamend.erase(ld.commentstreamend.find_last_not_of("~")+1); // Erase '~'
                            }
                            if (_wcsicmp(L"CommentLineAtStart", sk) == 0)
                            {
                                ld.commentlineatstart = _wtoi(ini[iniind].GetValue(langsect.c_str(), sk)) != 0;
                            }
                            if (_wcsicmp(L"FunctionRegex", sk) == 0)
                            {
                                ld.functionregex = CUnicodeUtils::StdGetUTF8(ini[iniind].GetValue(langsect.c_str(), sk));
                                ld.functionregex.erase(ld.functionregex.find_last_not_of("~")+1); // Erase '~'
                            }
                            if (_wcsicmp(L"FunctionRegexSort", sk) == 0)
                            {
                                ld.functionregexsort = _wtoi(ini[iniind].GetValue(langsect.c_str(), sk));
                            }
                            if (_wcsicmp(L"FunctionRegexTrim", sk) == 0)
                            {
                                std::string s = CUnicodeUtils::StdGetUTF8(ini[iniind].GetValue(langsect.c_str(), sk));
                                s.erase(s.find_last_not_of("~")+1); // Erase '~'
                                stringtok(ld.functionregextrim, s, true, ",");
                            }
                            if (_wcsicmp(L"UserFunctions", sk) == 0)
                            {
                                ld.userfunctions = _wtoi(ini[iniind].GetValue(langsect.c_str(), sk));
                            }
                        }
                        m_Langdata[CUnicodeUtils::StdGetUTF8(k)] = ld;
                    }
                }
            }
        }
    }

    m_bLoaded = true;
}

const std::map<int, std::string>& CLexStyles::GetKeywordsForExt( const std::string& ext )
{
    std::string v = ext;
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);

    auto it = m_extLang.find(v);
    if (it != m_extLang.end())
    {
        auto lt = m_Langdata.find(it->second);
        if (lt != m_Langdata.end())
        {
            GenerateUserKeywords(lt->second);
            return lt->second.keywordlist;
        }
    }
    return emptyIntStrVec;
}

const std::map<int, std::string>& CLexStyles::GetKeywordsForLang( const std::string& lang )
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
    {
        GenerateUserKeywords(lt->second);
        return lt->second.keywordlist;
    }

    return emptyIntStrVec;
}

bool CLexStyles::AddUserFunctionForLang(const std::string& lang, const std::string& fnc)
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
    {
        if (lt->second.userfunctions)
        {
            auto result = lt->second.userkeywords.insert(fnc);
            return result.second;
        }
    }
    return false;
}

void CLexStyles::GenerateUserKeywords(LanguageData& ld)
{
    if (ld.userfunctions && !ld.userkeywords.empty())
    {
        std::string keywords;
        for (const auto& w : ld.userkeywords)
        {
            keywords += w;
            keywords += " ";
        }
        ld.keywordlist[ld.userfunctions] = keywords;
    }
}

const LexerData& CLexStyles::GetLexerDataForExt( const std::string& ext ) const
{
    std::string v = ext;
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);
    auto it = m_extLang.find(v);
    if (it != m_extLang.end())
    {
        auto lt = m_Langdata.find(it->second);
        if (lt != m_Langdata.end())
        {
            auto ld = m_lexerdata.find(lt->second.lexer);
            if (ld != m_lexerdata.end())
                return ld->second;
        }
    }
    return emptyLexData;
}

const LexerData& CLexStyles::GetLexerDataForLang( const std::string& lang ) const
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
    {
        auto ld = m_lexerdata.find(lt->second.lexer);
        if (ld != m_lexerdata.end())
            return ld->second;
    }
    return emptyLexData;
}

std::wstring CLexStyles::GetLanguageForExt( const std::wstring& ext ) const
{
    std::string e = CUnicodeUtils::StdGetUTF8(ext);
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);
    auto it = m_extLang.find(e);
    if (it != m_extLang.end())
    {
        return CUnicodeUtils::StdGetUnicode(it->second);
    }
    return L"";
}

std::wstring CLexStyles::GetLanguageForPath(const std::wstring& path)
{
    std::wstring p = path;
    std::transform(p.begin(), p.end(), p.begin(), ::towlower);
    auto it = m_pathsLang.find(p);
    if (it != m_pathsLang.end())
    {
        // move the path to the top of the list
        for (auto li = m_pathsForLang.begin(); li != m_pathsForLang.end(); ++li)
        {
            if (it->first.compare(*li) == 0)
            {
                m_pathsForLang.erase(li);
                m_pathsForLang.push_front(it->first);
                break;
            }
        }
        return CUnicodeUtils::StdGetUnicode(it->second);
    }
    return L"";
}

std::wstring CLexStyles::GetUserExtensionsForLanguage( const std::wstring& lang ) const
{
    std::wstring exts;
    std::string l = CUnicodeUtils::StdGetUTF8(lang);
    for (const auto& e:m_userextLang)
    {
        if (e.second.compare(l) == 0)
        {
            if (!exts.empty())
                exts += L";";
            exts += CUnicodeUtils::StdGetUnicode(e.first);
        }
    }
    return exts;
}

std::vector<std::wstring> CLexStyles::GetLanguages() const
{
    std::vector<std::wstring> langs;
    for (const auto& it : m_Langdata)
    {
        langs.push_back(CUnicodeUtils::StdGetUnicode(it.first));
    }
    return langs;
}

void CLexStyles::ReplaceVariables( std::wstring& s, const std::map<std::wstring, std::wstring>& vars )
{
    size_t pos = s.find(L"$(");
    while (pos != std::wstring::npos)
    {
        size_t pos2 = s.find(L")", pos);
        std::wstring varname = s.substr(pos, pos2-pos+1);
        auto foundIt = vars.find(varname);
        if (foundIt != vars.end())
        {
            s = s.substr(0, pos) + foundIt->second + s.substr(pos2+1);
        }
        else
        {
            DebugBreak();
        }
        pos = s.find(L"$(", pos2);
    }
}

void CLexStyles::SetUserForeground( int ID, int style, COLORREF clr )
{
    LexerData& ld = m_userlexerdata[ID];
    StyleData& sd = ld.Styles[style];
    sd.ForegroundColor = clr;
}

void CLexStyles::SetUserBackground( int ID, int style, COLORREF clr )
{
    LexerData& ld = m_userlexerdata[ID];
    StyleData& sd = ld.Styles[style];
    sd.BackgroundColor = clr;
}

void CLexStyles::SetUserFont( int ID, int style, const std::wstring& font )
{
    LexerData& ld = m_userlexerdata[ID];
    StyleData& sd = ld.Styles[style];
    sd.FontName = font;
}

void CLexStyles::SetUserFontSize( int ID, int style, int size )
{
    LexerData& ld = m_userlexerdata[ID];
    StyleData& sd = ld.Styles[style];
    sd.FontSize = size;
}

void CLexStyles::SetUserFontStyle( int ID, int style, FontStyle fontstyle )
{
    LexerData& ld = m_userlexerdata[ID];
    StyleData& sd = ld.Styles[style];
    sd.FontStyle = fontstyle;
}

void CLexStyles::ResetUserData()
{
    m_userlexerdata.clear();
    m_userextLang.clear();
    m_extLang.clear();
    m_Langdata.clear();
    m_lexerdata.clear();
    Load();
}

void CLexStyles::SaveUserData()
{
    CSimpleIni ini;
    std::wstring userStyleFile = CAppUtils::GetDataPath() + L"\\userconfig";

    if (PathFileExists(userStyleFile.c_str()))
    {
        ini.LoadFile(userStyleFile.c_str());
    }

    for (const auto& it:m_userlexerdata)
    {
        if (it.first == 0)
            continue;
        // find the lexer section name
        std::wstring section = m_lexerSection[it.first];
        std::wstring v = CStringUtils::Format(L"%d", it.first);
        ini.SetValue(section.c_str(), L"Lexer", v.c_str());
        for (const auto& s:it.second.Styles)
        {
            std::wstring style = CStringUtils::Format(L"Style%d", s.first);
            std::wstring sSize = CStringUtils::Format(L"%d", s.second.FontSize);
            if (s.second.FontSize == 0)
                sSize.clear();
            std::wstring name = s.second.Name;
            if (name.empty())
                name = m_lexerdata[it.first].Styles[s.first].Name;
            int fore = GetRValue(s.second.ForegroundColor)<<16 | GetGValue(s.second.ForegroundColor)<<8 | GetBValue(s.second.ForegroundColor);
            int back = GetRValue(s.second.BackgroundColor)<<16 | GetGValue(s.second.BackgroundColor)<<8 | GetBValue(s.second.BackgroundColor);
            // styleNR=name;foreground;background;fontname;fontstyle;fontsize
            v = CStringUtils::Format(L"%s;%06X;%06X;%s;%d;%s", name.c_str(), fore, back, s.second.FontName.c_str(), s.second.FontStyle, sSize.c_str());
            ini.SetValue(section.c_str(), style.c_str(), v.c_str());
        }
    }

    // first clear all user extensions, then add them again to the ini file
    for (const auto& it : m_userextLang)
        ini.SetValue(L"language", CUnicodeUtils::StdGetUnicode(it.second).c_str(), L"");

    for (const auto& it : m_userextLang)
    {
        std::wstring v = ini.GetValue(L"language", CUnicodeUtils::StdGetUnicode(it.second).c_str(), L"");
        if (!v.empty())
            v += L";";
        v += CUnicodeUtils::StdGetUnicode(it.first);
        ini.SetValue(L"language", CUnicodeUtils::StdGetUnicode(it.second).c_str(), v.c_str());
    }

    // first clear all auto extensions, then add them again to the ini file
    for (const auto& it : m_autoextLang)
        ini.SetValue(L"autolanguage", CUnicodeUtils::StdGetUnicode(it.second).c_str(), L"");

    for (const auto& it : m_autoextLang)
    {
        std::wstring v = ini.GetValue(L"autolanguage", CUnicodeUtils::StdGetUnicode(it.second).c_str(), L"");
        if (!v.empty())
            v += L";";
        v += CUnicodeUtils::StdGetUnicode(it.first);
        ini.SetValue(L"autolanguage", CUnicodeUtils::StdGetUnicode(it.second).c_str(), v.c_str());
    }

    int pcount = 0;
    for (const auto& it : m_pathsLang)
    {
        std::wstring key = CStringUtils::Format(L"%03d", pcount);
        std::wstring val = it.first + L"*" + CUnicodeUtils::StdGetUnicode(it.second);
        ini.SetValue(L"pathlanguage", key.c_str(), val.c_str());
        ++pcount;
    }

    FILE * pFile = NULL;
    _tfopen_s(&pFile, userStyleFile.c_str(), _T("wb"));
    ini.SaveFile(pFile);
    fclose(pFile);

    ResetUserData();
}

void CLexStyles::SetUserExt( const std::wstring& ext, const std::wstring& lang )
{
    std::string alang = CUnicodeUtils::StdGetUTF8(lang);
    auto iter = m_userextLang.begin();
    auto endIter = m_userextLang.end();
    for (; iter != endIter;)
    {
        if (iter->second.compare(alang) == 0)
            m_userextLang.erase(iter++);
        else
            ++iter;
    }
    std::vector<std::wstring> exts;
    stringtok(exts, ext, true, L"; ,");
    for (auto e : exts)
    {
        m_userextLang[CUnicodeUtils::StdGetUTF8(e)] = alang;
    }
}

const std::string& CLexStyles::GetCommentLineForLang( const std::string& lang ) const
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
        return lt->second.commentline;
    return emptyString;
}

const std::string& CLexStyles::GetCommentStreamStartForLang( const std::string& lang ) const
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
        return lt->second.commentstreamstart;
    return emptyString;
}

const std::string& CLexStyles::GetCommentStreamEndForLang( const std::string& lang ) const
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
        return lt->second.commentstreamend;
    return emptyString;
}

bool CLexStyles::GetCommentLineAtStartForLang( const std::string& lang ) const
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
        return lt->second.commentlineatstart;
    return false;
}

const std::string& CLexStyles::GetFunctionRegexForLang( const std::string& lang ) const
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
        return lt->second.functionregex;
    return emptyString;
}

int CLexStyles::GetFunctionRegexSortForLang( const std::string& lang ) const
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
        return lt->second.functionregexsort;
    return 0;
}

const std::vector<std::string>& CLexStyles::GetFunctionRegexTrimForLang( const std::string& lang ) const
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
        return lt->second.functionregextrim;
    return emptyStringVector;
}

void CLexStyles::SetLangForPath(const std::wstring& path, const std::wstring& language)
{
    auto dotpos = path.find_last_of('.');
    auto slashpos = path.find_last_of(L"\\/");
    if (slashpos > dotpos)
        dotpos = std::wstring::npos;
    std::wstring sExt = path.substr(dotpos + 1);
    std::string e = CUnicodeUtils::StdGetUTF8(sExt);
    if (dotpos == std::wstring::npos)
    {
        // store the full path and the language
        std::wstring p = path;
        std::transform(p.begin(), p.end(), p.begin(), ::towlower);
        m_pathsLang[p] = CUnicodeUtils::StdGetUTF8(language);
        m_pathsForLang.push_front(p);
        while (m_pathsForLang.size() > 100)
            m_pathsForLang.pop_back();
        SaveUserData();
    }
    else
    {
        std::wstring lang = GetLanguageForExt(sExt);
        if (!lang.empty())
        {
            // extension already has a language set
            // only add this if the extension is set in m_autoextLang
            std::transform(e.begin(), e.end(), e.begin(), ::tolower);
            auto it = m_autoextLang.find(e);
            if (it == m_autoextLang.end())
                return;
        }
        m_autoextLang[e] = CUnicodeUtils::StdGetUTF8(language);
        m_extLang[e] = CUnicodeUtils::StdGetUTF8(language);
        SaveUserData();
    }
}

