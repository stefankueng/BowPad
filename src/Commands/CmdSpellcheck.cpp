// This file is part of BowPad.
//
// Copyright (C) 2015-2017, 2020-2022 - Stefan Kueng
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
#include "BowPad.h"
#include "CmdSpellcheck.h"
#include "ScintillaWnd.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "LexStyles.h"
#include "DebugOutput.h"
#include "AppUtils.h"
#include "PropertySet.h"
#include "SciLexer.h"
#include "OnOutOfScope.h"
#include "ResString.h"

#include <spellcheck.h>
#include <Richedit.h>

extern IUIFramework* g_pFramework;
extern UINT32        g_contextID;

namespace
{
ISpellCheckerFactoryPtr   g_spellCheckerFactory = nullptr;
ISpellCheckerPtr          g_spellChecker        = nullptr;
std::vector<std::wstring> g_languages;
UINT                      g_checkTimer = 0;
std::string               g_wordChars;
std::string               g_sentenceChars;
} // namespace

CCmdSpellCheck::CCmdSpellCheck(void* obj)
    : ICommand(obj)
    , m_enabled(true)
    , m_activeLexer(-1)
    , m_useComprehensiveCheck(false)
    , m_textBufLen(0)
    , m_lastCheckedPos(0)
{
    m_enabled    = CIniSettings::Instance().GetInt64(L"spellcheck", L"enabled", 1) != 0;
    g_checkTimer = GetTimerID();
    // try to create the spell checker factory
    HRESULT hr   = CoCreateInstance(__uuidof(SpellCheckerFactory), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_spellCheckerFactory));
    if (SUCCEEDED(hr))
    {
        // get all available languages
        IEnumStringPtr enumLanguages = nullptr;
        hr                           = g_spellCheckerFactory->get_SupportedLanguages(&enumLanguages);

        if (SUCCEEDED(hr))
        {
            while (S_OK == hr)
            {
                LPOLESTR lang = nullptr;
                hr            = enumLanguages->Next(1, &lang, nullptr);

                if (S_OK == hr)
                {
                    g_languages.push_back(lang);
                    CoTaskMemFree(lang);
                }
            }
        }

        if (!g_languages.empty())
        {
            g_pFramework->SetModes(UI_MAKEAPPMODE(0) | UI_MAKEAPPMODE(1));
        }

        m_lang         = CIniSettings::Instance().GetString(L"spellcheck", L"language", L"en-US");
        BOOL supported = FALSE;
        hr             = g_spellCheckerFactory->IsSupported(m_lang.c_str(), &supported);
        if (supported)
        {
            g_spellChecker = nullptr;
            hr             = g_spellCheckerFactory->CreateSpellChecker(m_lang.c_str(), &g_spellChecker);
        }
        m_textBufLen = 1024;
        m_textBuffer = std::make_unique<char[]>(m_textBufLen);
    }
    else
    {
        m_enabled = false;
    }

    // create a string with all the word chars
    g_wordChars.clear();
    g_sentenceChars.clear();
    for (int ch = 0; ch < 256; ++ch)
    {
        if (ch >= 0x80 || isalnum(ch) || ch == '_' || ch == '\'')
            g_wordChars += static_cast<char>(ch);
    }
    g_sentenceChars = g_wordChars;
    g_sentenceChars += ' ';
    g_sentenceChars += ',';
    g_sentenceChars += ';';
    g_sentenceChars += '"';
    g_sentenceChars += '\'';
    g_sentenceChars += '%';
    g_sentenceChars += '&';
    g_sentenceChars += '/';
    g_sentenceChars += '(';
    g_sentenceChars += ')';
    g_sentenceChars += '\r';
    g_sentenceChars += '\n';

    g_contextID = m_enabled && g_spellChecker ? cmdContextSpellMap : cmdContextMap;

    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

inline CCmdSpellCheck::~CCmdSpellCheck()
{
    g_spellChecker        = nullptr;
    g_spellCheckerFactory = nullptr;
}

