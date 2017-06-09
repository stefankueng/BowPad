// This file is part of BowPad.
//
// Copyright (C) 2014-2017 - Stefan Kueng
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

#include "CmdRandom.h"

#include "PathUtils.h"
#include "DirFileEnum.h"

#include <random>
#include <stdlib.h>
#include <time.h>

void CRandomFileList::InitPath(const std::wstring& path, bool nosubfolders)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    m_arUnShownFileList.clear();
    m_arShownFileList.clear();
    m_arShuffleList.clear();
    m_sPath = path;

    CDirFileEnum filefinder(m_sPath);
    bool bIsDirectory;
    std::wstring filename;
    std::list<std::wstring> tempList;
    while (filefinder.NextFile(filename, &bIsDirectory, !m_noSubs))
    {
        if (!bIsDirectory)
            tempList.emplace_back(filename);
    }
    tempList.sort([](const std::wstring& a, const std::wstring& b) -> bool
    {
        return _wcsicmp(a.c_str(), b.c_str()) > 0;
    });
    {
        auto lastIt = m_arUnShownFileList.end();
        for (const auto& f : tempList)
            lastIt = m_arUnShownFileList.emplace_hint(lastIt, f);
    }

    std::wstring temppath = m_sPath + L"\\_shownfilelist";
    if (nosubfolders)
        temppath += L"norecurse";
    HANDLE hFile = CreateFile(temppath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM, nullptr);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        BY_HANDLE_FILE_INFORMATION fileinfo;
        if (GetFileInformationByHandle(hFile, &fileinfo))
        {
            const int numberOfCharsInFile = fileinfo.nFileSizeLow / sizeof(wchar_t);
            auto buffer = std::make_unique<wchar_t[]>(numberOfCharsInFile + 1);
            DWORD readbytes;
            if (ReadFile(hFile, buffer.get(), fileinfo.nFileSizeLow, &readbytes, nullptr))
            {
                buffer[numberOfCharsInFile] = 0;
                wchar_t * pPath = buffer.get();
                auto pStart = buffer.get();
                auto pEnd = pStart + numberOfCharsInFile;
                auto lastIt = m_arShownFileList.end();
                while (pPath < pEnd)
                {
                    temppath = pPath;
                    pPath += temppath.size() + 1;
                    lastIt = m_arShownFileList.emplace_hint(lastIt, temppath);
                }
            }
        }
        CloseHandle(hFile);
    }

    for (auto it = m_arShownFileList.begin(); it != m_arShownFileList.end(); ++it)
    {
        auto findit = m_arUnShownFileList.find(*it);
        if (findit == m_arUnShownFileList.end())
        {
            m_arShownFileList.erase(*it);
            it = m_arShownFileList.begin();
        }
    }
    for (auto it = m_arShownFileList.begin(); it != m_arShownFileList.end(); ++it)
    {
        auto findit = m_arUnShownFileList.find(*it);
        if (findit != m_arUnShownFileList.end())
            m_arUnShownFileList.erase(findit);
    }

    for (const auto & file : m_arUnShownFileList)
        m_arShuffleList.push_back(file);
    auto gen = std::default_random_engine((unsigned int)time(0));
    shuffle(m_arShuffleList.begin(), m_arShuffleList.end(), gen);
    shuffle(m_arShuffleList.begin(), m_arShuffleList.end(), gen);
}

