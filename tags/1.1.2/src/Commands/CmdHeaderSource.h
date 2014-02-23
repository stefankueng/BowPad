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

#pragma once
#include "ICommand.h"
#include "BowPad.h"
#include "BowPadUI.h"
#include "DirFileEnum.h"
#include "ChoseDlg.h"

#include <vector>

class CCmdHeaderSource : public ICommand
{
public:

    CCmdHeaderSource(void * obj) : ICommand(obj)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }

    ~CCmdHeaderSource(void)
    {}

    virtual bool Execute() override
    {
        if (!HasActiveDocument())
            return false;
        // get the current path and filename
        CDocument doc = GetActiveDocument();
        std::wstring path = doc.m_path;
        std::transform(path.begin(), path.end(), path.begin(), ::tolower);

        std::wstring dirpath = path.substr(0, path.find_last_of('\\'));
        std::wstring filename = path.substr(path.find_last_of('\\') + 1);
        auto dotpos = filename.find_first_of('.');
        if (dotpos == std::wstring::npos)
            return false;

        std::wstring plainname = filename.substr(0, dotpos);
        CDirFileEnum enumerator(dirpath);
        std::wstring respath;
        bool bIsDir = false;
        std::vector<std::wstring> matchingfiles;
        std::vector<std::wstring> matchingfilenames;

        std::vector<std::wstring> ignoredExts;
        stringtok(ignoredExts, CIniSettings::Instance().GetString(L"HeaderSource", L"IgnoredExts", L"exe*obj*dll*ilk*lib*ncb*ipch*bml*pch*res*pdb*aps"), true, L"*");
        while (enumerator.NextFile(respath, &bIsDir, false))
        {
            if (bIsDir)
                continue;

            std::wstring resname = respath.substr(respath.find_last_of('\\') + 1);
            std::wstring ext;
            size_t dotpos = resname.find_last_of('.');
            if ((dotpos != std::wstring::npos) && ((dotpos + 1) < resname.size()))
                ext = resname.substr(dotpos + 1);
            std::wstring lowerrespath = respath;
            std::transform(lowerrespath.begin(), lowerrespath.end(), lowerrespath.begin(), ::tolower);
            std::wstring lowerresname = resname;
            std::transform(lowerresname.begin(), lowerresname.end(), lowerresname.begin(), ::tolower);

            if (lowerrespath.compare(path) == 0)
                continue;
            std::wstring sub = lowerresname.substr(0, plainname.size());
            if (plainname.compare(sub) == 0)
            {
                if ((lowerresname.size() > plainname.size()) && (lowerresname[plainname.size()] == '.'))
                {
                    bool useIt = true;
                    if (!ext.empty())
                    {
                        for (const auto& e : ignoredExts)
                        {
                            if (e.compare(ext) == 0)
                            {
                                useIt = false;
                                break;
                            }
                        }
                    }
                    if (useIt)
                    {
                        matchingfiles.push_back(respath);
                        matchingfilenames.push_back(resname);
                    }
                }
            }
        }

        if (matchingfiles.empty())
            return false;

        if (matchingfiles.size() == 1)
        {
            // only one match found: open it directly
            OpenFile(matchingfiles[0].c_str(), true);
        }
        else
        {
            // more than one match found: show a chose dialog
            CChoseDlg dlg(GetHwnd());
            dlg.SetList(matchingfilenames);
            int ret = (int)dlg.DoModal(hRes, IDD_CHOSE, GetHwnd());
            if (ret >= 0)
            {
                OpenFile(matchingfiles[ret].c_str(), true);
            }
        }

        return true;
    }

    virtual UINT GetCmdId() override { return cmdHeaderSource; }

};
