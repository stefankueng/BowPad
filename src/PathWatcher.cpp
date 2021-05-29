// This file is part of BowPad.
//
// Copyright (C) 2021 - Stefan Kueng
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
#include "PathWatcher.h"
#include "DebugOutput.h"

#include <Dbt.h>
#include <process.h>

CPathWatcher::CPathWatcher()
    : m_hCompPort(nullptr)
    , m_bRunning(TRUE)
{
    // enable the required privileges for this process

    LPCTSTR arPrivelegeNames[] = {SE_BACKUP_NAME,
                                  SE_RESTORE_NAME,
                                  SE_CHANGE_NOTIFY_NAME};

    for (int i = 0; i < _countof(arPrivelegeNames); ++i)
    {
        CAutoGeneralHandle hToken;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, hToken.GetPointer()))
        {
            TOKEN_PRIVILEGES tp = {1};

            if (LookupPrivilegeValue(nullptr, arPrivelegeNames[i], &tp.Privileges[0].Luid))
            {
                tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

                AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
            }
        }
    }

    unsigned int threadId = 0;
    m_hThread             = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, ThreadEntry, this, 0, &threadId));
}

CPathWatcher::~CPathWatcher()
{
    Stop();
    std::unique_lock locker(m_guard);
    ClearInfoMap();
}

void CPathWatcher::Stop()
{
    InterlockedExchange(&m_bRunning, FALSE);
    if (m_hCompPort)
    {
        PostQueuedCompletionStatus(m_hCompPort, 0, NULL, nullptr);
        m_hCompPort.CloseHandle();
    }

    if (m_hThread)
    {
        // the background thread sleeps for 200ms,
        // so lets wait for it to finish for 1000 ms.

        WaitForSingleObject(m_hThread, 1000);
        m_hThread.CloseHandle();
    }
}

bool CPathWatcher::RemovePath(const std::wstring& path)
{
    std::unique_lock locker(m_guard);

    CTraceToOutputDebugString::Instance()(TEXT(__FUNCTION__) L": RemovePath for %s\n", path.c_str());
    bool bRet = (watchedPaths.erase(path) != 0);
    m_hCompPort.CloseHandle();
    return bRet;
}

/**
     * Removes all watched paths
     */

void CPathWatcher::ClearPaths(bool includeChangedPaths)
{
    std::unique_lock locker(m_guard);
    watchedPaths.clear();
    m_hCompPort.CloseHandle();
    if (includeChangedPaths)
        m_changedPaths.clear();
}

bool CPathWatcher::AddPath(const std::wstring& path, bool recursive)
{
    std::unique_lock locker(m_guard);
    CTraceToOutputDebugString::Instance()(TEXT(__FUNCTION__) L": AddPath for %s\n", path.c_str());
    watchedPaths[path] = recursive;
    m_hCompPort.CloseHandle();
    return true;
}

unsigned int CPathWatcher::ThreadEntry(void* pContext)
{
    static_cast<CPathWatcher*>(pContext)->WorkerThread();
    return 0;
}

