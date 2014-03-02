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
#include "CmdLanguage.h"
#include "PropertySet.h"
#include "BowPad.h"
#include "StringUtils.h"
#include "ResString.h"
#include "DirFileEnum.h"
#include "AppUtils.h"
#include "PathUtils.h"
#include "version.h"

#include <vector>
#include <fstream>

std::vector<std::wstring> gLanguages;
std::vector<std::wstring> gRemotes;


HRESULT CCmdLanguage::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue )
{
    HRESULT hr = E_FAIL;

    if(key == UI_PKEY_Categories)
    {
        IUICollection* pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (FAILED(hr))
        {
            return hr;
        }

        // Create a property set for the main category.
        CPropertySet *pMain;
        hr = CPropertySet::CreateInstance(&pMain);
        if (FAILED(hr))
        {
            pCollection->Release();
            return hr;
        }

        // Load the label for the main category from the resource file.
        ResString sMain(hRes, IDS_LANGUAGE_AVAILABLE);
        // Initialize the property set with the label that was just loaded and a category id of 0.
        pMain->InitializeCategoryProperties(sMain, 0);
        pCollection->Add(pMain);
        pMain->Release();


        // Create a property set for the remote category.
        CPropertySet *pRemotes;
        hr = CPropertySet::CreateInstance(&pRemotes);
        if (FAILED(hr))
        {
            pCollection->Release();
            return hr;
        }

        ResString sRem(hRes, IDS_LANGUAGE_REMOTE);
        // Initialize the property set with the label that was just loaded and a category id of 1.
        pRemotes->InitializeCategoryProperties(sRem, 1);
        pCollection->Add(pRemotes);
        pRemotes->Release();

        pCollection->Release();

        hr = S_OK;
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        IUICollection* pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (FAILED(hr))
        {
            return hr;
        }
        pCollection->Clear();
        // Create an IUIImage from a resource id.
        IUIImage * pImg = nullptr;
        IUIImageFromBitmap * pifbFactory = nullptr;
        hr = CoCreateInstance(CLSID_UIRibbonImageFromBitmapFactory, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pifbFactory));
        if (FAILED(hr))
        {
            return hr;
        }

        // Load the bitmap from the resource file.
        HBITMAP hbm = (HBITMAP) LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_EMPTY), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
        if (hbm)
        {
            // Use the factory implemented by the framework to produce an IUIImage.
            hr = pifbFactory->CreateImage(hbm, UI_OWNERSHIP_TRANSFER, &pImg);
            if (FAILED(hr))
            {
                DeleteObject(hbm);
            }
        }
        pifbFactory->Release();

        if (FAILED(hr))
        {
            pCollection->Release();
            return hr;
        }

        gLanguages.clear();

        // English as the first language, always installed
        CPropertySet* pItem;
        hr = CPropertySet::CreateInstance(&pItem);
        if (FAILED(hr))
        {
            pCollection->Release();
            return hr;
        }
        pItem->InitializeItemProperties(pImg, L"English", 0);
        pCollection->Add(pItem);
        gLanguages.push_back(L"");
        pItem->Release();

        std::wstring path = CAppUtils::GetDataPath();
        CDirFileEnum enumerator(path);
        std::wstring respath;
        bool bIsDir = false;
        while (enumerator.NextFile(respath, &bIsDir, false))
        {
            if (_wcsicmp(CPathUtils::GetFileExtension(respath).c_str(), L"lang")==0)
            {
                if (CAppUtils::HasSameMajorVersion(respath))
                {
                    std::wstring sLocale = respath.substr(respath.find_last_of('_')+1);
                    sLocale = sLocale.substr(0, sLocale.find_last_of('.'));
                    int len = GetLocaleInfoEx(sLocale.c_str(), LOCALE_SLOCALIZEDLANGUAGENAME, 0, 0);
                    if (len)
                    {
                        std::unique_ptr<wchar_t[]> langbuf(new wchar_t[len]);
                        if (GetLocaleInfoEx(sLocale.c_str(), LOCALE_SLOCALIZEDLANGUAGENAME, langbuf.get(), len))
                        {
                            CPropertySet* pItem;
                            hr = CPropertySet::CreateInstance(&pItem);
                            if (FAILED(hr))
                            {
                                pCollection->Release();
                                return hr;
                            }
                            pItem->InitializeItemProperties(pImg, langbuf.get(), 0);
                            pCollection->Add(pItem);
                            gLanguages.push_back(sLocale);
                            pItem->Release();
                        }
                    }
                }
            }
        }

        if (gRemotes.empty())
        {
            // Dummy entry for fetching the remote language list
            CPropertySet* pItem;
            hr = CPropertySet::CreateInstance(&pItem);
            if (FAILED(hr))
            {
                pCollection->Release();
                return hr;
            }
            ResString sFetch(hRes, IDS_LANGUAGE_FETCH);
            pItem->InitializeItemProperties(pImg, sFetch, 1);
            pCollection->Add(pItem);
            gLanguages.push_back(L"*");
            pItem->Release();
        }
        else
        {
            for (auto it : gRemotes)
            {
                std::wstring sLocale = it;
                int len = GetLocaleInfoEx(sLocale.c_str(), LOCALE_SLOCALIZEDLANGUAGENAME, 0, 0);
                if (len)
                {
                    std::unique_ptr<wchar_t[]> langbuf(new wchar_t[len]);
                    if (GetLocaleInfoEx(sLocale.c_str(), LOCALE_SLOCALIZEDLANGUAGENAME, langbuf.get(), len))
                    {
                        CPropertySet* pItem;
                        hr = CPropertySet::CreateInstance(&pItem);
                        if (FAILED(hr))
                        {
                            pCollection->Release();
                            return hr;
                        }
                        pItem->InitializeItemProperties(pImg, langbuf.get(), 0);
                        pCollection->Add(pItem);
                        gLanguages.push_back(sLocale);
                        pItem->Release();
                    }
                }
            }
        }

        pImg->Release();
        pCollection->Release();
        hr = S_OK;
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
                if (_wcsicmp(gLanguages[i].c_str(), lang.c_str())==0)
                {
                    hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)i, ppropvarNewValue);
                    break;
                }
            }
        }
        hr = S_OK;
    }
    return hr;
}

