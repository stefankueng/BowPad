// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2019-2022 - Stefan Kueng
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
#include "BowPadUI.h"

#include "../ext/scintilla/include/ScintillaTypes.h"
#include "../ext/scintilla/src/KeyMap.h"
#include "../ext/scintilla/include/scintilla.h"

#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>

#include <vector>
#include <utility>

extern IUIFramework* g_pFramework;

static BYTE GetCtrlKeys()
{
    BYTE ctrlKeys = (GetKeyState(VK_CONTROL) & 0x8000) ? 0x08 : 0;
    if (GetKeyState(VK_SHIFT) & 0x8000)
        ctrlKeys |= 0x04;
    if (GetKeyState(VK_MENU) & 0x8000)
        ctrlKeys |= 0x10;
    return ctrlKeys;
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
        instance.LoadUIHeaders();
        instance.Load();
#ifdef _DEBUG
        // check if one of our shortcuts overrides a built-in Scintilla shortcut
        Scintilla::Internal::KeyMap kMap;
        const auto&                 keyMap = kMap.GetKeyMap();
        for (auto& k : instance.m_accelerators)
        {
            int         modifier = 0;
            std::string sMod;
            if (k.fVirt & 0x10)
            {
                modifier |= SCMOD_ALT;
                sMod += "Alt ";
            }
            if (k.fVirt & 0x08)
            {
                modifier |= SCMOD_CTRL;
                sMod += "Ctrl ";
            }
            if (k.fVirt & 0x04)
            {
                modifier |= SCMOD_SHIFT;
                sMod += "Shift ";
            }
            Scintilla::Internal::KeyModifiers sm(static_cast<Scintilla::Keys>(k.key1), static_cast<Scintilla::KeyMod>(modifier));
            auto                              foundIt = keyMap.find(sm);
            if (foundIt != keyMap.end())
            {
                // before hitting a break, check if it's a command we want to override
                switch (k.cmd)
                {
                    case cmdCopyPlain:
                    case cmdCutPlain:
                    case cmdEditSelection:
                    case cmdLineDuplicate:
                    case cmdLowercase:
                    case cmdPaste:
                    case cmdRedo:
                    case cmdSessionLast:
                    case cmdUndo:
                    case cmdUppercase:
                    case cmdNew:
                        continue;
                    default:
                        break;
                }
                const char key = static_cast<const char>(sm.key);
                sMod += key;
                OutputDebugString(L"\nScintilla key shortcut overridden! : ");
                OutputDebugStringA(sMod.c_str());
                for (const auto& [sCmd, cmd] : instance.m_resourceData)
                {
                    if (cmd == static_cast<int>(foundIt->second))
                    {
                        OutputDebugString(L" - ");
                        OutputDebugString(sCmd.c_str());
                    }
                }
                OutputDebugString(L"\n");
                OutputDebugString(k.sCmd.c_str());
                OutputDebugString(L"\n");
                DebugBreak();
            }
        }
#endif
    }
    return instance;
}

void CKeyboardShortcutHandler::Load()
{
    DWORD       resSize = 0;
    const char* resData = CAppUtils::GetResourceData(L"config", IDR_SHORTCUTS, resSize);
    if (resData != nullptr)
    {
        CSimpleIni ini;
        ini.SetMultiKey(true);
        ini.LoadFile(resData, resSize);
        Load(ini);
    }

    std::wstring userFile = CAppUtils::GetDataPath() + L"\\shortcuts.ini";
    CSimpleIni   userIni;
    userIni.SetMultiKey(true);
    userIni.LoadFile(userFile.c_str());
    Load(userIni);

    resSize = 0;
    resData = CAppUtils::GetResourceData(L"config", IDR_SHORTCUTSINTERNAL, resSize);
    if (resData != nullptr)
    {
        CSimpleIni ini;
        ini.SetMultiKey(true);
        ini.LoadFile(resData, resSize);
        Load(ini);
    }

    m_bLoaded = true;
}

