// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2021-2022 - Stefan Kueng
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
#include "PathUtils.h"
#include "UnicodeUtils.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <memory>
#include <strsafe.h>
#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>


// Implement the properties that describe a Recent Item to the Windows Ribbon
class CRecentFileProperties
    : public IUISimplePropertySet
{
public:
    // Static method to create an instance of the object.
    static HRESULT CreateInstance(LPCWSTR wszFullPath, bool bPinned, CRecentFileProperties** ppProperties)
    {
        if (!wszFullPath || !ppProperties)
            return E_POINTER;

        *ppProperties = nullptr;

        CRecentFileProperties* pProperties = new CRecentFileProperties();

        HRESULT     hr  = ::StringCchCopyW(pProperties->m_wszFullPath, MAX_PATH, wszFullPath);
        SHFILEINFOW sfi = {nullptr};

        DWORD_PTR dwPtr = 0;
        if (SUCCEEDED(hr))
            dwPtr = ::SHGetFileInfoW(wszFullPath, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_DISPLAYNAME | SHGFI_USEFILEATTRIBUTES);

        if (dwPtr != 0)
            hr = ::StringCchCopyW(pProperties->m_wszDisplayName, MAX_PATH, sfi.szDisplayName);
        else // Provide a reasonable fallback.
            hr = ::StringCchCopyW(pProperties->m_wszDisplayName, MAX_PATH, pProperties->m_wszFullPath);
        pProperties->m_pinnedState = bPinned;

        *ppProperties = pProperties;
        (*ppProperties)->AddRef();

        pProperties->Release();

        return hr;
    }

    // IUnknown methods.
    STDMETHODIMP_(ULONG)
    AddRef() override
    {
        return InterlockedIncrement(&m_cRef);
    }

    STDMETHODIMP_(ULONG)
    Release() override
    {
        LONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0)
        {
            delete this;
        }

        return cRef;
    }

    STDMETHODIMP QueryInterface(REFIID iid, void** ppv) override
    {
        if (!ppv)
            return E_POINTER;

        if (iid == __uuidof(IUnknown))
            *ppv = static_cast<IUnknown*>(this);
        else if (iid == __uuidof(IUISimplePropertySet))
            *ppv = static_cast<IUISimplePropertySet*>(this);
        else
        {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }

    // IUISimplePropertySet methods.
    STDMETHODIMP GetValue(REFPROPERTYKEY key, PROPVARIANT* value) override
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
        m_wszFullPath[0]    = L'\0';
        m_wszDisplayName[0] = L'\0';
    }
    virtual ~CRecentFileProperties() = default;

    LONG  m_cRef; // Reference count.
    WCHAR m_wszDisplayName[MAX_PATH];
    WCHAR m_wszFullPath[MAX_PATH];
    bool  m_pinnedState;
};

CMRU::CMRU()
    : m_bLoaded(false)
{
}

CMRU::~CMRU()
{
}

CMRU& CMRU::Instance()
{
    static CMRU instance;
    return instance;
}

