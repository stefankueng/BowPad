// This file is part of BowPad.
//
// Copyright (C) 2020 - Stefan Kueng
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
#include "BaseDialog.h"

#include <vector>

/**
 * base dialog with helper methods that deal with various controls
 */
class CBPBaseDialog : public CDialog
{
public:
    CBPBaseDialog();
    ~CBPBaseDialog();

    int  GetMaxCount(const std::wstring& section, const std::wstring& countKey, int defaultMaxCount) const;
    int  LoadData(std::vector<std::wstring>& data, int defaultMaxCount, const std::wstring& section, const std::wstring& countKey, const std::wstring& itemKeyFmt) const;
    void SaveData(const std::vector<std::wstring>& data, const std::wstring& section, const std::wstring& countKey, const std::wstring& itemKeyFmt);
    void SaveCombo(int combo_id, std::vector<std::wstring>& data) const;
    void LoadCombo(int combo_id, const std::vector<std::wstring>& data);
    void UpdateCombo(int comboId, const std::wstring& item, int maxCount);
    bool EnableComboBoxDeleteEvents(int combo_id, bool enable);

    static void FlashWindow(HWND hwnd);

    static std::string UnEscape(const std::string& str);
    static bool        ReadBase(const char* str, size_t* value, size_t base, size_t size);
    static size_t      GetBase(char current, size_t& size) noexcept;

    static LRESULT CALLBACK ComboBoxListSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
};
