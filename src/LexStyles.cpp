// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2019-2020 - Stefan Kueng
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
#include "PathUtils.h"
#include "BowPad.h"
#include "AppUtils.h"
#include "OnOutOfScope.h"
#include "GDIHelpers.h"
#include "DirFileEnum.h"

namespace
{
const LexerData                            emptyLexData;
const std::unordered_map<int, std::string> emptyUnorderedMap;
const std::map<int, std::string>           emptyIntStrVec;
const std::string                          emptyString;
const std::vector<std::string>             emptyStringVector;

static constexpr COLORREF fgColor = RGB(0, 0, 0);
static constexpr COLORREF bgColor = RGB(255, 255, 255);
}; // namespace

struct sLexDetectStrings
{
    std::string              lang;
    std::string              firstLine;
    std::vector<std::string> extensions;
};

static std::vector<sLexDetectStrings> lexDetectStrings = {
    // a '+' in front of the lexer name means the string can appear anywhere in the
    // first line of the document.
    // a '-' in front of the lexer name means the string must appear
    // at the beginning of the first line of the document
    {"+C/C++", "*- C++ -*"},
    {"-Xml", "<?xml"},
    {"-Bash", "<?php"},
    {"-Bash", "#!/bin/sh"},
    {"-Bash", "#!/bin/bash"},
    {"-Bash", "#! /bin/bash"},
    {"-Html", "<html>"},
    {"-Html", "<!DOCTYPE html>"},
    {"-Java", "#!groovy"},
    {"-Java", "#!/usr/bin/env groovy"}};

StyleData::StyleData()
    : ForegroundColor(fgColor)
    , BackgroundColor(bgColor)
    , FontStyle(FONTSTYLE_NORMAL)
    , FontSize(0)
    , eolfilled(false)
{
}

CLexStyles::CLexStyles()
    : m_bLoaded(false)
{
}

CLexStyles::~CLexStyles()
{
}

CLexStyles& CLexStyles::Instance()
{
    static CLexStyles instance;
    if (!instance.m_bLoaded)
    {
        instance.Load();
    }
    return instance;
}

void CLexStyles::ParseStyle(
    LPCWSTR                                               styleName,
    LPCWSTR                                               styleString,
    const std::unordered_map<std::wstring, std::wstring>& variables,
    StyleData&                                            style) const
{
    std::wstring v = styleString;
    ReplaceVariables(v, variables);
    std::vector<std::wstring> vec;
    stringtok(vec, v, false, L";");
    int      i = 0;
    COLORREF clr;
    for (const auto& s : vec)
    {
        switch (i)
        {
            case 0: // Name
#ifdef _DEBUG
                if (s.find(',') != std::wstring::npos)
                    APPVERIFYM(false, s.c_str());
#endif
                style.Name = s;
                break;
            case 1: // Foreground color
                if (GDIHelpers::HexStringToCOLORREF(s, &clr))
                    style.ForegroundColor = clr;
                else
                    APPVERIFYM(s.empty(), styleName);
                break;
            case 2: // Background color
                if (GDIHelpers::HexStringToCOLORREF(s, &clr))
                    style.BackgroundColor = clr;
                else
                    APPVERIFYM(s.empty(), styleName);
                break;
            case 3: // Font name
                style.FontName = s;
                break;
            case 4: // Font style
            {
                int fs;
                if (!CAppUtils::TryParse(s.c_str(), fs, true))
                    APPVERIFYM(false, styleName);
                style.FontStyle = (FontStyle)fs;
            }
            break;
            case 5: // Font size
                if (!CAppUtils::TryParse(s.c_str(), style.FontSize, true))
                    APPVERIFYM(false, styleName);
                break;
            case 6: // Override default background color in case the style was set with a variable
                if (GDIHelpers::HexStringToCOLORREF(s, &clr))
                    style.BackgroundColor = clr;
                else
                    APPVERIFYM(s.empty(), styleName);
                break;
            case 7: // eolstylefilled
                if (!s.empty())
                    style.eolfilled = true;
        }
        ++i;
    }
}

