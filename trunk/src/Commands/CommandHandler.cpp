// This file is part of BowPad.
//
// Copyright (C) 2013-2017 - Stefan Kueng
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

#include "CommandHandler.h"

#include "CmdMRU.h"
#include "CmdFiles.h"
#include "CmdUndo.h"
#include "CmdClipboard.h"
#include "CmdLineWrap.h"
#include "CmdWhiteSpace.h"
#include "CmdLoadEncoding.h"
#include "CmdCodeStyle.h"
#include "CmdEOL.h"
#include "CmdPrevNext.h"
#include "CmdFindReplace.h"
#include "CmdMisc.h"
#include "CmdStyleConfigurator.h"
#include "CmdVerticalEdge.h"
#include "CmdComment.h"
#include "CmdConvertCase.h"
#include "CmdLines.h"
#include "CmdPrint.h"
#include "CmdGotoLine.h"
#include "CmdGotoSymbol.h"
#include "CmdSelectTab.h"
#include "CmdBlanks.h"
#include "CmdZoom.h"
#include "CmdBookmarks.h"
#include "CmdRandom.h"
#include "CmdLanguage.h"
#include "CmdHeaderSource.h"
#include "CmdLaunch.h"
#include "CmdSession.h"
#include "CmdFunctions.h"
#include "CmdFont.h"
#include "CmdOpenSelection.h"
#include "CmdNewCopy.h"
#include "CmdDefaultEncoding.h"
#include "CmdScripts.h"
#include "CmdSpellcheck.h"
#include "CmdPlugins.h"
#include "CmdPluginsConfig.h"
#include "CmdTabList.h"
#include "CmdSort.h"
#include "CmdSummary.h"
#include "CmdLineNumbers.h"
#include "CmdFolding.h"

#include "DirFileEnum.h"
#include "AppUtils.h"
#include "PathUtils.h"
#include "UnicodeUtils.h"
#include "KeyboardShortcutHandler.h"

CCommandHandler::CCommandHandler()
    : m_highestCmdId(0)
{
}

std::unique_ptr<CCommandHandler> CCommandHandler::m_instance = nullptr;

CCommandHandler& CCommandHandler::Instance()
{
    if (m_instance == nullptr)
        m_instance.reset(new CCommandHandler());
    return *m_instance.get();
}

void CCommandHandler::ShutDown()
{
    m_instance.reset(nullptr);
}


ICommand * CCommandHandler::GetCommand( UINT cmdId )
{
    auto c = m_commands.find(cmdId);
    if (c != m_commands.end())
        return c->second.get();
    auto nc = m_nodeletecommands.find(cmdId);
    if (nc != m_nodeletecommands.end())
        return nc->second;
    return nullptr;
}