HRESULT CCmdLanguage::IUICommandHandlerExecute( UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/ )
{
    HRESULT hr = E_FAIL;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if ( key && *key == UI_PKEY_SelectedItem)
        {
            UINT selected;
            hr = UIPropertyToUInt32(*key, *ppropvarValue, &selected);
            if (gLanguages[selected] == L"*")
            {
                gRemotes.clear();
                // fetch list of remotely available languages
                std::wstring tempfile = CPathUtils::GetTempFilePath();

                std::wstring sLangURL = CStringUtils::Format(L"https://bowpad.googlecode.com/svn/branches/%d.%d.%d/Languages/Languages.txt", BP_VERMAJOR, BP_VERMINOR, BP_VERMICRO);
                HRESULT res = URLDownloadToFile(NULL, sLangURL.c_str(), tempfile.c_str(), 0, NULL);
                if (res == S_OK)
                {
                    std::wifstream fin(tempfile);

                    if (fin.is_open())
                    {
                        std::wstring line;
                        while (std::getline(fin, line))
                        {
                            if (!line.empty())
                                gRemotes.push_back(line);
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
#ifdef _WIN64
#define LANGPLAT L"x64"
#else
#define LANGPLAT L"x86"
#endif
                    std::wstring sLangURL = CStringUtils::Format(L"https://bowpad.googlecode.com/svn/branches/%d.%d.%d/Languages/%s/BowPad_%s.lang", BP_VERMAJOR, BP_VERMINOR, BP_VERMICRO, LANGPLAT, gLanguages[selected].c_str());
                    HRESULT res = URLDownloadToFile(NULL, sLangURL.c_str(), langfile.c_str(), 0, NULL);
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