void CPathWatcher::WorkerThread()
{
    DWORD          numBytes = 0;
    CDirWatchInfo* pdi      = nullptr;
    LPOVERLAPPED   lpOverlapped{};
    constexpr int  bufferSize = MAX_PATH * 4;
    TCHAR          buf[bufferSize]{};
    while (m_bRunning)
    {
        if (!watchedPaths.empty())
        {
            if (!m_hCompPort || !GetQueuedCompletionStatus(m_hCompPort,
                                                           &numBytes,
                                                           reinterpret_cast<PULONG_PTR>(&pdi),
                                                           &lpOverlapped,
                                                           INFINITE))
            {
                // Error retrieving changes
                // Clear the list of watched objects and recreate that list
                if (!m_bRunning)
                    return;
                {
                    std::unique_lock locker(m_guard);
                    ClearInfoMap();
                }
                DWORD lastErr = GetLastError();
                if ((m_hCompPort) && (lastErr != ERROR_SUCCESS) && (lastErr != ERROR_INVALID_HANDLE))
                {
                    m_hCompPort.CloseHandle();
                }
                std::unique_lock locker(m_guard);
                for (const auto& [watchedPath, recursive] : watchedPaths)
                {
                    CAutoFile hDir = CreateFile(watchedPath.c_str(),
                                                FILE_LIST_DIRECTORY,
                                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                                nullptr, //security attributes
                                                OPEN_EXISTING,
                                                FILE_FLAG_BACKUP_SEMANTICS | //required privileges: SE_BACKUP_NAME and SE_RESTORE_NAME.
                                                    FILE_FLAG_OVERLAPPED,
                                                nullptr);
                    if (!hDir)
                    {
                        // this could happen if a watched folder has been removed/renamed
                        m_hCompPort.CloseHandle();
                        watchedPaths.erase(watchedPath);
                        break;
                    }

                    auto pDirInfo = std::make_unique<CDirWatchInfo>(std::move(hDir), watchedPath.c_str(), recursive);
                    m_hCompPort   = CreateIoCompletionPort(pDirInfo->m_hDir, m_hCompPort, reinterpret_cast<ULONG_PTR>(pDirInfo.get()), 0);
                    if (m_hCompPort == NULL)
                    {
                        ClearInfoMap();
                        watchedPaths.erase(watchedPath);
                        break;
                    }
                    if (!ReadDirectoryChangesW(pDirInfo->m_hDir,
                                               pDirInfo->m_buffer,
                                               READ_DIR_CHANGE_BUFFER_SIZE,
                                               recursive ? TRUE : FALSE,
                                               FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
                                               &numBytes, // not used
                                               &pDirInfo->m_overlapped,
                                               nullptr)) //no completion routine!
                    {
                        ClearInfoMap();
                        watchedPaths.erase(watchedPath);
                        break;
                    }
                    CTraceToOutputDebugString::Instance()(TEXT(__FUNCTION__) L": watching path %s\n", watchedPath.c_str());
                    m_watchInfoMap[pDirInfo->m_hDir] = std::move(pDirInfo);
                }
            }
            else
            {
                if (!m_bRunning)
                    return;
                // NOTE: the longer this code takes to execute until ReadDirectoryChangesW
                // is called again, the higher the chance that we miss some
                // changes in the file system!
                if (pdi)
                {
                    if (numBytes)
                    {
                        PFILE_NOTIFY_INFORMATION pnotify = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(pdi->m_buffer);
                        if (reinterpret_cast<ULONG_PTR>(pnotify) - reinterpret_cast<ULONG_PTR>(pdi->m_buffer) <= READ_DIR_CHANGE_BUFFER_SIZE)
                        {
                            DWORD nOffset = pnotify->NextEntryOffset;
                            do
                            {
                                nOffset = pnotify->NextEntryOffset;
                                SecureZeroMemory(buf, bufferSize * sizeof(TCHAR));
                                wcsncpy_s(buf, bufferSize, pdi->m_dirPath.c_str(), bufferSize);
                                errno_t err = wcsncat_s(buf + pdi->m_dirPath.size(), bufferSize - pdi->m_dirPath.size(), pnotify->FileName, min(pnotify->FileNameLength / sizeof(WCHAR), bufferSize - pdi->m_dirPath.size()));
                                if (err == STRUNCATE)
                                {
                                    pnotify = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(reinterpret_cast<LPBYTE>(pnotify) + nOffset);
                                    continue;
                                }
                                buf[min(bufferSize - 1, pdi->m_dirPath.size() + (pnotify->FileNameLength / sizeof(WCHAR)))] = 0;
                                CTraceToOutputDebugString::Instance()(TEXT(__FUNCTION__) L": change notification for %s\n", buf);
                                {
                                    std::unique_lock locker(m_guard);
                                    m_changedPaths.emplace_back(pnotify->Action, buf);
                                }
                                pnotify = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(reinterpret_cast<LPBYTE>(pnotify) + nOffset);
                                if (reinterpret_cast<ULONG_PTR>(pnotify) - reinterpret_cast<ULONG_PTR>(pdi->m_buffer) > READ_DIR_CHANGE_BUFFER_SIZE)
                                    break;
                            } while (nOffset);
                        }
                    }
                    SecureZeroMemory(pdi->m_buffer, sizeof(pdi->m_buffer));
                    SecureZeroMemory(&pdi->m_overlapped, sizeof(pdi->m_overlapped));
                    if (!ReadDirectoryChangesW(pdi->m_hDir,
                                               pdi->m_buffer,
                                               READ_DIR_CHANGE_BUFFER_SIZE,
                                               pdi->m_recursive ? TRUE : FALSE,
                                               FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
                                               &numBytes, // not used
                                               &pdi->m_overlapped,
                                               nullptr)) //no completion routine!
                    {
                        // Since the call to ReadDirectoryChangesW failed, just
                        // wait a while. We don't want to have this thread
                        // running using 100% CPU if something goes completely
                        // wrong.
                        Sleep(200);
                    }
                }
            }
        } // if (!watchedPaths.empty())
        else
            Sleep(200);
    } // while (m_bRunning)
}

void CPathWatcher::ClearInfoMap()
{
    if (!m_watchInfoMap.empty())
    {
        std::unique_lock locker(m_guard);
        m_watchInfoMap.clear();
    }
    m_hCompPort.CloseHandle();
}

std::vector<std::tuple<DWORD, std::wstring>> CPathWatcher::GetChangedPaths()
{
    std::unique_lock locker(m_guard);
    auto             ret = m_changedPaths;
    m_changedPaths.clear();
    return ret;
}

CPathWatcher::CDirWatchInfo::CDirWatchInfo(CAutoFile&& hDir, const std::wstring& directoryName, bool recursive)
    : m_hDir(std::move(hDir))
    , m_dirName(directoryName)
    , m_recursive(recursive)
{
    m_dirPath = m_dirName;
    if (m_dirPath.at(m_dirPath.size() - 1) != '\\')
        m_dirPath += L"\\";
}

CPathWatcher::CDirWatchInfo::~CDirWatchInfo()
{
    CloseDirectoryHandle();
}

bool CPathWatcher::CDirWatchInfo::CloseDirectoryHandle()
{
    return m_hDir.CloseHandle();
}
