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

CCommandHandler::CCommandHandler(void)
{
}


CCommandHandler::~CCommandHandler(void)
{
}

CCommandHandler& CCommandHandler::Instance()
{
    static CCommandHandler instance;
    return instance;
}

ICommand * CCommandHandler::GetCommand( UINT cmdId )
{
    auto c = m_commands.find(cmdId);
    if (c != m_commands.end())
        return c->second.get();

    return nullptr;
}

void CCommandHandler::Init( void * obj )
{
    Add<CCmdMRU>(obj);
    Add<CCmdToggleTheme>(obj);
    Add<CCmdOpen>(obj);
    Add<CCmdSave>(obj);
    Add<CCmdSaveAll>(obj);
    Add<CCmdSaveAs>(obj);
    Add<CCmdReload>(obj);
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
    Add<CCmdUseTabs>(obj);
    Add<CCmdAutoBraces>(obj);
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

    Add<CCmdFindReplace>(obj);
    Add<CCmdFindNext>(obj);
    Add<CCmdFindPrev>(obj);
    Add<CCmdFindSelectedNext>(obj);
    Add<CCmdFindSelectedPrev>(obj);
    Add<CCmdGotoLine>(obj);
    Add<CCmdFunctions>(obj);

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

    Add<CCmdLaunchIE>(obj);
    Add<CCmdLaunchFirefox>(obj);
    Add<CCmdLaunchChrome>(obj);
    Add<CCmdLaunchSafari>(obj);
    Add<CCmdLaunchOpera>(obj);
    Add<CCmdLaunchSearch>(obj);
    Add<CCmdLaunchWikipedia>(obj);
    Add<CCmdLaunchConsole>(obj);
    Add<CCmdLaunchExplorer>(obj);

    for (int i = 0; i < 10; ++i)
    {
        Add<CCmdLaunchCustom>(i, obj);
    }
    Add<CCmdCustomCommands>(obj);

    Add<CCmdRandom>(obj);
}

void CCommandHandler::ScintillaNotify( Scintilla::SCNotification * pScn )
{
    for (auto& cmd:m_commands)
    {
        cmd.second.get()->ScintillaNotify(pScn);
    }
}

void CCommandHandler::TabNotify( TBHDR * ptbhdr )
{
    for (auto& cmd:m_commands)
    {
        cmd.second->TabNotify(ptbhdr);
    }
}

void CCommandHandler::OnClose()
{
    for (auto& cmd:m_commands)
    {
        cmd.second->OnClose();
    }
}

void CCommandHandler::OnDocumentClose(int index)
{
    for (auto& cmd:m_commands)
    {
        cmd.second->OnDocumentClose(index);
    }
}

void CCommandHandler::OnDocumentOpen(int index)
{
    for (auto& cmd : m_commands)
    {
        cmd.second->OnDocumentOpen(index);
    }
}

void CCommandHandler::OnDocumentSave(int index, bool bSaveAs)
{
    for (auto& cmd : m_commands)
    {
        cmd.second->OnDocumentSave(index, bSaveAs);
    }
}

void CCommandHandler::AfterInit()
{
    for (auto& cmd:m_commands)
    {
        cmd.second->AfterInit();
    }
}

void CCommandHandler::OnTimer(UINT id)
{
    for (auto& cmd:m_commands)
    {
        cmd.second->OnTimer(id);
    }
}