void CCmdSpellCheck::ScintillaNotify(SCNotification* pScn)
{
    switch (pScn->nmhdr.code)
    {
        case SCN_UPDATEUI:
            if (m_enabled && (pScn->updated & (SC_UPDATE_V_SCROLL | SC_UPDATE_H_SCROLL)) != 0)
            {
                m_lastCheckedPos = 0;
                SetTimer(GetHwnd(), g_checkTimer, 500, nullptr);
            }
            break;
        case SCN_MODIFIED:
            if (m_enabled && (pScn->modificationType & (SC_MOD_DELETETEXT | SC_MOD_INSERTTEXT)) != 0)
            {
                m_lastCheckedPos = 0;
                SetTimer(GetHwnd(), g_checkTimer, 500, nullptr);
            }
            break;
    }
}

void CCmdSpellCheck::Check()
{
    if (m_enabled && g_spellChecker)
    {
#ifdef _DEBUG
        ProfileTimer timer(L"SpellCheck");
#endif
        bool checkAll       = CIniSettings::Instance().GetInt64(L"spellcheck", L"checkall", 1) != 0;
        bool checkUppercase = CIniSettings::Instance().GetInt64(L"spellcheck", L"uppercase", 1) != 0;
        if (m_activeLexer != static_cast<int>(Scintilla().Lexer()))
        {
            m_activeLexer = static_cast<int>(Scintilla().Lexer());
            m_lexerData   = CLexStyles::Instance().GetLexerDataForLexer(m_activeLexer);
            m_keywords.clear();
            const auto& keywords = CLexStyles::Instance().GetKeywordsForLexer(m_activeLexer);
            for (const auto& [type, words] : keywords)
            {
                stringtokset(m_keywords, words, true, " ", true);
            }
            m_lastCheckedPos        = 0;
            m_useComprehensiveCheck = (m_activeLexer == SCLEX_NULL || m_activeLexer == SCLEX_INDENT);
        }

        auto wordCharsBuffer = GetWordChars();
        OnOutOfScope(Scintilla().SetWordChars(wordCharsBuffer.c_str()));

        if (m_useComprehensiveCheck)
            Scintilla().SetWordChars(g_sentenceChars.c_str());
        else
            Scintilla().SetWordChars(g_wordChars.c_str());
        Sci_TextRangeFull textRange{};
        auto              firstLine = Scintilla().FirstVisibleLine();
        auto              lastLine  = firstLine + Scintilla().LinesOnScreen();
        auto              firstPos  = static_cast<Sci_Position>(Scintilla().PositionFromLine(firstLine));
        textRange.chrg.cpMin        = firstPos;
        textRange.chrg.cpMax        = textRange.chrg.cpMin;
        auto lastPos                = Scintilla().PositionFromLine(lastLine) + Scintilla().LineLength(lastLine);
        auto textLength             = Scintilla().Length();
        if (lastPos < 0)
            lastPos = textLength - textRange.chrg.cpMin;
        if (m_lastCheckedPos)
        {
            textRange.chrg.cpMax = static_cast<Sci_Position>(m_lastCheckedPos);
        }
        auto start = GetTickCount64();
        while (textRange.chrg.cpMax < lastPos)
        {
            if (GetTickCount64() - start > 300)
            {
                m_lastCheckedPos = textRange.chrg.cpMax;
                if (g_checkTimer)
                    SetTimer(GetHwnd(), g_checkTimer, 10, nullptr);
                break;
            }
            textRange.chrg.cpMin = static_cast<Sci_Position>(Scintilla().WordStartPosition(textRange.chrg.cpMax + 1, TRUE));
            if (textRange.chrg.cpMin < textRange.chrg.cpMax && textRange.chrg.cpMin >= firstPos)
                break;
            textRange.chrg.cpMax = static_cast<Sci_Position>(Scintilla().WordEndPosition(textRange.chrg.cpMin, TRUE));
            if (textRange.chrg.cpMin == textRange.chrg.cpMax)
            {
                textRange.chrg.cpMax++;
                // since Scintilla squiggles to the end of the text even if told to stop one char before it,
                // we have to clear here the squiggly lines to the end.
                if (textRange.chrg.cpMin)
                {
                    Scintilla().SetIndicatorCurrent(INDIC_MISSPELLED);
                    Scintilla().IndicatorClearRange(textRange.chrg.cpMin - 1, textRange.chrg.cpMax - textRange.chrg.cpMin + 1);
                }
                continue;
            }
            while (textRange.chrg.cpMin < lastPos &&
                   textRange.chrg.cpMax > textRange.chrg.cpMin &&
                   Scintilla().IndicatorValueAt(INDIC_URLHOTSPOT, textRange.chrg.cpMin))
                ++textRange.chrg.cpMin;

            int style = Scintilla().StyleAt(textRange.chrg.cpMin);
            // check if the word is text, doc or comment
            if (!checkAll)
            {
                bool isText = true;
                switch (m_activeLexer)
                {
                    case SCLEX_NULL:   // text
                    case SCLEX_INDENT: // text
                        break;
                    case SCLEX_MARKDOWN:
                        switch (style)
                        {
                            case SCE_MARKDOWN_LINK:
                            case SCE_MARKDOWN_CODE:
                            case SCE_MARKDOWN_CODE2:
                            case SCE_MARKDOWN_CODEBK:
                                // mark word as correct (remove the squiggle line)
                                Scintilla().SetIndicatorCurrent(INDIC_MISSPELLED);
                                Scintilla().IndicatorClearRange(textRange.chrg.cpMin, textRange.chrg.cpMax - textRange.chrg.cpMin);
                                Scintilla().IndicatorClearRange(textRange.chrg.cpMin, textRange.chrg.cpMax - textRange.chrg.cpMin + 1);
                                Scintilla().SetIndicatorCurrent(INDIC_MISSPELLED_DEL);
                                Scintilla().IndicatorClearRange(textRange.chrg.cpMin, textRange.chrg.cpMax - textRange.chrg.cpMin);
                                Scintilla().IndicatorClearRange(textRange.chrg.cpMin, textRange.chrg.cpMax - textRange.chrg.cpMin + 1);
                                continue;
                        }
                        break;
                    default:
                        if (style < static_cast<int>(m_lexerData.styles.size()))
                        {
                            const auto& sStyle = m_lexerData.styles[style].name;
                            if ((sStyle.find(L"DOC") == std::wstring::npos) &&
                                (sStyle.find(L"COMMENT") == std::wstring::npos) &&
                                (sStyle.find(L"STRING") == std::wstring::npos) &&
                                (sStyle.find(L"TEXT") == std::wstring::npos))
                                isText = false;
                        }
                        if (!isText)
                        {
                            // mark word as correct (remove the squiggle line)
                            Scintilla().SetIndicatorCurrent(INDIC_MISSPELLED);
                            Scintilla().IndicatorClearRange(textRange.chrg.cpMin, textRange.chrg.cpMax - textRange.chrg.cpMin);
                            Scintilla().IndicatorClearRange(textRange.chrg.cpMin, textRange.chrg.cpMax - textRange.chrg.cpMin + 1);
                            Scintilla().SetIndicatorCurrent(INDIC_MISSPELLED_DEL);
                            Scintilla().IndicatorClearRange(textRange.chrg.cpMin, textRange.chrg.cpMax - textRange.chrg.cpMin);
                            Scintilla().IndicatorClearRange(textRange.chrg.cpMin, textRange.chrg.cpMax - textRange.chrg.cpMin + 1);
                            continue;
                        }
                }
            }
            assert(textRange.chrg.cpMax >= textRange.chrg.cpMin);
            if (m_textBufLen < (textRange.chrg.cpMax - textRange.chrg.cpMin + 2))
            {
                m_textBufLen = textRange.chrg.cpMax - textRange.chrg.cpMin + 1024;
                m_textBuffer = std::make_unique<char[]>(m_textBufLen);
            }
            textRange.lpstrText = m_textBuffer.get();
            if (textRange.chrg.cpMax < textLength)
            {
                textRange.chrg.cpMax++;
                Scintilla().GetTextRangeFull(&textRange);
            }
            else
            {
                Scintilla().GetTextRangeFull(&textRange);
                textRange.chrg.cpMax++;
            }
            auto len = strlen(textRange.lpstrText);
            if (len == 0)
            {
                textRange.chrg.cpMax--;
                Scintilla().GetTextRangeFull(&textRange);
                len = strlen(textRange.lpstrText);
                textRange.chrg.cpMax++;
                len++;
            }
            if (len && textRange.lpstrText[len])
                textRange.lpstrText[len - 1] = 0;
            textRange.chrg.cpMax--;
            if (textRange.lpstrText[0])
            {
                auto                  sWord             = CUnicodeUtils::StdGetUnicode(textRange.lpstrText);

                IEnumSpellingErrorPtr enumSpellingError = nullptr;
                HRESULT               hr                = S_FALSE;
                if (m_useComprehensiveCheck)
                    hr = g_spellChecker->ComprehensiveCheck(sWord.c_str(), &enumSpellingError);
                else
                    hr = g_spellChecker->Check(sWord.c_str(), &enumSpellingError);

                // first mark all text as correct (remove the squiggle lines)
                Scintilla().SetIndicatorCurrent(INDIC_MISSPELLED);
                Scintilla().IndicatorClearRange(textRange.chrg.cpMin, textRange.chrg.cpMax - textRange.chrg.cpMin);
                Scintilla().IndicatorClearRange(textRange.chrg.cpMin, textRange.chrg.cpMax - textRange.chrg.cpMin + 1);
                Scintilla().SetIndicatorCurrent(INDIC_MISSPELLED_DEL);
                Scintilla().IndicatorClearRange(textRange.chrg.cpMin, textRange.chrg.cpMax - textRange.chrg.cpMin);
                Scintilla().IndicatorClearRange(textRange.chrg.cpMin, textRange.chrg.cpMax - textRange.chrg.cpMin + 1);

                if (SUCCEEDED(hr))
                {
                    ISpellingErrorPtr spellingError = nullptr;
                    hr                              = enumSpellingError->Next(&spellingError);
                    while (hr == S_OK)
                    {
                        CORRECTIVE_ACTION action = CORRECTIVE_ACTION_NONE;
                        spellingError->get_CorrectiveAction(&action);
                        if (action != CORRECTIVE_ACTION_NONE)
                        {
                            // mark text as misspelled
                            ULONG errLen = 0;
                            spellingError->get_Length(&errLen);
                            ULONG errStart = 0;
                            spellingError->get_StartIndex(&errStart);

                            Sci_TextRangeFull wordRange{};
                            wordRange.chrg.cpMin = textRange.chrg.cpMin + errStart;
                            wordRange.chrg.cpMax = wordRange.chrg.cpMin + errLen;
                            wordRange.lpstrText  = m_textBuffer.get();
                            Scintilla().GetTextRangeFull(&wordRange);
                            sWord       = CUnicodeUtils::StdGetUnicode(wordRange.lpstrText);

                            bool ignore = false;
                            // ignore words that contain numbers/digits
                            if (std::ranges::any_of(sWord, ::iswdigit))
                                ignore = true;
                            // ignore words that contain uppercase letters in the middle
                            if (!ignore && !checkUppercase && (std::any_of(sWord.begin() + 1, sWord.end(), ::iswupper)))
                                ignore = true;

                            // ignore keywords of the currently selected lexer
                            if (!ignore && m_keywords.contains(textRange.lpstrText))
                                ignore = true;

                            if (!ignore)
                            {
                                Scintilla().SetIndicatorCurrent(action == CORRECTIVE_ACTION_DELETE ? INDIC_MISSPELLED_DEL : INDIC_MISSPELLED);
                                Scintilla().IndicatorFillRange(textRange.chrg.cpMin + errStart, errLen);
                            }
                        }
                        hr = enumSpellingError->Next(&spellingError);
                    }
                }
            }
        }
    }
}

