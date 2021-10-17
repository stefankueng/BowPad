// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2021 - Stefan Kueng
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
#include "BowPadUI.h"

class CCmdEOLBase : public ICommand
{
protected:
    // Don't do anything in this base, like call InvalidateUICommand which
    // might result in a virtual call and the derived class won't be setup.
    CCmdEOLBase(void* obj)
        : ICommand(obj)
    {
    }
    HRESULT                      IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override;
    void                         TabNotify(TBHDR* ptbHdr) override;
    bool                         Execute() override;
    virtual Scintilla::EndOfLine GetLineType() const = 0;
};

class CCmdEOLWin : public CCmdEOLBase
{
public:
    CCmdEOLWin(void* obj);
    UINT                 GetCmdId() override { return cmdEOLWin; }
    Scintilla::EndOfLine GetLineType() const override { return Scintilla::EndOfLine::CrLf; }
    void                 AfterInit() override;
};

class CCmdEOLUnix : public CCmdEOLBase
{
public:
    CCmdEOLUnix(void* obj);
    UINT                 GetCmdId() override { return cmdEOLUnix; }
    Scintilla::EndOfLine GetLineType() const override { return Scintilla::EndOfLine::Lf; }
    void                 AfterInit() override;
};

class CCmdEOLMac : public CCmdEOLBase
{
public:
    CCmdEOLMac(void* obj);
    UINT                 GetCmdId() override { return cmdEOLMac; }
    Scintilla::EndOfLine GetLineType() const override { return Scintilla::EndOfLine::Cr; }
    void                 AfterInit() override;
};
