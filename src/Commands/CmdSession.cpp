// This file is part of BowPad.
//
// Copyright (C) 2014-2018, 2020 - Stefan Kueng
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
#include "ICommand.h"
#include "BowPadUI.h"
#include "BowPad.h"

#include "CmdSession.h"

#include "StringUtils.h"
#include "PathUtils.h"
#include "IniSettings.h"
#include "CmdLineParser.h"
#include "OnOutOfScope.h"
#include "ResString.h"
#include "ProgressDlg.h"
#include "SysInfo.h"
#include "AppUtils.h"
#include <sstream>
#include <string>
#include <vector>
#include <iterator>
#include <iostream>

namespace
{
const wchar_t g_sessionSection[]         = {L"TabSession"};
const wchar_t g_sessionSectionElevated[] = {L"TabSessionElevated"};
const int     BP_DEFAULT_SESSION_SIZE    = 500;
}; // namespace

static const wchar_t* sessionSection()
{
    if (SysInfo::Instance().IsUACEnabled() && SysInfo::Instance().IsElevated())
        return g_sessionSectionElevated;
    return g_sessionSection;
}

static void SetAutoLoad(bool bAutoLoad)
{
    CIniSettings::Instance().SetInt64(sessionSection(), L"autoload", bAutoLoad);
}

static bool GetAutoLoad()
{
    bool bAutoLoad = CIniSettings::Instance().GetInt64(sessionSection(), L"autoload", 0) != 0;
    return bAutoLoad;
}

static std::wstring GetBackupPath()
{
    auto handleModified = CIniSettings::Instance().GetInt64(sessionSection(), L"handlemodified", 1) != 0;
    handleModified      = handleModified && GetAutoLoad();
    auto sessionPath    = CAppUtils::GetDataPath() + L"\\BowPad\\backup";
    CPathUtils::CreateRecursiveDirectory(sessionPath);
    if (handleModified)
        return sessionPath;
    return {};
}

bool CCmdSessionLoad::Execute()
{
    RestoreSavedSession();
    return true;
}

void CCmdSessionLoad::OnClose()
{
    // BowPad is closing, save the current session
    if (!firstInstance)
        return;

    auto& settings       = CIniSettings::Instance();
    bool  bAutoLoad      = GetAutoLoad();
    auto  handleModified = CIniSettings::Instance().GetInt64(sessionSection(), L"handlemodified", 1) != 0;
    // first remove the whole session section
    settings.Delete(sessionSection(), nullptr);
    // now restore the settings of the session section
    SetAutoLoad(bAutoLoad);
    CIniSettings::Instance().SetInt64(sessionSection(), L"handlemodified", handleModified ? 1 : 0);
    // now go through all tabs and save their state
    int tabcount  = GetTabCount();
    int activetab = GetActiveTabIndex();
    int saveindex = 0;

    auto SavePosSettings = [&](int saveindex, const CPosData& pos) {
        settings.SetInt64(sessionSection(), CStringUtils::Format(L"selmode%d", saveindex).c_str(), pos.m_nSelMode);
        settings.SetInt64(sessionSection(), CStringUtils::Format(L"startpos%d", saveindex).c_str(), pos.m_nStartPos);
        settings.SetInt64(sessionSection(), CStringUtils::Format(L"endpos%d", saveindex).c_str(), pos.m_nEndPos);
        settings.SetInt64(sessionSection(), CStringUtils::Format(L"scrollwidth%d", saveindex).c_str(), pos.m_nScrollWidth);
        settings.SetInt64(sessionSection(), CStringUtils::Format(L"xoffset%d", saveindex).c_str(), pos.m_xOffset);
        settings.SetInt64(sessionSection(), CStringUtils::Format(L"firstvisible%d", saveindex).c_str(), pos.m_nFirstVisibleLine);
        settings.SetInt64(sessionSection(), CStringUtils::Format(L"wraplineoffset%d", saveindex).c_str(), pos.m_nWrapLineOffset);
        settings.SetInt64(sessionSection(), CStringUtils::Format(L"laststyleline%d", saveindex).c_str(), pos.m_lastStyleLine);
        std::wostringstream out;
        if (!pos.m_lineStateVector.empty())
        {
            std::copy(pos.m_lineStateVector.begin(), pos.m_lineStateVector.end() - 1, std::ostream_iterator<size_t, wchar_t>(out, L";"));
            out << pos.m_lineStateVector.back();
        }
        std::wstring result(out.str());
        settings.SetString(sessionSection(), CStringUtils::Format(L"foldlines%d", saveindex).c_str(), result.c_str());
    };

    auto sessionPath = GetBackupPath();

    int sessionSize = (int)settings.GetInt64(sessionSection(), L"session_size", BP_DEFAULT_SESSION_SIZE);
    // No point saving more than we are prepared to load.
    int savecount = min(tabcount, sessionSize);
    for (int i = 0; i < savecount; ++i)
    {
        auto  docId = GetDocIDFromTabIndex(i);
        auto& doc   = GetModDocumentFromID(docId);
        if (doc.m_path.empty())
        {
            if (sessionPath.empty())
                continue;
        }
        if (i == activetab)
        {
            CPosData pos;
            SaveCurrentPos(pos);

            settings.SetInt64(sessionSection(), CStringUtils::Format(L"activetab%d", saveindex).c_str(), 1);
            SavePosSettings(saveindex, pos);
        }
        else
        {
            SavePosSettings(saveindex, doc.m_position);
            settings.SetInt64(sessionSection(), CStringUtils::Format(L"activetab%d", saveindex).c_str(), 0);
        }

        if (!sessionPath.empty() && (doc.m_bIsDirty || doc.m_bNeedsSaving))
        {
            auto filename = CPathUtils::GetFileName(doc.m_path);
            auto title    = GetTitleForTabIndex(i);
            if (doc.m_path.empty())
            {
                filename = title;
            }
            auto backupPath = CStringUtils::Format(L"%s\\%d%s", sessionPath.c_str(), saveindex, filename.c_str());
            settings.SetString(sessionSection(), CStringUtils::Format(L"origpath%d", saveindex).c_str(), doc.m_path.c_str());
            settings.SetString(sessionSection(), CStringUtils::Format(L"origtitle%d", saveindex).c_str(), title.c_str());
            doc.m_path         = backupPath;
            doc.m_bIsDirty     = false;
            doc.m_bNeedsSaving = false;
            SaveDoc(docId, backupPath);
        }
        settings.SetString(sessionSection(), CStringUtils::Format(L"path%d", saveindex).c_str(), doc.m_path.c_str());
        settings.SetInt64(sessionSection(), CStringUtils::Format(L"tabspace%d", saveindex).c_str(), (int)doc.m_TabSpace);
        settings.SetInt64(sessionSection(), CStringUtils::Format(L"readdir%d", saveindex).c_str(), (int)doc.m_ReadDir);

        ++saveindex;
    }
}