void CCommandHandler::Init(void * obj)
{
    Add<CCmdMRU>(obj);
    Add<CCmdToggleTheme>(obj);
    Add<CCmdOpen>(obj);
    Add<CCmdSave>(obj);
    Add<CCmdSaveAll>(obj);
    Add<CCmdSaveAuto>(obj);
    Add<CCmdSaveAs>(obj);
    Add<CCmdReload>(obj);
    Add<CCmdWriteProtect>(obj);
    Add<CCmdSummary>(obj);
    Add<CCmdFileDelete>(obj);
    Add<CCmdPrint>(obj);
    Add<CCmdPrintNow>(obj);
    Add<CCmdPageSetup>(obj);
    Add<CCmdSessionLoad>(obj);
    Add<CCmdSessionAutoLoad>(obj);
    Add<CCmdSessionRestoreLast>(obj);
    Add<CCmdUndo>(obj);
    Add<CCmdRedo>(obj);
    Add<CCmdCut>(obj);
    Add<CCmdCutPlain>(obj);
    Add<CCmdCopy>(obj);
    Add<CCmdCopyPlain>(obj);
    Add<CCmdPaste>(obj);
    Add<CCmdDelete>(obj);
    Add<CCmdSelectAll>(obj);
    Add<CCmdGotoBrace>(obj);
    Add<CCmdConfigShortcuts>(obj);
    Add<CCmdLineWrap>(obj);
    Add<CCmdWhiteSpace>(obj);
    Add<CCmdLineNumbers>(obj);
    Add<CCmdUseTabs>(obj);
    Add<CCmdAutoBraces>(obj);
    Add<CCmdViewFileTree>(obj);
    Add<CCmdLanguage>(obj);
    Add<CCmdTabSize>(obj);
    Add<CCmdLoadAsEncoded>(obj);
    Add<CCmdConvertEncoding>(obj);
    Add<CCmdCodeStyle>(obj);
    Add<CCmdStyleConfigurator>(obj);

    Add<CCmdEOLWin>(obj);
    Add<CCmdEOLUnix>(obj);
    Add<CCmdEOLMac>(obj);

    Add<CCmdPrevNext>(obj);
    Add<CCmdPrevious>(obj);
    Add<CCmdNext>(obj);
    Add<CCmdTabList>(obj);

    Add<CCmdFindReplace>(obj);
    Add<CCmdFindNext>(obj);
    Add<CCmdFindPrev>(obj);
    Add<CCmdFindSelectedNext>(obj);
    Add<CCmdFindSelectedPrev>(obj);
    Add<CCmdFindFile>(obj);
    Add<CCmdFunctions>(obj);
    Add<CCmdGotoLine>(obj);
    Add<CCmdGotoSymbol>(obj);

    Add<CCmdBookmarks>(obj);
    Add<CCmdBookmarkToggle>(obj);
    Add<CCmdBookmarkClearAll>(obj);
    Add<CCmdBookmarkNext>(obj);
    Add<CCmdBookmarkPrev>(obj);

    Add<CCmdVerticalEdge>(obj);
    Add<CCmdFont>(obj);

    Add<CCmdComment>(obj);
    Add<CCmdUnComment>(obj);
    Add<CCmdConvertUppercase>(obj);
    Add<CCmdConvertLowercase>(obj);
    Add<CCmdConvertTitlecase>(obj);

    Add<CCmdLineDuplicate>(obj);
    Add<CCmdLineSplit>(obj);
    Add<CCmdLineJoin>(obj);
    Add<CCmdLineUp>(obj);
    Add<CCmdLineDown>(obj);
    Add<CCmdSort>(obj);
    Add<CCmdInitFoldingMargin>(obj);
    Add<CCmdFoldingOn>(obj);
    Add<CCmdFoldingOff>(obj);
    Add<CCmdFoldAll>(obj);

    for (int i = 0; i < 10; ++i)
    {
        Add<CCmdFoldLevel>(i, obj);
    }

    Add<CCmdTrim>(obj);
    Add<CCmdTabs2Spaces>(obj);
    Add<CCmdSpaces2Tabs>(obj);

    Add<CCmdSelectTab>(obj);
    Add<CCmdZoom100>(obj);
    Add<CCmdZoomIn>(obj);
    Add<CCmdZoomOut>(obj);

    Add<CCmdNewCopy>(obj);
    Add<CCmdDefaultEncoding>(obj);

    Add<CCmdHeaderSource>(obj);
    Add<CCmdOpenSelection>(obj);

    Add<CCmdSpellcheck>(obj);
    Add<CCmdSpellcheckLang>(obj);
    Add<CCmdSpellcheckCorrect>(obj);
    Add<CCmdSpellcheckAll>(obj);
    Add<CCmdSpellcheckUpper>(obj);

    Add<CCmdLaunchIE>(obj);
    Add<CCmdLaunchFirefox>(obj);
    Add<CCmdLaunchChrome>(obj);
    Add<CCmdLaunchSafari>(obj);
    Add<CCmdLaunchOpera>(obj);
    Add<CCmdLaunchSearch>(obj);
    Add<CCmdLaunchWikipedia>(obj);
    Add<CCmdLaunchConsole>(obj);
    Add<CCmdLaunchExplorer>(obj);
    Add<CCmdPlugins>(obj);
    Add<CCmdPluginsConfig>(obj);

    for (int i = 0; i < 10; ++i)
    {
        Add<CCmdLaunchCustom>(i, obj);
    }
    Add<CCmdCustomCommands>(obj);

    Add<CCmdRandom>(obj);

    InsertPlugins(obj);
}

void CCommandHandler::ScintillaNotify( SCNotification * pScn )
{
    for (auto& cmd : m_commands)
    {
        cmd.second->ScintillaNotify(pScn);
    }
    for (auto& cmd : m_nodeletecommands)
    {
        cmd.second->ScintillaNotify(pScn);
    }
}

void CCommandHandler::TabNotify( TBHDR * ptbhdr )
{
    for (auto& cmd:m_commands)
    {
        cmd.second->TabNotify(ptbhdr);
    }
    for (auto& cmd : m_nodeletecommands)
    {
        cmd.second->TabNotify(ptbhdr);
    }
}

