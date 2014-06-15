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
#include "MRU.h"
#include "AppUtils.h"

#include <iostream>
#include <fstream>
#include "codecvt.h"
#include <algorithm>
#include <cctype>
#include <memory>
#include <functional>
#include <strsafe.h>
#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>

const int PinnedMask = 0x8000;

// Implement the properties that describe a Recent Item to the Windows Ribbon
class CRecentFileProperties
    : public IUISimplePropertySet
{
public:

    // Static method to create an instance of the object.
    static HRESULT CreateInstance(LPCWSTR wszFullPath, bool bPinned, CRecentFileProperties **ppProperties)
    {
        if (!wszFullPath || !ppProperties)
            return E_POINTER;

        *ppProperties = NULL;

        HRESULT hr;

        CRecentFileProperties* pProperties = new CRecentFileProperties();

        if (pProperties != NULL)
        {
            hr = ::StringCchCopyW(pProperties->m_wszFullPath, MAX_PATH, wszFullPath);
            SHFILEINFOW sfi = {0};

            DWORD_PTR dwPtr = NULL;
            if (SUCCEEDED(hr))
                dwPtr = ::SHGetFileInfoW(wszFullPath, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_DISPLAYNAME | SHGFI_USEFILEATTRIBUTES);

            if (dwPtr != NULL)
                hr = ::StringCchCopyW(pProperties->m_wszDisplayName, MAX_PATH, sfi.szDisplayName);
            else // Provide a reasonable fallback.
                hr = ::StringCchCopyW(pProperties->m_wszDisplayName, MAX_PATH, pProperties->m_wszFullPath);
            pProperties->m_pinnedState = bPinned;
        }
        else
            hr = E_OUTOFMEMORY;

        if (SUCCEEDED(hr))
        {
            *ppProperties = pProperties;
            (*ppProperties)->AddRef();
        }

        if (pProperties)
            pProperties->Release();

        return hr;
    }

    // IUnknown methods.
    STDMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&m_cRef);
    }

    STDMETHODIMP_(ULONG) Release()
    {
        LONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0)
        {
            delete this;
        }

        return cRef;
    }

    STDMETHODIMP QueryInterface(REFIID iid, void** ppv)
    {
        if (!ppv)
            return E_POINTER;

        if (iid == __uuidof(IUnknown))
            *ppv = static_cast<IUnknown*>(this);
        else if (iid == __uuidof(IUISimplePropertySet))
            *ppv = static_cast<IUISimplePropertySet*>(this);
        else
        {
            *ppv = NULL;
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }

    // IUISimplePropertySet methods.
    STDMETHODIMP GetValue(REFPROPERTYKEY key, PROPVARIANT *value)
    {
        HRESULT hr = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

        if (key == UI_PKEY_Label)
            hr = UIInitPropertyFromString(UI_PKEY_Label, m_wszDisplayName, value);
        else if (key == UI_PKEY_LabelDescription)
            hr = UIInitPropertyFromString(UI_PKEY_LabelDescription, m_wszFullPath, value);
        else if (key == UI_PKEY_Pinned)
            hr = UIInitPropertyFromBoolean(UI_PKEY_Pinned, m_pinnedState, value);
        return hr;
    }

private:
    CRecentFileProperties()
        : m_cRef(1)
        , m_pinnedState(false)
    {
        m_wszFullPath[0] = L'\0';
        m_wszDisplayName[0] = L'\0';
    }

    LONG    m_cRef;                        // Reference count.
    WCHAR   m_wszDisplayName[MAX_PATH];
    WCHAR   m_wszFullPath[MAX_PATH];
    bool    m_pinnedState;
};


CMRU::CMRU(void)
    : m_bLoaded(false)
{
}

CMRU::~CMRU(void)
{
}

CMRU& CMRU::Instance()
{
    static CMRU instance;
    return instance;
}

