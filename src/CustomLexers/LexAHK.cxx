// This file is part of BowPad.
//
// Copyright (C) 2024 - Stefan Kueng
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

// lexer based on the AHK lexer from Notepad3 (https://github.com/rizonesoft/Notepad3)

#include "stdafx.h"
#include <string>
#include <assert.h>
#include <map>
//
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
#include "../lexilla/lexlib/WordList.h"

using namespace Lexilla;
using namespace Scintilla;

enum AhkStyles
{
    Default = 0,
    CommentLine,
    CommentBlock,
    Esc,
    SynOperator,
    ExpOperator,
    String,
    Number,
    Identifier,
    VarRef,
    Label,
    WordCf,
    WordCmd,
    WordFn,
    WordDir,
    WordKb,
    WordVar,
    WordSp,
    WordUd,
    VarRefKw,
    Error
};

struct OptionsAhk
{
    bool fold;
    bool foldComment;
    bool foldCompact;
    OptionsAhk()
        : fold(false)
        , foldComment(true)
        , foldCompact(true)
    {
    }
};

static const char* const ahkWordLists[] =
    {
        "Flow of Control",
        "Commands",
        "Functions",
        "Directives",
        "Keys & Buttons",
        "Variables",
        "Special Parameters (keywords)",
        "User Defined",
        nullptr};

struct OptionSetAhk : public OptionSet<OptionsAhk>
{
    OptionSetAhk()
    {
        DefineProperty("fold", &OptionsAhk::fold);
        DefineProperty("fold.comment", &OptionsAhk::foldComment);
        DefineProperty("fold.compact", &OptionsAhk::foldCompact);

        DefineWordListSets(ahkWordLists);
    }
};

class LexerAhk : public DefaultLexer
{
    OptionsAhk   options;
    OptionSetAhk osAhk;

    WordList     controlFlow;
    WordList     commands;
    WordList     functions;
    WordList     directives;
    WordList     keysButtons;
    WordList     variables;
    WordList     specialParams;
    WordList     userDefined;

    CharacterSet wordChar;
    CharacterSet expOperator;

public:
    LexerAhk()
        : DefaultLexer("AHK", SCLEX_AUTOMATIC + 103, nullptr, 0)
        , wordChar(CharacterSet::setAlphaNum, "@#$_[]?")
        , expOperator(CharacterSet::setNone, "+-*/!~&|^=<>.()")
    {
    }

    ~LexerAhk() override = default;

    void SCI_METHOD Release() override
    {
        delete this;
    }

    int SCI_METHOD Version() const override
    {
        return lvRelease5;
    }

    const char* SCI_METHOD PropertyNames() override
    {
        return osAhk.PropertyNames();
    }

    int SCI_METHOD PropertyType(const char* name) override
    {
        return osAhk.PropertyType(name);
    }

    Sci_Position SCI_METHOD PropertySet(const char* key, const char* val) override;

    const char* SCI_METHOD  PropertyGet(const char* name) override
    {
        return osAhk.PropertyGet(name);
    }

    const char* SCI_METHOD DescribeProperty(const char* name) override
    {
        return osAhk.DescribeProperty(name);
    }

    const char* SCI_METHOD DescribeWordListSets() override
    {
        return osAhk.DescribeWordListSets();
    }

    Sci_Position SCI_METHOD WordListSet(int n, const char* wl) override;

    void SCI_METHOD         Lex(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, IDocument* pAccess) override;

    void SCI_METHOD         Fold(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, IDocument* pAccess) override;

    void* SCI_METHOD        PrivateCall(int, void*) override
    {
        return nullptr;
    }

    static ILexer5* LexerFactoryAHK()
    {
        return new LexerAhk();
    }

private:
    void HighlightKeyword(char currentWord[], StyleContext& sc) const;
};

Sci_Position SCI_METHOD LexerAhk::PropertySet(const char* key, const char* val)
{
    if (osAhk.PropertySet(&options, key, val))
    {
        return 0;
    }
    return -1;
}

