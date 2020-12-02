// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2018, 2020 - Stefan Kueng
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
#include "CmdLanguage.h"
#include "PropertySet.h"
#include "BowPad.h"
#include "StringUtils.h"
#include "ResString.h"
#include "DirFileEnum.h"
#include "AppUtils.h"
#include "PathUtils.h"
#include "TempFile.h"
#include "version.h"

#include <fstream>

namespace
{
// Global but not externally accessible.
std::vector<std::wstring> gLanguages;
std::vector<std::wstring> gRemotes;

const int CAT_LANGS_AVAILABLE = 0;
const int CAT_LANGS_REMOTE    = 1;
}; // namespace

HRESULT CCmdLanguage::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue)
{
    HRESULT hr = E_FAIL;

    if (key == UI_PKEY_Categories)
    {
        IUICollectionPtr pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;

        hr = CAppUtils::AddCategory(pCollection, CAT_LANGS_AVAILABLE, IDS_LANGUAGE_AVAILABLE);
        if (FAILED(hr))
            return hr;
        hr = CAppUtils::AddCategory(pCollection, CAT_LANGS_REMOTE, IDS_LANGUAGE_REMOTE);
        if (FAILED(hr))
            return hr;
        return S_OK;
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        IUICollectionPtr pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;
        pCollection->Clear();

        gLanguages.clear();

        // English as the first language, always installed
        CAppUtils::AddStringItem(pCollection, L"English", CAT_LANGS_AVAILABLE, nullptr);
        gLanguages.push_back(L"");

        std::wstring path = CAppUtils::GetDataPath();
        CDirFileEnum enumerator(path);
        std::wstring respath;
        bool         bIsDir = false;
        while (enumerator.NextFile(respath, &bIsDir, false))
        {
            if (bIsDir)   // Only interested in files.
                continue; // Continue on to the next, save indentation.

            // Only interested in files that match BowPad*_*.lang, to extract the
            // DE part as a locale name. If no "_" is present, the file is either
            // incorrectly named or is an  unrelated file that just happens to
            // have a .lang extension. Either way, if it happens we can't process it.
            if (CPathUtils::PathCompare(CPathUtils::GetFileExtension(respath), L"lang") != 0)
                continue; // Name of no interest.

            // Make sure the file matches our naming convention of BowPad*_Local.
            // If not, alert attention to any problems if in testing or diagnostic
            // mode but otherwise we'll just ignore the files.
            std::wstring filename = CPathUtils::GetFileNameWithoutExtension(respath);
            auto         pos      = filename.find_last_of(L'_');
            if (pos == std::wstring::npos) // No "_"
            {
                APPVERIFY(false);
                continue;
            }
            // Extract the locale name, note the extension is already removed.
            std::wstring sLocale = filename.substr(pos + 1);
            if (sLocale.empty()) // Just BowPad_, no language name.
            {
                APPVERIFY(false);
                continue;
            }
            // Now that we're sure the file obeys all the name conventions to
            // be expected to be a file we want, check the version.
            if (!CAppUtils::HasSameMajorVersion(respath))
                continue; // Assume old/unversioned, so ignore it.

            int len = GetLocaleInfoEx(sLocale.c_str(), LOCALE_SLOCALIZEDLANGUAGENAME, 0, 0);
            if (len <= 0) // Assume unrecognized language.
                continue;
            auto langbuf = std::make_unique<wchar_t[]>(len);
            if (GetLocaleInfoEx(sLocale.c_str(), LOCALE_SLOCALIZEDLANGUAGENAME, langbuf.get(), len))
            {
                CAppUtils::AddStringItem(pCollection, langbuf.get(), CAT_LANGS_AVAILABLE, nullptr);
                gLanguages.push_back(sLocale);
            }
        }

        if (gRemotes.empty())
        {
            // Dummy entry for fetching the remote language list
            CAppUtils::AddResStringItem(pCollection, IDS_LANGUAGE_FETCH, CAT_LANGS_REMOTE, nullptr);
            gLanguages.push_back(L"*");
        }
        else
        {
            for (const auto& sLocale : gRemotes)
            {
                int len = GetLocaleInfoEx(sLocale.c_str(), LOCALE_SLOCALIZEDLANGUAGENAME, 0, 0);
                // Can't process this, continue to the next, save indentation.
                if (len <= 0)
                    continue;
                auto langbuf = std::make_unique<wchar_t[]>(len);
                if (GetLocaleInfoEx(sLocale.c_str(), LOCALE_SLOCALIZEDLANGUAGENAME, langbuf.get(), len))
                {
                    CAppUtils::AddStringItem(pCollection, langbuf.get(), CAT_LANGS_AVAILABLE, nullptr);
                    gLanguages.push_back(sLocale);
                }
            }
        }

        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);

        return S_OK;
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        std::wstring lang = CIniSettings::Instance().GetString(L"UI", L"language", L"");
        if (lang.empty())
            hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)0, ppropvarNewValue);
        else
        {
            for (size_t i = 0; i < gLanguages.size(); ++i)
            {
                if (_wcsicmp(gLanguages[i].c_str(), lang.c_str()) == 0)
                {
                    hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)i, ppropvarNewValue);
                    break;
                }
            }
        }
        return S_OK;
    }
    else // Event not handled.
    {
        return E_NOTIMPL;
    }
    assert(false); // Fallthrough not expected.
    return E_NOTIMPL;
}

