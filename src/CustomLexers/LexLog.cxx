// This file is part of BowPad.
//
// Copyright (C) 2020 - Stefan Kueng
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <cassert>
#include <ctype.h>
#include <string>
#include <map>
#include <set>
#include <vector>

#include "StringUtils.h"

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "../scintilla/lexlib/WordList.h"
#include "../scintilla/lexlib/LexAccessor.h"
#include "../scintilla/lexlib/Accessor.h"
#include "../scintilla/lexlib/StyleContext.h"
#include "../scintilla/lexlib/CharacterSet.h"
#include "../scintilla/lexlib/LexerModule.h"
#include "../scintilla/lexlib/OptionSet.h"
#include "../scintilla/lexlib/DefaultLexer.h"

using namespace Scintilla;

namespace
{
// Use an unnamed namespace to protect the functions and classes from name conflicts
enum LogStyles
{
    Default = 0,
    Block,
    String,
    Number,
    InfoDefault,
    InfoBlock,
    InfoString,
    InfoNumber,
    WarnDefault,
    WarnBlock,
    WarnString,
    WarnNumber,
    ErrorDefault,
    ErrorBlock,
    ErrorString,
    ErrorNumber,
};

enum LogStates
{
    None,
    Debug,
    Info,
    Warn,
    Error
};

static inline bool IsAWordChar(const int ch)
{
    return (ch < 0x80) && (isalnum(ch) || ch == '.' || ch == '_' || ch == '$');
}

static char ascii_toupper_char(char c)
{
    return ('a' <= c && c <= 'z') ? c ^ 0x20 : c; // ^ autovectorizes to PXOR: runs on more ports than paddb
}

static LogStates GetState(LogStyles style)
{
    switch (style)
    {
        case LogStyles::Default:
        case LogStyles::Block:
        case LogStyles::String:
        case LogStyles::Number:
            return LogStates::Debug;
        case LogStyles::InfoDefault:
        case LogStyles::InfoBlock:
        case LogStyles::InfoString:
        case LogStyles::InfoNumber:
            return LogStates::Info;
        case LogStyles::WarnDefault:
        case LogStyles::WarnBlock:
        case LogStyles::WarnString:
        case LogStyles::WarnNumber:
            return LogStates::Warn;
        case LogStyles::ErrorDefault:
        case LogStyles::ErrorBlock:
        case LogStyles::ErrorString:
        case LogStyles::ErrorNumber:
            return LogStates::Error;
    }
    return LogStates::Debug;
}

static LogStyles GetLogStyle(LogStyles style, LogStates state)
{
    switch (style)
    {
        case LogStyles::Default:
            switch (state)
            {
                default:
                case LogStates::None:
                    return style;
                case LogStates::Debug:
                    return style;
                case LogStates::Info:
                    return LogStyles::InfoDefault;
                case LogStates::Warn:
                    return LogStyles::WarnDefault;
                case LogStates::Error:
                    return LogStyles::ErrorDefault;
            }
            break;
        case LogStyles::Block:
            switch (state)
            {
                default:
                case LogStates::None:
                    return style;
                case LogStates::Debug:
                    return style;
                case LogStates::Info:
                    return LogStyles::InfoBlock;
                case LogStates::Warn:
                    return LogStyles::WarnBlock;
                case LogStates::Error:
                    return LogStyles::ErrorBlock;
            }
            break;
        case LogStyles::String:
            switch (state)
            {
                default:
                case LogStates::None:
                    return style;
                case LogStates::Debug:
                    return style;
                case LogStates::Info:
                    return LogStyles::InfoString;
                case LogStates::Warn:
                    return LogStyles::WarnString;
                case LogStates::Error:
                    return LogStyles::ErrorString;
            }
            break;
        case LogStyles::Number:
            switch (state)
            {
                default:
                case LogStates::None:
                    return style;
                case LogStates::Debug:
                    return style;
                case LogStates::Info:
                    return LogStyles::InfoNumber;
                case LogStates::Warn:
                    return LogStyles::WarnNumber;
                case LogStates::Error:
                    return LogStyles::ErrorNumber;
            }
            break;
    }
    return style;
}

} // namespace

class LexerLog : public DefaultLexer
{
public:
    LexerLog()
        : DefaultLexer("log", SCLEX_AUTOMATIC + 101)
    {
    }

    virtual ~LexerLog()
    {
    }

    int SCI_METHOD Version() const override
    {
        return lvRelease5;
    }

