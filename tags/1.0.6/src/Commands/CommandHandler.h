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

#include <string>
#include <map>

class CCommandHandler
{
private:
    CCommandHandler(void);
    ~CCommandHandler(void);

public:
    static CCommandHandler& Instance();

    void                            Init(void * obj);
    ICommand *                      GetCommand(UINT cmdId);
    void                            ScintillaNotify( Scintilla::SCNotification * pScn );
    void                            TabNotify( TBHDR * ptbhdr );
    void                            OnClose();
    void                            OnDocumentClose(int index);
    void                            AfterInit();
private:
    std::map<UINT, ICommand*>       m_commands;
};