HRESULT CMRU::PopulateRibbonRecentItems(PROPVARIANT* pvarValue)
{
    if (!m_bLoaded)
        Load();

    // split the vector into two: one with the pinned items and one with the unpinned items
    std::vector<MRUItem> pinneditems;
    std::vector<MRUItem> unpinneditems;
    for (const auto& item : m_mruVec)
    {
        if (item.pinned)
            pinneditems.push_back(item);
        else
            unpinneditems.push_back(item);
    }

    HRESULT    hr  = E_FAIL;
    SAFEARRAY* psa = SafeArrayCreateVector(VT_UNKNOWN, 0, static_cast<ULONG>(m_mruVec.size()));
    LONG       i   = 0;

    auto fillItems = [](const std::vector<MRUItem>& items, SAFEARRAY* psa, LONG& i) {
        for (auto countPathPair = items.crbegin(); countPathPair != items.crend(); ++countPathPair)
        {
            if (i >= CIniSettings::Instance().GetInt64(L"Defaults", L"MRUSize", 20))
                break;
            const MRUItem& mru = *countPathPair;

            CRecentFileProperties* pPropertiesObj = nullptr;
            auto                   hr             = CRecentFileProperties::CreateInstance(mru.path.c_str(), mru.pinned, &pPropertiesObj);
            if (SUCCEEDED(hr))
            {
                IUnknown* pUnk = nullptr;

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

            i++;
        }
    };

    fillItems(pinneditems, psa, i);
    fillItems(unpinneditems, psa, i);

    // We will only populate items up to before the first failed item, and discard the rest.
    SAFEARRAYBOUND sab = {static_cast<ULONG>(i), 0};
    SafeArrayRedim(psa, &sab);
    hr = UIInitPropertyFromIUnknownArray(UI_PKEY_RecentItems, psa, pvarValue);

    SafeArrayDestroy(psa);
    return hr;
}

void CMRU::AddPath(const std::wstring& path)
{
    if (!m_bLoaded)
        Load();

    bool pinned = false;

    // Erase the first MRU item that might already exist
    // with the same path (case insensitive)
    // Remember if it was pinned if it was found.
    for (auto it = m_mruVec.begin(); it != m_mruVec.end(); ++it)
    {
        if (CPathUtils::PathCompare(path, it->path) == 0)
        {
            pinned = it->pinned;
            m_mruVec.erase(it);
            break;
        }
    }
    if (m_mruVec.size() >= static_cast<size_t>(CIniSettings::Instance().GetInt64(L"Defaults", L"MRUSize", 20)))
    {
        // Clear out an old entry if it's not pinned
        for (auto it = m_mruVec.begin(); it != m_mruVec.end(); ++it)
        {
            if (!it->pinned)
            {
                m_mruVec.erase(it);
                break;
            }
        }
    }

    m_mruVec.push_back(MRUItem(path, pinned));
    Save();
}

void CMRU::RemovePath(const std::wstring& path, bool removeEvenIfPinned)
{
    if (!m_bLoaded)
        Load();

    for (auto it = m_mruVec.begin(); it != m_mruVec.end(); ++it)
    {
        if (CPathUtils::PathCompare(path, it->path) == 0)
        {
            if (removeEvenIfPinned || !it->pinned)
                m_mruVec.erase(it);
            break;
        }
    }
    Save();
}

std::wstring CMRU::GetMRUFilename()
{
    std::wstring path = CAppUtils::GetDataPath();
    path              = CPathUtils::Append(path, L"mru");
    return path;
}

void CMRU::Load()
{
    m_bLoaded = true;

    std::wstring path = GetMRUFilename();

    std::ifstream file;
    try
    {
        file.open(path);
        if (!file.good())
            return;

        constexpr int maxLineLength = 1024;
        char   line[maxLineLength + 1]{};

        for (;;)
        {
            file.getline(line, maxLineLength);
            if (file.gcount() <= 0)
                break;
            std::wstring sLine = CUnicodeUtils::StdGetUnicode(line);
            // Line format is : x*filename
            // Where x can be '0' (unpinned) or '1' (pinned)
            size_t pos = sLine.find(L'*');
            if (pos != std::wstring::npos)
            {
                std::wstring sPinned = sLine.substr(0, pos);
                std::wstring sPath   = sLine.substr(pos + 1);
                bool         pinned  = (sPinned == L"1");
                m_mruVec.push_back(MRUItem(sPath, pinned));
            }
        }
        file.close();
    }
    catch (const std::ios_base::failure&)
    {
        return;
    }
    catch (const std::exception&)
    {
        return;
    }

    m_bLoaded = true;
}

void CMRU::Save() const
{
    std::wstring path = GetMRUFilename();

    try
    {
        std::ofstream file;
        file.open(path);
        if (!file.good())
            return;

        for (const auto& mru : m_mruVec)
            file << (mru.pinned ? "1" : "0") << "*" << CUnicodeUtils::StdGetUTF8(mru.path) << std::endl;

        file.close();
    }
    catch (std::ios_base::failure&)
    {
        return;
    }
    catch (std::runtime_error&)
    {
        return;
    }
}

void CMRU::PinPath(const std::wstring& path, bool bPin)
{
    if (!m_bLoaded)
        Load();

    for (auto& mru : m_mruVec) // Intentionally mutable.
    {
        if (CPathUtils::PathCompare(mru.path, path) == 0)
        {
            // Technically paths are case insensitive on the file system
            // so updating the path might is kind of updating it to itself but
            // do it anyway so it reflects the actual case used even
            // if it refers to the same file.
            bool wasPinned = mru.pinned;
            mru            = MRUItem(path, bPin); // Container update.
            if (wasPinned != bPin)
                Save();
            break; // Assume there is only one path with the same name here.
        }
    }
}