void CCommandHandler::OnClose()
{
    for (auto& cmd : m_commands)
    {
        cmd.second->OnClose();
    }
    for (auto& cmd : m_nodeletecommands)
    {
        cmd.second->OnClose();
    }
}

void CCommandHandler::OnDocumentClose(DocID id)
{
    for (auto& cmd : m_commands)
    {
        cmd.second->OnDocumentClose(id);
    }
    for (auto& cmd : m_nodeletecommands)
    {
        cmd.second->OnDocumentClose(id);
    }
}

void CCommandHandler::OnDocumentOpen(DocID id)
{
    for (auto& cmd : m_commands)
    {
        cmd.second->OnDocumentOpen(id);
    }
    for (auto& cmd : m_nodeletecommands)
    {
        cmd.second->OnDocumentOpen(id);
    }
}

void CCommandHandler::OnDocumentSave(DocID id, bool bSaveAs)
{
    for (auto& cmd : m_commands)
    {
        cmd.second->OnDocumentSave(id, bSaveAs);
    }
    for (auto& cmd : m_nodeletecommands)
    {
        cmd.second->OnDocumentSave(id, bSaveAs);
    }
}

void CCommandHandler::AfterInit()
{
    for (auto& cmd : m_commands)
    {
        cmd.second->AfterInit();
    }
    for (auto& cmd : m_nodeletecommands)
    {
        cmd.second->AfterInit();
    }
}

void CCommandHandler::OnTimer(UINT id)
{
    for (auto& cmd : m_commands)
    {
        cmd.second->OnTimer(id);
    }
    for (auto& cmd : m_nodeletecommands)
    {
        cmd.second->OnTimer(id);
    }
}

void CCommandHandler::OnThemeChanged(bool bDark)
{
    for (auto& cmd : m_commands)
    {
        cmd.second->OnThemeChanged(bDark);
    }
    for (auto& cmd : m_nodeletecommands)
    {
        cmd.second->OnThemeChanged(bDark);
    }
}

void CCommandHandler::OnLangChanged()
{
    for (auto& cmd : m_commands)
    {
        cmd.second->OnLangChanged();
    }
    for (auto& cmd : m_nodeletecommands)
    {
        cmd.second->OnLangChanged();
    }
}

void CCommandHandler::InsertPlugins(void * obj)
{
    // scan the paths, find all plugin files, create a plugin object
    // for every found file and store the plugin for later use
    std::wstring sPluginDir = CAppUtils::GetDataPath();
    sPluginDir += L"\\plugins";
    CDirFileEnum filefinder(sPluginDir);
    bool bIsDirectory;
    std::wstring filename;
    UINT pluginCmd = m_highestCmdId + 1000;
    while (filefinder.NextFile(filename, &bIsDirectory, true))
    {
        if (!bIsDirectory)
        {
            try
            {
                auto pScript = std::make_unique<CCmdScript>(obj);
                int cmdID = ++pluginCmd;
                if (pScript->Create(filename))
                {
                    pScript->SetCmdId(cmdID);
                    std::wstring sName = CPathUtils::GetParentDirectory(filename);
                    sName = CPathUtils::GetFileName(sName);
                    m_pluginversion.emplace(sName, pScript->m_version);
                    m_commands.emplace(cmdID, std::move(pScript));
                    m_plugins.emplace(cmdID, sName);
                    CKeyboardShortcutHandler::Instance().AddCommand(sName, pluginCmd);
                }
            }
            catch (const std::exception& e)
            {
                if (CIniSettings::Instance().GetInt64(L"Debug", L"usemessagebox", 0))
                {
                    MessageBox(nullptr, L"BowPad", CUnicodeUtils::StdGetUnicode(e.what()).c_str(), MB_ICONERROR);
                }
                else
                {
                    CTraceToOutputDebugString::Instance()(L"BowPad : ");
                    CTraceToOutputDebugString::Instance()(e.what());
                    CTraceToOutputDebugString::Instance()(L"\n");
                }
            }
        }
    }
}

int CCommandHandler::GetPluginVersion(const std::wstring& name)
{
    auto it = m_pluginversion.find(name);
    if (it != m_pluginversion.end())
        return it->second;
    return 0;
}

void CCommandHandler::AddCommand(ICommand * cmd)
{
    m_highestCmdId = max(m_highestCmdId, cmd->GetCmdId());
    auto at = m_nodeletecommands.emplace(cmd->GetCmdId(), cmd);
    assert(at.second); // Verify no command has the same ID as an existing command.
}
