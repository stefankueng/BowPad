// This file is part of BowPad.
//
// Copyright (C) 2014, 2016, 2020§ - Stefan Kueng
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
#include "CmdNewCopy.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "PathUtils.h"
#include "ResString.h"

bool CCmdNewCopy::Execute()
{
    if (HasActiveDocument())
    {
        // create a copy of the active document
        size_t len = ScintillaCall(SCI_GETLENGTH);
        auto textbuf = std::make_unique<char[]>(len + 1);
        ScintillaCall(SCI_GETTEXT, len + 1, (sptr_t)textbuf.get());

        auto& doc = GetModActiveDocument();
        SaveCurrentPos(doc.m_position);
        SendMessage(GetHwnd(), WM_COMMAND, MAKEWPARAM(cmdNew, 1), 0);
        auto& docnew = GetModActiveDocument();
        auto d = docnew.m_document;
        docnew = doc;
        docnew.m_path.clear();
        docnew.m_document = d;
        docnew.m_bDoSaveAs = true;
        docnew.m_bIsDirty = true;
        docnew.m_bNeedsSaving = true;

        SetupLexerForLang(docnew.GetLanguage());
        ScintillaCall(SCI_APPENDTEXT, len, (sptr_t)textbuf.get());
        RestoreCurrentPos(docnew.m_position);
        ScintillaCall(SCI_SETSAVEPOINT);
        std::wstring sTitle = CPathUtils::GetFileName(doc.m_path);
        if (sTitle.empty())
            sTitle = GetCurrentTitle();
        std::wstring sExt = CPathUtils::GetFileExtension(doc.m_path);
        ResString sTitleFormat(g_hRes, IDS_COPYTITLE);
        if (sExt.empty())
        {
            sTitle = CStringUtils::Format(sTitleFormat, sTitle.c_str(), sExt.c_str());
            sTitle.erase(sTitle.end() - 1);
        }
        else
            sTitle = CStringUtils::Format(sTitleFormat, sTitle.substr(0, sTitle.size() - sExt.size() - 1).c_str(), sExt.c_str());
        SetCurrentTitle(sTitle.c_str());
        UpdateTab(this->GetActiveTabIndex());
        UpdateStatusBar(true);
    }
    return true;
}
