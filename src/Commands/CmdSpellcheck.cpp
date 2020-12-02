// This file is part of BowPad.
//
// Copyright (C) 2015-2017, 2020 - Stefan Kueng
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

#include <spellcheck.h>
#include <Richedit.h>

extern IUIFramework* g_pFramework;
extern UINT32        g_contextID;

namespace
{
ISpellCheckerFactoryPtr   g_spellCheckerFactory = nullptr;
ISpellCheckerPtr          g_SpellChecker        = nullptr;
std::vector<std::wstring> g_languages;
UINT                      g_checktimer = 0;
std::string               g_wordchars;
} // namespace

CCmdSpellcheck::CCmdSpellcheck(void* obj)
    : ICommand(obj)
    , m_enabled(true)
    , m_activeLexer(-1)
    , m_textbuflen(0)
    , m_lastcheckedpos(0)
{
    m_enabled    = CIniSettings::Instance().GetInt64(L"spellcheck", L"enabled", 1) != 0;
    g_checktimer = GetTimerID();
    // try to create the spell checker factory
    HRESULT hr = CoCreateInstance(__uuidof(SpellCheckerFactory), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_spellCheckerFactory));
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
            g_SpellChecker = nullptr;
            hr             = g_spellCheckerFactory->CreateSpellChecker(m_lang.c_str(), &g_SpellChecker);
        }
        m_textbuflen = 1024;
        m_textbuffer = std::make_unique<char[]>(m_textbuflen);
    }
    else
    {
        m_enabled = false;
    }

    // create a string with all the word chars
    g_wordchars.clear();
    for (int ch = 0; ch < 256; ++ch)
    {
        if (ch >= 0x80 || isalnum(ch) || ch == '_' || ch == '\'')
            g_wordchars += (char)ch;
    }

    g_contextID = m_enabled && g_SpellChecker ? cmdContextSpellMap : cmdContextMap;

    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

inline CCmdSpellcheck::~CCmdSpellcheck()
{
    g_SpellChecker        = nullptr;
    g_spellCheckerFactory = nullptr;
}

void CCmdSpellcheck::ScintillaNotify(SCNotification* pScn)
{
    switch (pScn->nmhdr.code)
    {
        case SCN_UPDATEUI:
            if (m_enabled && (pScn->updated & (SC_UPDATE_V_SCROLL | SC_UPDATE_H_SCROLL)) != 0)
            {
                m_lastcheckedpos = 0;
                SetTimer(GetHwnd(), g_checktimer, 500, nullptr);
            }
            break;
        case SCN_MODIFIED:
            if (m_enabled && (pScn->modificationType & (SC_MOD_DELETETEXT | SC_MOD_INSERTTEXT)) != 0)
            {
                m_lastcheckedpos = 0;
                SetTimer(GetHwnd(), g_checktimer, 500, nullptr);
            }
            break;
    }
}

