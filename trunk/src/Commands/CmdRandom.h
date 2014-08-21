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
#include "BowPadUI.h"

#include <vector>
#include <set>

class CRandomFileList
{
public:
    CRandomFileList(void){}
    ~CRandomFileList(void)
    {
    }

public:
    void InitPath(const std::wstring& path, bool nosubfolders);

    std::wstring GetRandomFile();

    std::wstring GoBack();

    size_t GetCount();

    size_t GetShownCount();

    void Save();

    void SetShown(std::wstring file);

    void SetNotShown(std::wstring file);

    void RemoveFromShown(std::wstring file);

    void RemoveFromNotShown(std::wstring file);

private:
    std::wstring m_sPath;
    std::set<std::wstring> m_arShownFileList;
    std::set<std::wstring> m_arUnShownFileList;
    std::set<std::wstring> m_arShownRepeatFileList;
    std::vector<std::wstring> m_arSessionFiles;
};


// opens a random file from the same directory as the current file shown in the current tab
class CCmdRandom : public ICommand
{
public:

    CCmdRandom(void * obj) : ICommand(obj)
    {
    }

    ~CCmdRandom(void)
    {
    }

    bool Execute() override;

    UINT GetCmdId() override { return cmdRandom; }

    void OnClose() override;

private:
    std::wstring                m_scannedDir;
    CRandomFileList             m_fileList;
};

