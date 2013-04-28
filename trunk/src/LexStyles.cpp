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
#include "stdafx.h"
#include "LexStyles.h"
#include "SimpleIni.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "BowPad.h"

#include <vector>

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
                CSimpleIni ini;
                ini.LoadFile(lpResLock, dwSizeRes);

                std::map<std::wstring, std::wstring> variables;
                CSimpleIni::TNamesDepend lexvars;
                ini.GetAllKeys(L"variables", lexvars);
                for (auto l : lexvars)
                {
                    std::wstring v = ini.GetValue(L"variables", l);
                    std::wstring key = l;
                    key = L"$(" + key + L")";
                    variables[key] = v;
                }


                std::map<std::wstring, int> lexers;
                CSimpleIni::TNamesDepend lexkeys;
                ini.GetAllKeys(L"lexers", lexkeys);
                for (auto l : lexkeys)
                {
                    std::wstring v = ini.GetValue(L"lexers", l);
                    ReplaceVariables(v, variables);
                    std::vector<std::wstring> langs;
                    stringtok(langs, v, true, L";");
                    int lex = _wtoi(ini.GetValue(l, L"Lexer", L""));
                    for (auto x : langs)
                    {
                        lexers[x] = lex;
                    }

                    // now parse all the data for this lexer
                    LexerData lexerdata;
                    lexerdata.ID = lex;
                    CSimpleIni::TNamesDepend lexdatakeys;
                    ini.GetAllKeys(l, lexdatakeys);
                    for (auto it : lexdatakeys)
                    {
                        if (_wcsnicmp(L"Style", it, 5) == 0)
                        {
                            StyleData style;
                            std::wstring v = ini.GetValue(l, it);
                            ReplaceVariables(v, variables);
                            std::vector<std::wstring> vec;
                            stringtok(vec, v, false, L";");
                            int i = 0;
                            wchar_t * endptr;
                            unsigned long hexval;
                            for (auto s : vec)
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
                        }
                        if (_wcsnicmp(L"Prop_", it, 5) == 0)
                        {
                            lexerdata.Properties[CUnicodeUtils::StdGetUTF8(it+5)] = CUnicodeUtils::StdGetUTF8(ini.GetValue(l, it, L""));
                        }
                    }
                    m_lexerdata[lexerdata.ID] = lexerdata;
                }

                CSimpleIni::TNamesDepend langkeys;
                ini.GetAllKeys(L"language", langkeys);
                for (auto k : langkeys)
                {
                    std::wstring v = ini.GetValue(L"language", k);
                    ReplaceVariables(v, variables);
                    std::vector<std::wstring> exts;
                    stringtok(exts, v, true, L";");
                    for (auto e : exts)
                    {
                        m_extLang[CUnicodeUtils::StdGetUTF8(e)] = CUnicodeUtils::StdGetUTF8(k);
                    }

                    std::wstring langsect = L"lang_";
                    langsect += k;
                    CSimpleIni::TNamesDepend specLangKeys;
                    ini.GetAllKeys(langsect.c_str(), specLangKeys);
                    LanguageData ld;
                    ld.lexer = lexers[k];
                    for (auto sk : specLangKeys)
                    {
                        if (_wcsnicmp(L"keywords", sk, 8) == 0)
                        {
                            ld.keywordlist[_wtol(sk+8)] = CUnicodeUtils::StdGetUTF8(ini.GetValue(langsect.c_str(), sk));
                        }
                    }
                    m_Langdata[CUnicodeUtils::StdGetUTF8(k)] = ld;
                }
            }
        }
    }

    m_bLoaded = true;
}

std::map<int, std::string> CLexStyles::GetKeywordsForExt( const std::string& ext ) const
{
    std::string v = ext;
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);

    auto it = m_extLang.find(v);
    if (it != m_extLang.end())
    {
        auto lt = m_Langdata.find(it->second);
        if (lt != m_Langdata.end())
            return lt->second.keywordlist;
    }
    std::map<int, std::string> empty;
    return empty;
}

std::map<int, std::string> CLexStyles::GetKeywordsForLang( const std::string& lang ) const
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
        return lt->second.keywordlist;

    std::map<int, std::string> empty;
    return empty;
}

LexerData CLexStyles::GetLexerDataForExt( const std::string& ext ) const
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
    return LexerData();
}

LexerData CLexStyles::GetLexerDataForLang( const std::string& lang ) const
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
    {
        auto ld = m_lexerdata.find(lt->second.lexer);
        if (ld != m_lexerdata.end())
            return ld->second;
    }
    return LexerData();
}

std::wstring CLexStyles::GetLanguageForExt( const std::wstring& ext ) const
{
    std::string e = CUnicodeUtils::StdGetUTF8(ext);
    std::transform(e.begin(), e.end(), e.begin(), ::towlower);
    auto it = m_extLang.find(e);
    if (it != m_extLang.end())
    {
        return CUnicodeUtils::StdGetUnicode(it->second);
    }
    return L"";
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
        std::wstring varname = s.substr(pos, s.find(L")", pos)-pos+1);
        auto foundIt = vars.find(varname);
        if (foundIt != vars.end())
        {
            SearchReplace(s, varname, foundIt->second);
        }
        else
        {
            DebugBreak();
        }
        pos = s.find(L"$(", pos);
    }
}
