// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2019-2024 - Stefan Kueng
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
const std::string                          emptyString;
const std::vector<std::string>             emptyStringVector;

constexpr COLORREF fgColor = RGB(0, 0, 0);
constexpr COLORREF bgColor = RGB(255, 255, 255);
}; // namespace

struct LexDetectStrings
{
    std::string              lang;
    std::string              firstLine;
    std::vector<std::string> extensions;
};

struct AnnotationData
{
    std::string sRegex;
    std::string sText;
};

static std::vector<LexDetectStrings> lexDetectStrings = {
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
    {"-Java", "#!/usr/bin/env groovy"},
    {"-JavaScript", "#!/usr/bin/env zx"}};

StyleData::StyleData()
    : foregroundColor(fgColor)
    , backgroundColor(bgColor)
    , fontStyle(Fontstyle_Normal)
    , fontSize(0)
    , eolFilled(false)
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
    COLORREF clr{};
    for (const auto& s : vec)
    {
        switch (i)
        {
            case 0: // Name
#ifdef _DEBUG
                if (s.find(',') != std::wstring::npos)
                    APPVERIFYM(false, s.c_str());
#endif
                style.name = s;
                break;
            case 1: // Foreground color
                if (GDIHelpers::HexStringToCOLORREF(s, &clr))
                    style.foregroundColor = clr;
                else
                    APPVERIFYM(s.empty(), styleName);
                break;
            case 2: // Background color
                if (GDIHelpers::HexStringToCOLORREF(s, &clr))
                    style.backgroundColor = clr;
                else
                    APPVERIFYM(s.empty(), styleName);
                break;
            case 3: // Font name
                style.fontName = s;
                break;
            case 4: // Font style
            {
                int fs;
                if (!CAppUtils::TryParse(s.c_str(), fs, true))
                    APPVERIFYM(false, styleName);
                style.fontStyle = static_cast<FontStyle>(fs);
            }
            break;
            case 5: // Font size
                if (!CAppUtils::TryParse(s.c_str(), style.fontSize, true))
                    APPVERIFYM(false, styleName);
                break;
            case 6: // Override default background color in case the style was set with a variable
                if (GDIHelpers::HexStringToCOLORREF(s, &clr))
                    style.backgroundColor = clr;
                else
                    APPVERIFYM(s.empty(), styleName);
                break;
            case 7: // eolstylefilled
                if (!s.empty())
                    style.eolFilled = true;
                break;
            default:
                break;
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
        std::get<0>(inis.back())->SetUnicode();
        std::get<0>(inis.back())->LoadFile(resData, resLen);
    }

    CDirFileEnum enumerator(CAppUtils::GetDataPath());
    bool         bIsDir = false;
    std::wstring path;
    while (enumerator.NextFile(path, &bIsDir, false))
    {
        if (CPathUtils::GetFileExtension(path) == L"bplex")
        {
            inis.push_back(std::make_tuple(std::make_unique<CSimpleIni>(), true));
            std::get<0>(inis.back())->SetUnicode();
            std::get<0>(inis.back())->LoadFile(path.c_str());
        }
    }

    std::wstring userStyleFile = CAppUtils::GetDataPath() + L"\\userconfig";
    if (PathFileExists(userStyleFile.c_str()))
    {
        inis.push_back(std::make_tuple(std::make_unique<CSimpleIni>(), true));
        std::get<0>(inis.back())->SetUnicode();
        std::get<0>(inis.back())->LoadFile(userStyleFile.c_str());
    }

    std::unordered_map<std::wstring, std::wstring> variables;
    for (const auto& [ini, isUserConfig] : inis)
    {
        CSimpleIni::TNamesDepend lexVars;
        ini->GetAllKeys(L"variables", lexVars);
        for (const auto& l : lexVars)
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
        CSimpleIni::TNamesDepend lexKeys;
        ini->GetAllKeys(L"lexers", lexKeys);
        for (const auto& l : lexKeys)
        {
            APPVERIFY(*l != L'\0');
            int lex;
            if (!CAppUtils::TryParse(ini->GetValue(l, L"Lexer", L""), lex, false))
                APPVERIFY(false);
            m_lexerSection[lex] = l;
        }
        std::wstring slex;
        for (const auto& [lexerId, lexerName] : m_lexerSection)
        {
            slex = ini->GetValue(lexerName.c_str(), L"Lexer", L"");
            if (slex.empty())
                continue;
            int lex;
            if (!CAppUtils::TryParse(slex.c_str(), lex, false))
                APPVERIFY(false);
            std::wstring lexName = ini->GetValue(lexerName.c_str(), L"LexerName", L"");
#ifdef _DEBUG
            assert(!lexName.empty());
#endif
            if (lexName.empty() && lex)
            {
                auto lexIt = m_lexerData.find(lex);
                if (lexIt != m_lexerData.end())
                {
                    lexName = CUnicodeUtils::StdGetUnicode(lexIt->second.name);
                }
            }
            std::wstring v = ini->GetValue(L"lexers", lexerName.c_str(), L"");
            if (!v.empty())
            {
                ReplaceVariables(v, variables);
                std::vector<std::wstring> langs;
                stringtok(langs, v, true, L";");
                for (const auto& lang : langs)
                    lexers[lang] = lex;
            }

            // now parse all the data for this lexer
            LexerData lexerData = m_lexerData[lex];
            LexerData userLexerData;
            lexerData.id       = lex;
            lexerData.name     = CUnicodeUtils::StdGetUTF8(lexName);
            userLexerData.id   = lex;
            userLexerData.name = CUnicodeUtils::StdGetUTF8(lexName);
            CSimpleIni::TNamesDepend lexDataKeys;
            ini->GetAllKeys(lexerName.c_str(), lexDataKeys);
            std::map<int, AnnotationData> annotations;
            for (const auto& it : lexDataKeys)
            {
                if (_wcsnicmp(L"Style", it, 5) == 0)
                {
                    StyleData style;
                    ParseStyle(it, ini->GetValue(lexerName.c_str(), it, L""), variables, style);
                    int pos;
                    if (!CAppUtils::TryParse(it + 5, pos, false))
                        APPVERIFY(false);
                    lexerData.styles[pos]     = style;
                    userLexerData.styles[pos] = std::move(style);
                }
                else if (_wcsnicmp(L"Prop_", it, 5) == 0)
                {
                    std::string n                          = CUnicodeUtils::StdGetUTF8(it + 5);
                    std::string vl                         = CUnicodeUtils::StdGetUTF8(ini->GetValue(lexerName.c_str(), it, L""));
                    lexerData.properties[n]                = vl;
                    userLexerData.properties[std::move(n)] = std::move(vl);
                }
                else if (_wcsnicmp(L"ann", it, 3) == 0)
                {
                    auto num = _wtoi(it + 3);
                    if (_wcsicmp(it + 6, L"regex") == 0)
                    {
                        assert(annotations.find(num) == annotations.end());
                        annotations[num].sRegex = CUnicodeUtils::StdGetUTF8(ini->GetValue(lexerName.c_str(), it, L""));
                    }
                    if (_wcsicmp(it + 6, L"text") == 0)
                    {
                        annotations[num].sText = CUnicodeUtils::StdGetUTF8(ini->GetValue(lexerName.c_str(), it, L""));
                    }
                }
            }
            if (!annotations.empty())
            {
                for (const auto& data : annotations)
                {
                    lexerData.annotations[data.second.sRegex] = data.second.sText;
                }
            }
            m_lexerData[lexerData.id] = std::move(lexerData);
            if (isUserConfig)
                m_userLexerData[lexerData.id] = std::move(userLexerData);
        }
    }

    for (const auto& [ini, isUserConfig] : inis)
    {
        CSimpleIni::TNamesDepend fileLangKeys;
        ini->GetAllKeys(L"filelanguage", fileLangKeys);
        for (const auto& fileLangKey : fileLangKeys)
        {
            auto         utf8FileLangKey = CUnicodeUtils::StdGetUTF8(fileLangKey);
            std::wstring v               = ini->GetValue(L"filelanguage", fileLangKey);
            ReplaceVariables(v, variables);
            std::vector<std::wstring> files;
            stringtok(files, v, true, L";");
            for (const auto& f : files)
            {
                m_fileLang[CUnicodeUtils::StdGetUTF8(CPathUtils::GetFileName(f))] = utf8FileLangKey;
            }
        }
        CSimpleIni::TNamesDepend autolangeys;
        ini->GetAllKeys(L"autolanguage", autolangeys);
        for (const auto& autoLangKey : autolangeys)
        {
            auto         utf8AutoLangKey = CUnicodeUtils::StdGetUTF8(autoLangKey);
            std::wstring v               = ini->GetValue(L"autolanguage", autoLangKey);
            ReplaceVariables(v, variables);
            std::vector<std::wstring> exts;
            stringtok(exts, v, true, L";");
            for (const auto& e : exts)
            {
                m_autoExtLang[CUnicodeUtils::StdGetUTF8(e)] = utf8AutoLangKey;
            }
        }
        if (isUserConfig)
        {
            CSimpleIni::TNamesDepend autoPathKeys;
            ini->GetAllKeys(L"pathlanguage", autoPathKeys);
            for (const auto& appPathKey : autoPathKeys)
            {
                std::wstring v = ini->GetValue(L"pathlanguage", appPathKey);
                auto         f = v.find(L'*');
                if (f != std::wstring::npos)
                {
                    std::wstring p = v.substr(0, f);
                    m_pathsLang[p] = CUnicodeUtils::StdGetUTF8(v.substr(f + 1));
                    m_pathsForLang.push_back(std::move(p));
                }
            }
        }

        CSimpleIni::TNamesDepend langKeys;
        ini->GetAllKeys(L"language", langKeys);
        for (const auto& langKey : langKeys)
        {
            auto         utf8LangKey = CUnicodeUtils::StdGetUTF8(langKey);
            auto&        langData    = m_langData[utf8LangKey];
            std::wstring v           = ini->GetValue(L"language", langKey, L"");
            ReplaceVariables(v, variables);
            std::vector<std::wstring> exts;
            stringtok(exts, v, true, L";");
            std::wstring filters;
            for (const auto& e : exts)
            {
                m_extLang.insert({CUnicodeUtils::StdGetUTF8(e), utf8LangKey});
                if (isUserConfig)
                    m_userExtLang[CUnicodeUtils::StdGetUTF8(e)] = utf8LangKey;
                else
                {
                    if (!filters.empty())
                        filters.push_back(TEXT(';'));
                    filters += TEXT("*.") + e;
                }
            }
            if (!filters.empty())
            {
                auto sKey    = std::wstring(langKey) + TEXT(" file");
                auto foundIt = std::find_if(m_fileTypes.begin(), m_fileTypes.end(),
                                            [&](const auto& item) { return (std::get<0>(item) == sKey); });
                if (foundIt != m_fileTypes.end())
                    m_fileTypes.erase(foundIt);
                m_fileTypes.push_back(std::make_pair(sKey, std::move(filters)));
            }

            std::wstring langSect = L"lang_";
            langSect += langKey;
            CSimpleIni::TNamesDepend specLangKeys;
            ini->GetAllKeys(langSect.c_str(), specLangKeys);
            LanguageData ld;
            if (isUserConfig)
            {
                ld = langData;
            }
            ld.lexer = lexers[langKey];
            for (const auto& sk : specLangKeys)
            {
                if (_wcsnicmp(L"keywords", sk, 8) == 0)
                {
                    int vn;
                    if (!CAppUtils::TryParse(sk + 8, vn, false))
                        APPVERIFY(false);
                    ld.keywordList[vn] = CUnicodeUtils::StdGetUTF8(ini->GetValue(langSect.c_str(), sk));
                }
                else if (_wcsicmp(L"CommentLine", sk) == 0)
                {
                    ld.commentLine = CUnicodeUtils::StdGetUTF8(ini->GetValue(langSect.c_str(), sk));
                    ld.commentLine.erase(ld.commentLine.find_last_not_of('~') + 1); // Erase '~'
                }
                else if (_wcsicmp(L"CommentStreamStart", sk) == 0)
                {
                    ld.commentStreamStart = CUnicodeUtils::StdGetUTF8(ini->GetValue(langSect.c_str(), sk));
                    ld.commentStreamStart.erase(ld.commentStreamStart.find_last_not_of('~') + 1); // Erase '~'
                }
                else if (_wcsicmp(L"CommentStreamEnd", sk) == 0)
                {
                    ld.commentStreamEnd = CUnicodeUtils::StdGetUTF8(ini->GetValue(langSect.c_str(), sk));
                    ld.commentStreamEnd.erase(ld.commentStreamEnd.find_last_not_of('~') + 1); // Erase '~'
                }
                else if (_wcsicmp(L"CommentLineAtStart", sk) == 0)
                {
                    int vn = 0;
                    if (!CAppUtils::TryParse(
                            ini->GetValue(langSect.c_str(), sk), vn, false))
                        APPVERIFY(false);
                    ld.commentLineAtStart = vn != 0;
                }
                else if (_wcsicmp(L"FunctionRegex", sk) == 0)
                {
                    ld.functionRegex = CUnicodeUtils::StdGetUTF8(ini->GetValue(langSect.c_str(), sk));
                    ld.functionRegex.erase(ld.functionRegex.find_last_not_of('~') + 1); // Erase '~'
                }
                else if (_wcsicmp(L"FunctionRegexSort", sk) == 0)
                {
                    if (!CAppUtils::TryParse(
                            ini->GetValue(langSect.c_str(), sk),
                            ld.functionRegexSort, false))
                        APPVERIFY(false);
                }
                else if (_wcsicmp(L"FunctionRegexTrim", sk) == 0)
                {
                    std::string s = CUnicodeUtils::StdGetUTF8(ini->GetValue(langSect.c_str(), sk));
                    s.erase(s.find_last_not_of('~') + 1); // Erase '~'
                    stringtok(ld.functionRegexTrim, s, true, ",");
                }
                else if (_wcsicmp(L"AutoCompleteRegex", sk) == 0)
                {
                    ld.autoCompleteRegex = CUnicodeUtils::StdGetUTF8(ini->GetValue(langSect.c_str(), sk));
                }
                else if (_wcsicmp(L"UserFunctions", sk) == 0)
                {
                    if (!CAppUtils::TryParse(
                            ini->GetValue(langSect.c_str(), sk),
                            ld.userFunctions, false))
                        APPVERIFY(false);
                }
                else if (_wcsicmp(L"DetectionString+", sk) == 0)
                {
                    LexDetectStrings lds;
                    lds.lang      = "+" + CUnicodeUtils::StdGetUTF8(langKey);
                    lds.firstLine = CUnicodeUtils::StdGetUTF8(ini->GetValue(langSect.c_str(), sk));
                    auto sExts    = CUnicodeUtils::StdGetUTF8(ini->GetValue(langSect.c_str(), L"DetectionStringExts"));
                    stringtok(lds.extensions, sExts, true, ";");
                    lexDetectStrings.push_back(std::move(lds));
                }
                else if (_wcsicmp(L"DetectionString-", sk) == 0)
                {
                    LexDetectStrings lds;
                    lds.lang      = "-" + CUnicodeUtils::StdGetUTF8(langKey);
                    lds.firstLine = CUnicodeUtils::StdGetUTF8(ini->GetValue(langSect.c_str(), sk));
                    auto sExts    = CUnicodeUtils::StdGetUTF8(ini->GetValue(langSect.c_str(), L"DetectionStringExts"));
                    stringtok(lds.extensions, sExts, true, ";");
                    lexDetectStrings.push_back(std::move(lds));
                }
                else if (_wcsicmp(L"autocompletion", sk) == 0)
                {
                    ld.autocompletionWords = CUnicodeUtils::StdGetUTF8(ini->GetValue(langSect.c_str(), sk));
                }
            }
            langData = std::move(ld);
        }
        CSimpleIni::TNamesDepend hiddenKeys;
        ini->GetAllKeys(L"hiddenLangs", hiddenKeys);
        for (const auto& hiddenKey : hiddenKeys)
        {
            m_hiddenLangs.insert(hiddenKey);
        }
    }
    m_fileTypes.sort([](const auto& first, const auto& second) {
        return _wcsicmp(first.first.c_str(), second.first.c_str()) < 0;
    });
    m_fileTypes.push_front(std::make_pair(TEXT("All files"), TEXT("*.*")));
    for (auto& [name, mask] : m_fileTypes)
        m_filterSpec.push_back({name.c_str(), mask.c_str()});

    m_bLoaded = true;
}

