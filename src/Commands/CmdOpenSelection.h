// This file is part of BowPad.
//
// Copyright (C) 2014 - Stefan Kueng
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
#include "ICommand.h"
#include "BowPad.h"
#include "BowPadUI.h"
#include "UnicodeUtils.h"

class CCmdOpenSelection : public ICommand
{
public:

    CCmdOpenSelection(void * obj) : ICommand(obj)
    {
    }

    ~CCmdOpenSelection(void)
    {
    }

    virtual bool Execute() override
    {
        std::wstring path = GetPathUnderCursor();
        if (!path.empty())
        {
            return OpenFile(path.c_str(), true);
        }
        return false;
    }

    virtual UINT GetCmdId() override { return cmdOpenSelection; }

    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
    {
        if (UI_PKEY_Enabled == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, !GetPathUnderCursor().empty(), ppropvarNewValue);
        }
        if (UI_PKEY_Label == key)
        {
            // add the filename to the command label
            std::wstring path = GetPathUnderCursor();
            auto slashpos = path.find_last_of('\\');
            if (slashpos != std::wstring::npos)
            {
                path = path.substr(slashpos+1);
            }

            ResString label(hRes, cmdOpenSelection_LabelTitle_RESID);
            if (!path.empty())
            {
                return UIInitPropertyFromString(UI_PKEY_Label, CStringUtils::Format(L"%s \"%s\"", (LPCWSTR)label, path.c_str()).c_str(), ppropvarNewValue);
            }
            else
            {
                return UIInitPropertyFromString(UI_PKEY_Label, label, ppropvarNewValue);
            }
        }
        return E_NOTIMPL;
    }

private:
    std::wstring GetPathUnderCursor()
    {
        ScintillaCall(SCI_SETWORDCHARS, 0, (LPARAM)"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.,#/\\");
        size_t pos = ScintillaCall(SCI_GETCURRENTPOS);
        Scintilla::Sci_TextRange tr = { 0 };
        tr.chrg.cpMin = static_cast<long>(ScintillaCall(SCI_WORDSTARTPOSITION, pos, false));
        tr.chrg.cpMax = static_cast<long>(ScintillaCall(SCI_WORDENDPOSITION, pos, false));
        std::unique_ptr<char[]> word(new char[tr.chrg.cpMax - tr.chrg.cpMin + 2]);
        tr.lpstrText = word.get();
        ScintillaCall(SCI_GETTEXTRANGE, 0, (sptr_t)&tr);
        std::wstring sWord = CUnicodeUtils::StdGetUnicode(tr.lpstrText);
        ScintillaCall(SCI_SETCHARSDEFAULT);

        InvalidateUICommand(cmdOpenSelection, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Label);
        InvalidateUICommand(cmdOpenSelection, UI_INVALIDATIONS_STATE, &UI_PKEY_Enabled);

        if (PathIsRelative(sWord.c_str()))
        {
            CDocument doc = GetActiveDocument();
            if (!doc.m_path.empty())
            {
                auto slashpos = doc.m_path.find_last_of('\\');
                if (slashpos != std::wstring::npos)
                {
                    std::wstring path = doc.m_path.substr(0, slashpos) + L"\\" + sWord;
                    if (PathFileExists(path.c_str()) && !PathIsDirectory(path.c_str()))
                        return path;
                    path = doc.m_path.substr(0, slashpos);
                    slashpos = path.find_last_of('\\');
                    if (slashpos != std::wstring::npos)
                    {
                        path = doc.m_path.substr(0, slashpos) + L"\\" + sWord;
                        if (PathFileExists(path.c_str()) && !PathIsDirectory(path.c_str()))
                            return path;
                    }
                }
            }
        }
        if (PathFileExists(sWord.c_str()) && !PathIsDirectory(sWord.c_str()))
        {
            return sWord;
        }
        return L"";
    }
};
