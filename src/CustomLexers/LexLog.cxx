// This file is part of BowPad.
//
// Copyright (C) 2020-2021 - Stefan Kueng
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
#include <stdarg.h>
#include <cassert>
#include <ctype.h>
#include <string>
#include <vector>

#include "StringUtils.h"

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "../lexilla/lexlib/LexAccessor.h"
#include "../lexilla/lexlib/StyleContext.h"
#include "../lexilla/lexlib/CharacterSet.h"
#include "../lexilla/lexlib/LexerModule.h"
#include "../lexilla/lexlib/OptionSet.h"
#include "../lexilla/lexlib/DefaultLexer.h"

using namespace Scintilla;
using namespace Lexilla;

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

//static bool IsAWordChar(const int ch)
//{
//    return (ch < 0x80) && (isalnum(ch) || ch == '.' || ch == '_' || ch == '$');
//}

//static LogStates GetState(LogStyles style)
//{
//    switch (style)
//    {
//        case LogStyles::Default:
//        case LogStyles::Block:
//        case LogStyles::String:
//        case LogStyles::Number:
//            return LogStates::Debug;
//        case LogStyles::InfoDefault:
//        case LogStyles::InfoBlock:
//        case LogStyles::InfoString:
//        case LogStyles::InfoNumber:
//            return LogStates::Info;
//        case LogStyles::WarnDefault:
//        case LogStyles::WarnBlock:
//        case LogStyles::WarnString:
//        case LogStyles::WarnNumber:
//            return LogStates::Warn;
//        case LogStyles::ErrorDefault:
//        case LogStyles::ErrorBlock:
//        case LogStyles::ErrorString:
//        case LogStyles::ErrorNumber:
//            return LogStates::Error;
//    }
//    return LogStates::Debug;
//}

bool hasTokenAsWord(const std::string& token, const std::string_view& sLine)
{
    if (auto pos = sLine.find(token); pos != std::string::npos)
    {
        auto boundaryBefore = pos == 0 || !isalpha(sLine[pos - 1]);
        auto boundaryAfter  = pos == (sLine.size() - token.size()) || !isalpha(sLine[pos + token.size()]);
        if (boundaryBefore && boundaryAfter)
            return true;
    }
    return false;
}

LogStyles GetLogStyle(LogStyles style, LogStates state)
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
        case InfoDefault:
            break;
        case InfoBlock:
            break;
        case InfoString:
            break;
        case InfoNumber:
            break;
        case WarnDefault:
            break;
        case WarnBlock:
            break;
        case WarnString:
            break;
        case WarnNumber:
            break;
        case ErrorDefault:
            break;
        case ErrorBlock:
            break;
        case ErrorString:
            break;
        case ErrorNumber:
            break;
        default:
            break;
    }
    return style;
}

} // namespace

struct OptionsSimple
{
    std::string              debugstrings;
    std::string              infostrings;
    std::string              warnstrings;
    std::string              errorstrings;
    std::vector<std::string> debugTokens;
    std::vector<std::string> infoTokens;
    std::vector<std::string> warnTokens;
    std::vector<std::string> errorTokens;
};

struct OptionSetSimple : public OptionSet<OptionsSimple>
{
    OptionSetSimple()
    {
        DefineProperty("debugstrings", &OptionsSimple::debugstrings);
        DefineProperty("infostrings", &OptionsSimple::infostrings);
        DefineProperty("warnstrings", &OptionsSimple::warnstrings);
        DefineProperty("errorstrings", &OptionsSimple::errorstrings);
    }
};

class LexerLog : public DefaultLexer
{
    OptionsSimple   options;
    OptionSetSimple osSimple;

public:
    LexerLog()
        : DefaultLexer("log", SCLEX_AUTOMATIC + 101)
    {
    }

    ~LexerLog() override
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
        return osSimple.PropertyNames();
    }

    int SCI_METHOD PropertyType(const char* name) override
    {
        return osSimple.PropertyType(name);
    }

    const char* SCI_METHOD DescribeProperty(const char* name) override
    {
        return osSimple.DescribeProperty(name);
    }

    Sci_Position SCI_METHOD PropertySet(const char* key, const char* val) override;

    const char* SCI_METHOD PropertyGet(const char* key) override
    {
        return osSimple.PropertyGet(key);
    }

    const char* SCI_METHOD DescribeWordListSets() override
    {
        return osSimple.DescribeWordListSets();
    }

    Sci_Position SCI_METHOD WordListSet(int /*n*/, const char* /*wl*/) override
    {
        return {};
    }

    void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess) override;

    void SCI_METHOD Fold(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess) override;

    void* SCI_METHOD PrivateCall(int, void*) override
    {
        return nullptr;
    }

    static ILexer5* LexerFactorySimple()
    {
        return new LexerLog();
    }
};