const std::unordered_map<int, std::string>& CLexStyles::GetKeywordsForLang(const std::string& lang)
{
    auto lt = m_langData.find(lang);
    if (lt != m_langData.end())
    {
        GenerateUserKeywords(lt->second);
        return lt->second.keywordList;
    }

    return emptyUnorderedMap;
}

const std::unordered_map<int, std::string>& CLexStyles::GetKeywordsForLexer(int lexer)
{
    for (auto it = m_langData.begin(); it != m_langData.end(); ++it)
    {
        if (it->second.lexer == lexer)
        {
            GenerateUserKeywords(it->second);
            return it->second.keywordList;
        }
    }

    return emptyUnorderedMap;
}

LanguageData* CLexStyles::GetLanguageData(const std::string& lang)
{
    auto lt = m_langData.find(lang);
    if (lt != m_langData.end())
    {
        return &lt->second;
    }
    return nullptr;
}

bool CLexStyles::AddUserFunctionForLang(const std::string& lang, const std::string& fnc)
{
    auto lt = m_langData.find(lang);
    if (lt != m_langData.end())
    {
        if (lt->second.userFunctions)
        {
            auto [it, success] = lt->second.userKeyWords.insert(fnc);
            if (success)
                lt->second.userKeyWordsUpdated = true;
            return success;
        }
    }
    return false;
}