HRESULT CMRU::PopulateRibbonRecentItems( PROPVARIANT* pvarValue )
{
    if (!m_bLoaded)
        Load();

    HRESULT hr = E_FAIL;
    SAFEARRAY* psa = SafeArrayCreateVector(VT_UNKNOWN, 0, (ULONG)m_mruVec.size());
    LONG i = 0;
    for (auto countPathPair = m_mruVec.crbegin(); countPathPair != m_mruVec.crend(); ++countPathPair)
    {
        CRecentFileProperties* pPropertiesObj = nullptr;
        hr = CRecentFileProperties::CreateInstance(std::get<0>(*countPathPair).c_str(), std::get<1>(*countPathPair), &pPropertiesObj);

        if (SUCCEEDED(hr))
        {
            IUnknown* pUnk = NULL;

            hr = pPropertiesObj->QueryInterface(__uuidof(IUnknown), reinterpret_cast<void**>(&pUnk));

            if (SUCCEEDED(hr))
            {
                hr = SafeArrayPutElement(psa, &i, static_cast<void*>(pUnk));
                pUnk->Release();
            }
        }

        if (pPropertiesObj)
            pPropertiesObj->Release();

        if (FAILED(hr))
            break;

        i ++;
    }
    // We will only populate items up to before the first failed item, and discard the rest.
    SAFEARRAYBOUND sab = {i,0};
    SafeArrayRedim(psa, &sab);
    hr = UIInitPropertyFromIUnknownArray(UI_PKEY_RecentItems, psa, pvarValue);

    SafeArrayDestroy(psa);
    return hr;
}

void CMRU::AddPath( const std::wstring& path )
{
    if (!m_bLoaded)
        Load();

    bool pinned = false;
    std::wstring lpath = path;
    std::transform(lpath.begin(), lpath.end(), lpath.begin(), ::tolower);

    for (auto it = m_mruVec.begin(); it != m_mruVec.end(); ++it)
    {
        std::wstring lp = std::get<0>(*it);
        std::transform(lp.begin(), lp.end(), lp.begin(), ::tolower);
        if (lpath.compare(lp) == 0)
        {
            pinned = std::get<1>(*it);
            m_mruVec.erase(it);
            break;
        }
    }
    if (m_mruVec.size() >= 20)
    {
        // clear out old entries if they're not pinned
        for (auto it = m_mruVec.begin(); it != m_mruVec.end(); ++it)
        {
            if (std::get<1>(*it))
                continue;
            m_mruVec.erase(it);
            break;
        }
    }

    m_mruVec.push_back(std::make_tuple(path, pinned));
    Save();
}

void CMRU::RemovePath( const std::wstring& path, bool removeEvenIfPinned )
{
    if (!m_bLoaded)
        Load();

    std::wstring lpath = path;
    std::transform(lpath.begin(), lpath.end(), lpath.begin(), ::tolower);
    for (auto it = m_mruVec.begin(); it != m_mruVec.end(); ++it)
    {
        std::wstring lp = std::get<0>(*it);
        std::transform(lp.begin(), lp.end(), lp.begin(), ::tolower);
        if (lpath.compare(lp) == 0)
        {
            if (removeEvenIfPinned || !std::get<1>(*it))
                m_mruVec.erase(it);
            break;
        }
    }
    Save();
}

void CMRU::Load()
{
    m_bLoaded = true;
    std::wstring path = CAppUtils::GetDataPath() + L"\\mru";

    std::wifstream File;
    File.imbue(std::locale(std::locale(), new utf8_conversion()));
    try
    {
        File.open(path);
    }
    catch (std::ios_base::failure e)
    {
        return;
    }
    if (!File.good())
        return;

    const int maxlinelength = 65535;

    std::unique_ptr<wchar_t[]> line(new wchar_t[maxlinelength]);
    do
    {
        File.getline(line.get(), maxlinelength);

        std::wstring sLine = line.get();
        size_t pos = sLine.find('*');
        if (pos != std::wstring::npos)
        {
            std::wstring sPinned = sLine.substr(0, pos);
            std::wstring sPath  = sLine.substr(pos+1);
            m_mruVec.push_back(std::make_tuple(sPath, sPinned.compare(L"1")==0));
        }
    } while (File.gcount() > 0);
    File.close();

    m_bLoaded = true;
}

void CMRU::Save()
{
    std::wstring path = CAppUtils::GetDataPath() + L"\\mru";

    std::wofstream File;
    File.imbue(std::locale(std::locale(), new utf8_conversion()));
    File.open(path);
    if (!File.good())
        return;

    for (auto t : m_mruVec)
    {
        File << (std::get<1>(t) ? L"1" : L"0") << L"*" << std::get<0>(t) << std::endl;
    }
    File.close();
}

void CMRU::PinPath( const std::wstring& path, bool bPin )
{
    if (!m_bLoaded)
        Load();
    bool bPinned = false;
    size_t i = 0;
    for (auto it : m_mruVec)
    {
        if (std::get<0>(it).compare(path)==0)
        {
            bPinned = std::get<1>(it);
            m_mruVec[i] = std::make_tuple(path, bPin);
            if (bPinned != bPin)
                Save();
            break;
        }
        ++i;
    }
}