void CLexStyles::Load()
{
    std::vector<std::tuple<std::unique_ptr<CSimpleIni>, bool>> inis;
    DWORD                                                      resLen  = 0;
    const char*                                                resData = CAppUtils::GetResourceData(L"config", IDR_LEXSTYLES, resLen);
    if (resData != nullptr)
    {
        inis.push_back(std::make_tuple(std::make_unique<CSimpleIni>(), false));
        std::get<0>(inis.back())->LoadFile(resData, resLen);
    }

    CDirFileEnum enumerator(CAppUtils::GetDataPath());
    bool         bIsDir = false;
    std::wstring path;
    while (enumerator.NextFile(path, &bIsDir, false))
    {
        if (CPathUtils::GetFileExtension(path) == L"bplex")
        {
            inis.push_back(std::make_tuple(std::make_unique<CSimpleIni>(), false));
            std::get<0>(inis.back())->LoadFile(path.c_str());
        }
    }

    std::wstring userStyleFile = CAppUtils::GetDataPath() + L"\\userconfig";
    if (PathFileExists(userStyleFile.c_str()))
    {
        inis.push_back(std::make_tuple(std::make_unique<CSimpleIni>(), true));
        std::get<0>(inis.back())->LoadFile(userStyleFile.c_str());
    }

    std::unordered_map<std::wstring, std::wstring> variables;
    for (const auto& [ini, isUserConfig] : inis)
    {
        CSimpleIni::TNamesDepend lexvars;
        ini->GetAllKeys(L"variables", lexvars);
        for (const auto& l : lexvars)
        {
            APPVERIFY(*l != L'\0');
            std::wstring v   = ini->GetValue(L"variables", l);
            std::wstring key = L"$(";
            key += l;
            key += L")";
            variables[std::move(key)] = std::move(v);
        }
    }

    std::map<std::wstring, int> lexers;
    for (const auto& [ini, isUserConfig] : inis)
    {
        CSimpleIni::TNamesDepend lexkeys;
        ini->GetAllKeys(L"lexers", lexkeys);
        for (const auto& l : lexkeys)
        {
            APPVERIFY(*l != L'\0');
            int lex;
            if (!CAppUtils::TryParse(ini->GetValue(l, L"Lexer", L""), lex, false))
                APPVERIFY(false);
            m_lexerSection[lex] = l;
        }
        std::wstring slex;
        for (const auto& l : m_lexerSection)
        {
            slex = ini->GetValue(l.second.c_str(), L"Lexer", L"");
            if (slex.empty())
                continue;
            int lex;
            if (!CAppUtils::TryParse(slex.c_str(), lex, false))
                APPVERIFY(false);

            std::wstring v = ini->GetValue(L"lexers", l.second.c_str(), L"");
            if (!v.empty())
            {
                ReplaceVariables(v, variables);
                std::vector<std::wstring> langs;
                stringtok(langs, v, true, L";");
                for (const auto& lang : langs)
                    lexers[lang] = lex;
            }

            // now parse all the data for this lexer
            LexerData lexerdata = m_lexerdata[lex];
            LexerData userlexerdata;
            lexerdata.ID     = lex;
            userlexerdata.ID = lex;
            CSimpleIni::TNamesDepend lexdatakeys;
            ini->GetAllKeys(l.second.c_str(), lexdatakeys);
            for (const auto& it : lexdatakeys)
            {
                if (_wcsnicmp(L"Style", it, 5) == 0)
                {
                    StyleData style;
                    ParseStyle(it, ini->GetValue(l.second.c_str(), it, L""), variables, style);
                    int pos;
                    if (!CAppUtils::TryParse(it + 5, pos, false))
                        APPVERIFY(false);
                    lexerdata.Styles[pos]     = style;
                    userlexerdata.Styles[pos] = std::move(style);
                }
                else if (_wcsnicmp(L"Prop_", it, 5) == 0)
                {
                    std::string n                          = CUnicodeUtils::StdGetUTF8(it + 5);
                    std::string vl                         = CUnicodeUtils::StdGetUTF8(ini->GetValue(l.second.c_str(), it, L""));
                    lexerdata.Properties[n]                = vl;
                    userlexerdata.Properties[std::move(n)] = std::move(vl);
                }
                else if (_wcsnicmp(L"hidden", it, 6) == 0)
                {
                    lexerdata.hidden = _wtoi(ini->GetValue(l.second.c_str(), it, L"")) != 0;
                }
            }
            m_lexerdata[lexerdata.ID] = std::move(lexerdata);
            if (isUserConfig)
                m_userlexerdata[lexerdata.ID] = std::move(userlexerdata);
        }
    }

    for (const auto& [ini, isUserConfig] : inis)
    {
        CSimpleIni::TNamesDepend filelangkeys;
        ini->GetAllKeys(L"filelanguage", filelangkeys);
        for (const auto& filelangkey : filelangkeys)
        {
            auto         utf8filelangkey = CUnicodeUtils::StdGetUTF8(filelangkey);
            std::wstring v               = ini->GetValue(L"filelanguage", filelangkey);
            ReplaceVariables(v, variables);
            std::vector<std::wstring> files;
            stringtok(files, v, true, L";");
            for (const auto& f : files)
            {
                m_fileLang[CUnicodeUtils::StdGetUTF8(CPathUtils::GetFileName(f))] = utf8filelangkey;
            }
        }
        CSimpleIni::TNamesDepend autolangkeys;
        ini->GetAllKeys(L"autolanguage", autolangkeys);
        for (const auto& autolangkey : autolangkeys)
        {
            auto         utf8autolangkey = CUnicodeUtils::StdGetUTF8(autolangkey);
            std::wstring v               = ini->GetValue(L"autolanguage", autolangkey);
            ReplaceVariables(v, variables);
            std::vector<std::wstring> exts;
            stringtok(exts, v, true, L";");
            for (const auto& e : exts)
            {
                m_autoextLang[CUnicodeUtils::StdGetUTF8(e)] = utf8autolangkey;
            }
        }
        if (isUserConfig)
        {
            CSimpleIni::TNamesDepend autopathkeys;
            ini->GetAllKeys(L"pathlanguage", autopathkeys);
            for (const auto& apppathkey : autopathkeys)
            {
                std::wstring v = ini->GetValue(L"pathlanguage", apppathkey);
                auto         f = v.find(L'*');
                if (f != std::wstring::npos)
                {
                    std::wstring p = v.substr(0, f);
                    m_pathsLang[p] = CUnicodeUtils::StdGetUTF8(v.substr(f + 1));
                    m_pathsForLang.push_back(std::move(p));
                }
            }
        }

        CSimpleIni::TNamesDepend langkeys;
        ini->GetAllKeys(L"language", langkeys);
        for (const auto& langkey : langkeys)
        {
            auto         utf8langkey = CUnicodeUtils::StdGetUTF8(langkey);
            auto&        langdata    = m_Langdata[utf8langkey];
            std::wstring v           = ini->GetValue(L"language", langkey, L"");
            ReplaceVariables(v, variables);
            std::vector<std::wstring> exts;
            stringtok(exts, v, true, L";");
            std::wstring filters;
            for (const auto& e : exts)
            {
                m_extLang.insert({CUnicodeUtils::StdGetUTF8(e), utf8langkey});
                if (isUserConfig)
                    m_userextLang[CUnicodeUtils::StdGetUTF8(e)] = utf8langkey;
                else
                {
                    if (!filters.empty())
                        filters.push_back(TEXT(';'));
                    filters += TEXT("*.") + e;
                }
            }
            if (!filters.empty())
            {
                auto sKey = std::wstring(langkey) + TEXT(" file");
                auto foundIt = std::find_if(m_fileTypes.begin(), m_fileTypes.end(),
                    [&](const auto& item) { return (std::get<0>(item) == sKey); });
                if (foundIt != m_fileTypes.end())
                    m_fileTypes.erase(foundIt);
                m_fileTypes.push_back(std::make_pair(sKey, std::move(filters)));
            }

            std::wstring langsect = L"lang_";
            langsect += langkey;
            CSimpleIni::TNamesDepend specLangKeys;
            ini->GetAllKeys(langsect.c_str(), specLangKeys);
            LanguageData ld;
            if (isUserConfig)
            {
                ld = langdata;
            }
            ld.lexer = lexers[langkey];
            for (const auto& sk : specLangKeys)
            {
                if (_wcsnicmp(L"keywords", sk, 8) == 0)
                {
                    int vn;
                    if (!CAppUtils::TryParse(sk + 8, vn, false))
                        APPVERIFY(false);
                    ld.keywordlist[vn] = CUnicodeUtils::StdGetUTF8(ini->GetValue(langsect.c_str(), sk));
                }
                else if (_wcsicmp(L"CommentLine", sk) == 0)
                {
                    ld.commentline = CUnicodeUtils::StdGetUTF8(ini->GetValue(langsect.c_str(), sk));
                    ld.commentline.erase(ld.commentline.find_last_not_of('~') + 1); // Erase '~'
                }
                else if (_wcsicmp(L"CommentStreamStart", sk) == 0)
                {
                    ld.commentstreamstart = CUnicodeUtils::StdGetUTF8(ini->GetValue(langsect.c_str(), sk));
                    ld.commentstreamstart.erase(ld.commentstreamstart.find_last_not_of('~') + 1); // Erase '~'
                }
                else if (_wcsicmp(L"CommentStreamEnd", sk) == 0)
                {
                    ld.commentstreamend = CUnicodeUtils::StdGetUTF8(ini->GetValue(langsect.c_str(), sk));
                    ld.commentstreamend.erase(ld.commentstreamend.find_last_not_of('~') + 1); // Erase '~'
                }
                else if (_wcsicmp(L"CommentLineAtStart", sk) == 0)
                {
                    int vn;
                    if (!CAppUtils::TryParse(
                            ini->GetValue(langsect.c_str(), sk), vn, false))
                        APPVERIFY(false);
                    ld.commentlineatstart = vn != 0;
                }
                else if (_wcsicmp(L"FunctionRegex", sk) == 0)
                {
                    ld.functionregex = CUnicodeUtils::StdGetUTF8(ini->GetValue(langsect.c_str(), sk));
                    ld.functionregex.erase(ld.functionregex.find_last_not_of('~') + 1); // Erase '~'
                }
                else if (_wcsicmp(L"FunctionRegexSort", sk) == 0)
                {
                    if (!CAppUtils::TryParse(
                            ini->GetValue(langsect.c_str(), sk),
                            ld.functionregexsort, false))
                        APPVERIFY(false);
                }
                else if (_wcsicmp(L"FunctionRegexTrim", sk) == 0)
                {
                    std::string s = CUnicodeUtils::StdGetUTF8(ini->GetValue(langsect.c_str(), sk));
                    s.erase(s.find_last_not_of('~') + 1); // Erase '~'
                    stringtok(ld.functionregextrim, s, true, ",");
                }
                else if (_wcsicmp(L"UserFunctions", sk) == 0)
                {
                    if (!CAppUtils::TryParse(
                            ini->GetValue(langsect.c_str(), sk),
                            ld.userfunctions, false))
                        APPVERIFY(false);
                }
                else if (_wcsicmp(L"DetectionString+", sk) == 0)
                {
                    sLexDetectStrings lds;
                    lds.lang      = "+" + CUnicodeUtils::StdGetUTF8(langkey);
                    lds.firstLine = CUnicodeUtils::StdGetUTF8(ini->GetValue(langsect.c_str(), sk));
                    auto sExts    = CUnicodeUtils::StdGetUTF8(ini->GetValue(langsect.c_str(), L"DetectionStringExts"));
                    stringtok(lds.extensions, sExts, true, ";");
                    lexDetectStrings.push_back(std::move(lds));
                }
                else if (_wcsicmp(L"DetectionString-", sk) == 0)
                {
                    sLexDetectStrings lds;
                    lds.lang      = "-" + CUnicodeUtils::StdGetUTF8(langkey);
                    lds.firstLine = CUnicodeUtils::StdGetUTF8(ini->GetValue(langsect.c_str(), sk));
                    auto sExts    = CUnicodeUtils::StdGetUTF8(ini->GetValue(langsect.c_str(), L"DetectionStringExts"));
                    stringtok(lds.extensions, sExts, true, ";");
                    lexDetectStrings.push_back(std::move(lds));
                }
            }
            langdata = std::move(ld);
        }
    }
    m_fileTypes.sort([](const auto& first, const auto& second) {
        return _wcsicmp(first.first.c_str(), second.first.c_str()) < 0;
    });
    m_fileTypes.push_front(std::make_pair(TEXT("All files"), TEXT("*.*")));
    for (auto& e : m_fileTypes)
        m_filterSpec.push_back({e.first.c_str(), e.second.c_str()});

    m_bLoaded = true;
}