void CCmdSpellCheck::OnTimer(UINT id)
{
    if (id == g_checkTimer)
    {
        KillTimer(GetHwnd(), g_checkTimer);
        Check();
    }
}

HRESULT CCmdSpellCheck::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, m_enabled, pPropVarNewValue);
    }
    return E_NOTIMPL;
}

bool CCmdSpellCheck::Execute()
{
    // If we don't have a spell checker installed/working, we won't be enabled
    // and we don't want to allow anything to toggle us to becoming enabled
    // because we won't work.
    if (!g_spellCheckerFactory)
        return false;
    m_enabled = CIniSettings::Instance().GetInt64(L"spellcheck", L"enabled", 1) != 0;
    m_enabled = !m_enabled;
    CIniSettings::Instance().SetInt64(L"spellcheck", L"enabled", m_enabled);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    if (m_enabled)
    {
        m_lastCheckedPos = 0;
        Check();
    }
    else
    {
        Scintilla().SetIndicatorCurrent(INDIC_MISSPELLED);
        Scintilla().IndicatorClearRange(0, Scintilla().Length());
        Scintilla().SetIndicatorCurrent(INDIC_MISSPELLED_DEL);
        Scintilla().IndicatorClearRange(0, Scintilla().Length());
    }
    g_contextID = m_enabled && g_spellChecker ? cmdContextSpellMap : cmdContextMap;
    return true;
}