Sci_Position SCI_METHOD LexerAhk::WordListSet(int n, const char* wl)
{
    WordList* wordListN = nullptr;
    switch (n)
    {
        case 0:
            wordListN = &controlFlow;
            break;

        case 1:
            wordListN = &commands;
            break;

        case 2:
            wordListN = &functions;
            break;

        case 3:
            wordListN = &directives;
            break;

        case 4:
            wordListN = &keysButtons;
            break;

        case 5:
            wordListN = &variables;
            break;

        case 6:
            wordListN = &specialParams;
            break;

        case 7:
            wordListN = &userDefined;
            break;
    }

    int firstModification = -1;
    if (wordListN)
    {
        if (wordListN->Set(wl))
        {
            firstModification = 0;
        }
    }
    return firstModification;
}

void LexerAhk::HighlightKeyword(char currentWord[], StyleContext& sc) const
{
    if (controlFlow.InList(currentWord))
    {
        sc.ChangeState(AhkStyles::WordCf);
    }
    else if (commands.InList(currentWord))
    {
        sc.ChangeState(AhkStyles::WordCmd);
    }
    else if (functions.InList(currentWord))
    {
        sc.ChangeState(AhkStyles::WordFn);
    }
    else if (currentWord[0] == '#' && directives.InList(currentWord + 1))
    {
        sc.ChangeState(AhkStyles::WordDir);
    }
    else if (keysButtons.InList(currentWord))
    {
        sc.ChangeState(AhkStyles::WordKb);
    }
    else if (variables.InList(currentWord))
    {
        sc.ChangeState(AhkStyles::WordVar);
    }
    else if (specialParams.InList(currentWord))
    {
        sc.ChangeState(AhkStyles::WordSp);
    }
    else if (userDefined.InList(currentWord))
    {
        sc.ChangeState(AhkStyles::WordUd);
    }
    else
    {
        sc.ChangeState(AhkStyles::Default);
    }
}