void CKeyboardShortcutHandler::Load(const CSimpleIni& ini)
{
    CSimpleIni::TNamesDepend virtKeys;
    ini.GetAllKeys(L"virtkeys", virtKeys);
    for (auto keyName : virtKeys)
    {
        unsigned long  vk            = 0;
        const wchar_t* keyDataString = ini.GetValue(L"virtkeys", keyName);
        if (CAppUtils::TryParse(keyDataString, vk, false, 0, 16))
            m_virtKeys[keyName] = static_cast<UINT>(vk);
        else
            APPVERIFYM(false, L"Invalid Virtual Key ini file. Cannot convert key '%s' to uint.", keyName);
    }

    CSimpleIni::TNamesDepend shortkeys;
    ini.GetAllKeys(L"shortcuts", shortkeys);

    for (auto cmd : shortkeys)
    {
        CSimpleIni::TNamesDepend values;
        ini.GetAllValues(L"shortcuts", cmd, values);
        for (auto v : values)
        {
            std::vector<std::wstring> keyVec;
            stringtok(keyVec, v, false, L",");
            KshAccel accel;
            accel.sCmd = cmd;
            accel.cmd  = static_cast<WORD>(_wtoi(cmd));
            if (accel.cmd == 0)
            {
                auto it = m_resourceData.find(cmd);
                if (it != m_resourceData.end())
                    accel.cmd = static_cast<WORD>(it->second);
            }
            for (size_t i = 0; i < keyVec.size(); ++i)
            {
                switch (i)
                {
                    case 0:
                        if (CStringUtils::find_caseinsensitive(keyVec[i], L"alt") != std::wstring::npos)
                            accel.fVirt |= 0x10;
                        if (CStringUtils::find_caseinsensitive(keyVec[i], L"ctrl") != std::wstring::npos)
                            accel.fVirt |= 0x08;
                        if (CStringUtils::find_caseinsensitive(keyVec[i], L"shift") != std::wstring::npos)
                            accel.fVirt |= 0x04;
                        break;
                    case 1:
                    {
                        auto keyVecI = keyVec[i].c_str();
                        auto whereAt = m_virtKeys.cend();
                        for (auto it = m_virtKeys.cbegin(); it != m_virtKeys.cend(); ++it)
                        {
                            if (_wcsicmp(keyVecI, it->first.c_str()) == 0)
                            {
                                whereAt = it;
                                break;
                            }
                        }
                        if (whereAt != m_virtKeys.cend())
                        {
                            accel.fVirt1 = TRUE;
                            accel.key1   = static_cast<WORD>(whereAt->second);
                        }
                        else
                        {
                            if ((keyVec[i].size() > 2) && (keyVec[i][0] == '0') && (keyVec[i][1] == 'x'))
                                accel.key1 = static_cast<WORD>(wcstol(keyVec[i].c_str(), nullptr, 0));
                            else
                                accel.key1 = static_cast<WORD>(::towupper(keyVec[i][0]));
                        }
                    }
                    break;
                    case 2:
                    {
                        auto keyVecI = keyVec[i].c_str();
                        auto whereAt = m_virtKeys.cend();
                        for (auto it = m_virtKeys.cbegin(); it != m_virtKeys.cend(); ++it)
                        {
                            if (_wcsicmp(keyVecI, it->first.c_str()) == 0)
                            {
                                whereAt = it;
                                break;
                            }
                        }
                        if (whereAt != m_virtKeys.cend())
                        {
                            accel.fVirt2 = TRUE;
                            accel.key2   = static_cast<WORD>(whereAt->second);
                        }
                        else
                        {
                            if ((keyVec[i].size() > 2) && (keyVec[i][0] == '0') && (keyVec[i][1] == 'x'))
                                accel.key2 = static_cast<WORD>(wcstol(keyVec[i].c_str(), nullptr, 0));
                            else
                                accel.key2 = static_cast<WORD>(::towupper(keyVec[i][0]));
                        }
                        break;
                    }
                    default:
                        break;
                } // switch
            }
            m_accelerators.push_back(std::move(accel));
        }
    }
}

LRESULT CALLBACK CKeyboardShortcutHandler::TranslateAccelerator(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM /*lParam*/)
{
    switch (uMsg)
    {
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            auto ctrlkeys = GetCtrlKeys();
            int  nonvirt  = MapVirtualKey(static_cast<UINT>(wParam), MAPVK_VK_TO_CHAR);
            for (auto accel = m_accelerators.crbegin(); accel != m_accelerators.crend(); ++accel)
            {
                if (accel->fVirt == ctrlkeys)
                {
                    if ((m_lastKey == 0) &&
                        (((accel->key1 == wParam) && accel->fVirt1) || (nonvirt && (accel->key1 == nonvirt) && !accel->fVirt1)))
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
                            // ignore alt codes
                            if (accel->fVirt == 0x10)
                            {
                                if (accel->key1 >= VK_NUMPAD0 && accel->key1 <= VK_NUMPAD9)
                                    return FALSE;
                            }
                            return TRUE;
                        }
                    }
                    else if ((m_lastKey == accel->key1) &&
                             (((accel->key2 == wParam) && accel->fVirt2) || (nonvirt && (accel->key2 == nonvirt) && !accel->fVirt2)))
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
            auto ctrlKeys = GetCtrlKeys();
            if (ctrlKeys == 0)
                m_lastKey = 0;
        }
        break;
        default:
            break;
    }
    return FALSE;
}

std::wstring CKeyboardShortcutHandler::GetShortCutStringForCommand(WORD cmd) const
{
    auto whereAt = std::find_if(m_accelerators.begin(), m_accelerators.end(),
                                [&](const auto& item) { return (item.cmd == cmd); });
    if (whereAt != m_accelerators.end())
    {
        return MakeShortCutKeyForAccel(*whereAt);
    }
    return {};
}