HRESULT CCmdLanguage::IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    HRESULT hr = E_FAIL;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if (key && *key == UI_PKEY_SelectedItem)
        {
            UINT selected;
            hr = UIPropertyToUInt32(*key, *ppropvarValue, &selected);
            if (gLanguages[selected] == L"*")
            {
                gRemotes.clear();
                // fetch list of remotely available languages
                std::wstring tempfile = CTempFiles::Instance().GetTempFilePath(true);

                std::wstring sLangURL = CStringUtils::Format(L"https://github.com/stefankueng/BowPad/raw/%d.%d.%d/Languages/Languages.txt", BP_VERMAJOR, BP_VERMINOR, BP_VERMICRO);
                HRESULT      res      = URLDownloadToFile(nullptr, sLangURL.c_str(), tempfile.c_str(), 0, nullptr);
                if (res == S_OK)
                {
                    std::wifstream fin(tempfile);

                    if (fin.is_open())
                    {
                        std::wstring line;
                        while (std::getline(fin, line))
                        {
                            if (!line.empty())
                                gRemotes.push_back(std::move(line));
                        }
                    }
                    ResString sLangLoadOk(hRes, IDS_LANGUAGE_DOWNLOADOK);
                    MessageBox(GetHwnd(), sLangLoadOk, L"BowPad", MB_ICONINFORMATION);
                    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
                }
                else
                {
                    ResString sLangLoadFailed(hRes, IDS_LANGUAGE_DOWNLOADFAILED);
                    MessageBox(GetHwnd(), sLangLoadFailed, L"BowPad", MB_ICONERROR);
                }
            }
            else
            {
                std::wstring langfile = CAppUtils::GetDataPath();
                langfile += L"\\BowPad_";
                langfile += gLanguages[selected];
                langfile += L".lang";
                if (!gLanguages[selected].empty() && !CAppUtils::HasSameMajorVersion(langfile))
                {
                    DeleteFile(langfile.c_str());
                    std::wstring sLangURL = CStringUtils::Format(L"https://github.com/stefankueng/BowPad/raw/%d.%d.%d/Languages/%s/BowPad_%s.lang", BP_VERMAJOR, BP_VERMINOR, BP_VERMICRO, LANGPLAT, gLanguages[selected].c_str());
                    HRESULT      res      = URLDownloadToFile(nullptr, sLangURL.c_str(), langfile.c_str(), 0, nullptr);
                    if (FAILED(res))
                    {
                        ResString sLangLoadFailed(hRes, IDS_LANGUAGE_DOWNLOADFAILEDFILE);
                        MessageBox(GetHwnd(), sLangLoadFailed, L"BowPad", MB_ICONERROR);
                        return res;
                    }
                }
                CIniSettings::Instance().SetString(L"UI", L"language", gLanguages[selected].c_str());
                ResString sLangRestart(hRes, IDS_LANGUAGE_RESTART);
                MessageBox(GetHwnd(), sLangRestart, L"BowPad", MB_ICONINFORMATION);
            }
            hr = S_OK;
        }
    }
    return hr;
}