CCmdSpellCheckLang::CCmdSpellCheckLang(void* obj)
    : ICommand(obj)
{
}

HRESULT CCmdSpellCheckLang::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue)
{
    HRESULT hr = E_FAIL;

    if (key == UI_PKEY_Categories)
    {
        IUICollectionPtr pCollection;
        hr = pPropVarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;

        for (wchar_t i = 'A'; i <= 'Z'; ++i)
        {
            // Create a property set for the category.
            CPropertySet* pCat;
            hr = CPropertySet::CreateInstance(&pCat);
            if (FAILED(hr))
            {
                return hr;
            }

            wchar_t sName[2] = {i, L'\0'};
            // Initialize the property set with the label that was just loaded and a category id of 0.
            pCat->InitializeCategoryProperties(sName, i - 'A');
            pCollection->Add(pCat);
            pCat->Release();
        }
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        IUICollectionPtr pCollection;
        hr = pPropVarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;

        // populate the dropdown with the languages
        wchar_t buf[1024] = {};
        for (const auto& lang : g_languages)
        {
            if (GetLocaleInfoEx(lang.c_str(), LOCALE_SLOCALIZEDDISPLAYNAME, buf, _countof(buf)))
            {
                int catId = buf[0] - 'A';
                if (catId > 26)
                    catId = buf[0] - 'a';
                CAppUtils::AddStringItem(pCollection, buf, catId, EMPTY_IMAGE);
            }
            else
            {
                int catId = lang[0] - 'A';
                if (catId > 26)
                    catId = lang[0] - 'a';
                CAppUtils::AddStringItem(pCollection, lang.c_str(), catId, EMPTY_IMAGE);
            }
        }
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
        InvalidateUICommand(UI_INVALIDATIONS_VALUE, &UI_PKEY_SelectedItem);
        hr = S_OK;
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        std::wstring lang = CIniSettings::Instance().GetString(L"spellcheck", L"language", L"en-US");
        hr                = S_FALSE;
        for (size_t i = 0; i < g_languages.size(); ++i)
        {
            if (g_languages[i] == lang)
            {
                hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, static_cast<UINT>(i), pPropVarNewValue);
                break;
            }
        }
    }
    return hr;
}