    void SCI_METHOD Release() override
    {
        delete this;
    }

    const char* SCI_METHOD PropertyNames() override
    {
        return nullptr;
    }

    int SCI_METHOD PropertyType(const char* /*name*/) override
    {
        return 0;
    }

    const char* SCI_METHOD DescribeProperty(const char* /*name*/) override
    {
        return nullptr;
    }

    Sci_Position SCI_METHOD PropertySet(const char* /*key*/, const char* /*val*/) override
    {
        return {};
    }

    const char* SCI_METHOD PropertyGet(const char* /*key*/) override
    {
        return nullptr;
    }

    const char* SCI_METHOD DescribeWordListSets() override
    {
        return nullptr;
    }

    Sci_Position SCI_METHOD WordListSet(int /*n*/, const char* /*wl*/) override
    {
        return {};
    }

    void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess) override;

    void SCI_METHOD Fold(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess) override;

    void* SCI_METHOD PrivateCall(int, void*) override
    {
        return NULL;
    }

    static ILexer5* LexerFactorySimple()
    {
        return new LexerLog();
    }
};

void SCI_METHOD LexerLog::Lex(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess)
{
    bool   numberIsHex = false;
    size_t lineSize    = 1000;
    auto   line        = std::make_unique<char[]>(lineSize);

    LexAccessor  styler(pAccess);
    StyleContext sc(startPos, length, initStyle, styler);

    int       bracketStart = 0;
    LogStates logState     = LogStates::None;
    for (; sc.More(); sc.Forward())
    {
        if (sc.atLineStart)
        {
            logState     = LogStates::None;
            auto lineEnd = pAccess->LineEnd(sc.currentLine);
            auto lineLen = lineEnd - sc.currentPos + 2;
            if (lineSize < lineLen)
            {
                lineSize = lineLen + 200;
                line     = std::make_unique<char[]>(lineSize);
            }
            pAccess->GetCharRange(line.get(), sc.currentPos, lineEnd - sc.currentPos);
            for (size_t i = 0; i < lineLen; ++i)
                line[i] = ascii_toupper_char(line[i]);
            std::string_view sline(line.get(), lineEnd - sc.currentPos + 2);
            if (auto pos = sline.find("DEBUG"); pos != std::string::npos)
            {
                if (pos == 0 || (!isalpha(sline[pos - 1])) && sline[pos - 1] != '"')
                    logState = LogStates::Debug;
            }
            if (auto pos = sline.find("{D}"); pos != std::string::npos)
            {
                if (pos == 0 || (!isalpha(sline[pos - 1])) && sline[pos - 1] != '"')
                    logState = LogStates::Debug;
            }
            if (auto pos = sline.find("INF"); pos != std::string::npos)
            {
                if (pos == 0 || (!isalpha(sline[pos - 1])) && sline[pos - 1] != '"')
                    logState = LogStates::Info;
            }
            if (auto pos = sline.find("{I}"); pos != std::string::npos)
            {
                if (pos == 0 || (!isalpha(sline[pos - 1])) && sline[pos - 1] != '"')
                    logState = LogStates::Info;
            }
            if (auto pos = sline.find("WARN"); pos != std::string::npos)
            {
                if (pos == 0 || (!isalpha(sline[pos - 1])) && sline[pos - 1] != '"')
                    logState = LogStates::Warn;
            }
            if (auto pos = sline.find("{W}"); pos != std::string::npos)
            {
                if (pos == 0 || (!isalpha(sline[pos - 1])) && sline[pos - 1] != '"')
                    logState = LogStates::Warn;
            }
            if (auto pos = sline.find("{ERR}"); pos != std::string::npos)
            {
                if (pos == 0 || (!isalpha(sline[pos - 1])) && sline[pos - 1] != '"')
                    logState = LogStates::Error;
            }
            if (auto pos = sline.find("{CRIT}"); pos != std::string::npos)
            {
                if (pos == 0 || (!isalpha(sline[pos - 1])) && sline[pos - 1] != '"')
                    logState = LogStates::Error;
            }
            if (auto pos = sline.find("{E}"); pos != std::string::npos)
            {
                if (pos == 0 || (!isalpha(sline[pos - 1])) && sline[pos - 1] != '"')
                    logState = LogStates::Error;
            }
            if (auto pos = sline.find("{C}"); pos != std::string::npos)
            {
                if (pos == 0 || (!isalpha(sline[pos - 1])) && sline[pos - 1] != '"')
                    logState = LogStates::Error;
            }
        }
        // Determine if the current state should terminate.
        switch (sc.state)
        {
            case LogStyles::Number:
            case LogStyles::InfoNumber:
            case LogStyles::WarnNumber:
            case LogStyles::ErrorNumber:
                if (!IsAlphaNumeric(sc.ch))
                {
                    sc.SetState(GetLogStyle(LogStyles::Default, logState));
                }
                else if ((numberIsHex && !(MakeLowerCase(sc.ch) == 'x' || MakeLowerCase(sc.ch) == 'e' ||
                                           IsADigit(sc.ch, 16) || sc.ch == '.' || sc.ch == '-' || sc.ch == '+')) ||
                         (!numberIsHex && !(MakeLowerCase(sc.ch) == 'e' || IsADigit(sc.ch) || sc.ch == '.' || sc.ch == '-' || sc.ch == '+')))
                {
                    // check '-' for possible -10e-5. Add '+' as well.
                    numberIsHex    = false;
                    auto baseStyle = GetLogStyle(LogStyles::Default, logState);
                    sc.ChangeState(baseStyle);
                    sc.SetState(baseStyle);
                }
                break;
            case LogStyles::String:
            case LogStyles::InfoString:
            case LogStyles::WarnString:
            case LogStyles::ErrorString:
                if ((sc.ch == '\'' || sc.ch == '"') && sc.chPrev != '\\')
                {
                    sc.ForwardSetState(GetLogStyle(LogStyles::Default, logState));
                }
                else if (sc.atLineEnd)
                {
                    sc.ChangeState(GetLogStyle(LogStyles::String, logState));
                    sc.ForwardSetState(GetLogStyle(LogStyles::Default, logState));
                }
                break;
            case LogStyles::Block:
            case LogStyles::InfoBlock:
            case LogStyles::WarnBlock:
            case LogStyles::ErrorBlock:
                switch (bracketStart)
                {
                    case '{':
                        if (sc.ch == '}')
                            sc.ForwardSetState(GetLogStyle(LogStyles::Default, logState));
                        break;
                    case '[':
                        if (sc.ch == ']')
                            sc.ForwardSetState(GetLogStyle(LogStyles::Default, logState));
                        break;
                    case '(':
                        if (sc.ch == ')')
                            sc.ForwardSetState(GetLogStyle(LogStyles::Default, logState));
                        break;
                }
                if (sc.atLineEnd)
                {
                    sc.ChangeState(GetLogStyle(LogStyles::Block, logState));
                    sc.ForwardSetState(GetLogStyle(LogStyles::Default, logState));
                }

                break;
        }

        // Determine if a new state should be entered.
        if ((sc.state == LogStyles::Default) ||
            (sc.state == LogStyles::InfoDefault) ||
            (sc.state == LogStyles::WarnDefault) ||
            (sc.state == LogStyles::ErrorDefault))
        {
            if (((IsADigit(sc.ch) || (sc.ch == '.' && IsADigit(sc.chNext)) || ((sc.ch == '-' || sc.ch == '+') && (IsADigit(sc.chNext) || sc.chNext == '.')) || (MakeLowerCase(sc.ch) == 'e' && (IsADigit(sc.chNext) || sc.chNext == '+' || sc.chNext == '-')))))
            {
                if ((sc.ch == '0' && MakeLowerCase(sc.chNext) == 'x') ||
                    ((sc.ch == '-' || sc.ch == '+') && sc.chNext == '0' && MakeLowerCase(sc.GetRelative(2)) == 'x'))
                {
                    numberIsHex = true;
                }
                sc.SetState(GetLogStyle(LogStyles::Number, logState));
            }
            else if (sc.ch == '\'' || sc.ch == '"')
            {
                sc.SetState(GetLogStyle(LogStyles::String, logState));
            }
            else if (sc.ch == '{' || sc.ch == '[' || sc.ch == '(')
            {
                sc.SetState(GetLogStyle(LogStyles::Block, logState));
                bracketStart = sc.ch;
            }
        }

        if (sc.atLineEnd)
        {
            // Reset states to begining of colourise so no surprises
            // if different sets of lines lexed.
            numberIsHex = false;
        }
    }
    sc.Complete();
}

void SCI_METHOD LexerLog::Fold(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess)
{
    // no folding : log files are usually big, and this simply is too slow
}

LexerModule lmLog(SCLEX_AUTOMATIC + 101, LexerLog::LexerFactorySimple, "log", 0);
