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
#include <map>
#include <vector>

class KSH_Accel
{
public:
    KSH_Accel()
        : fVirt(0)
        , fVirt1(FALSE)
        , key1(0)
        , fVirt2(FALSE)
        , key2(0)
        , cmd(0)
    {}
    ~KSH_Accel() {}

    BYTE            fVirt;      // 0x10 = ALT, 0x08 = Ctrl, 0x04 = Shift
    BOOL            fVirt1;     // true if key1 is a virtual key, false if it's a character code
    WORD            key1;
    BOOL            fVirt2;
    WORD            key2;
    WORD            cmd;        // the command
    std::wstring    name;
};

class CKeyboardShortcutHandler
{
public:
    static CKeyboardShortcutHandler&        Instance();
    LRESULT CALLBACK                        TranslateAccelerator(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    std::wstring                            GetShortCutStringForCommand(WORD cmd);
private:
    CKeyboardShortcutHandler(void);
    ~CKeyboardShortcutHandler(void);

    void                                    Load();

private:
    bool                                    m_bLoaded;
    std::vector<KSH_Accel>                  m_accelerators;
    std::map<std::wstring, UINT>            m_virtkeys;
    WORD                                    m_lastKey;
};