void SCI_METHOD LexerAhk::Lex(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess)
{
    LexAccessor  styler(pAccess);
    StyleContext sc(startPos, length, initStyle, styler);

    // Do not leak onto next line
    if (initStyle != AhkStyles::CommentBlock &&
        initStyle != AhkStyles::String)
    {
        initStyle = AhkStyles::Default;
    }
    int  currentState = initStyle;
    int  nextState    = -1;
    char currentWord[256];

    // True if in a continuation section
    bool bContinuationSection = (initStyle == AhkStyles::String);
    // Indicate if the lexer has seen only spaces since the start of the line
    bool bOnlySpaces          = (!bContinuationSection);
    // Indicate if since the start of the line, lexer met only legal label chars
    bool bIsLabel             = false;
    // Distinguish hotkeys from hotstring
    bool bIsHotkey            = false;
    bool bIsHotstring         = false;
    // In an expression
    bool bInExpression        = false;
    // A quoted string in an expression (share state with continuation section string)
    bool bInExprString        = false;
    // To accept A-F chars in a number
    bool bInHexNumber         = false;

    for (; sc.More(); sc.Forward())
    {
        if (nextState >= 0)
        {
            // I need to reset a state before checking new char
            sc.SetState(nextState);
            nextState = -1;
        }
        if (sc.state == AhkStyles::SynOperator)
        {
            // Only one char (if two detected, we move Forward() anyway)
            sc.SetState(AhkStyles::Default);
        }
        if (sc.atLineEnd && (bIsHotkey || bIsHotstring))
        {
            // I make the hotkeys and hotstrings more visible
            // by changing the line end to LABEL style (if style uses eolfilled)
            bIsHotkey = bIsHotstring = false;
            sc.SetState(AhkStyles::Label);
        }
        if (sc.atLineStart)
        {
            if (sc.state != AhkStyles::CommentBlock &&
                !bContinuationSection)
            {
                // Prevent some styles from leaking back to previous line
                sc.SetState(AhkStyles::Default);
            }
            bOnlySpaces   = true;
            bIsLabel      = false;
            bInExpression = false; // I don't manage multiline expressions yet!
            bInHexNumber  = false;
        }

        // Manage cases occuring in (almost) all states (not in comments)
        if (sc.state != AhkStyles::CommentLine &&
            sc.state != AhkStyles::CommentBlock &&
            !IsASpace(sc.ch))
        {
            if (sc.ch == '`')
            {
                // Backtick, escape sequence
                currentState = sc.state;
                sc.SetState(AhkStyles::Esc);
                sc.Forward();
                nextState = currentState;
                continue;
            }
            if (sc.ch == '%' && !bIsHotstring && !bInExprString &&
                sc.state != AhkStyles::VarRef &&
                sc.state != AhkStyles::VarRefKw &&
                sc.state != AhkStyles::Error)
            {
                if (IsASpace(sc.chNext))
                {
                    if (sc.state == AhkStyles::String)
                    {
                        // Illegal unquoted character!
                        sc.SetState(AhkStyles::Error);
                    }
                    else
                    {
                        // % followed by a space is expression start
                        bInExpression = true;
                    }
                }
                else
                {
                    // Variable reference
                    currentState = sc.state;
                    sc.SetState(AhkStyles::SynOperator);
                    nextState = AhkStyles::VarRef;
                    continue;
                }
            }
            if (sc.state != AhkStyles::String && !bInExpression)
            {
                // Management of labels, hotkeys, hotstrings and remapping

                // Check if the starting string is a label candidate
                if (bOnlySpaces &&
                    sc.ch != ',' && sc.ch != ';' && sc.ch != ':' &&
                    sc.ch != '%' && sc.ch != '`')
                {
                    // A label cannot start with one of the above chars
                    bIsLabel = true;
                }

                // The current state can be IDENTIFIER or DEFAULT,
                // depending on if the label starts with a word char or not
                if (bIsLabel && sc.ch == ':' &&
                    (IsASpace(sc.chNext) || sc.atLineEnd))
                {
                    // ?l/a|b\e^l!:
                    // Only ; comment should be allowed after
                    sc.ChangeState(AhkStyles::Label);
                    sc.SetState(AhkStyles::SynOperator);
                    nextState = AhkStyles::Default;
                    continue;
                }
                else if (sc.Match(':', ':'))
                {
                    if (bOnlySpaces)
                    {
                        // Hotstring ::aa::Foo
                        bIsHotstring = true;
                        sc.SetState(AhkStyles::SynOperator);
                        sc.Forward();
                        nextState = AhkStyles::Label;
                        continue;
                    }
                    // Hotkey F2:: or remapping a::b
                    bIsHotkey = true;
                    // Check if it is a known key
                    sc.GetCurrent(currentWord, sizeof(currentWord));
                    if (keysButtons.InList(currentWord))
                    {
                        sc.ChangeState(AhkStyles::WordKb);
                    }
                    sc.SetState(AhkStyles::SynOperator);
                    sc.Forward();
                    if (bIsHotstring)
                    {
                        nextState = AhkStyles::String;
                    }
                    continue;
                }
            }
        }
        // Check if the current string is still a label candidate
        // Labels are much more permissive than regular identifiers...
        if (bIsLabel &&
            (sc.ch == ',' || sc.ch == '%' || sc.ch == '`' || IsASpace(sc.ch)))
        {
            // Illegal character in a label
            bIsLabel = false;
        }

        // Determine if the current state should terminate.
        if (sc.state == AhkStyles::CommentLine)
        {
            if (sc.atLineEnd)
            {
                sc.SetState(AhkStyles::Default);
            }
        }
        else if (sc.state == AhkStyles::CommentBlock)
        {
            if (bOnlySpaces && sc.Match('*', '/'))
            {
                // End of comment at start of line (skipping white space)
                sc.Forward();
                sc.ForwardSetState(AhkStyles::Default);
            }
        }
        else if (sc.state == AhkStyles::ExpOperator)
        {
            if (!expOperator.Contains(sc.ch))
            {
                sc.SetState(AhkStyles::Default);
            }
        }
        else if (sc.state == AhkStyles::String)
        {
            if (bContinuationSection)
            {
                if (bOnlySpaces && sc.ch == ')')
                {
                    // End of continuation section
                    bContinuationSection = false;
                    sc.SetState(AhkStyles::SynOperator);
                }
            }
            else if (bInExprString)
            {
                if (sc.ch == '\"')
                {
                    if (sc.chNext == '\"')
                    {
                        // In expression string, double quotes are doubled to escape them
                        sc.Forward(); // Skip it
                    }
                    else
                    {
                        bInExprString = false;
                        sc.ForwardSetState(AhkStyles::Default);
                    }
                }
                else if (sc.atLineEnd)
                {
                    sc.ChangeState(AhkStyles::Error);
                }
            }
            else
            {
                if (sc.ch == ';' && IsASpace(sc.chPrev))
                {
                    // Line comments after code must be preceded by a space
                    sc.SetState(AhkStyles::CommentLine);
                }
            }
        }
        else if (sc.state == AhkStyles::Number)
        {
            if (bInHexNumber)
            {
                if (!IsADigit(sc.ch, 16))
                {
                    bInHexNumber = false;
                    sc.SetState(AhkStyles::Default);
                }
            }
            else if (!(IsADigit(sc.ch) || sc.ch == '.'))
            {
                sc.SetState(AhkStyles::Default);
            }
        }
        else if (sc.state == AhkStyles::Identifier)
        {
            if (!wordChar.Contains(sc.ch))
            {
                sc.GetCurrent(currentWord, sizeof(currentWord));
                HighlightKeyword(currentWord, sc);
                if (strcmp(currentWord, "if") == 0)
                {
                    bInExpression = true;
                }
                sc.SetState(AhkStyles::Default);
            }
        }
        else if (sc.state == AhkStyles::VarRef)
        {
            if (sc.ch == '%')
            {
                // End of variable reference
                sc.GetCurrent(currentWord, sizeof(currentWord));
                if (variables.InList(currentWord))
                {
                    sc.ChangeState(AhkStyles::VarRefKw);
                }
                sc.SetState(AhkStyles::SynOperator);
                nextState = currentState;
                continue;
            }
            else if (!wordChar.Contains(sc.ch))
            {
                // Oops! Probably no terminating %
                sc.ChangeState(AhkStyles::Error);
            }
        }
        else if (sc.state == AhkStyles::Label)
        {
            // Hotstring -- modifier or trigger string :*:aa::Foo or ::aa::Foo
            if (sc.ch == ':')
            {
                sc.SetState(AhkStyles::SynOperator);
                if (sc.chNext == ':')
                {
                    sc.Forward();
                }
                nextState = AhkStyles::Label;
                continue;
            }
        }

        // Determine if a new state should be entered
        if (sc.state == AhkStyles::Default)
        {
            if (sc.ch == ';' &&
                (bOnlySpaces || IsASpace(sc.chPrev)))
            {
                // Line comments are alone on the line or are preceded by a space
                sc.SetState(AhkStyles::CommentLine);
            }
            else if (bOnlySpaces && sc.Match('/', '*'))
            {
                // Comment at start of line (skipping white space)
                sc.SetState(AhkStyles::CommentBlock);
                sc.Forward();
            }
            else if (sc.ch == '{' || sc.ch == '}')
            {
                // Code block or special key {Enter}
                sc.SetState(AhkStyles::SynOperator);
            }
            else if (bOnlySpaces && sc.ch == '(')
            {
                // Continuation section
                bContinuationSection = true;
                sc.SetState(AhkStyles::SynOperator);
                nextState = AhkStyles::String; // !!! Can be an expression!
            }
            else if (sc.Match(':', '=') ||
                     sc.Match('+', '=') ||
                     sc.Match('-', '=') ||
                     sc.Match('/', '=') ||
                     sc.Match('*', '='))
            {
                // Expression assignment
                bInExpression = true;
                sc.SetState(AhkStyles::SynOperator);
                sc.Forward();
                nextState = AhkStyles::Default;
            }
            else if (expOperator.Contains(sc.ch))
            {
                sc.SetState(AhkStyles::ExpOperator);
            }
            else if (sc.ch == '\"')
            {
                bInExprString = true;
                sc.SetState(AhkStyles::String);
            }
            else if (sc.ch == '0' && (sc.chNext == 'x' || sc.chNext == 'X'))
            {
                // Hexa, skip forward as we don't accept any other alpha char (beside A-F) inside
                bInHexNumber = true;
                sc.SetState(AhkStyles::Number);
                sc.Forward(2);
            }
            else if (IsADigit(sc.ch) || (sc.ch == '.' && IsADigit(sc.chNext)))
            {
                sc.SetState(AhkStyles::Number);
            }
            else if (wordChar.Contains(sc.ch))
            {
                sc.SetState(AhkStyles::Identifier);
            }
            else if (sc.ch == ',')
            {
                sc.SetState(AhkStyles::SynOperator);
                nextState = AhkStyles::Default;
            }
            else if (sc.ch == ':')
            {
                if (bOnlySpaces)
                {
                    // Start of hotstring :*:foo::Stuff or ::btw::Stuff
                    bIsHotstring = true;
                    sc.SetState(AhkStyles::SynOperator);
                    if (sc.chNext == ':')
                    {
                        sc.Forward();
                    }
                    nextState = AhkStyles::Label;
                }
            }
            else if (wordChar.Contains(sc.ch))
            {
                sc.SetState(AhkStyles::Identifier);
            }
        }
        if (!IsASpace(sc.ch))
        {
            bOnlySpaces = false;
        }
    }
    // End of file: complete any pending changeState
    if (sc.state == AhkStyles::Identifier)
    {
        sc.GetCurrent(currentWord, sizeof(currentWord));
        HighlightKeyword(currentWord, sc);
    }
    else if (sc.state == AhkStyles::String && bInExprString)
    {
        sc.ChangeState(AhkStyles::Error);
    }
    else if (sc.state == AhkStyles::VarRef)
    {
        sc.ChangeState(AhkStyles::Error);
    }
    sc.Complete();
}

