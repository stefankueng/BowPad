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
#pragma once
#include "SmartHandle.h"
#include <string>
#include <set>
#include <map>
#include <mutex>
#include <vector>

constexpr auto READ_DIR_CHANGE_BUFFER_SIZE = 4096;
constexpr auto MAX_CHANGED_PATHS           = 4000;

/**
 * \ingroup Utils
 * Watches the file system for changes.
 *
 * When a CPathWatcher object is created, a new thread is started which
 * waits for file system change notifications.
 * To add folders to the list of watched folders, call \c AddPath().
 */
class CPathWatcher
{
public:
    CPathWatcher();
    ~CPathWatcher();

    /**
     * Adds a new path to be watched. The path \b must point to a directory.
     * If the path is already watched because a parent of that path is already
     * watched recursively, then the new path is just ignored and the method
     * returns false.
     */
    bool AddPath(const std::wstring& path, bool recursive = false);

    /**
     * Removes a path and all its children from the watched list.
     */
    bool RemovePath(const std::wstring& path);

    /**
     * Removes all watched paths
     */
    void ClearPaths(bool includeChangedPaths);

    /**
     * Returns the number of recursively watched paths.
     */
    size_t GetNumberOfWatchedPaths() const { return watchedPaths.size(); }

    /**
     * Returns all changed paths since the last call to GetChangedPaths
     */
    std::vector<std::tuple<DWORD, std::wstring>> GetChangedPaths();

    /**
     * Stops the watching thread.
     */
    void Stop();

private:
    static unsigned int __stdcall ThreadEntry(void* pContext);
    void WorkerThread();

    void ClearInfoMap();

private:
    std::recursive_mutex m_guard{};
    CAutoGeneralHandle   m_hThread;
    CAutoGeneralHandle   m_hCompPort;
    volatile LONG        m_bRunning = FALSE;

    std::map<std::wstring, bool> watchedPaths; ///< list of watched paths.

    /**
     * Helper class: provides information about watched directories.
     */
    class CDirWatchInfo
    {
    private:
        CDirWatchInfo()                       = delete;
        CDirWatchInfo(const CDirWatchInfo& i) = delete;
        CDirWatchInfo& operator=(const CDirWatchInfo& rhs) = delete;

    public:
        CDirWatchInfo(CAutoFile&& hDir, const std::wstring& directoryName, bool recursive);
        ~CDirWatchInfo();

    public:
        bool CloseDirectoryHandle();

        CAutoFile    m_hDir;                                  ///< handle to the directory that we're watching
        std::wstring m_dirName;                               ///< the directory that we're watching
        CHAR         m_buffer[READ_DIR_CHANGE_BUFFER_SIZE]{}; ///< buffer for ReadDirectoryChangesW
        std::wstring m_dirPath;                               ///< the directory name we're watching with a backslash at the end
        bool         m_recursive = false;                     ///< if the path is watched for changes including all subfolders
        OVERLAPPED   m_overlapped{};
    };

    std::map<HANDLE, std::unique_ptr<CDirWatchInfo>> m_watchInfoMap;

    HDEVNOTIFY                                   m_hDev = nullptr;
    std::vector<std::tuple<DWORD, std::wstring>> m_changedPaths;
};
