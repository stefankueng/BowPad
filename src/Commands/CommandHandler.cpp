// This file is part of BowPad.
//
// Copyright (C) 2013-2018, 2020-2022 - Stefan Kueng
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

#include "CmdBlanks.h"
#include "CmdBookmarks.h"
#include "CmdClipboard.h"
#include "CmdCodeStyle.h"
#include "CmdComment.h"
#include "CmdConvertCase.h"
#include "CmdDefaultEncoding.h"
#include "CmdEditSelection.h"
#include "CmdEOL.h"
#include "CmdFiles.h"
#include "CmdFindReplace.h"
#include "CmdFolding.h"
#include "CmdFont.h"
#include "CmdFunctions.h"
#include "CmdGotoLine.h"
#include "CmdGotoSymbol.h"
#include "CmdHeaderSource.h"
#include "CmdLanguage.h"
#include "CmdLaunch.h"
#include "CmdLineNumbers.h"
#include "CmdLines.h"
#include "CmdLineWrap.h"
#include "CmdLoadEncoding.h"
#include "CmdMisc.h"
#include "CmdMRU.h"
#include "CmdNewCopy.h"
#include "CmdOpenSelection.h"
#include "CmdPlugins.h"
#include "CmdPluginsConfig.h"
#include "CmdPrevNext.h"
#include "CmdPrint.h"
#include "CmdRandom.h"
#include "CmdRegexCapture.h"
#include "CmdScripts.h"
#include "CmdSelectTab.h"
#include "CmdSession.h"
#include "CmdSort.h"
#include "CmdSpellcheck.h"
#include "CmdStyleConfigurator.h"
#include "CmdSummary.h"
#include "CmdTabList.h"
#include "CmdUndo.h"
#include "CmdVerticalEdge.h"
#include "CmdWhiteSpace.h"
#include "CmdZoom.h"

#include "DirFileEnum.h"
#include "AppUtils.h"
#include "PathUtils.h"
#include "UnicodeUtils.h"
#include "KeyboardShortcutHandler.h"
#include "IniSettings.h"

#include <fstream>

CCommandHandler::CCommandHandler()
    : m_highestCmdId(0)
{
}

std::unique_ptr<CCommandHandler> CCommandHandler::m_instance = nullptr;

CCommandHandler&                 CCommandHandler::Instance()
{
    if (m_instance == nullptr)
        m_instance.reset(new CCommandHandler());
    return *m_instance.get();
}

void CCommandHandler::ShutDown()
{
    m_instance.reset(nullptr);
}

ICommand* CCommandHandler::GetCommand(UINT cmdId)
{
    auto c = m_commands.find(cmdId);
    if (c != m_commands.end())
        return c->second.get();
    auto nc = m_noDeleteCommands.find(cmdId);
    if (nc != m_noDeleteCommands.end())
        return nc->second;
    return nullptr;
}

void CCommandHandler::Init(void* obj)
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
    Add<CCmdSessionAutoSave>(obj);
    Add<CCmdSessionRestoreLast>(obj);
    Add<CCmdUndo>(obj);
    Add<CCmdRedo>(obj);
    Add<CCmdCut>(obj);
    Add<CCmdCutPlain>(obj);
    Add<CCmdCopy>(obj);
    Add<CCmdCopyPlain>(obj);
    Add<CCmdPaste>(obj);
    Add<CCmdPasteHtml>(obj);
    Add<CCmdDelete>(obj);
    Add<CCmdSelectAll>(obj);
    Add<CCmdGotoBrace>(obj);
    Add<CCmdConfigShortcuts>(obj);
    Add<CCmdLineWrap>(obj);
    Add<CCmdLineWrapIndent>(obj);
    Add<CCmdWhiteSpace>(obj);
    Add<CCmdLineNumbers>(obj);
    Add<CCmdUseTabs>(obj);
    Add<CCmdAutoBraces>(obj);
    Add<CCmdAutoComplete>(obj);
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
    Add<CCmdRegexCapture>(obj);

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
    Add<CCmdEditSelection>(obj);
    Add<CCmdRemoveEmptyLines>(obj);
    Add<CCmdEditEndOfSelectedLines>(obj);
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

    Add<CCmdSpellCheck>(obj);
    Add<CCmdSpellCheckLang>(obj);
    Add<CCmdSpellCheckCorrect>(obj);
    Add<CCmdSpellCheckAll>(obj);
    Add<CCmdSpellCheckUpper>(obj);

    Add<CCmdLaunchEdge>(obj);
    Add<CCmdLaunchIe>(obj);
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

void CCommandHandler::ScintillaNotify(SCNotification* pScn)
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->ScintillaNotify(pScn);
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->ScintillaNotify(pScn);
    }
}

void CCommandHandler::TabNotify(TBHDR* ptbHdr)
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->TabNotify(ptbHdr);
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->TabNotify(ptbHdr);
    }
}

void CCommandHandler::OnClose()
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnClose();
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnClose();
    }
}

void CCommandHandler::OnDocumentClose(DocID docId)
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnDocumentClose(docId);
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnDocumentClose(docId);
    }
}

void CCommandHandler::OnDocumentOpen(DocID docId)
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnDocumentOpen(docId);
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnDocumentOpen(docId);
    }
}

void CCommandHandler::OnBeforeDocumentSave(DocID docId)
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnBeforeDocumentSave(docId);
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnBeforeDocumentSave(docId);
    }
}

void CCommandHandler::OnDocumentSave(DocID docId, bool bSaveAs)
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnDocumentSave(docId, bSaveAs);
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnDocumentSave(docId, bSaveAs);
    }
}