const std::unordered_map<int, std::string>& CLexStyles::GetKeywordsForLang(const std::string& lang)
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
    {
        GenerateUserKeywords(lt->second);
        return lt->second.keywordlist;
    }

    return emptyUnorderedMap;
}

const std::unordered_map<int, std::string>& CLexStyles::GetKeywordsForLexer(int lexer)
{
    for (auto it = m_Langdata.begin(); it != m_Langdata.end(); ++it)
    {
        if (it->second.lexer == lexer)
        {
            GenerateUserKeywords(it->second);
            return it->second.keywordlist;
        }
    }

    return emptyUnorderedMap;
}

LanguageData* CLexStyles::GetLanguageData(const std::string& lang)
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
    {
        return &lt->second;
    }
    return nullptr;
}

bool CLexStyles::AddUserFunctionForLang(const std::string& lang, const std::string& fnc)
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
    {
        if (lt->second.userfunctions)
        {
            auto result = lt->second.userkeywords.insert(fnc);
            if (result.second)
                lt->second.userkeywordsupdated = true;
            return result.second;
        }
    }
    return false;
}

void CLexStyles::GenerateUserKeywords(LanguageData& ld)
{
    if (!ld.userkeywordsupdated)
        return;
    if (ld.userfunctions)
    {
        std::string keywords;
        for (const auto& w : ld.userkeywords)
        {
            keywords += w;
            keywords += ' ';
        }
        ld.keywordlist[ld.userfunctions] = std::move(keywords);
    }
    ld.userkeywordsupdated = false;
}

