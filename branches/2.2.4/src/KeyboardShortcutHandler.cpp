// This file is part of BowPad.
//
// Copyright (C) 2013-2016 - Stefan Kueng
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
#include "UnicodeUtils.h"
#include "BowPad.h"
#include "ResString.h"
#include "AppUtils.h"

#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>

#include <vector>
#include <utility>

extern IUIFramework *g_pFramework;

namespace
{
    inline BYTE GetCtrlKeys()
    {
        BYTE ctrlkeys = (GetKeyState(VK_CONTROL) & 0x8000) ? 0x08 : 0;
        if (GetKeyState(VK_SHIFT) & 0x8000)
            ctrlkeys |= 0x04;
        if (GetKeyState(VK_MENU) & 0x8000)
            ctrlkeys |= 0x10;
        return ctrlkeys;
    }

}

CKeyboardShortcutHandler::CKeyboardShortcutHandler()
    : m_bLoaded(false)
    , m_lastKey(0)
{
}

CKeyboardShortcutHandler::~CKeyboardShortcutHandler()
{
}


CKeyboardShortcutHandler& CKeyboardShortcutHandler::Instance()
{
    static CKeyboardShortcutHandler instance;
    if (!instance.m_bLoaded)
    {
        instance.LoadUIHeader();
        instance.Load();
    }
    return instance;
}

void CKeyboardShortcutHandler::Load()
{
    HRSRC hResource = FindResource(NULL, MAKEINTRESOURCE(IDR_SHORTCUTS), L"config");
    if (hResource)
    {
        HGLOBAL hResourceLoaded = LoadResource(NULL, hResource);
        if (hResourceLoaded)
        {
            char * lpResLock = (char *) LockResource(hResourceLoaded);
            DWORD dwSizeRes = SizeofResource(NULL, hResource);
            if (lpResLock)
            {
                CSimpleIni ini;
                ini.SetMultiKey(true);
                ini.LoadFile(lpResLock, dwSizeRes);
                Load(ini);
            }
        }
    }

    std::wstring userFile = CAppUtils::GetDataPath() + L"\\shortcuts.ini";
    CSimpleIni userIni;
    userIni.SetMultiKey(true);
    userIni.LoadFile(userFile.c_str());
    Load(userIni);

    m_bLoaded = true;
}

void CKeyboardShortcutHandler::Load( const CSimpleIni& ini )
{
    CSimpleIni::TNamesDepend virtkeys;
    ini.GetAllKeys(L"virtkeys", virtkeys);
    for (auto keyName : virtkeys)
    {
        unsigned long vk = 0;
        const wchar_t* keyDataString = ini.GetValue(L"virtkeys", keyName);
        if (CAppUtils::TryParse(keyDataString, vk, false, 0, 16))
            m_virtkeys[CStringUtils::to_lower(keyName)] = (UINT)vk;
        else
            APPVERIFYM(false, L"Invalid Virtual Key ini file. Cannot convert key '%s' to uint.", keyName);
    }

    CSimpleIni::TNamesDepend shortkeys;
    ini.GetAllKeys(L"shortcuts", shortkeys);

    for (auto cmd : shortkeys)
    {
        CSimpleIni::TNamesDepend values;
        ini.GetAllValues(L"shortcuts", cmd, values);
        for (auto vv : values)
        {
            std::wstring v = vv;
            CStringUtils::emplace_to_lower(v);
            std::vector<std::wstring> keyvec;
            stringtok(keyvec, v, false, L",");
            KSH_Accel accel;
            accel.sCmd = cmd;
            accel.cmd = (WORD)_wtoi(cmd);
            if (accel.cmd == 0)
            {
                auto it = m_resourceData.find(cmd);
                if (it != m_resourceData.end())
                    accel.cmd = (WORD)it->second;
            }
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
                {
                    auto whereAt = m_virtkeys.find(keyvec[i]);
                    if (whereAt != m_virtkeys.end())
                    {
                        accel.fVirt1 = TRUE;
                        accel.key1 = (WORD)whereAt->second;
                    }
                    else
                    {
                        // REVIEW: should base be set to 16 here or are regular integers allowed?
                        if ((keyvec[i].size() > 2) && (keyvec[i][0] == '0') && (keyvec[i][1] == 'x'))
                            accel.key1 = (WORD)wcstol(keyvec[i].c_str(), nullptr, 0);
                        else
                            accel.key1 = (WORD)::towupper(keyvec[i][0]);
                    }
                }
                    break;
                case 2:
                {
                    auto whereAt = m_virtkeys.find(keyvec[i]);
                    if (whereAt != m_virtkeys.end())
                    {
                        accel.fVirt2 = TRUE;
                        accel.key2 = (WORD)whereAt->second;
                    }
                    else
                    {
                        if ((keyvec[i].size() > 2) && (keyvec[i][0] == '0') && (keyvec[i][1] == 'x'))
                            accel.key2 = (WORD)wcstol(keyvec[i].c_str(), nullptr, 0);
                        else
                            accel.key2 = (WORD)::towupper(keyvec[i][0]);
                    }
                    break;
                }
                } // switch
            }
            m_accelerators.push_back(std::move(accel));
        }
    }
}