HRESULT CCmdSpellCheckLang::IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* pPropVarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    HRESULT hr = E_FAIL;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if (key && *key == UI_PKEY_SelectedItem)
        {
            if (g_spellCheckerFactory)
            {
                UINT selected;
                hr = UIPropertyToUInt32(*key, *pPropVarValue, &selected);
                InvalidateUICommand(cmdFunctions, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
                std::wstring lang      = g_languages[selected];

                BOOL         supported = FALSE;
                hr                     = g_spellCheckerFactory->IsSupported(lang.c_str(), &supported);
                if (supported)
                {
                    g_spellChecker = nullptr;
                    hr             = g_spellCheckerFactory->CreateSpellChecker(lang.c_str(), &g_spellChecker);
                    if (SUCCEEDED(hr))
                    {
                        CIniSettings::Instance().SetString(L"spellcheck", L"language", lang.c_str());
                        if (g_checkTimer)
                            SetTimer(GetHwnd(), g_checkTimer, 1, nullptr);
                    }
                }
            }
            hr = S_OK;
        }
    }
    return hr;
}

CCmdSpellCheckCorrect::CCmdSpellCheckCorrect(void* obj)
    : ICommand(obj)
{
}

HRESULT CCmdSpellCheckCorrect::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue)
{
    HRESULT hr = E_FAIL;

    if (key == UI_PKEY_Categories)
    {
        ResString        sCorrect(g_hRes, IDS_SPELLCHECK_CORRECT);
        ResString        sIgnore(g_hRes, IDS_SPELLCHECK_IGNORE);

        IUICollectionPtr pCollection;
        hr = pPropVarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;

        // Create a property set for the category.
        CPropertySet* pCat;
        hr = CPropertySet::CreateInstance(&pCat);
        if (FAILED(hr))
            return hr;
        pCat->InitializeCategoryProperties(sCorrect, 0);
        pCollection->Add(pCat);
        pCat->Release();

        hr = CPropertySet::CreateInstance(&pCat);
        if (FAILED(hr))
            return hr;
        pCat->InitializeCategoryProperties(sIgnore, 1);
        pCollection->Add(pCat);
        pCat->Release();
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        m_suggestions.clear();
        IUICollectionPtr pCollection;
        hr = pPropVarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;

        // The list will retain whatever from last time so clear it.
        pCollection->Clear();

        // populate the dropdown with the suggestions
        if (g_spellChecker)
        {
            std::wstring sWord;

            auto         wordcharsbuffer = GetWordChars();
            OnOutOfScope(Scintilla().SetWordChars(wordcharsbuffer.c_str()));

            Scintilla().SetWordChars(g_wordChars.c_str());
            sWord                                  = CUnicodeUtils::StdGetUnicode(GetCurrentWord(false));

            IEnumStringPtr enumSpellingSuggestions = nullptr;
            hr                                     = g_spellChecker->Suggest(sWord.c_str(), &enumSpellingSuggestions);
            if (SUCCEEDED(hr))
            {
                hr = S_OK;
                while (hr == S_OK)
                {
                    LPOLESTR suggestion = nullptr;
                    hr                  = enumSpellingSuggestions->Next(1, &suggestion, nullptr);

                    if (S_OK == hr)
                    {
                        m_suggestions.push_back(suggestion);
                        CAppUtils::AddStringItem(pCollection, suggestion, 0, EMPTY_IMAGE);

                        CoTaskMemFree(suggestion);
                    }
                }
            }
        }
        if (m_suggestions.empty())
        {
            ResString sNoSuggestions(g_hRes, IDS_SPELLCHECK_NOSUGGESTIONS);
            CAppUtils::AddStringItem(pCollection, sNoSuggestions, 0, EMPTY_IMAGE);
        }
        else
        {
            ResString sIgnoreSession(g_hRes, IDS_SPELLCHECK_IGNORESESSION);
            ResString sAddToDictionary(g_hRes, IDS_SPELLCHECK_ADDTODICTIONARY);

            CAppUtils::AddStringItem(pCollection, sIgnoreSession, 1, EMPTY_IMAGE);
            CAppUtils::AddStringItem(pCollection, sAddToDictionary, 1, EMPTY_IMAGE);
        }
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
        InvalidateUICommand(UI_INVALIDATIONS_VALUE, &UI_PKEY_SelectedItem);

        hr = S_OK;
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        return UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, static_cast<UINT>(UI_COLLECTION_INVALIDINDEX), pPropVarNewValue);
    }
    return hr;
}