void CLexStyles::Reload()
{
    m_extLang.clear();
    m_fileLang.clear();
    m_Langdata.clear();
    m_lexerdata.clear();
    m_lexerSection.clear();
    m_userlexerdata.clear();
    m_userextLang.clear();
    m_autoextLang.clear();
    m_pathsLang.clear();
    m_pathsForLang.clear();
    m_fileTypes.clear();
    m_filterSpec.clear();

    Load();
}

const LexerData& CLexStyles::GetLexerDataForLang(const std::string& lang) const
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

const LexerData& CLexStyles::GetLexerDataForLexer(int lexer) const
{
    auto ld = m_lexerdata.find(lexer);
    if (ld != m_lexerdata.end())
        return ld->second;
    return emptyLexData;
}

const std::string& CLexStyles::GetLanguageForLexer(int lexer) const
{
    for (const auto& data : m_Langdata)
    {
        if (data.second.lexer == lexer)
            return data.first;
    }
    return emptyString;
}

std::string CLexStyles::GetLanguageForPath(const std::wstring& path)
{
    auto it = std::find_if(m_pathsLang.begin(), m_pathsLang.end(), [&](const auto& toFind) {
        return _wcsicmp(toFind.first.c_str(), path.c_str()) == 0;
    });
    if (it != m_pathsLang.end())
    {
        // move the path to the top of the list
        for (auto li = m_pathsForLang.begin(); li != m_pathsForLang.end(); ++li)
        {
            if (_wcsicmp(it->first.c_str(), li->c_str()) == 0)
            {
                m_pathsForLang.erase(li);
                m_pathsForLang.push_front(it->first);
                break;
            }
        }
        return it->second;
    }

    auto pathA = CUnicodeUtils::StdGetUTF8(CPathUtils::GetFileName(path));
    auto fit   = std::find_if(m_fileLang.begin(), m_fileLang.end(), [&](const auto& toFind) {
        return _stricmp(toFind.first.c_str(), pathA.c_str()) == 0;
    });

    if (fit != m_fileLang.end())
    {
        return fit->second;
    }
    auto ext = CUnicodeUtils::StdGetUTF8(CPathUtils::GetFileExtension(path));

    auto eit = std::find_if(m_extLang.begin(), m_extLang.end(), [&](const auto& toFind) {
        return _stricmp(toFind.first.c_str(), ext.c_str()) == 0;
    });
    if (eit != m_extLang.end())
    {
        return eit->second;
    }

    auto ait = std::find_if(m_autoextLang.begin(), m_autoextLang.end(), [&](const auto& toFind) {
        return _stricmp(toFind.first.c_str(), ext.c_str()) == 0;
    });
    if (ait != m_autoextLang.end())
    {
        return ait->second;
    }
    return "";
}