std::wstring CRandomFileList::GetRandomFile()
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_shuffleIndex >= m_arShuffleList.size())
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
            bool bIsDirectory;
            std::wstring filename;
            {
                CDirFileEnum filefinder(m_sPath);
                while (filefinder.NextFile(filename, &bIsDirectory, !m_noSubs))
                {
                    if (!bIsDirectory)
                        m_arUnShownFileList.insert(m_arUnShownFileList.end(), filename);
                }
            }
            if (m_arUnShownFileList.size() < 5)
            {
                CDirFileEnum filefinder(m_sPath);
                while (filefinder.NextFile(filename, &bIsDirectory, true))
                {
                    if (!bIsDirectory)
                        m_arUnShownFileList.insert(m_arUnShownFileList.end(), filename);
                }
            }
        }
        m_arShuffleList.clear();
        for (const auto & file : m_arUnShownFileList)
            m_arShuffleList.push_back(file);
        auto gen = std::default_random_engine((unsigned int)time(0));
        shuffle(m_arShuffleList.begin(), m_arShuffleList.end(), gen);
        m_shuffleIndex = 0;
    }
    if (m_arShuffleList.size() <= m_shuffleIndex)
        return L"";
    std::wstring filename = m_arShuffleList[m_shuffleIndex];
    ++m_shuffleIndex;
    m_arShownFileList.insert(filename);

    return filename;
}

std::wstring CRandomFileList::GoBack()
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_shuffleIndex >= 2)
    {
        std::wstring filename = m_arShuffleList[m_shuffleIndex - 2];
        --m_shuffleIndex;
        return filename;
    }
    m_shuffleIndex = m_arShuffleList.size() - 1;
    return m_arShuffleList[m_shuffleIndex];
}

size_t CRandomFileList::GetCount()
{
    return (m_arUnShownFileList.size() + m_arShownFileList.size());
}

size_t CRandomFileList::GetShownCount() {
    return m_arShownFileList.size();
}

void CRandomFileList::Save()
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_arShownFileList.empty() && m_arUnShownFileList.empty())
        return;

    std::wstring path = m_sPath + L"\\_shownfilelist";
    HANDLE hFile = CreateFile(path.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, nullptr);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        try
        {
            auto buffer = std::make_unique<wchar_t[]>(MAX_PATH * m_arShownFileList.size());
            wchar_t * pBuffer = buffer.get();
            for (auto it = m_arShownFileList.begin(); it != m_arShownFileList.end(); ++it)
            {
                _tcscpy_s(pBuffer, MAX_PATH, it->c_str());
                pBuffer += it->size() + 1;
            }
            DWORD byteswritten;
            WriteFile(hFile, buffer.get(), DWORD(pBuffer - buffer.get())*sizeof(wchar_t), &byteswritten, nullptr);
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
                WriteFile(hFile, filebuffer, DWORD(_tcslen(filebuffer)+1)*sizeof(wchar_t), &byteswritten, nullptr);
            }
            CloseHandle(hFile);
        }
    }
}

void CRandomFileList::SetShown(std::wstring file)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    m_arShownFileList.insert(file);
}

void CRandomFileList::SetNotShown(std::wstring file)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    std::set<std::wstring>::iterator it = m_arShownFileList.find(file);
    if (it != m_arShownFileList.end())
        m_arShownFileList.erase(it);
    m_arShownRepeatFileList.insert(file);
}

void CRandomFileList::RemoveFromShown(const std::wstring& file)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    m_arShownFileList.erase(file);
}

void CRandomFileList::RemoveFromNotShown(const std::wstring& file)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    m_arUnShownFileList.erase(file);
    m_arShownRepeatFileList.erase(file);
}

void CRandomFileList::SetNewPath(const std::wstring& fileold, const std::wstring& filenew)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    auto foundIT = std::find(m_arShuffleList.begin(), m_arShuffleList.end(), fileold);
    if (foundIT != m_arShuffleList.end())
    {
        (*foundIT) = filenew;
    }
}

bool CCmdRandom::Execute()
{
    if (!HasActiveDocument())
        return false;
    int tabIndex = GetActiveTabIndex();
    const auto& doc = GetActiveDocument();
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
        while (CPathUtils::PathCompare(CPathUtils::GetFileExtension(showpath), L"txt") && count)
        {
            showpath = m_fileList.GetRandomFile();
            --count;
        }
        // now load that file in the same tab
        if (OpenFile(showpath.c_str(), 0) >= 0)
        {
            CloseTab(tabIndex, false);
        }
    }
    return true;
}

void CCmdRandom::OnClose()
{
    if (!m_scannedDir.empty())
        m_fileList.Save();
}