HRESULT CCmdSpellCheckCorrect::IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* pPropVarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    HRESULT hr = E_FAIL;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if (key && *key == UI_PKEY_SelectedItem)
        {
            UINT selected;
            hr = UIPropertyToUInt32(*key, *pPropVarValue, &selected);
            if (SUCCEEDED(hr))
            {
                std::wstring sWord;

                Scintilla().SetCharsDefault();
                sWord = CUnicodeUtils::StdGetUnicode(GetCurrentWord(false));

                if (m_suggestions.empty())
                    ++selected; // adjust for the "no corrections found" entry
                if (selected < m_suggestions.size())
                {
                    auto currentPos = Scintilla().CurrentPos();
                    auto startPos   = Scintilla().WordStartPosition(currentPos, true);
                    auto endPos     = Scintilla().WordEndPosition(currentPos, true);
                    Scintilla().SetSelection(startPos, endPos);
                    Scintilla().ReplaceSel(CUnicodeUtils::StdGetUTF8(m_suggestions[selected]).c_str());
                }
                else
                {
                    if (g_spellChecker)
                    {
                        if (selected == m_suggestions.size())
                        {
                            // ignore for this session
                            g_spellChecker->Ignore(sWord.c_str());
                        }
                        if (selected == (m_suggestions.size() + 1))
                        {
                            // add to Dictionary
                            g_spellChecker->Add(sWord.c_str());
                        }
                    }
                }
            }
            hr = S_OK;
            InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
        }
    }
    return hr;
}

