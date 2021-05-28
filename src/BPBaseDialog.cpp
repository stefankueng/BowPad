// This file is part of BowPad.
//
// Copyright (C) 2020-2021 - Stefan Kueng
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
#include "BPBaseDialog.h"
#include "StringUtils.h"

#include <vector>

CBPBaseDialog::CBPBaseDialog()
{
}

CBPBaseDialog::~CBPBaseDialog()
{
}

int CBPBaseDialog::GetMaxCount(const std::wstring& section, const std::wstring& countKey, int defaultMaxCount)
{
    int maxCount = static_cast<int>(CIniSettings::Instance().GetInt64(section.c_str(), countKey.c_str(), defaultMaxCount));
    if (maxCount <= 0)
        maxCount = defaultMaxCount;
    return maxCount;
}

// The return value is the maximum count allowed, not the default count or count loaded.
// For the loaded count, check data.size().
int CBPBaseDialog::LoadData(std::vector<std::wstring>& data, int defaultMaxCount, const std::wstring& section, const std::wstring& countKey, const std::wstring& itemKeyFmt) const
{
    data.clear();
    int          maxCount = GetMaxCount(section, countKey, defaultMaxCount);
    std::wstring itemKey;
    for (int i = 0; i < maxCount; ++i)
    {
        itemKey            = CStringUtils::Format(itemKeyFmt.c_str(), i);
        std::wstring value = CIniSettings::Instance().GetString(section.c_str(), itemKey.c_str(), L"");
        if (!value.empty())
            data.push_back(std::move(value));
    }
    return maxCount;
}

void CBPBaseDialog::SaveData(const std::vector<std::wstring>& data, const std::wstring& section, const std::wstring& /*countKey*/, const std::wstring& itemKeyFmt) const
{
    int i = 0;
    for (const auto& item : data)
    {
        std::wstring itemKey = CStringUtils::Format(itemKeyFmt.c_str(), i);
        if (!item.empty())
            CIniSettings::Instance().SetString(section.c_str(), itemKey.c_str(), item.c_str());
        ++i;
    }
    // Terminate list with an empty key.
    std::wstring itemKey = CStringUtils::Format(itemKeyFmt.c_str(), i);
    CIniSettings::Instance().SetString(section.c_str(), itemKey.c_str(), L"");
}

void CBPBaseDialog::LoadCombo(int comboID, const std::vector<std::wstring>& data)
{
    HWND hCombo = GetDlgItem(*this, comboID);
    for (const auto& item : data)
    {
        if (!item.empty())
            ComboBox_InsertString(hCombo, -1, item.c_str());
    }
}

void CBPBaseDialog::SaveCombo(int comboID, std::vector<std::wstring>& data) const
{
    data.clear();
    HWND hCombo = GetDlgItem(*this, comboID);
    int  count  = ComboBox_GetCount(hCombo);
    for (int i = 0; i < count; ++i)
    {
        std::wstring item;
        auto         itemSize = ComboBox_GetLBTextLen(hCombo, i);
        item.resize(itemSize + 1LL);
        ComboBox_GetLBText(hCombo, i, item.data());
        item.resize(itemSize);
        data.emplace_back(std::move(item));
    }
}

void CBPBaseDialog::UpdateCombo(int comboId, const std::wstring& item, int maxCount)
{
    if (item.empty())
        return;
    HWND hCombo = GetDlgItem(*this, comboId);
    int  count  = ComboBox_GetCount(hCombo);
    // Remove any excess items to ensure we our within the maximum,
    // and never exceed it.
    while (count > maxCount)
    {
        --count;
        ComboBox_DeleteString(hCombo, count);
    }
    int  pos        = ComboBox_FindStringExact(hCombo, -1, item.c_str());
    bool itemExists = (pos != CB_ERR);
    // If the item exists, make sure it's selected and at the top.
    if (itemExists)
    {
        ComboBox_DeleteString(hCombo, pos);
        int whereAt = ComboBox_InsertString(hCombo, 0, item.c_str());
        if (whereAt >= 0)
            ComboBox_SetCurSel(hCombo, whereAt);
    }
    else
    {
        // Item does not exist, prepare to add it by purging the oldest
        // item if there is one and we need to in order to stay within the limit.
        if (count > 0 && count >= maxCount)
        {
            ComboBox_DeleteString(hCombo, count - 1);
            --count;
        }
        if (count < maxCount)
        {
            int whereAt = ComboBox_InsertString(hCombo, 0, item.c_str());
            if (whereAt >= 0)
                ComboBox_SetCurSel(hCombo, whereAt);
        }
    }
}