std::string CLexStyles::GetLanguageForDocument(const CDocument& doc, CScintillaWnd& edit)
{
    if (doc.m_path.empty())
        return "Text";
    auto sExt           = CUnicodeUtils::StdGetUTF8(CPathUtils::GetFileExtension(doc.m_path));
    bool hasExtOverride = false;
    for (const auto& m : lexDetectStrings)
    {
        for (const auto& e : m.extensions)
        {
            if (_stricmp(sExt.c_str(), e.c_str()) == 0)
            {
                hasExtOverride = true;
                break;
            }
        }
    }

    auto lang = GetLanguageForPath(doc.m_path);
    if (!lang.empty() && !hasExtOverride)
        return lang;

    // no known extension, and no previously set lexer for this path:
    // try using the file content to determine a lexer.
    // Since this needs to be fast, we don't do excessive checks but
    // keep it very, very simple.
    edit.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
    OnOutOfScope(
        edit.Call(SCI_SETDOCPOINTER, 0, 0););

    std::string line = edit.GetLine(0);
    for (const auto& m : lexDetectStrings)
    {
        auto foundpos = line.find(m.firstLine);
        if (((m.lang[0] == '-') && (foundpos == 0)) ||
            ((m.lang[0] == '+') && (foundpos != std::string::npos)))
        {
            lang = &m.lang[1];
            break;
        }
    }
    // Unknown language,use "Text" as default
    if (lang.empty())
        lang = "Text";

    return lang;
}