void CCmdSpellCheckCorrect::ScintillaNotify(SCNotification* pScn)
{
    if (pScn->nmhdr.code == SCN_UPDATEUI)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
    }
}

CCmdSpellCheckAll::CCmdSpellCheckAll(void* obj)
    : ICommand(obj)
{
}

bool CCmdSpellCheckAll::Execute()
{
    bool bCheckAll = CIniSettings::Instance().GetInt64(L"spellcheck", L"checkall", 1) != 0;
    bCheckAll      = !bCheckAll;
    CIniSettings::Instance().SetInt64(L"spellcheck", L"checkall", bCheckAll);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    if (g_checkTimer)
        SetTimer(GetHwnd(), g_checkTimer, 1, nullptr);
    return true;
}

void CCmdSpellCheckAll::AfterInit()
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

HRESULT CCmdSpellCheckAll::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        bool bCheckAll = CIniSettings::Instance().GetInt64(L"spellcheck", L"checkall", 1) != 0;
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, !bCheckAll, pPropVarNewValue);
    }
    return E_NOTIMPL;
}

CCmdSpellCheckUpper::CCmdSpellCheckUpper(void* obj)
    : ICommand(obj)
{
}

bool CCmdSpellCheckUpper::Execute()
{
    bool bCheckUpper = CIniSettings::Instance().GetInt64(L"spellcheck", L"uppercase", 1) != 0;
    bCheckUpper      = !bCheckUpper;
    CIniSettings::Instance().SetInt64(L"spellcheck", L"uppercase", bCheckUpper);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    if (g_checkTimer)
        SetTimer(GetHwnd(), g_checkTimer, 1, nullptr);
    return true;
}

void CCmdSpellCheckUpper::AfterInit()
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

HRESULT CCmdSpellCheckUpper::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        bool bCheckUpper = CIniSettings::Instance().GetInt64(L"spellcheck", L"uppercase", 1) != 0;
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, bCheckUpper, pPropVarNewValue);
    }
    return E_NOTIMPL;
}
