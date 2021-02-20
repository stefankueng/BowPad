// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2020-2021 - Stefan Kueng
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
#include <unordered_map>
#include <vector>
#include <unordered_set>

class KshAccel
{
public:
    KshAccel()
        : fVirt(0)
        , fVirt1(false)
        , key1(0)
        , fVirt2(false)
        , key2(0)
        , cmd(0)
    {
    }
    ~KshAccel() {}

    BYTE         fVirt;  // 0x10 = ALT, 0x08 = Ctrl, 0x04 = Shift
    bool         fVirt1; // true if key1 is a virtual key, false if it's a character code
    WORD         key1;
    bool         fVirt2;
    WORD         key2;
    WORD         cmd;  // the command
    std::wstring sCmd; // the command as a string
};

class CKeyboardShortcutHandler
{
public:
    static CKeyboardShortcutHandler&              Instance();
    LRESULT CALLBACK                              TranslateAccelerator(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    std::wstring                                  GetShortCutStringForCommand(WORD cmd) const;
    std::wstring                                  GetTooltipTitleForCommand(WORD cmd) const;
    void                                          Reload();
    void                                          UpdateTooltips();
    void                                          ToolTipUpdated(WORD cmd);
    const std::unordered_map<std::wstring, int>&  GetResourceData() const { return m_resourceData; }
    const std::unordered_map<std::wstring, UINT>& GetVirtKeys() const { return m_virtKeys; }
    const std::vector<KshAccel>&                  GetAccelerators() const { return m_accelerators; }
    void                                          AddCommand(const std::wstring& name, int id);
    static std::wstring                           MakeShortCutKeyForAccel(const KshAccel& accel);

private:
    CKeyboardShortcutHandler();
    ~CKeyboardShortcutHandler();

    void LoadUIHeaders();
    void LoadUIHeader(const char* resData, DWORD resSize);
    void Load();
    void Load(const CSimpleIni& ini);

private:
    bool                                   m_bLoaded;
    std::vector<KshAccel>                  m_accelerators;
    std::unordered_map<std::wstring, UINT> m_virtKeys;
    WORD                                   m_lastKey;
    std::unordered_map<std::wstring, int>  m_resourceData;
    std::unordered_set<WORD>               m_tooltipTitlesToUpdate;
};