std::wstring CLexStyles::GetUserExtensionsForLanguage(const std::wstring& lang) const
{
    std::wstring exts;
    std::string  l = CUnicodeUtils::StdGetUTF8(lang);
    for (const auto& e : m_userextLang)
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

bool CLexStyles::GetDefaultExtensionForLanguage(const std::string& lang, std::wstring& ext, UINT& index) const
{
    // if there's a user set extension, use that
    for (const auto& e : m_userextLang)
    {
        if (e.second.compare(lang) == 0)
        {
            ext = CUnicodeUtils::StdGetUnicode(e.first);
            return true;
        }
    }
    // Search in filter pattern, use the first extension as the default and return file type index.
    auto langW = CUnicodeUtils::StdGetUnicode(lang) + TEXT(" file");
    for (size_t i = 0, count = m_filterSpec.size(); i < count; ++i)
    {
        if (m_filterSpec[i].pszName == langW)
        {
            index    = (UINT)i;
            auto pos = wcschr(m_filterSpec[i].pszSpec, TEXT(';'));
            if (pos)
                ext.assign(m_filterSpec[i].pszSpec + 2, pos);
            else
                ext = m_filterSpec[i].pszSpec + 2;
            return true;
        }
    }
    return false;
}

size_t CLexStyles::GetFilterSpceCount() const
{
    return m_filterSpec.size();
}

const COMDLG_FILTERSPEC* CLexStyles::GetFilterSpceData() const
{
    return m_filterSpec.data();
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

std::map<std::string, LanguageData>& CLexStyles::GetLanguageDataMap()
{
    return m_Langdata;
}

void CLexStyles::ReplaceVariables(std::wstring& s, const std::unordered_map<std::wstring, std::wstring>& vars) const
{
    size_t pos = s.find(L"$(");
    while (pos != std::wstring::npos)
    {
        size_t       pos2    = s.find(L')', pos);
        std::wstring varname = s.substr(pos, pos2 - pos + 1);
        auto         foundIt = vars.find(varname);
        if (foundIt != vars.end())
        {
            s = s.substr(0, pos) + foundIt->second + s.substr(pos2 + 1);
        }
        else
        {
            DebugBreak();
        }
        pos = s.find(L"$(", pos2);
    }
}

void CLexStyles::SetUserForeground(int ID, int style, COLORREF clr)
{
    LexerData& ld = m_userlexerdata[ID];
    if (ld.Styles.find(style) != ld.Styles.end())
    {
        StyleData& sd      = ld.Styles[style];
        sd.ForegroundColor = clr;
    }
    else
    {
        StyleData sd       = m_lexerdata[ID].Styles[style];
        sd.ForegroundColor = clr;
        ld.Styles[style]   = sd;
    }
}

void CLexStyles::SetUserBackground(int ID, int style, COLORREF clr)
{
    LexerData& ld = m_userlexerdata[ID];
    if (ld.Styles.find(style) != ld.Styles.end())
    {
        StyleData& sd      = ld.Styles[style];
        sd.BackgroundColor = clr;
    }
    else
    {
        StyleData sd       = m_lexerdata[ID].Styles[style];
        sd.BackgroundColor = clr;
        ld.Styles[style]   = sd;
    }
}

void CLexStyles::SetUserFont(int ID, int style, const std::wstring& font)
{
    LexerData& ld = m_userlexerdata[ID];
    if (ld.Styles.find(style) != ld.Styles.end())
    {
        StyleData& sd = ld.Styles[style];
        sd.FontName   = font;
    }
    else
    {
        StyleData sd     = m_lexerdata[ID].Styles[style];
        sd.FontName      = font;
        ld.Styles[style] = sd;
    }
}

void CLexStyles::SetUserFontSize(int ID, int style, int size)
{
    LexerData& ld = m_userlexerdata[ID];
    if (ld.Styles.find(style) != ld.Styles.end())
    {
        StyleData& sd = ld.Styles[style];
        sd.FontSize   = size;
    }
    else
    {
        StyleData sd     = m_lexerdata[ID].Styles[style];
        sd.FontSize      = size;
        ld.Styles[style] = sd;
    }
}

void CLexStyles::SetUserFontStyle(int ID, int style, FontStyle fontstyle)
{
    LexerData& ld = m_userlexerdata[ID];
    if (ld.Styles.find(style) != ld.Styles.end())
    {
        StyleData& sd = ld.Styles[style];
        sd.FontStyle  = fontstyle;
    }
    else
    {
        StyleData sd     = m_lexerdata[ID].Styles[style];
        sd.FontStyle     = fontstyle;
        ld.Styles[style] = sd;
    }
}

void CLexStyles::ResetUserData()
{
    m_userlexerdata.clear();
    m_userextLang.clear();
    m_extLang.clear();
    m_Langdata.clear();
    m_lexerdata.clear();
    m_fileTypes.clear();
    m_filterSpec.clear();
    Load();
}

void CLexStyles::SetUserHidden(int ID, bool hidden)
{
    LexerData& ld = m_userlexerdata[ID];
    ld.hidden = hidden;
}

void CLexStyles::SaveUserData()
{
    CSimpleIni   ini;
    std::wstring userStyleFile = CAppUtils::GetDataPath() + L"\\userconfig";

    if (PathFileExists(userStyleFile.c_str()))
    {
        ini.LoadFile(userStyleFile.c_str());
    }

    for (const auto& it : m_userlexerdata)
    {
        int lexerID = it.first;
        if (lexerID == 0)
            continue;
        // find the lexer section name
        std::wstring section = m_lexerSection[lexerID];
        ini.Delete(section.c_str(), nullptr);
        std::wstring v       = std::to_wstring(lexerID);
        if (!it.second.Styles.empty() || it.second.hidden)
        {
            ini.SetValue(section.c_str(), L"Lexer", v.c_str());
            for (const auto& styleEntry : it.second.Styles)
            {
                const int    styleID = styleEntry.first;
                const auto& styleData = styleEntry.second;
                std::wstring style = CStringUtils::Format(L"Style%d", styleID);
                std::wstring sSize = std::to_wstring(styleData.FontSize);
                if (styleData.FontSize == 0)
                    sSize.clear();
                int fore = GetRValue(styleData.ForegroundColor) << 16 | GetGValue(styleData.ForegroundColor) << 8 | GetBValue(styleData.ForegroundColor);
                int back = GetRValue(styleData.BackgroundColor) << 16 | GetGValue(styleData.BackgroundColor) << 8 | GetBValue(styleData.BackgroundColor);
                // styleNR=name;foreground;background;fontname;fontstyle;fontsize
                const auto& name = styleData.Name.empty() ? m_lexerdata[lexerID].Styles[styleID].Name : styleData.Name;
                v = CStringUtils::Format(L"%s;%06X;%06X;%s;%d;%s", name.c_str(), fore, back, styleData.FontName.c_str(), styleData.FontStyle, sSize.c_str());
                ini.SetValue(section.c_str(), style.c_str(), v.c_str());
            }
            ini.SetValue(section.c_str(), L"hidden", std::to_wstring(it.second.hidden).c_str());
        }
    }

    // first clear all user extensions, then add them again to the ini file
    ini.Delete(L"language", nullptr);

    for (const auto& it : m_userextLang)
    {
        auto         lang = CUnicodeUtils::StdGetUnicode(it.second);
        std::wstring v    = ini.GetValue(L"language", lang.c_str(), L"");
        if (!v.empty())
            v += L";";
        v += CUnicodeUtils::StdGetUnicode(it.first);
        ini.SetValue(L"language", lang.c_str(), v.c_str());
    }

    // first clear all auto extensions, then add them again to the ini file
    ini.Delete(L"autolanguage", nullptr);

    for (const auto& it : m_autoextLang)
    {
        auto         lang = CUnicodeUtils::StdGetUnicode(it.second);
        std::wstring v    = ini.GetValue(L"autolanguage", lang.c_str(), L"");
        if (!v.empty())
            v += L";";
        v += CUnicodeUtils::StdGetUnicode(it.first);
        ini.SetValue(L"autolanguage", lang.c_str(), v.c_str());
    }

    int pcount = 0;
    for (const auto& it : m_pathsLang)
    {
        std::wstring key = CStringUtils::Format(L"%03d", pcount);
        std::wstring val = it.first + L"*" + CUnicodeUtils::StdGetUnicode(it.second);
        ini.SetValue(L"pathlanguage", key.c_str(), val.c_str());
        ++pcount;
    }

    FILE* pFile = nullptr;
    _wfopen_s(&pFile, userStyleFile.c_str(), L"wb");
    ini.SaveFile(pFile);
    fclose(pFile);

    ResetUserData();
}

void CLexStyles::SetUserExt(const std::wstring& ext, const std::string& lang)
{
    auto iter    = m_userextLang.begin();
    auto endIter = m_userextLang.end();
    for (; iter != endIter;)
    {
        if (iter->second.compare(lang) == 0)
            iter = m_userextLang.erase(iter);
        else
            ++iter;
    }
    std::vector<std::wstring> exts;
    stringtok(exts, ext, true, L"; ,");
    for (const auto& e : exts)
    {
        m_userextLang[CUnicodeUtils::StdGetUTF8(e)] = lang;
    }
}

const std::string& CLexStyles::GetCommentLineForLang(const std::string& lang) const
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
        return lt->second.commentline;
    return emptyString;
}

const std::string& CLexStyles::GetCommentStreamStartForLang(const std::string& lang) const
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
        return lt->second.commentstreamstart;
    return emptyString;
}

