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

extern IUIFramework *g_pFramework;

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
    {
        instance.LoadUIHeader();
        instance.Load();
    }
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

void CKeyboardShortcutHandler::Load( CSimpleIni& ini )
{
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
        CSimpleIni::TNamesDepend values;
        ini.GetAllValues(L"shortcuts", l, values);
        for (auto vv : values)
        {
            std::wstring v = vv;
            std::transform(v.begin(), v.end(), v.begin(), ::tolower);
            std::vector<std::wstring> keyvec;
            stringtok(keyvec, v, false, L",");
            KSH_Accel accel;
            accel.sCmd = l;
            accel.cmd = (WORD)_wtoi(l);
            if (accel.cmd == 0)
            {
                auto it = m_resourceData.find(l);
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
                    if (m_virtkeys.find(keyvec[i]) != m_virtkeys.end())
                    {
                        accel.fVirt1 = TRUE;
                        accel.key1 = (WORD)m_virtkeys[keyvec[i]];
                    }
                    else
                    {
                        accel.key1 = (WORD)::towupper(keyvec[i][0]);
                        if ((keyvec[i].size() > 2) && (keyvec[i][0]=='0') && (keyvec[i][1]=='x'))
                            accel.key1 = (WORD)wcstol(keyvec[i].c_str(), NULL, 0);
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
                        if ((keyvec[i].size() > 2) && (keyvec[i][0]=='0') && (keyvec[i][1]=='x'))
                            accel.key2 = (WORD)wcstol(keyvec[i].c_str(), NULL, 0);
                    }
                    break;
                }
            }
            m_accelerators.push_back(accel);
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
            WORD ctrlkeys = (GetKeyState(VK_CONTROL)&0x8000) ? 0x08 : 0;
            ctrlkeys |= (GetKeyState(VK_SHIFT)&0x8000) ? 0x04 : 0;
            ctrlkeys |= (GetKeyState(VK_MENU)&0x8000) ? 0x10 : 0;
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

std::wstring CKeyboardShortcutHandler::GetShortCutStringForCommand( WORD cmd )
{
    for (const auto& accel : m_accelerators)
    {
        if (accel.cmd == cmd)
        {
            std::wstring s;
            wchar_t buf[MAX_PATH] = {0};
            if (accel.fVirt & 0x08)
            {
                LONG sc = MapVirtualKey(VK_CONTROL, MAPVK_VK_TO_VSC);
                sc <<= 16;
                GetKeyNameText(sc, buf, _countof(buf));
                if (!s.empty())
                    s += L"+";
                s += buf;
            }
            if (accel.fVirt & 0x10)
            {
                LONG sc = MapVirtualKey(VK_MENU, MAPVK_VK_TO_VSC);
                sc <<= 16;
                GetKeyNameText(sc, buf, _countof(buf));
                if (!s.empty())
                    s += L"+";
                s += buf;
            }
            if (accel.fVirt & 0x04)
            {
                LONG sc = MapVirtualKey(VK_SHIFT, MAPVK_VK_TO_VSC);
                sc <<= 16;
                GetKeyNameText(sc, buf, _countof(buf));
                if (!s.empty())
                    s += L"+";
                s += buf;
            }
            if (accel.key1)
            {
                LONG nScanCode = MapVirtualKey(accel.key1, MAPVK_VK_TO_VSC);
                switch(accel.key1)
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
                    nScanCode |= 0x0100; // Add extended bit
                }
                nScanCode <<= 16;
                GetKeyNameText(nScanCode, buf, _countof(buf));
                if (!s.empty())
                    s += L"+";
                s += buf;
            }

            if (accel.key2)
            {
                LONG nScanCode = MapVirtualKey(accel.key2, MAPVK_VK_TO_VSC);
                switch(accel.key2)
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
                    nScanCode |= 0x0100; // Add extended bit
                }
                nScanCode <<= 16;
                GetKeyNameText(nScanCode, buf, _countof(buf));
                if (!s.empty())
                    s += L",";
                s += buf;
            }
            return L"(" + s + L")";
        }
    }
    return L"";
}

void CKeyboardShortcutHandler::LoadUIHeader()
{
    m_resourceData.clear();
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(IDR_BOWPADUIH), L"config");
    if (hRes)
    {
        HGLOBAL hResourceLoaded = LoadResource(NULL, hRes);
        if (hResourceLoaded)
        {
            char * lpResLock = (char *) LockResource(hResourceLoaded);
            DWORD dwSizeRes = SizeofResource(NULL, hRes);
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
                                std::wstring s = CUnicodeUtils::StdGetUnicode(sIDString);
                                m_resourceData[s] = ID;
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

std::wstring CKeyboardShortcutHandler::GetTooltipTitleForCommand( WORD cmd )
{
    std::wstring sAcc = GetShortCutStringForCommand((WORD)cmd);
    if (!sAcc.empty())
    {
        std::wstring sID;
        for (const auto& sc : m_resourceData)
        {
            if (sc.second == cmd)
            {
                sID = sc.first;
                break;
            }
        }
        sID += L"_TooltipTitle_RESID";
        auto ttIDit = m_resourceData.find(sID);
        if (ttIDit != m_resourceData.end())
        {
            ResString rs(hRes, ttIDit->second);
            std::wstring sRes = (LPCWSTR)rs;
            sRes += L" ";
            sRes += sAcc;
            return sRes;
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
            it->cmd = id;
            break;
        }
    }
}