void CLexStyles::GenerateUserKeywords(LanguageData& ld) const
{
    if (!ld.userKeyWordsUpdated)
        return;
    if (ld.userFunctions)
    {
        std::string keywords;
        for (const auto& w : ld.userKeyWords)
        {
            keywords += w;
            keywords += ' ';
        }
        ld.keywordList[ld.userFunctions] = std::move(keywords);
    }
    ld.userKeyWordsUpdated = false;
}

void CLexStyles::Reload()
{
    m_extLang.clear();
    m_fileLang.clear();
    m_langData.clear();
    m_lexerData.clear();
    m_lexerSection.clear();
    m_userLexerData.clear();
    m_userExtLang.clear();
    m_autoExtLang.clear();
    m_pathsLang.clear();
    m_pathsForLang.clear();
    m_fileTypes.clear();
    m_filterSpec.clear();
    m_hiddenLangs.clear();

    Load();
}

const LexerData& CLexStyles::GetLexerDataForLang(const std::string& lang) const
{
    auto lt = m_langData.find(lang);
    if (lt != m_langData.end())
    {
        auto ld = m_lexerData.find(lt->second.lexer);
        if (ld != m_lexerData.end())
            return ld->second;
    }
    return emptyLexData;
}

const LexerData& CLexStyles::GetLexerDataForLexer(int lexer) const
{
    auto ld = m_lexerData.find(lexer);
    if (ld != m_lexerData.end())
        return ld->second;
    return emptyLexData;
}