void CCmdSpellcheck::Check()
{
    if (m_enabled && g_SpellChecker)
    {
#ifdef _DEBUG
        ProfileTimer timer(L"SpellCheck");
#endif
        bool CheckAll       = CIniSettings::Instance().GetInt64(L"spellcheck", L"checkall", 1) != 0;
        bool CheckUppercase = CIniSettings::Instance().GetInt64(L"spellcheck", L"uppercase", 1) != 0;
        if (m_activeLexer != (int)ScintillaCall(SCI_GETLEXER))
        {
            m_activeLexer = (int)ScintillaCall(SCI_GETLEXER);
            m_lexerData   = CLexStyles::Instance().GetLexerDataForLexer(m_activeLexer);
            m_keywords.clear();
            const auto& keywords = CLexStyles::Instance().GetKeywordsForLexer(m_activeLexer);
            for (const auto& words : keywords)
            {
                stringtokset(m_keywords, words.second, true, " ", true);
            }
            m_lastcheckedpos = 0;
        }

        auto wordcharsbuffer = GetWordChars();
        OnOutOfScope(ScintillaCall(SCI_SETWORDCHARS, 0, (sptr_t)wordcharsbuffer.c_str()));

        ScintillaCall(SCI_SETWORDCHARS, 0, (sptr_t)g_wordchars.c_str());
        Sci_TextRange textrange{};
        auto          firstline = ScintillaCall(SCI_GETFIRSTVISIBLELINE);
        auto          lastline  = firstline + ScintillaCall(SCI_LINESONSCREEN);
        textrange.chrg.cpMin    = (Sci_PositionCR)ScintillaCall(SCI_POSITIONFROMLINE, firstline);
        textrange.chrg.cpMax    = textrange.chrg.cpMin;
        auto lastpos            = ScintillaCall(SCI_POSITIONFROMLINE, lastline) + ScintillaCall(SCI_LINELENGTH, lastline);
        auto textlength         = ScintillaCall(SCI_GETLENGTH);
        if (lastpos < 0)
            lastpos = textlength - textrange.chrg.cpMin;
        if (m_lastcheckedpos)
        {
            textrange.chrg.cpMax = (Sci_PositionCR)m_lastcheckedpos;
        }
        auto start = GetTickCount64();
        while (textrange.chrg.cpMax < lastpos)
        {
            if (GetTickCount64() - start > 300)
            {
                m_lastcheckedpos = textrange.chrg.cpMax;
                if (g_checktimer)
                    SetTimer(GetHwnd(), g_checktimer, 10, nullptr);
                break;
            }
            textrange.chrg.cpMin = (Sci_PositionCR)ScintillaCall(SCI_WORDSTARTPOSITION, textrange.chrg.cpMax + 1, TRUE);
            if (textrange.chrg.cpMin < textrange.chrg.cpMax)
                break;
            textrange.chrg.cpMax = (Sci_PositionCR)ScintillaCall(SCI_WORDENDPOSITION, textrange.chrg.cpMin, TRUE);
            if (textrange.chrg.cpMin == textrange.chrg.cpMax)
            {
                textrange.chrg.cpMax++;
                // since Scintilla squiggles to the end of the text even if told to stop one char before it,
                // we have to clear here the squiggly lines to the end.
                if (textrange.chrg.cpMin)
                {
                    ScintillaCall(SCI_SETINDICATORCURRENT, INDIC_MISSPELLED);
                    ScintillaCall(SCI_INDICATORCLEARRANGE, textrange.chrg.cpMin - 1, textrange.chrg.cpMax - textrange.chrg.cpMin + 1);
                }
                continue;
            }
            if (ScintillaCall(SCI_INDICATORVALUEAT, INDIC_URLHOTSPOT, textrange.chrg.cpMin))
                continue;

            int style = (int)ScintillaCall(SCI_GETSTYLEAT, textrange.chrg.cpMin);
            // check if the word is text, doc or comment
            if (!CheckAll)
            {
                bool isText = true;
                switch (m_activeLexer)
                {
                    case SCLEX_NULL: // text
                    case SCLEX_MARKDOWN:
                        break;
                    default:
                        if (style < (int)m_lexerData.Styles.size())
                        {
                            const auto& sStyle = m_lexerData.Styles[style].Name;
                            if ((sStyle.find(L"DOC") == std::wstring::npos) &&
                                (sStyle.find(L"COMMENT") == std::wstring::npos) &&
                                (sStyle.find(L"STRING") == std::wstring::npos) &&
                                (sStyle.find(L"TEXT") == std::wstring::npos))
                                isText = false;
                        }
                        if (!isText)
                        {
                            // mark word as correct (remove the squiggle line)
                            ScintillaCall(SCI_SETINDICATORCURRENT, INDIC_MISSPELLED);
                            ScintillaCall(SCI_INDICATORCLEARRANGE, textrange.chrg.cpMin, textrange.chrg.cpMax - textrange.chrg.cpMin);
                            ScintillaCall(SCI_INDICATORCLEARRANGE, textrange.chrg.cpMin, textrange.chrg.cpMax - textrange.chrg.cpMin + 1);
                            continue;
                        }
                }
            }
            assert(textrange.chrg.cpMax >= textrange.chrg.cpMin);
            if (m_textbuflen < (textrange.chrg.cpMax - textrange.chrg.cpMin + 2))
            {
                m_textbuflen = textrange.chrg.cpMax - textrange.chrg.cpMin + 1024;
                m_textbuffer = std::make_unique<char[]>(m_textbuflen);
            }
            textrange.lpstrText = m_textbuffer.get();
            if (textrange.chrg.cpMax < textlength)
            {
                textrange.chrg.cpMax++;
                ScintillaCall(SCI_GETTEXTRANGE, 0, (sptr_t)&textrange);
            }
            else
            {
                ScintillaCall(SCI_GETTEXTRANGE, 0, (sptr_t)&textrange);
                textrange.chrg.cpMax++;
            }
            auto len = strlen(textrange.lpstrText);
            if (len == 0)
            {
                textrange.chrg.cpMax--;
                ScintillaCall(SCI_GETTEXTRANGE, 0, (sptr_t)&textrange);
                len = strlen(textrange.lpstrText);
                textrange.chrg.cpMax++;
                len++;
            }
            if (len)
                textrange.lpstrText[len - 1] = 0;
            textrange.chrg.cpMax--;
            if (textrange.lpstrText[0])
            {
                auto sWord = CUnicodeUtils::StdGetUnicode(textrange.lpstrText);
                // ignore words that contain numbers/digits
                if (std::any_of(sWord.begin(), sWord.end(), ::iswdigit))
                    continue;
                // ignore words that contain uppercase letters in the middle
                if (!CheckUppercase && (std::any_of(sWord.begin() + 1, sWord.end(), ::iswupper)))
                {
                    // mark word as correct (remove the squiggle line)
                    ScintillaCall(SCI_SETINDICATORCURRENT, INDIC_MISSPELLED);
                    ScintillaCall(SCI_INDICATORCLEARRANGE, textrange.chrg.cpMin, textrange.chrg.cpMax - textrange.chrg.cpMin);
                    ScintillaCall(SCI_INDICATORCLEARRANGE, textrange.chrg.cpMin, textrange.chrg.cpMax - textrange.chrg.cpMin + 1);
                    continue;
                }
                // ignore keywords of the currently selected lexer
                if (m_keywords.find(textrange.lpstrText) != m_keywords.end())
                    continue;

                IEnumSpellingErrorPtr enumSpellingError = nullptr;
                HRESULT               hr                = g_SpellChecker->Check(sWord.c_str(), &enumSpellingError);
                bool                  misspelled        = false;
                if (SUCCEEDED(hr))
                {
                    ISpellingErrorPtr spellingError = nullptr;
                    hr                              = enumSpellingError->Next(&spellingError);
                    if (hr == S_OK)
                    {
                        CORRECTIVE_ACTION action = CORRECTIVE_ACTION_NONE;
                        spellingError->get_CorrectiveAction(&action);
                        if (action != CORRECTIVE_ACTION_NONE)
                        {
                            // mark word as misspelled
                            misspelled = true;
                            ScintillaCall(SCI_SETINDICATORCURRENT, INDIC_MISSPELLED);
                            ScintillaCall(SCI_INDICATORFILLRANGE, textrange.chrg.cpMin, textrange.chrg.cpMax - textrange.chrg.cpMin);
                        }
                    }
                }

                if (!misspelled)
                {
                    // mark word as correct (remove the squiggle line)
                    ScintillaCall(SCI_SETINDICATORCURRENT, INDIC_MISSPELLED);
                    ScintillaCall(SCI_INDICATORCLEARRANGE, textrange.chrg.cpMin, textrange.chrg.cpMax - textrange.chrg.cpMin);
                    ScintillaCall(SCI_INDICATORCLEARRANGE, textrange.chrg.cpMin, textrange.chrg.cpMax - textrange.chrg.cpMin + 1);
                }
            }
        }
    }
}