const std::string& CLexStyles::GetCommentStreamEndForLang(const std::string& lang) const
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
        return lt->second.commentstreamend;
    return emptyString;
}

bool CLexStyles::GetCommentLineAtStartForLang(const std::string& lang) const
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
        return lt->second.commentlineatstart;
    return false;
}

const std::string& CLexStyles::GetFunctionRegexForLang(const std::string& lang) const
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
        return lt->second.functionregex;
    return emptyString;
}

int CLexStyles::GetFunctionRegexSortForLang(const std::string& lang) const
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
        return lt->second.functionregexsort;
    return 0;
}

const std::vector<std::string>& CLexStyles::GetFunctionRegexTrimForLang(const std::string& lang) const
{
    auto lt = m_Langdata.find(lang);
    if (lt != m_Langdata.end())
        return lt->second.functionregextrim;
    return emptyStringVector;
}

void CLexStyles::SetLangForPath(const std::wstring& path, const std::string& language)
{
    std::string lang = GetLanguageForPath(path);
    // there's nothing to do if the language is already set for that path
    if (lang.compare(language))
    {
        std::wstring sExt = CPathUtils::GetFileExtension(path);
        if (!sExt.empty())
        {
            // extension has a different language set than the user selected
            // only add this if the extension is set in m_autoextLang
            std::string e  = CUnicodeUtils::StdGetUTF8(sExt);
            auto        it = std::find_if(m_extLang.begin(), m_extLang.end(), [&](const auto& toFind) {
                return _stricmp(toFind.first.c_str(), e.c_str()) == 0;
            });
            if (it == m_extLang.end())
            {
                // set user selected language as the default for this extension
                m_autoextLang.erase(e);
                m_autoextLang[e] = language;
                SaveUserData();
                return;
            }
            else
                m_autoextLang.erase(e);
        }
        // store the full path and the language
        m_pathsLang[path] = language;
        m_pathsForLang.push_front(path);
        while (m_pathsForLang.size() > 100)
            m_pathsForLang.pop_back();
        SaveUserData();
    }
}