void CCmdSessionLoad::RestoreSavedSession()
{
    ProfileTimer profile(L"RestoreSavedSession");
    auto&        settings          = CIniSettings::Instance();
    int          numFilesToRestore = 0;

    int sessionSize = (int)settings.GetInt64(sessionSection(), L"session_size", BP_DEFAULT_SESSION_SIZE);
    for (int fileNum = 0; fileNum < sessionSize; ++fileNum)
    {
        std::wstring key  = CStringUtils::Format(L"path%d", fileNum);
        std::wstring path = settings.GetString(sessionSection(), key.c_str(), L"");
        if (path.empty())
            break;
        ++numFilesToRestore;
    }
    if (numFilesToRestore == 0)
        return;

    // Block the UI to avoid excessive drawing/painting if loading more than
    // one file. Don't block the UI when loading one or less files as that
    // itself would lead to excessive repainting.
    OnOutOfScope(
        if (numFilesToRestore > 1) {
            BlockAllUIUpdates(false);
            HideProgressCtrl();
        });
    if (numFilesToRestore > 1)
    {
        BlockAllUIUpdates(true);
        ShowProgressCtrl((UINT)settings.GetInt64(L"View", L"progressdelay", 1000));
    }

    DocID                     activeDoc;
    const unsigned int        openflags   = OpenFlags::IgnoreIfMissing | OpenFlags::NoActivate;
    int                       filecount   = 0;
    auto                      sessionPath = GetBackupPath();
    std::vector<std::wstring> filesToDelete;
    for (int fileNum = 0; fileNum < sessionSize; ++fileNum)
    {
        std::wstring key  = CStringUtils::Format(L"path%d", fileNum);
        std::wstring path = settings.GetString(sessionSection(), key.c_str(), L"");
        if (path.empty())
            break;
        ++filecount;
        SetProgress(filecount, numFilesToRestore);
        int tabIndex = OpenFile(path.c_str(), openflags);
        if (tabIndex < 0)
            continue;
        // Don't use the index to track the active tab, as it's probably
        // not safe long term to assume the index where a tab was loaded
        // remains the same after other files load.
        auto docId = GetDocIDFromTabIndex(tabIndex);
        if (!docId.IsValid())
            continue;
        auto& doc               = GetModDocumentFromID(docId);
        auto& pos               = doc.m_position;
        pos.m_nSelMode          = (size_t)settings.GetInt64(sessionSection(), CStringUtils::Format(L"selmode%d", fileNum).c_str(), 0);
        pos.m_nStartPos         = (size_t)settings.GetInt64(sessionSection(), CStringUtils::Format(L"startpos%d", fileNum).c_str(), 0);
        pos.m_nEndPos           = (size_t)settings.GetInt64(sessionSection(), CStringUtils::Format(L"endpos%d", fileNum).c_str(), 0);
        pos.m_nScrollWidth      = (size_t)settings.GetInt64(sessionSection(), CStringUtils::Format(L"scrollwidth%d", fileNum).c_str(), 0);
        pos.m_xOffset           = (size_t)settings.GetInt64(sessionSection(), CStringUtils::Format(L"xoffset%d", fileNum).c_str(), 0);
        pos.m_nFirstVisibleLine = (size_t)settings.GetInt64(sessionSection(), CStringUtils::Format(L"firstvisible%d", fileNum).c_str(), 0);
        pos.m_nWrapLineOffset   = (size_t)settings.GetInt64(sessionSection(), CStringUtils::Format(L"wraplineoffset%d", fileNum).c_str(), 0);
        pos.m_lastStyleLine     = (size_t)settings.GetInt64(sessionSection(), CStringUtils::Format(L"laststyleline%d", fileNum).c_str(), 0);
        doc.m_TabSpace          = (TabSpace)settings.GetInt64(sessionSection(), CStringUtils::Format(L"tabspace%d", fileNum).c_str(), 0);
        doc.m_ReadDir           = (ReadDirection)settings.GetInt64(sessionSection(), CStringUtils::Format(L"readdir%d", fileNum).c_str(), 0);
        auto folds              = settings.GetString(sessionSection(), CStringUtils::Format(L"foldlines%d", fileNum).c_str(), 0);
        pos.m_lineStateVector.clear();
        if (folds)
        {
            stringtok(pos.m_lineStateVector, folds, true, L";", false);
        }
        auto origPath = settings.GetString(sessionSection(), CStringUtils::Format(L"origpath%d", fileNum).c_str(), nullptr);
        if (origPath)
        {
            filesToDelete.push_back(doc.m_path);
            doc.m_path         = origPath;
            doc.m_bIsDirty     = true;
            doc.m_bNeedsSaving = true;
            auto origTitle     = settings.GetString(sessionSection(), CStringUtils::Format(L"origtitle%d", fileNum).c_str(), nullptr);
            SetTitleForDocID(docId, origTitle);
            UpdateFileTime(doc, true);
            UpdateTab(GetTabIndexFromDocID(docId));
            settings.Delete(sessionSection(), CStringUtils::Format(L"origpath%d", fileNum).c_str());
            settings.SetString(sessionSection(), CStringUtils::Format(L"path%d", fileNum).c_str(), doc.m_path.c_str());
        }

        if ((int)settings.GetInt64(sessionSection(), CStringUtils::Format(L"activetab%d", fileNum).c_str(), 0))
            activeDoc = docId;
        RestoreCurrentPos(doc.m_position);
    }
    if (activeDoc.IsValid())
    {
        int activeTabIndex = GetTabIndexFromDocID(activeDoc);
        TabActivateAt(activeTabIndex);
    }

    for (const auto& path : filesToDelete)
    {
        if (!path.empty())
            DeleteFile(path.c_str());
    }
    if (!filesToDelete.empty())
    {
        settings.Save();
    }
}

