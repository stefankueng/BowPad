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

#include "stdafx.h"
#include "CmdOpenselection.h"
#include "BowPad.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "PathUtils.h"

bool CCmdOpenSelection::Execute()
{
    std::wstring path = GetPathUnderCursor();
    if (!path.empty())
    {
        return OpenFile(path.c_str(), OpenFlags::AddToMRU);
    }
    return false;
}

HRESULT CCmdOpenSelection::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_Enabled == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, !GetPathUnderCursor().empty(), ppropvarNewValue);
    }
    if (UI_PKEY_Label == key)
    {
        // add the filename to the command label
        std::wstring path = GetPathUnderCursor();
        path = CPathUtils::GetFileName(path);

        ResString label(hRes, cmdOpenSelection_LabelTitle_RESID);
        if (!path.empty())
        {
            std::wstring tip = CStringUtils::Format(L"%s \"%s\"", (LPCWSTR)label, path.c_str());
            return UIInitPropertyFromString(UI_PKEY_Label, tip.c_str(), ppropvarNewValue);
        }
        else
            return UIInitPropertyFromString(UI_PKEY_Label, label, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

std::wstring CCmdOpenSelection::GetPathUnderCursor()
{
    ScintillaCall(SCI_SETWORDCHARS, 0, (LPARAM)"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.,#/\\");
    size_t pos = ScintillaCall(SCI_GETCURRENTPOS);
    std::string sWordA = GetTextRange(static_cast<long>(ScintillaCall(SCI_WORDSTARTPOSITION, pos, false)), static_cast<long>(ScintillaCall(SCI_WORDENDPOSITION, pos, false)));
    std::wstring sWord = CUnicodeUtils::StdGetUnicode(sWordA);
    ScintillaCall(SCI_SETCHARSDEFAULT);

    InvalidateUICommand(cmdOpenSelection, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Label);
    InvalidateUICommand(cmdOpenSelection, UI_INVALIDATIONS_STATE, &UI_PKEY_Enabled);

    // Look for the file name under the cursor in either the same directory
    // as the current document or in it's parent directory.
    // Finally try look for it in the current directory.
    if (PathIsRelative(sWord.c_str()))
    {
        CDocument doc = GetActiveDocument();
        if (!doc.m_path.empty())
        {
            std::wstring parent = CPathUtils::GetParentDirectory(doc.m_path);
            if (!parent.empty())
            {
                std::wstring path = parent;
                path = CPathUtils::Append(parent, sWord);
                if (PathFileExists(path.c_str()) && !PathIsDirectory(path.c_str()))
                    return path;
                parent = CPathUtils::GetParentDirectory(parent);
                if (!parent.empty())
                {
                    path = CPathUtils::Append(parent, sWord);
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
