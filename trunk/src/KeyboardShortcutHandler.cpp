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
#include "KeyboardShortcutHandler.h"
#include "SimpleIni.h"
#include "StringUtils.h"
#include "BowPad.h"

#include <vector>

CKeyboardShortcutHandler::CKeyboardShortcutHandler(void)
    : m_bLoaded(false)
    , m_lastKey(0)
{
}


CKeyboardShortcutHandler::~CKeyboardShortcutHandler(void)
{
}


CKeyboardShortcutHandler& CKeyboardShortcutHandler::Instance()
{
    static CKeyboardShortcutHandler instance;
    if (!instance.m_bLoaded)
        instance.Load();
    return instance;
}

void CKeyboardShortcutHandler::Load()
{
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(IDR_SHORTCUTS), L"config");
    if (hRes)
    {
        HGLOBAL hResourceLoaded = LoadResource(NULL, hRes);
        if (hResourceLoaded)
        {
            char * lpResLock = (char *) LockResource(hResourceLoaded);
            DWORD dwSizeRes = SizeofResource(NULL, hRes);
            if (lpResLock)
            {
                CSimpleIni ini;
                ini.LoadFile(lpResLock, dwSizeRes);

                CSimpleIni::TNamesDepend virtkeys;
                ini.GetAllKeys(L"virtkeys", virtkeys);
                for (auto l : virtkeys)
                {
                    wchar_t * endptr;
                    std::wstring v = l;
                    std::transform(v.begin(), v.end(), v.begin(), ::tolower);
                    m_virtkeys[v] = wcstol(ini.GetValue(L"virtkeys", l), &endptr, 16);
                }

                CSimpleIni::TNamesDepend shortkeys;
                ini.GetAllKeys(L"shortcuts", shortkeys);
                for (auto l : shortkeys)
                {
                    std::wstring v = ini.GetValue(L"shortcuts", l);
                    std::transform(v.begin(), v.end(), v.begin(), ::tolower);
                    std::vector<std::wstring> tokens;
                    stringtok(tokens, v, false, L";");
                    if (tokens.size() < 3)
                        continue;
                    std::wstring keys = tokens[2];
                    std::vector<std::wstring> keyvec;
                    stringtok(keyvec, keys, false, L",");
                    KSH_Accel accel;
                    accel.cmd = (WORD)_wtoi(tokens[0].c_str());
                    for (size_t i = 0; i < keyvec.size(); ++i)
                    {
                        switch (i)
                        {
                        case 0:
                            if (keyvec[i].find(L"alt") != std::wstring::npos)
                                accel.fVirt |= 0x10;
                            if (keyvec[i].find(L"ctrl") != std::wstring::npos)
                                accel.fVirt |= 0x08;
                            if (keyvec[i].find(L"shift") != std::wstring::npos)
                                accel.fVirt |= 0x04;
                            break;
                        case 1:
                            if (m_virtkeys.find(keyvec[i]) != m_virtkeys.end())
                            {
                                accel.fVirt1 = TRUE;
                                accel.key1 = (WORD)m_virtkeys[keyvec[i]];
                            }
                            else
                            {
                                accel.key1 = (WORD)::towupper(keyvec[i][0]);
                            }
                            break;
                        case 2:
                            if (m_virtkeys.find(keyvec[i]) != m_virtkeys.end())
                            {
                                accel.fVirt2 = TRUE;
                                accel.key2 = (WORD)m_virtkeys[keyvec[i]];
                            }
                            else
                            {
                                accel.key2 = (WORD)::towupper(keyvec[i][0]);
                            }
                            break;
                        }
                    }
                    m_accelerators.push_back(accel);
                }
            }
        }
    }

    m_bLoaded = true;
}

LRESULT CALLBACK CKeyboardShortcutHandler::TranslateAccelerator( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM /*lParam*/ )
{
    switch (uMsg)
    {
    case WM_KEYDOWN:
        {
            WORD ctrlkeys = (GetKeyState(VK_CONTROL)&0x8000) ? 0x08 : 0;
            ctrlkeys |= (GetKeyState(VK_SHIFT)&0x8000) ? 0x04 : 0;
            ctrlkeys |= (GetKeyState(VK_MENU)&0x8000) ? 0x10 : 0;
            for (auto accel : m_accelerators)
            {
                if (accel.fVirt == ctrlkeys)
                {
                    if ((m_lastKey == 0) && (accel.key1 == wParam))
                    {
                        if (accel.key2 == 0)
                        {
                            // execute the command
                            SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(accel.cmd, 1), 0);
                            m_lastKey = 0;
                            return TRUE;
                        }
                        else
                        {
                            m_lastKey = accel.key1;
                            return TRUE;
                        }
                    }
                    else if ((m_lastKey == accel.key1) && (accel.key2 == wParam))
                    {
                        // execute the command
                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(accel.cmd, 1), 0);
                        m_lastKey = 0;
                        return TRUE;
                    }
                }
            }
        }
        break;
    case WM_KEYUP:
        {
            WORD ctrlkeys = (GetKeyState(VK_CONTROL)&0x8000) ? 0x08 : 0;
            ctrlkeys |= (GetKeyState(VK_SHIFT)&0x8000) ? 0x04 : 0;
            ctrlkeys |= (GetKeyState(VK_MENU)&0x8000) ? 0x10 : 0;
            if (ctrlkeys == 0)
                m_lastKey = 0;
        }
        break;
    default:
        break;
    }
    return FALSE;
}
