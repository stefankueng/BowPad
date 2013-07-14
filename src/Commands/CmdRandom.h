// This file is part of BowPad.
//
// Copyright (C) 2013 - Stefan Kueng
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
#include "PathUtils.h"
#include "BowPadUI.h"
#include "DirFileEnum.h"

#include <vector>
#include <set>
#define _CRT_RAND_S
#include <stdlib.h>
#include <time.h>


class CRandomFileList
{
public:
    CRandomFileList(void){}
    ~CRandomFileList(void)
    {
        Save();
    }

public:
    void InitPath(const std::wstring& path, bool nosubfolders)
    {
        m_arUnShownFileList.clear();
        m_arShownFileList.clear();
        m_sPath = path;

        CDirFileEnum filefinder(m_sPath);
        bool bIsDirectory;
        std::wstring filename;
        while (filefinder.NextFile(filename, &bIsDirectory, !nosubfolders))
        {
            if (!bIsDirectory)
                m_arUnShownFileList.insert(filename);
        }
        std::wstring temppath = m_sPath + L"\\_shownfilelist";
        if (nosubfolders)
            temppath += L"norecurse";
        HANDLE hFile = CreateFile(temppath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            BY_HANDLE_FILE_INFORMATION fileinfo;
            if (GetFileInformationByHandle(hFile, &fileinfo))
            {
                std::unique_ptr<wchar_t[]> buffer(new wchar_t[fileinfo.nFileSizeLow]);
                DWORD readbytes;
                if (ReadFile(hFile, buffer.get(), fileinfo.nFileSizeLow, &readbytes, NULL))
                {
                    wchar_t * pPath = buffer.get();
                    while (pPath < (buffer.get()+fileinfo.nFileSizeLow))
                    {
                        std::wstring temppath = pPath;
                        pPath += temppath.size()+1;
                        m_arShownFileList.insert(temppath);
                    }
                }
            }
            CloseHandle(hFile);
        }

        for (std::set<std::wstring>::iterator it = m_arShownFileList.begin(); it != m_arShownFileList.end(); ++it)
        {
            std::set<std::wstring>::iterator findit = m_arUnShownFileList.find(*it);
            if (findit == m_arUnShownFileList.end())
            {
                m_arShownFileList.erase(*it);
                it = m_arShownFileList.begin();
            }
        }
        for (std::set<std::wstring>::iterator it = m_arShownFileList.begin(); it != m_arShownFileList.end(); ++it)
        {
            std::set<std::wstring>::iterator findit = m_arUnShownFileList.find(*it);
            if (findit != m_arUnShownFileList.end())
                m_arUnShownFileList.erase(findit);
        }
        srand(GetTickCount());
    }

    std::wstring GetRandomFile()
    {
        if (m_arUnShownFileList.empty())
        {
            if (!m_arShownRepeatFileList.empty())
            {
                m_arUnShownFileList = m_arShownRepeatFileList;
                m_arShownRepeatFileList.clear();
            }
            else
            {
                m_arUnShownFileList.clear();
                m_arShownFileList.clear();
                CDirFileEnum filefinder(m_sPath);
                bool bIsDirectory;
                std::wstring filename;
                while (filefinder.NextFile(filename, &bIsDirectory))
                {
                    m_arUnShownFileList.insert(m_arUnShownFileList.end(), filename);
                }
            }
        }
        DWORD ticks = GetTickCount() * rand();
        INT_PTR index = 0;
        if (!m_arUnShownFileList.empty())
            index = ticks % (m_arUnShownFileList.size());
        std::set<std::wstring>::iterator it = m_arUnShownFileList.begin();
        for (int i = 0; i < index; ++i)
            ++it;
        std::wstring filename;
        if (it != m_arUnShownFileList.end())
        {
            filename = *it;
            m_arUnShownFileList.erase(it);
            m_arShownFileList.insert(filename);
            m_arSessionFiles.push_back(filename);
        }
        return filename;
    }

    std::wstring GoBack()
    {
        if (m_arSessionFiles.empty())
            return GetRandomFile();
        std::wstring filename = m_arSessionFiles.back();
        m_arSessionFiles.pop_back();
        m_arUnShownFileList.insert(filename);
        return filename;
    }

    size_t GetCount() { return (m_arUnShownFileList.size() + m_arShownFileList.size()); }
    size_t GetShownCount() { return m_arShownFileList.size(); }
    void Save()
    {
        if (m_arShownFileList.empty() && m_arUnShownFileList.empty())
            return;

        std::wstring path = m_sPath + L"\\_shownfilelist";
        HANDLE hFile = CreateFile(path.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            try
            {
                std::unique_ptr<wchar_t[]> buffer(new wchar_t[MAX_PATH * m_arShownFileList.size()]);
                wchar_t * pBuffer = buffer.get();
                for (std::set<std::wstring>::iterator it = m_arShownFileList.begin(); it != m_arShownFileList.end(); ++it)
                {
                    _tcscpy_s(pBuffer, MAX_PATH, it->c_str());
                    pBuffer += it->size() + 1;
                }
                DWORD byteswritten;
                WriteFile(hFile, buffer.get(), DWORD(pBuffer - buffer.get())*sizeof(wchar_t), &byteswritten, NULL);
                CloseHandle(hFile);
            }
            catch (std::bad_alloc &/*e*/)
            {
                for (std::set<std::wstring>::iterator it = m_arShownFileList.begin(); it != m_arShownFileList.end(); ++it)
                {
                    wchar_t filebuffer[MAX_PATH];
                    ZeroMemory(filebuffer, sizeof(filebuffer));
                    _tcscpy_s(filebuffer, it->c_str());
                    DWORD byteswritten;
                    WriteFile(hFile, filebuffer, DWORD(_tcslen(filebuffer)+1)*sizeof(wchar_t), &byteswritten, NULL);
                }
                CloseHandle(hFile);
            }
        }
    }

    void SetShown(std::wstring file)
    {
        m_arShownFileList.insert(file);
        m_arSessionFiles.push_back(file);
    }

    void SetNotShown(std::wstring file)
    {
        std::set<std::wstring>::iterator it = m_arShownFileList.find(file);
        if (it != m_arShownFileList.end())
            m_arShownFileList.erase(it);
        m_arShownRepeatFileList.insert(file);
    }

    void RemoveFromShown(std::wstring file)
    {
        m_arShownFileList.erase(file);
    }

    void RemoveFromNotShown(std::wstring file)
    {
        m_arUnShownFileList.erase(file);
        m_arShownRepeatFileList.erase(file);
    }

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

    virtual bool Execute()
    {
        int tabIndex = GetCurrentTabIndex();
        CDocument doc = GetDocument(tabIndex);
        if (doc.m_path.empty())
            return false;
        std::wstring dir = CPathUtils::GetParentDirectory(doc.m_path);
        if (!dir.empty() && (dir != m_scannedDir))
        {
            if (!m_scannedDir.empty())
                m_fileList.Save();
            m_fileList.InitPath(dir, false);
            m_scannedDir = dir;
        }
        if (dir == m_scannedDir)
        {
            std::wstring showpath;
            int count = (int)m_fileList.GetCount();
            while (_wcsicmp(CPathUtils::GetFileExtension(showpath).c_str(), L"txt") && count)
            {
                showpath = m_fileList.GetRandomFile();
                --count;
            }
            // now load that file in the same tab
            if (OpenFile(showpath.c_str()))
            {
                CloseTab(tabIndex);
            }
        }
        return true;
    }

    virtual UINT GetCmdId() { return cmdRandom; }

private:
    std::wstring                m_scannedDir;
    CRandomFileList             m_fileList;
};