LRESULT CALLBACK CKeyboardShortcutHandler::TranslateAccelerator( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM /*lParam*/ )
{
    switch (uMsg)
    {
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        {
            auto ctrlkeys = GetCtrlKeys();
            int nonvirt = MapVirtualKey((UINT)wParam, MAPVK_VK_TO_CHAR);
            for (auto accel = m_accelerators.crbegin(); accel != m_accelerators.crend(); ++accel)
            {
                if (accel->fVirt == ctrlkeys)
                {
                    if ((m_lastKey == 0) &&
                        (((accel->key1 == wParam) && accel->fVirt1) || (nonvirt && (accel->key1 == nonvirt) && !accel->fVirt1))
                        )
                    {
                        if (accel->key2 == 0)
                        {
                            // execute the command
                            if (GetForegroundWindow() == hwnd)
                            {
                                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(accel->cmd, 1), 0);
                                m_lastKey = 0;
                                return TRUE;
                            }
                        }
                        else
                        {
                            m_lastKey = accel->key1;
                            return TRUE;
                        }
                    }
                    else if ((m_lastKey == accel->key1) &&
                             (((accel->key2 == wParam) && accel->fVirt2) || (nonvirt && (accel->key2 == nonvirt) && !accel->fVirt2))
                        )
                    {
                        // execute the command
                        if (GetForegroundWindow() == hwnd)
                        {
                            SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(accel->cmd, 1), 0);
                            m_lastKey = 0;
                            return TRUE;
                        }
                    }
                }
            }
        }
        break;
    case WM_KEYUP:
        {
            auto ctrlkeys = GetCtrlKeys();
            if (ctrlkeys == 0)
                m_lastKey = 0;
        }
        break;
    default:
        break;
    }
    return FALSE;
}

std::wstring CKeyboardShortcutHandler::GetShortCutStringForCommand( WORD cmd ) const
{
    auto whereAt = std::find_if(m_accelerators.begin(), m_accelerators.end(),
        [&](const auto& item) { return (item.cmd == cmd); });
    if (whereAt != m_accelerators.end())
    {
        return MakeShortCutKeyForAccel(*whereAt);
    }
    return L"";
}

void CKeyboardShortcutHandler::LoadUIHeader()
{
    m_resourceData.clear();
    HRSRC hResource = FindResource(NULL, MAKEINTRESOURCE(IDR_BOWPADUIH), L"config");
    if (hResource)
    {
        HGLOBAL hResourceLoaded = LoadResource(NULL, hResource);
        if (hResourceLoaded)
        {
            char * lpResLock = (char *) LockResource(hResourceLoaded);
            DWORD dwSizeRes = SizeofResource(NULL, hResource);
            if (lpResLock)
            {
                // parse the header file
                DWORD lastLineStart = 0;
                for (DWORD ind = 0; ind < dwSizeRes; ++ind)
                {
                    if (*((char*)(lpResLock + ind)) == '\n')
                    {
                        std::string sLine(lpResLock + lastLineStart, ind-lastLineStart);
                        lastLineStart = ind + 1;
                        // cut off '#define'
                        if (sLine.empty())
                            continue;
                        if (sLine[0] == '/')
                            continue;
                        auto spacepos = sLine.find(' ');
                        if (spacepos != std::string::npos)
                        {
                            auto spacepos2 = sLine.find(' ', spacepos+1);
                            if (spacepos2 != std::string::npos)
                            {
                                std::string sIDString = sLine.substr(spacepos+1, spacepos2-spacepos-1);
                                int ID = atoi(sLine.substr(spacepos2).c_str());
                                m_resourceData[CUnicodeUtils::StdGetUnicode(sIDString)] = ID;
                            }
                        }
                    }
                }
            }
        }
    }
}