void CCommandHandler::OnClipboardChanged()
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnClipboardChanged();
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnClipboardChanged();
    }
}

void CCommandHandler::BeforeLoad()
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->BeforeLoad();
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->BeforeLoad();
    }
}

void CCommandHandler::AfterInit()
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->AfterInit();
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->AfterInit();
    }
}

void CCommandHandler::OnTimer(UINT timerId)
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnTimer(timerId);
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnTimer(timerId);
    }
}

void CCommandHandler::OnThemeChanged(bool bDark)
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnThemeChanged(bDark);
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnThemeChanged(bDark);
    }
}

void CCommandHandler::OnLangChanged()
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnLangChanged();
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnLangChanged();
    }
}

void CCommandHandler::OnStylesSet()
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnStylesSet();
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnStylesSet();
    }
}

void CCommandHandler::InsertPlugins(void* obj)
{
    // scan the paths, find all plugin files, create a plugin object
    // for every found file and store the plugin for later use
    std::wstring sPluginDir = CAppUtils::GetDataPath();
    sPluginDir += L"\\plugins";
    CDirFileEnum                                        fileFinder(sPluginDir);
    bool                                                bIsDirectory;
    std::wstring                                        fileName;

    std::map<std::wstring, std::unique_ptr<CCmdScript>> scripts;
    while (fileFinder.NextFile(fileName, &bIsDirectory, true))
    {
        if (!bIsDirectory)
        {
            if (fileName.ends_with(L"bpj") || fileName.ends_with(L"bpv"))
            {
                try
                {
                    auto pScript = std::make_unique<CCmdScript>(obj, CPathUtils::GetParentDirectory(fileName));
                    if (pScript->Create(fileName))
                    {
                        try
                        {
                            auto          descPath = CPathUtils::GetParentDirectory(fileName) + L"\\" + CPathUtils::GetFileNameWithoutExtension(fileName) + L".desc";
                            std::ifstream fin(descPath);
                            if (fin.is_open())
                            {
                                std::string lineA;
                                std::getline(fin, lineA); // version
                                std::getline(fin, lineA); // min BP version
                                std::getline(fin, lineA); // description

                                SearchReplace(lineA, "\\n", "\n");
                                SearchReplace(lineA, "\\r", "");
                                SearchReplace(lineA, "\\t", "\t");
                                auto line = CUnicodeUtils::StdGetUnicode(lineA);
                                pScript->SetDescription(line);
                                fin.close();
                            }
                        }
                        catch (const std::exception&)
                        {
                        }
                        std::wstring sName = CPathUtils::GetParentDirectory(fileName);
                        sName              = CPathUtils::GetFileName(sName);
                        scripts[sName]     = std::move(pScript);
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

    std::set<UINT> usedCmdIds;
    for (const auto& [name, script] : scripts)
    {
        auto foundCmd = static_cast<UINT>(CIniSettings::Instance().GetInt64(L"pluginCmdMap", name.c_str(), 0));
        if (foundCmd)
        {
            if (usedCmdIds.find(foundCmd) == usedCmdIds.end())
            {
                script->SetCmdId(foundCmd);
                usedCmdIds.insert(foundCmd);
            }
        }
    }

    CIniSettings::Instance().Delete(L"pluginCmdMap", nullptr);
    UINT pluginCmd = cmdPluginCmd00;
    for (auto& [name, script] : scripts)
    {
        if (script->GetCmdId() == 0)
        {
            // find the next free plugin cmd id
            while (usedCmdIds.find(pluginCmd) != usedCmdIds.end())
                ++pluginCmd;
            usedCmdIds.insert(pluginCmd);

            script->SetCmdId(pluginCmd);
            m_pluginVersion[name] = script->m_version;
            m_commands[pluginCmd] = std::move(script);
            m_plugins[pluginCmd]  = name;
            CKeyboardShortcutHandler::Instance().AddCommand(name, pluginCmd);
            CIniSettings::Instance().SetInt64(L"pluginCmdMap", name.c_str(), pluginCmd);
        }
        else
        {
            auto cmdId            = script->GetCmdId();
            m_pluginVersion[name] = script->m_version;
            m_commands[cmdId]     = std::move(script);
            m_plugins[cmdId]      = name;
            CKeyboardShortcutHandler::Instance().AddCommand(name, cmdId);
            CIniSettings::Instance().SetInt64(L"pluginCmdMap", name.c_str(), cmdId);
        }
    }
}

void CCommandHandler::PluginNotify(UINT cmdId, const std::wstring& pluginName, LPARAM data)
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnPluginNotify(cmdId, pluginName, data);
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnPluginNotify(cmdId, pluginName, data);
    }
}

int CCommandHandler::GetPluginVersion(const std::wstring& name)
{
    auto it = m_pluginVersion.find(name);
    if (it != m_pluginVersion.end())
        return it->second;
    return 0;
}

void CCommandHandler::AddCommand(ICommand* cmd)
{
    m_highestCmdId     = max(m_highestCmdId, cmd->GetCmdId());
    auto [it, success] = m_noDeleteCommands.emplace(cmd->GetCmdId(), cmd);
    assert(success); // Verify no command has the same ID as an existing command.
}

void CCommandHandler::AddCommand(UINT cmdId)
{
    m_highestCmdId     = max(m_highestCmdId, cmdId);
    auto [it, success] = m_noDeleteCommands.emplace(cmdId, nullptr);
    assert(success); // Verify no command has the same ID as an existing command.
}