void CCmdSpellcheck::OnTimer(UINT id)
{
    if (id == g_checktimer)
    {
        KillTimer(GetHwnd(), g_checktimer);
        Check();
    }
}

HRESULT CCmdSpellcheck::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, m_enabled, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

bool CCmdSpellcheck::Execute()
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
        m_lastcheckedpos = 0;
        Check();
    }
    else
    {
        ScintillaCall(SCI_SETINDICATORCURRENT, INDIC_MISSPELLED);
        ScintillaCall(SCI_INDICATORCLEARRANGE, 0, ScintillaCall(SCI_GETLENGTH));
    }
    g_contextID = m_enabled && g_SpellChecker ? cmdContextSpellMap : cmdContextMap;
    return true;
}

CCmdSpellcheckLang::CCmdSpellcheckLang(void* obj)
    : ICommand(obj)
{
}

HRESULT CCmdSpellcheckLang::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue)
{
    HRESULT hr = E_FAIL;

    if (key == UI_PKEY_Categories)
    {
        IUICollectionPtr pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
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
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
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
                CAppUtils::AddStringItem(pCollection, buf, catId, nullptr);
            }
            else
            {
                int catId = lang[0] - 'A';
                if (catId > 26)
                    catId = lang[0] - 'a';
                CAppUtils::AddStringItem(pCollection, lang.c_str(), catId, nullptr);
            }
        }
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
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
                hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)i, ppropvarNewValue);
                break;
            }
        }
    }
    return hr;
}

