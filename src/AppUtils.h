// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2020 - Stefan Kueng
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

#pragma once
#include <string>

class CAppUtils
{
public:
    CAppUtils();
    ~CAppUtils();

    static std::wstring             GetDataPath(HMODULE hMod = nullptr);
    static std::wstring             GetSessionID();
    static bool                     CheckForUpdate(bool force);
    static bool                     DownloadUpdate(HWND hWnd, bool bInstall);
    static bool                     ShowUpdateAvailableDialog(HWND hWnd);
    static HRESULT CALLBACK         TDLinkClickCallback(HWND hwnd, UINT uNotification, WPARAM wParam, LPARAM lParam, LONG_PTR dwRefData);
    static bool                     HasSameMajorVersion(const std::wstring& path);
    static HRESULT                  AddResStringItem(IUICollectionPtr& collection, int resId, int cat = UI_COLLECTION_INVALIDINDEX, IUIImagePtr pImg = nullptr);
    static HRESULT                  AddStringItem(IUICollectionPtr& collection, LPCWSTR text, int cat = UI_COLLECTION_INVALIDINDEX, IUIImagePtr pImg = nullptr);
    static HRESULT                  AddCommandItem(IUICollectionPtr& collection, int cat, int commandId, UI_COMMANDTYPE commandType);
    static HRESULT                  AddCategory(IUICollectionPtr& coll, int catId, int catNameResId);
    static bool                     FailedShowMessage(HRESULT hr);
    static bool                     ShowDropDownList(HWND hWnd, LPCWSTR ctrlName);
    static HRESULT                  CreateImage(LPCWSTR resName, IUIImagePtr& pOutImg );
    static bool                     TryParse(const wchar_t* s, int& result, bool emptyOk = false, int def = 0, int base = 10);
    static bool                     TryParse(const wchar_t* s, unsigned long& result, bool emptyOk = false, unsigned long def = 0, int base = 10);
    static std::wstring             GetProgramFilesX86Folder();
    static const char*              GetResourceData(const wchar_t * resname, int id, DWORD& reslen);
private:
    static std::wstring             updatefilename;
    static std::wstring             updateurl;
};