CCmdSessionAutoLoad::CCmdSessionAutoLoad(void* obj)
    : CCmdSessionLoad(obj)
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
}

bool CCmdSessionAutoLoad::Execute()
{
    SetAutoLoad(!GetAutoLoad());
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
    InvalidateUICommand(cmdSessionAutoSave, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
    return true;
}

HRESULT CCmdSessionAutoLoad::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key,
                                                             const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, GetAutoLoad(), ppropvarNewValue);
    }
    else if (UI_PKEY_Enabled == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, firstInstance, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

void CCmdSessionAutoLoad::BeforeLoad()
{
    if (GetAutoLoad())
    {
        if (firstInstance)
            RestoreSavedSession();
    }
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

CCmdSessionAutoSave::CCmdSessionAutoSave(void* obj)
    : CCmdSessionLoad(obj)
{
}

bool CCmdSessionAutoSave::Execute()
{
    auto handleModified = CIniSettings::Instance().GetInt64(sessionSection(), L"handlemodified", 1) != 0;
    handleModified      = !handleModified && GetAutoLoad();
    CIniSettings::Instance().SetInt64(sessionSection(), L"handlemodified", handleModified ? 1 : 0);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    return true;
}

HRESULT CCmdSessionAutoSave::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key,
                                                             const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        auto handleModified = CIniSettings::Instance().GetInt64(sessionSection(), L"handlemodified", 1) != 0;
        handleModified      = handleModified && GetAutoLoad();
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, handleModified, ppropvarNewValue);
    }
    else if (UI_PKEY_Enabled == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, GetAutoLoad() && firstInstance, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

bool CCmdSessionRestoreLast::Execute()
{
    if (m_docstates.empty())
        return false;
    const SessionItem si  = m_docstates.back();
    const auto&       pos = si.posData;
    if (OpenFile(si.path.c_str(), OpenFlags::AddToMRU))
        RestoreCurrentPos(pos);
    m_docstates.pop_back();
    return true;
}

void CCmdSessionRestoreLast::OnDocumentClose(DocID id)
{
    const auto& doc = GetDocumentFromID(id);
    if (doc.m_path.empty())
        return;

    auto activeTabIndex = GetActiveTabIndex();
    auto index          = GetTabIndexFromDocID(id);
    if (index == activeTabIndex)
    {
        CPosData pos;
        SaveCurrentPos(pos);
        m_docstates.emplace_back(doc.m_path, pos);
    }
    else
    {
        m_docstates.emplace_back(doc.m_path, doc.m_position);
    }
}