HRESULT CCmdSpellcheckLang::IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    HRESULT hr = E_FAIL;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if (key && *key == UI_PKEY_SelectedItem)
        {
            if (g_spellCheckerFactory)
            {
                UINT selected;
                hr = UIPropertyToUInt32(*key, *ppropvarValue, &selected);
                InvalidateUICommand(cmdFunctions, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
                std::wstring lang = g_languages[selected];

                BOOL supported = FALSE;
                hr             = g_spellCheckerFactory->IsSupported(lang.c_str(), &supported);
                if (supported)
                {
                    g_SpellChecker = nullptr;
                    hr             = g_spellCheckerFactory->CreateSpellChecker(lang.c_str(), &g_SpellChecker);
                    if (SUCCEEDED(hr))
                    {
                        CIniSettings::Instance().SetString(L"spellcheck", L"language", lang.c_str());
                    }
                }
            }
            hr = S_OK;
        }
    }
    return hr;
}

CCmdSpellcheckCorrect::CCmdSpellcheckCorrect(void* obj)
    : ICommand(obj)
{
}

HRESULT CCmdSpellcheckCorrect::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue)
{
    HRESULT hr = E_FAIL;

    if (key == UI_PKEY_Categories)
    {
        ResString sCorrect(hRes, IDS_SPELLCHECK_CORRECT);
        ResString sIgnore(hRes, IDS_SPELLCHECK_IGNORE);

        IUICollectionPtr pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
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
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;

        // The list will retain whatever from last time so clear it.
        pCollection->Clear();

        // populate the dropdown with the suggestions
        if (g_SpellChecker)
        {
            std::wstring sWord;

            auto wordcharsbuffer = GetWordChars();
            OnOutOfScope(ScintillaCall(SCI_SETWORDCHARS, 0, (sptr_t)wordcharsbuffer.c_str()));

            ScintillaCall(SCI_SETWORDCHARS, 0, (sptr_t)g_wordchars.c_str());
            sWord = CUnicodeUtils::StdGetUnicode(GetCurrentWord());

            IEnumStringPtr enumSpellingSuggestions = nullptr;
            hr                                     = g_SpellChecker->Suggest(sWord.c_str(), &enumSpellingSuggestions);
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
                        CAppUtils::AddStringItem(pCollection, suggestion, 0, nullptr);

                        CoTaskMemFree(suggestion);
                    }
                }
            }
        }
        if (m_suggestions.empty())
        {
            ResString sNoSuggestions(hRes, IDS_SPELLCHECK_NOSUGGESTIONS);
            CAppUtils::AddStringItem(pCollection, sNoSuggestions, 0, nullptr);
        }
        else
        {
            ResString sIgnoreSession(hRes, IDS_SPELLCHECK_IGNORESESSION);
            ResString sAddToDictionary(hRes, IDS_SPELLCHECK_ADDTODICTIONARY);

            CAppUtils::AddStringItem(pCollection, sIgnoreSession, 1, nullptr);
            CAppUtils::AddStringItem(pCollection, sAddToDictionary, 1, nullptr);
        }
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);

        hr = S_OK;
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        return UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)UI_COLLECTION_INVALIDINDEX, ppropvarNewValue);
    }
    return hr;
}