void CKeyboardShortcutHandler::UpdateTooltips(bool bAll)
{
    if (bAll)
    {
        m_tooltiptitlestoupdate.clear();
        for (const auto& rsc : m_resourceData)
        {
            std::wstring sAcc = GetShortCutStringForCommand((WORD)rsc.second);
            if (!sAcc.empty())
            {
                g_pFramework->InvalidateUICommand(rsc.second, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_TooltipTitle);
                m_tooltiptitlestoupdate.insert((WORD)rsc.second);
            }
        }
    }
    else
    {
        for (const auto& w : m_tooltiptitlestoupdate)
        {
            std::wstring sAcc = GetShortCutStringForCommand(w);
            if (!sAcc.empty())
            {
                g_pFramework->InvalidateUICommand(w, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_TooltipTitle);
            }
        }
    }
}

std::wstring CKeyboardShortcutHandler::GetTooltipTitleForCommand( WORD cmd ) const
{
    std::wstring sAcc = GetShortCutStringForCommand((WORD)cmd);
    if (!sAcc.empty())
    {
        auto whereAt = std::find_if(m_resourceData.begin(), m_resourceData.end(),
            [&](const auto& item) { return (item.second == cmd); });
        if (whereAt != m_resourceData.end())
        {
            auto sID = whereAt->first;
            sID += L"_TooltipTitle_RESID";

            auto ttIDit = m_resourceData.find(sID);
            if (ttIDit != m_resourceData.end())
            {
                auto sRes = LoadResourceWString(hRes, ttIDit->second);
                sRes += L' ';
                sRes += sAcc;
                return sRes;
            }
        }
    }
    return L"";
}

void CKeyboardShortcutHandler::ToolTipUpdated( WORD cmd )
{
    m_tooltiptitlestoupdate.erase(cmd);
}

void CKeyboardShortcutHandler::Reload()
{
    Load();
}

void CKeyboardShortcutHandler::AddCommand(const std::wstring& name, int id)
{
    for (auto it = m_accelerators.begin(); it != m_accelerators.end(); ++it)
    {
        if (it->sCmd.compare(name) == 0)
        {
            it->cmd = (WORD)id;
            break;
        }
    }
}

std::wstring CKeyboardShortcutHandler::MakeShortCutKeyForAccel(const KSH_Accel& accel)
{
    std::wstring shortCut;
    wchar_t buf[MAX_PATH];
    if (accel.fVirt & 0x08)
    {
        LONG sc = MapVirtualKey(VK_CONTROL, MAPVK_VK_TO_VSC);
        sc <<= 16;
        int len = GetKeyNameText(sc, buf, _countof(buf));
        if (!shortCut.empty())
            shortCut += L"+";
        if (len > 0)
            shortCut += buf;
    }
    if (accel.fVirt & 0x10)
    {
        LONG sc = MapVirtualKey(VK_MENU, MAPVK_VK_TO_VSC);
        sc <<= 16;
        int len = GetKeyNameText(sc, buf, _countof(buf));
        if (!shortCut.empty())
            shortCut += L"+";
        if (len > 0)
            shortCut += buf;
    }
    if (accel.fVirt & 0x04)
    {
        LONG sc = MapVirtualKey(VK_SHIFT, MAPVK_VK_TO_VSC);
        sc <<= 16;
        int len = GetKeyNameText(sc, buf, _countof(buf));
        if (!shortCut.empty())
            shortCut += L"+";
        if (len > 0)
            shortCut += buf;
    }
    if (accel.key1)
    {
        LONG nScanCode = MapVirtualKey(accel.key1, MAPVK_VK_TO_VSC);
        switch (accel.key1)
        {
            // Keys which are "extended" (except for Return which is Numeric Enter as extended)
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_NEXT:  // Page down
        case VK_PRIOR: // Page up
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_SNAPSHOT:
        case VK_OEM_COMMA:
        case VK_OEM_PERIOD:
            nScanCode |= 0x0100; // Add extended bit
        }
        nScanCode <<= 16;
        int len = GetKeyNameText(nScanCode, buf, _countof(buf));
        if (!shortCut.empty())
            shortCut += L"+";
        if (len > 0)
            shortCut += buf;
    }

    if (accel.key2)
    {
        LONG nScanCode = MapVirtualKey(accel.key2, MAPVK_VK_TO_VSC);
        switch (accel.key2)
        {
            // Keys which are "extended" (except for Return which is Numeric Enter as extended)
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_NEXT:  // Page down
        case VK_PRIOR: // Page up
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_SNAPSHOT:
        case VK_OEM_COMMA:
        case VK_OEM_PERIOD:
            nScanCode |= 0x0100; // Add extended bit
        }
        nScanCode <<= 16;
        int len = GetKeyNameText(nScanCode, buf, _countof(buf));
        if (!shortCut.empty())
            shortCut += L",";
        if (len > 0)
            shortCut += buf;
    }
    return L"(" + shortCut + L")";
}
