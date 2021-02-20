// This file is part of BowPad.
//
// Copyright (C) 2013 - 2014, 2016, 2021 Stefan Kueng
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
#include <vector>

struct MRUItem
{
    MRUItem(const std::wstring& path, bool pinned)
        : path(path), pinned(pinned) { }
    std::wstring path;
    bool pinned;
};

class CMRU
{
private:
    CMRU();
    ~CMRU();

public:
    static CMRU& Instance();

    HRESULT                         PopulateRibbonRecentItems(PROPVARIANT* pvarValue);
    void                            AddPath(const std::wstring& path);
    void                            RemovePath(const std::wstring& path, bool removeEvenIfPinned);
    void                            PinPath(const std::wstring& path, bool bPin);

private:
    static std::wstring GetMRUFilename();
    void                Load();
    void                Save();

private:
    bool                            m_bLoaded;
    std::vector<MRUItem>            m_mruVec;
};