bool CBPBaseDialog::EnableComboBoxDeleteEvents(int comboID, bool enable)
{
    auto hCombo = GetDlgItem(*this, comboID);
    APPVERIFY(hCombo != nullptr);
    if (!hCombo)
        return false;
    COMBOBOXINFO comboInfo = {sizeof comboInfo};
    APPVERIFY(SendMessage(hCombo, CB_GETCOMBOBOXINFO, 0, reinterpret_cast<LPARAM>(&comboInfo)) != 0);
    if (enable)
        return SetWindowSubclass(comboInfo.hwndItem, ComboBoxListSubClassProc, 0,
                                 reinterpret_cast<DWORD_PTR>(this)) != FALSE;
    return RemoveWindowSubclass(comboInfo.hwndItem, ComboBoxListSubClassProc, 0) != FALSE;
}

void CBPBaseDialog::FlashWindow(HWND hWnd)
{
    FLASHWINFO fi{sizeof(FLASHWINFO)};
    fi.hwnd      = hWnd;
    fi.dwFlags   = FLASHW_CAPTION;
    fi.uCount    = 5;
    fi.dwTimeout = 40;
    FlashWindowEx(&fi);
}

LRESULT CALLBACK CBPBaseDialog::ComboBoxListSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
    //CFindReplaceDlg* pThis = reinterpret_cast<CFindReplaceDlg*>(dwRefData);
    switch (uMsg)
    {
        case WM_SYSKEYDOWN:
        {
            HWND hCombo        = GetParent(hWnd);
            auto isDroppedDown = ComboBox_GetDroppedState(hCombo) != FALSE;
            if (isDroppedDown)
            {
                auto shiftDown   = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                auto controlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                auto altDown     = (GetKeyState(VK_MENU) & 0x8000) != 0;
                auto delDown     = (GetKeyState(VK_DELETE) & 0x8000) != 0;
                if (altDown && delDown && !controlDown && !shiftDown)
                {
                    // We don't use ResetContent because that clears
                    // the edit control as well.
                    int count = ComboBox_GetCount(hCombo);
                    while (count > 0)
                    {
                        --count;
                        ComboBox_DeleteString(hCombo, count);
                    }
                }
            }
            break;
        }
        case WM_KEYDOWN:
        {
            // When the dropdown list of the combobox is showing:
            // Let control+delete clear the combobox if the listbox is currently dropped.
            // Let delete do a delete of the currently selected item.
            if (wParam == VK_DELETE)
            {
                HWND hCombo        = GetParent(hWnd);
                auto isDroppedDown = ComboBox_GetDroppedState(hCombo);
                if (isDroppedDown)
                {
                    auto shiftDown   = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    auto controlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                    if (!shiftDown && !controlDown)
                    {
                        int curSel = ComboBox_GetCurSel(hCombo);
                        if (curSel >= 0)
                            ComboBox_DeleteString(hCombo, curSel);
                    }
                }
            }
            break;
        }
        case WM_NCDESTROY:
        {
            RemoveWindowSubclass(hWnd, ComboBoxListSubClassProc, uIdSubclass);
            break;
        }
        default:
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

std::string CBPBaseDialog::UnEscape(const std::string& str)
{
    size_t      charLeft = str.length();
    std::string result;
    result.reserve(charLeft);
    for (size_t i = 0; i < str.length(); ++i)
    {
        char current = str[i];
        --charLeft;
        if (current == '\\' && charLeft > 0)
        {
            // possible escape sequence
            ++i;
            --charLeft;
            current = str[i];
            switch (current)
            {
                case 'r':
                    result.push_back('\r');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case '0':
                    result.push_back('\0');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                case '\\':
                    result.push_back('\\');
                    break;
                default:
                {
                    size_t size;
                    auto   base = GetBase(current, size);
                    if (base != 0 && charLeft >= size)
                    {
                        size_t res = 0;
                        if (ReadBase(&str[i + 1], &res, base, size))
                        {
                            result.push_back(static_cast<char>(res));
                            i += size;
                            break;
                        }
                    }
                    // Unknown/invalid sequence, treat as regular text.
                    result.push_back('\\');
                    result.push_back(current);
                }
            }
        }
        else
        {
            result.push_back(current);
        }
    }
    return result;
}

bool CBPBaseDialog::ReadBase(const char* str, size_t* value, size_t base, size_t size)
{
    size_t i = 0, temp = 0;
    *value   = 0;
    char max = '0' + static_cast<char>(base) - 1;
    while (i < size)
    {
        char current = str[i];
        if (current >= 'A')
        {
            current &= 0xdf;
            current -= ('A' - '0' - 10);
        }
        else if (current > '9')
            return false;

        if (current >= '0' && current <= max)
        {
            temp *= base;
            temp += (current - '0');
        }
        else
        {
            return false;
        }
        ++i;
    }
    *value = temp;
    return true;
}

size_t CBPBaseDialog::GetBase(char current, size_t& size) noexcept
{
    switch (current)
    {
        case 'b': // 11111111
            size = 8;
            return 2;
        case 'o': // 377
            size = 3;
            return 8;
        case 'd': // 255
            size = 3;
            return 10;
        case 'x': // 0xFF
            size = 2;
            return 16;
        case 'u': // 0xCDCD
            size = 4;
            return 16;
        default:
            break;
    }
    size = 0;
    return 0;
}
