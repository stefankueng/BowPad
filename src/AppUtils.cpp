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
#include "AppUtils.h"


CAppUtils::CAppUtils(void)
{
}


CAppUtils::~CAppUtils(void)
{
}

std::wstring CAppUtils::GetDataPath(HMODULE hMod)
{
    DWORD len = 0;
    DWORD bufferlen = MAX_PATH;     // MAX_PATH is not the limit here!
    std::unique_ptr<wchar_t[]> path(new wchar_t[bufferlen]);
    do
    {
        bufferlen += MAX_PATH;      // MAX_PATH is not the limit here!
        path = std::unique_ptr<wchar_t[]>(new wchar_t[bufferlen]);
        len = GetModuleFileName(hMod, path.get(), bufferlen);
    } while(len == bufferlen);
    std::wstring sPath = path.get();
    sPath = sPath.substr(0, sPath.find_last_of('\\'));
    return sPath;
}