const std::string& CLexStyles::GetLanguageForLexer(int lexer) const
{
    for (const auto& [lang, data] : m_langData)
    {
        if (data.lexer == lexer)
            return lang;
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

    auto ait = std::find_if(m_autoExtLang.begin(), m_autoExtLang.end(), [&](const auto& toFind) {
        return _stricmp(toFind.first.c_str(), ext.c_str()) == 0;
    });
    if (ait != m_autoExtLang.end())
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
    edit.Scintilla().SetDocPointer(doc.m_document);
    OnOutOfScope(
        edit.Scintilla().SetDocPointer(nullptr););

    std::string line = edit.GetLine(0);
    for (const auto& m : lexDetectStrings)
    {
        auto foundPos = line.find(m.firstLine);
        if (((m.lang[0] == '-') && (foundPos == 0)) ||
            ((m.lang[0] == '+') && (foundPos != std::string::npos)))
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
    for (const auto& [ext, language] : m_userExtLang)
    {
        if (language.compare(l) == 0)
        {
            if (!exts.empty())
                exts += L";";
            exts += CUnicodeUtils::StdGetUnicode(ext);
        }
    }
    return exts;
}

bool CLexStyles::GetDefaultExtensionForLanguage(const std::string& lang, std::wstring& ext, UINT& index) const
{
    // if there's a user set extension, use that
    for (const auto& [extension, language] : m_userExtLang)
    {
        if (language.compare(lang) == 0)
        {
            ext = CUnicodeUtils::StdGetUnicode(extension);
            return true;
        }
    }
    // Search in filter pattern, use the first extension as the default and return file type index.
    auto langW = CUnicodeUtils::StdGetUnicode(lang) + TEXT(" file");
    for (size_t i = 0, count = m_filterSpec.size(); i < count; ++i)
    {
        if (m_filterSpec[i].pszName == langW)
        {
            index    = static_cast<UINT>(i);
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

bool CLexStyles::IsLanguageHidden(const std::wstring& lang) const
{
    return m_hiddenLangs.find(lang) != m_hiddenLangs.end();
}

size_t CLexStyles::GetFilterSpecCount() const
{
    return m_filterSpec.size();
}

const COMDLG_FILTERSPEC* CLexStyles::GetFilterSpecData() const
{
    return m_filterSpec.data();
}

std::vector<std::wstring> CLexStyles::GetLanguages() const
{
    std::vector<std::wstring> langs;
    for (const auto& [name, data] : m_langData)
    {
        langs.push_back(CUnicodeUtils::StdGetUnicode(name));
    }
    return langs;
}

std::map<std::string, LanguageData>& CLexStyles::GetLanguageDataMap()
{
    return m_langData;
}

void CLexStyles::ReplaceVariables(std::wstring& s, const std::unordered_map<std::wstring, std::wstring>& vars)
{
    size_t pos = s.find(L"$(");
    while (pos != std::wstring::npos)
    {
        size_t       pos2    = s.find(L')', pos);
        std::wstring varName = s.substr(pos, pos2 - pos + 1);
        auto         foundIt = vars.find(varName);
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

void CLexStyles::SetUserForeground(int id, int style, COLORREF clr)
{
    LexerData& ld = m_userLexerData[id];
    if (ld.styles.find(style) != ld.styles.end())
    {
        StyleData& sd      = ld.styles[style];
        sd.foregroundColor = clr;
    }
    else
    {
        StyleData sd       = m_lexerData[id].styles[style];
        sd.foregroundColor = clr;
        ld.styles[style]   = sd;
    }
}

void CLexStyles::SetUserBackground(int id, int style, COLORREF clr)
{
    LexerData& ld = m_userLexerData[id];
    if (ld.styles.find(style) != ld.styles.end())
    {
        StyleData& sd      = ld.styles[style];
        sd.backgroundColor = clr;
    }
    else
    {
        StyleData sd       = m_lexerData[id].styles[style];
        sd.backgroundColor = clr;
        ld.styles[style]   = sd;
    }
}

void CLexStyles::SetUserFont(int id, int style, const std::wstring& font)
{
    LexerData& ld = m_userLexerData[id];
    if (ld.styles.find(style) != ld.styles.end())
    {
        StyleData& sd = ld.styles[style];
        sd.fontName   = font;
    }
    else
    {
        StyleData sd     = m_lexerData[id].styles[style];
        sd.fontName      = font;
        ld.styles[style] = sd;
    }
}

void CLexStyles::SetUserFontSize(int id, int style, int size)
{
    LexerData& ld = m_userLexerData[id];
    if (ld.styles.find(style) != ld.styles.end())
    {
        StyleData& sd = ld.styles[style];
        sd.fontSize   = size;
    }
    else
    {
        StyleData sd     = m_lexerData[id].styles[style];
        sd.fontSize      = size;
        ld.styles[style] = sd;
    }
}

void CLexStyles::SetUserFontStyle(int id, int style, FontStyle fontstyle)
{
    LexerData& ld = m_userLexerData[id];
    if (ld.styles.find(style) != ld.styles.end())
    {
        StyleData& sd = ld.styles[style];
        sd.fontStyle  = fontstyle;
    }
    else
    {
        StyleData sd     = m_lexerData[id].styles[style];
        sd.fontStyle     = fontstyle;
        ld.styles[style] = sd;
    }
}

void CLexStyles::ResetUserData()
{
    m_userLexerData.clear();
    m_userExtLang.clear();
    m_extLang.clear();
    m_langData.clear();
    m_lexerData.clear();
    m_fileTypes.clear();
    m_filterSpec.clear();
    m_hiddenLangs.clear();
    Load();
}

void CLexStyles::ResetUserData(const std::string& language)
{
    CSimpleIni ini;
    ini.SetUnicode();
    std::wstring userStyleFile = CAppUtils::GetDataPath() + L"\\userconfig";

    if (PathFileExists(userStyleFile.c_str()))
    {
        ini.LoadFile(userStyleFile.c_str());
    }
    const auto& lexData = CLexStyles::Instance().GetLexerDataForLang(language);
    const auto  lexID   = lexData.id;

    for (const auto& [id, section] : m_lexerSection)
    {
        if (id == lexID)
        {
            ini.Delete(section.c_str(), nullptr);
        }
    }

    FILE* pFile = nullptr;
    _wfopen_s(&pFile, userStyleFile.c_str(), L"wb");
    ini.SaveFile(pFile);
    fclose(pFile);
    ResetUserData();
}

void CLexStyles::SaveUserData()
{
    CSimpleIni   ini;
    ini.SetUnicode();
    std::wstring userStyleFile = CAppUtils::GetDataPath() + L"\\userconfig";

    if (PathFileExists(userStyleFile.c_str()))
    {
        ini.LoadFile(userStyleFile.c_str());
    }

    for (const auto& [lexerId, lexerData] : m_userLexerData)
    {
        if (lexerId == 0)
            continue;
        // find the lexer section name
        std::wstring section = m_lexerSection[lexerId];
        ini.Delete(section.c_str(), nullptr);
        std::wstring v = std::to_wstring(lexerId);
        ini.SetValue(section.c_str(), L"Lexer", v.c_str());
        ini.SetValue(section.c_str(), L"LexerName", CUnicodeUtils::StdGetUnicode(lexerData.name).c_str());
        if (lexerData.name.empty())
        {
            auto lexIt = m_lexerData.find(lexerId);
            if (lexIt != m_lexerData.end())
            {
                ini.SetValue(section.c_str(), L"LexerName", CUnicodeUtils::StdGetUnicode(lexIt->second.name).c_str());
            }
        }
        bool        hasChangedEntries = false;
        const auto& origStyleData     = m_lexerData[lexerId].styles;
        for (const auto& [styleId, styleData] : lexerData.styles)
        {
            std::wstring style = CStringUtils::Format(L"Style%d", styleId);
            std::wstring sSize = std::to_wstring(styleData.fontSize);
            if (styleData.fontSize == 0)
                sSize.clear();
            int fore = GetRValue(styleData.foregroundColor) << 16 | GetGValue(styleData.foregroundColor) << 8 | GetBValue(styleData.foregroundColor);
            int back = GetRValue(styleData.backgroundColor) << 16 | GetGValue(styleData.backgroundColor) << 8 | GetBValue(styleData.backgroundColor);
            // styleNR=name;foreground;background;fontname;fontstyle;fontsize
            const auto& name = styleData.name.empty() ? m_lexerData[lexerId].styles[styleId].name : styleData.name;
            v                = CStringUtils::Format(L"%s;%06X;%06X;%s;%d;%s", name.c_str(), fore, back, styleData.fontName.c_str(), styleData.fontStyle, sSize.c_str());
            if (auto origStyleIt = origStyleData.find(styleId); origStyleIt != origStyleData.end())
            {
                if (styleData != origStyleIt->second)
                {
                    ini.SetValue(section.c_str(), style.c_str(), v.c_str());
                    hasChangedEntries = true;
                }
            }
            else
            {
                ini.SetValue(section.c_str(), style.c_str(), v.c_str());
                hasChangedEntries = true;
            }
        }
        if (!hasChangedEntries)
            ini.Delete(section.c_str(), nullptr);
    }

    ini.Delete(L"hiddenLangs", nullptr);
    for (const auto& l : m_hiddenLangs)
        ini.SetValue(L"hiddenLangs", l.c_str(), L"hidden");

    // first clear all user extensions, then add them again to the ini file
    ini.Delete(L"language", nullptr);

    for (const auto& [ext, name] : m_userExtLang)
    {
        const auto& origExtIt = m_extLang.find(ext);
        if (origExtIt != m_extLang.end())
        {
            if (origExtIt->second == name)
                continue;
        }
        auto         lang = CUnicodeUtils::StdGetUnicode(name);
        std::wstring v    = ini.GetValue(L"language", lang.c_str(), L"");
        if (!v.empty())
            v += L";";
        v += CUnicodeUtils::StdGetUnicode(ext);
        ini.SetValue(L"language", lang.c_str(), v.c_str());
    }

    // first clear all auto extensions, then add them again to the ini file
    ini.Delete(L"autolanguage", nullptr);

    for (const auto& [lexLang, lexExt] : m_autoExtLang)
    {
        auto         lang = CUnicodeUtils::StdGetUnicode(lexExt);
        std::wstring v    = ini.GetValue(L"autolanguage", lang.c_str(), L"");
        if (!v.empty())
            v += L";";
        v += CUnicodeUtils::StdGetUnicode(lexLang);
        ini.SetValue(L"autolanguage", lang.c_str(), v.c_str());
    }

    int pCount = 0;
    for (const auto& [path, lang] : m_pathsLang)
    {
        std::wstring key = CStringUtils::Format(L"%03d", pCount);
        std::wstring val = path + L"*" + CUnicodeUtils::StdGetUnicode(lang);
        ini.SetValue(L"pathlanguage", key.c_str(), val.c_str());
        ++pCount;
    }

    FILE* pFile = nullptr;
    _wfopen_s(&pFile, userStyleFile.c_str(), L"wb");
    ini.SaveFile(pFile);
    fclose(pFile);

    ResetUserData();
}

void CLexStyles::SetUserExt(const std::wstring& ext, const std::string& lang)
{
    auto iter    = m_userExtLang.begin();
    auto endIter = m_userExtLang.end();
    for (; iter != endIter;)
    {
        if (iter->second.compare(lang) == 0)
            iter = m_userExtLang.erase(iter);
        else
            ++iter;
    }
    std::vector<std::wstring> exts;
    stringtok(exts, ext, true, L"; ,");
    for (const auto& e : exts)
    {
        m_userExtLang[CUnicodeUtils::StdGetUTF8(e)] = lang;
    }
}

void CLexStyles::SetLanguageHidden(const std::wstring& lang, bool hidden)
{
    if (hidden)
        m_hiddenLangs.insert(lang);
    else
        m_hiddenLangs.erase(lang);
}

const std::string& CLexStyles::GetCommentLineForLang(const std::string& lang) const
{
    auto lt = m_langData.find(lang);
    if (lt != m_langData.end())
        return lt->second.commentLine;
    return emptyString;
}

const std::string& CLexStyles::GetCommentStreamStartForLang(const std::string& lang) const
{
    auto lt = m_langData.find(lang);
    if (lt != m_langData.end())
        return lt->second.commentStreamStart;
    return emptyString;
}

const std::string& CLexStyles::GetCommentStreamEndForLang(const std::string& lang) const
{
    auto lt = m_langData.find(lang);
    if (lt != m_langData.end())
        return lt->second.commentStreamEnd;
    return emptyString;
}

bool CLexStyles::GetCommentLineAtStartForLang(const std::string& lang) const
{
    auto lt = m_langData.find(lang);
    if (lt != m_langData.end())
        return lt->second.commentLineAtStart;
    return false;
}

const std::string& CLexStyles::GetFunctionRegexForLang(const std::string& lang) const
{
    auto lt = m_langData.find(lang);
    if (lt != m_langData.end())
        return lt->second.functionRegex;
    return emptyString;
}

int CLexStyles::GetFunctionRegexSortForLang(const std::string& lang) const
{
    auto lt = m_langData.find(lang);
    if (lt != m_langData.end())
        return lt->second.functionRegexSort;
    return 0;
}

const std::vector<std::string>& CLexStyles::GetFunctionRegexTrimForLang(const std::string& lang) const
{
    auto lt = m_langData.find(lang);
    if (lt != m_langData.end())
        return lt->second.functionRegexTrim;
    return emptyStringVector;
}

const std::string& CLexStyles::GetAutoCompleteRegexForLang(const std::string& lang) const
{
    auto lt = m_langData.find(lang);
    if (lt != m_langData.end())
        return lt->second.autoCompleteRegex;
    return emptyString;
}

std::unordered_map<std::string, std::string> CLexStyles::GetAutoCompleteWords() const
{
    std::unordered_map<std::string, std::string> autocompleteWords;
    for (const auto& [lang, data] : m_langData)
    {
        if (!data.autocompletionWords.empty())
            autocompleteWords[lang] = data.autocompletionWords;
    }
    return autocompleteWords;
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
            // only add this if the extension is set in m_autoExtLang
            std::string e  = CUnicodeUtils::StdGetUTF8(sExt);
            auto        it = std::find_if(m_extLang.begin(), m_extLang.end(), [&](const auto& toFind) {
                return _stricmp(toFind.first.c_str(), e.c_str()) == 0;
            });
            if (it == m_extLang.end())
            {
                // set user selected language as the default for this extension
                m_autoExtLang.erase(e);
                m_autoExtLang[e] = language;
                SaveUserData();
                return;
            }
            else
                m_autoExtLang.erase(e);
        }
        // store the full path and the language
        m_pathsLang[path] = language;
        m_pathsForLang.push_front(path);
        while (m_pathsForLang.size() > 100)
            m_pathsForLang.pop_back();
        SaveUserData();
    }
}