void SCI_METHOD LexerAhk::Fold(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, IDocument* pAccess)
{
    if (!options.fold)
    {
        return;
    }

    LexAccessor   styler(pAccess);

    bool const    foldComment  = options.foldComment; // props.GetInt("fold.comment") != 0;
    bool const    foldCompact  = options.foldCompact; // props.GetInt("fold.compact", 1) != 0;

    Sci_PositionU endPos       = startPos + lengthDoc;

    bool          bOnlySpaces  = true;
    int           lineCurrent  = styler.GetLine(startPos);
    int           levelCurrent = SC_FOLDLEVELBASE;
    if (lineCurrent > 0)
    {
        levelCurrent = styler.LevelAt(lineCurrent - 1) & SC_FOLDLEVELNUMBERMASK;
    }
    int  levelNext = levelCurrent;
    char chNext    = styler[startPos];
    int  styleNext = styler.StyleAt(startPos);
    int  style     = initStyle;
    for (Sci_PositionU i = startPos; i < endPos; i++)
    {
        char ch       = chNext;
        chNext        = styler.SafeGetCharAt(i + 1);
        int stylePrev = style;
        style         = styleNext;
        styleNext     = styler.StyleAt(i + 1);
        bool atEOL    = (ch == '\r' && chNext != '\n') || (ch == '\n');
        if (foldComment && style == AhkStyles::CommentBlock)
        {
            if (stylePrev != AhkStyles::CommentBlock)
            {
                levelNext++;
            }
            else if ((styleNext != AhkStyles::CommentBlock) && !atEOL)
            {
                // Comments don't end at end of line and the next character may be unstyled.
                levelNext--;
            }
        }
        if (style == AhkStyles::SynOperator)
        {
            if (ch == '(' || ch == '{')
            {
                levelNext++;
            }
            else if (ch == ')' || ch == '}')
            {
                levelNext--;
            }
        }
        if (atEOL)
        {
            int level = levelCurrent;
            if (bOnlySpaces && foldCompact)
            {
                // Empty line
                level |= SC_FOLDLEVELWHITEFLAG;
            }
            if (!bOnlySpaces && levelNext > levelCurrent)
            {
                level |= SC_FOLDLEVELHEADERFLAG;
            }
            if (level != styler.LevelAt(lineCurrent))
            {
                styler.SetLevel(lineCurrent, level);
            }
            lineCurrent++;
            levelCurrent = levelNext;
            bOnlySpaces  = true;
        }
        if (!isspacechar(ch))
        {
            bOnlySpaces = false;
        }
    }
}

LexerModule lmAhk(SCLEX_AUTOMATIC + 103, LexerAhk::LexerFactoryAHK, "bp_ahk", ahkWordLists);