Sci_Position SCI_METHOD LexerLog::PropertySet(const char* key, const char* val)
{
    if (osSimple.PropertySet(&options, key, val))
    {
        if (strcmp(key, "debugstrings") == 0)
        {
            std::transform(options.debugstrings.begin(), options.debugstrings.end(), options.debugstrings.begin(), [](char c) { return static_cast<char>(::tolower(c)); });
            stringtok(options.debugTokens, options.debugstrings, true, " \t\n", false);
        }
        if (strcmp(key, "infostrings") == 0)
        {
            std::transform(options.infostrings.begin(), options.infostrings.end(), options.infostrings.begin(), [](char c) { return static_cast<char>(::tolower(c)); });
            stringtok(options.infoTokens, options.infostrings, true, " \t\n", false);
        }
        if (strcmp(key, "warnstrings") == 0)
        {
            std::transform(options.warnstrings.begin(), options.warnstrings.end(), options.warnstrings.begin(), [](char c) { return static_cast<char>(::tolower(c)); });
            stringtok(options.warnTokens, options.warnstrings, true, " \t\n", false);
        }
        if (strcmp(key, "errorstrings") == 0)
        {
            std::transform(options.errorstrings.begin(), options.errorstrings.end(), options.errorstrings.begin(), [](char c) { return static_cast<char>(::tolower(c)); });
            stringtok(options.errorTokens, options.errorstrings, true, " \t\n", false);
        }

        return 0;
    }
    return -1;
}

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
                line[i] = ::tolower(line[i]);
            std::string_view sLine(line.get(), lineEnd - sc.currentPos + 2);
            for (const auto& token : options.debugTokens)
            {
                if (hasTokenAsWord(token, sLine))
                    logState = LogStates::Debug;
            }
            for (const auto& token : options.infoTokens)
            {
                if (hasTokenAsWord(token, sLine))
                    logState = LogStates::Info;
            }
            for (const auto& token : options.warnTokens)
            {
                if (hasTokenAsWord(token, sLine))
                    logState = LogStates::Warn;
            }
            for (const auto& token : options.errorTokens)
            {
                if (hasTokenAsWord(token, sLine))
                    logState = LogStates::Error;
            }
            sc.SetState(GetLogStyle(LogStyles::Default, logState));
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
                    sc.ChangeState(GetLogStyle(LogStyles::Default, logState));
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
                    default:
                        break;
                }
                if (sc.atLineEnd)
                {
                    sc.ChangeState(GetLogStyle(LogStyles::Block, logState));
                    sc.ForwardSetState(GetLogStyle(LogStyles::Default, logState));
                }
                break;
            default:
                break;
        }

        // Determine if a new state should be entered.
        if ((sc.state == LogStyles::Default) ||
            (sc.state == LogStyles::InfoDefault) ||
            (sc.state == LogStyles::WarnDefault) ||
            (sc.state == LogStyles::ErrorDefault))
        {
            if (!IsAlphaNumeric(sc.chPrev) &&
                (IsADigit(sc.ch) ||
                 (sc.ch == '.' && IsADigit(sc.chNext)) ||
                 ((sc.ch == '-' || sc.ch == '+') && (IsADigit(sc.chNext) || sc.chNext == '.')) ||
                 (MakeLowerCase(sc.ch) == 'e' && (IsADigit(sc.chNext) || sc.chNext == '+' || sc.chNext == '-'))))
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

void SCI_METHOD LexerLog::Fold(Sci_PositionU /*startPos*/, Sci_Position /*length*/, int /*initStyle*/, IDocument* /*pAccess*/)
{
    // no folding : log files are usually big, and this simply is too slow
}

LexerModule lmLog(SCLEX_AUTOMATIC + 101, LexerLog::LexerFactorySimple, "bp_log", nullptr);
