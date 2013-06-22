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

#pragma once
#include <string>

class CAppUtils
{
public:
    CAppUtils(void);
    ~CAppUtils(void);

    static std::wstring             GetDataPath(HMODULE hMod = NULL);
    static std::wstring             GetSessionID();
    static bool                     CheckForUpdate(bool force);
    static bool                     DownloadUpdate(HWND hWnd, bool bInstall);
    static bool                     ShowUpdateAvailableDialog(HWND hWnd);
    static HRESULT CALLBACK         TDLinkClickCallback(HWND hwnd, UINT uNotification, WPARAM wParam, LPARAM lParam, LONG_PTR dwRefData);

private:
    static std::wstring             updatefilename;
    static std::wstring             updateurl;
};