void CKeyboardShortcutHandler::LoadUIHeaders()
{
    m_resourceData.clear();
    DWORD       resSize = 0;
    const char* resData = CAppUtils::GetResourceData(L"config", IDR_BOWPADUIH, resSize);
    LoadUIHeader(resData, resSize);

    resData = CAppUtils::GetResourceData(L"config", IDR_SCINTILLAH, resSize);
    LoadUIHeader(resData, resSize);
}

void CKeyboardShortcutHandler::LoadUIHeader(const char* resData, DWORD resSize)
{
    if (resData != nullptr)
    {
        // parse the header file
        DWORD lastLineStart = 0;
        for (DWORD ind = 0; ind < resSize; ++ind)
        {
            if (resData[ind] == '\n')
            {
                std::string sLine(resData + lastLineStart, ind - lastLineStart);
                lastLineStart = ind + 1;
                // cut off '#define'
                if (sLine.empty())
                    continue;
                if (sLine[0] == '/')
                    continue;
                if (sLine.find("#define") == std::string::npos)
                    continue;
                auto spacePos = sLine.find(' ');
                if (spacePos != std::string::npos)
                {
                    auto spacePos2 = sLine.find(' ', spacePos + 1);
                    if (spacePos2 != std::string::npos)
                    {
                        std::string sIDString                                   = sLine.substr(spacePos + 1, spacePos2 - spacePos - 1);
                        int         id                                          = atoi(sLine.substr(spacePos2).c_str());
                        m_resourceData[CUnicodeUtils::StdGetUnicode(sIDString)] = id;
                    }
                }
            }
        }
    }
}

void CKeyboardShortcutHandler::UpdateTooltips()
{
    for (const auto& [sCmd, cmd] : m_resourceData)
    {
        std::wstring sAcc = GetShortCutStringForCommand(static_cast<WORD>(cmd));
        if (!sAcc.empty())
        {
            g_pFramework->InvalidateUICommand(cmd, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_TooltipTitle);
        }
    }
}

std::wstring CKeyboardShortcutHandler::GetTooltipTitleForCommand(WORD cmd) const
{
    std::wstring sAcc = GetShortCutStringForCommand(static_cast<WORD>(cmd));
    if (!sAcc.empty())
    {
        auto whereAt = std::find_if(m_resourceData.begin(), m_resourceData.end(),
                                    [&](const auto& item) { return (item.second == cmd); });
        if (whereAt != m_resourceData.end())
        {
            auto sID = whereAt->first;
            sID += L"_TooltipTitle_RESID";

            const auto& ttIDit = m_resourceData.find(sID);
            if (ttIDit != m_resourceData.end())
            {
                auto sRes = LoadResourceWString(g_hRes, ttIDit->second);
                sRes += L' ';
                sRes += sAcc;
                return sRes;
            }
        }
    }
    return {};
}

void CKeyboardShortcutHandler::ToolTipUpdated(WORD cmd)
{
    m_tooltipTitlesToUpdate.erase(cmd);
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
            it->cmd = static_cast<WORD>(id);
            break;
        }
    }
}

std::wstring CKeyboardShortcutHandler::MakeShortCutKeyForAccel(const KshAccel& accel)
{
    std::wstring shortCut;
    wchar_t      buf[128] = {};

    auto mapKey = [&](UINT code) {
        LONG sc = MapVirtualKey(code, MAPVK_VK_TO_VSC);
        sc <<= 16;
        int len = GetKeyNameText(sc, buf, _countof(buf));
        if (!shortCut.empty())
            shortCut += L"+";
        if (len > 0)
            shortCut += buf;
    };

    auto scanKey = [&](UINT code) {
        LONG nScanCode = MapVirtualKey(code, MAPVK_VK_TO_VSC);
        switch (code)
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
                break;
            default:
                break;
        }
        nScanCode <<= 16;
        int len = GetKeyNameText(nScanCode, buf, _countof(buf));
        if (len == 0)
        {
            buf[0] = static_cast<wchar_t>(code);
            buf[1] = 0;
            len    = 1;
        }
        if (!shortCut.empty())
            shortCut += L"+";
        if (len > 0)
            shortCut += buf;
    };

    if (accel.fVirt & 0x08)
    {
        mapKey(VK_CONTROL);
    }
    if (accel.fVirt & 0x10)
    {
        mapKey(VK_MENU);
    }
    if (accel.fVirt & 0x04)
    {
        mapKey(VK_SHIFT);
    }
    if (accel.key1)
    {
        scanKey(accel.key1);
    }

    if (accel.key2)
    {
        scanKey(accel.key2);
    }
    return L"(" + shortCut + L")";
}