HRESULT CCmdSpellcheckCorrect::IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    HRESULT hr = E_FAIL;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if (key && *key == UI_PKEY_SelectedItem)
        {
            UINT selected;
            hr = UIPropertyToUInt32(*key, *ppropvarValue, &selected);
            if (SUCCEEDED(hr))
            {
                std::wstring sWord;

                ScintillaCall(SCI_SETCHARSDEFAULT);
                sWord = CUnicodeUtils::StdGetUnicode(GetCurrentWord());

                if (m_suggestions.empty())
                    ++selected; // adjust for the "no corrections found" entry
                if (selected < m_suggestions.size())
                {
                    auto currentPos = ScintillaCall(SCI_GETCURRENTPOS);
                    auto startPos   = ScintillaCall(SCI_WORDSTARTPOSITION, currentPos, true);
                    auto endPos     = ScintillaCall(SCI_WORDENDPOSITION, currentPos, true);
                    ScintillaCall(SCI_SETSELECTION, startPos, endPos);
                    ScintillaCall(SCI_REPLACESEL, 0, (sptr_t)CUnicodeUtils::StdGetUTF8(m_suggestions[selected]).c_str());
                }
                else
                {
                    if (g_SpellChecker)
                    {
                        if (selected == m_suggestions.size())
                        {
                            // ignore for this session
                            g_SpellChecker->Ignore(sWord.c_str());
                        }
                        if (selected == (m_suggestions.size() + 1))
                        {
                            // add to Dictionary
                            g_SpellChecker->Add(sWord.c_str());
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

void CCmdSpellcheckCorrect::ScintillaNotify(SCNotification* pScn)
{
    if (pScn->nmhdr.code == SCN_UPDATEUI)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
    }
}

CCmdSpellcheckAll::CCmdSpellcheckAll(void* obj)
    : ICommand(obj)
{
}

bool CCmdSpellcheckAll::Execute()
{
    bool bCheckall = CIniSettings::Instance().GetInt64(L"spellcheck", L"checkall", 1) != 0;
    bCheckall      = !bCheckall;
    CIniSettings::Instance().SetInt64(L"spellcheck", L"checkall", bCheckall);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    if (g_checktimer)
        SetTimer(GetHwnd(), g_checktimer, 1, nullptr);
    return true;
}

void CCmdSpellcheckAll::AfterInit()
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

HRESULT CCmdSpellcheckAll::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        bool bCheckall = CIniSettings::Instance().GetInt64(L"spellcheck", L"checkall", 1) != 0;
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, !bCheckall, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

CCmdSpellcheckUpper::CCmdSpellcheckUpper(void* obj)
    : ICommand(obj)
{
}

bool CCmdSpellcheckUpper::Execute()
{
    bool bCheckupper = CIniSettings::Instance().GetInt64(L"spellcheck", L"uppercase", 1) != 0;
    bCheckupper      = !bCheckupper;
    CIniSettings::Instance().SetInt64(L"spellcheck", L"uppercase", bCheckupper);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    if (g_checktimer)
        SetTimer(GetHwnd(), g_checktimer, 1, nullptr);
    return true;
}

void CCmdSpellcheckUpper::AfterInit()
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

HRESULT CCmdSpellcheckUpper::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        bool bCheckupper = CIniSettings::Instance().GetInt64(L"spellcheck", L"uppercase", 1) != 0;
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, bCheckupper, ppropvarNewValue);
    }
    return E_NOTIMPL;
}
