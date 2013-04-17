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
#include "stdafx.h"
#include "CommandHandler.h"

#include "CmdMRU.h"
#include "CmdFiles.h"
#include "CmdUndo.h"
#include "CmdClipboard.h"
#include "CmdLineWrap.h"
#include "CmdWhiteSpace.h"

CCommandHandler::CCommandHandler(void)
{
}


CCommandHandler::~CCommandHandler(void)
{
    for (auto c:m_commands)
    {
        delete c.second;
    }
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
        return c->second;

    return nullptr;
}

void CCommandHandler::Init( void * obj )
{
    ICommand * pCmd = new CCmdMRU(obj);
    m_commands[pCmd->GetCmdId()] = pCmd;
    pCmd = new CCmdOpen(obj);
    m_commands[pCmd->GetCmdId()] = pCmd;
    pCmd = new CCmdSave(obj);
    m_commands[pCmd->GetCmdId()] = pCmd;
    pCmd = new CCmdSaveAll(obj);
    m_commands[pCmd->GetCmdId()] = pCmd;
    pCmd = new CCmdSaveAs(obj);
    m_commands[pCmd->GetCmdId()] = pCmd;
    pCmd = new CCmdUndo(obj);
    m_commands[pCmd->GetCmdId()] = pCmd;
    pCmd = new CCmdRedo(obj);
    m_commands[pCmd->GetCmdId()] = pCmd;
    pCmd = new CCmdCut(obj);
    m_commands[pCmd->GetCmdId()] = pCmd;
    pCmd = new CCmdCopy(obj);
    m_commands[pCmd->GetCmdId()] = pCmd;
    pCmd = new CCmdPaste(obj);
    m_commands[pCmd->GetCmdId()] = pCmd;
    pCmd = new CCmdLineWrap(obj);
    m_commands[pCmd->GetCmdId()] = pCmd;
    pCmd = new CCmdWhiteSpace(obj);
    m_commands[pCmd->GetCmdId()] = pCmd;
}

void CCommandHandler::ScintillaNotify( Scintilla::SCNotification * pScn )
{
    for (auto cmd:m_commands)
    {
        cmd.second->ScintillaNotify(pScn);
    }
}

void CCommandHandler::TabNotify( TBHDR * ptbhdr )
{
    for (auto cmd:m_commands)
    {
        cmd.second->TabNotify(ptbhdr);
    }
}

