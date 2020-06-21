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
enum SimpleStyles
{
    Default = 0,
    Comment,
    CommentLine,
    Number,
    String,
    Operator,
    Identifier,
    Word1,
    Word2,
    Word3,
    Word4,
    Word5,
    Word6,
    Word7,
    Word8,
    Word9
};

struct OptionsSimple
{
    std::string stringchars;
    bool        stylenumbers = false;
    std::string operators;

    std::string linecomment;
    std::string inlineCommentStart;
    std::string inlineCommentEnd;

    int  eol              = 0;
    bool ws1CaseSensitive = false;
    bool ws2CaseSensitive = false;
    bool ws3CaseSensitive = false;
    bool ws4CaseSensitive = false;
    bool ws5CaseSensitive = false;
    bool ws6CaseSensitive = false;
    bool ws7CaseSensitive = false;
    bool ws8CaseSensitive = false;
    bool ws9CaseSensitive = false;

    std::vector<std::string> operatorsvec;

    bool fold         = false;
    bool foldComments = false;
    bool foldAtWS1    = false;
    bool foldAtWS2    = false;
    bool foldAtWS3    = false;
    bool foldAtWS4    = false;
    bool foldAtWS5    = false;
    bool foldAtWS6    = false;
    bool foldAtWS7    = false;
    bool foldAtWS8    = false;
    bool foldAtWS9    = false;

    void initOperators()
    {
        stringtok(operatorsvec, operators, true, " \t\n");
    }
};

const char* const simpleWordLists[] = {
    "keywords 1",
    "keywords 2",
    "keywords 3",
    "keywords 4",
    "keywords 5",
    "keywords 6",
    "keywords 7",
    "keywords 8",
    "keywords 9",
    0,
};

struct OptionSetSimple : public OptionSet<OptionsSimple>
{
    OptionSetSimple()
    {
        DefineProperty("stringchars", &OptionsSimple::stringchars);
        DefineProperty("stylenumbers", &OptionsSimple::stylenumbers);
        DefineProperty("operators", &OptionsSimple::operators);

        DefineProperty("linecomment", &OptionsSimple::linecomment);
        DefineProperty("inlineCommentStart", &OptionsSimple::inlineCommentStart);
        DefineProperty("inlineCommentEnd", &OptionsSimple::inlineCommentEnd);
        DefineProperty("eol", &OptionsSimple::eol);

        DefineProperty("ws1CaseSensitive", &OptionsSimple::ws1CaseSensitive);
        DefineProperty("ws2CaseSensitive", &OptionsSimple::ws2CaseSensitive);
        DefineProperty("ws3CaseSensitive", &OptionsSimple::ws3CaseSensitive);
        DefineProperty("ws4CaseSensitive", &OptionsSimple::ws4CaseSensitive);
        DefineProperty("ws5CaseSensitive", &OptionsSimple::ws5CaseSensitive);
        DefineProperty("ws6CaseSensitive", &OptionsSimple::ws6CaseSensitive);
        DefineProperty("ws7CaseSensitive", &OptionsSimple::ws7CaseSensitive);
        DefineProperty("ws8CaseSensitive", &OptionsSimple::ws8CaseSensitive);
        DefineProperty("ws9CaseSensitive", &OptionsSimple::ws9CaseSensitive);

        DefineProperty("fold", &OptionsSimple::fold);
        DefineProperty("foldComments", &OptionsSimple::foldComments);
        DefineProperty("foldAtWS1", &OptionsSimple::foldAtWS1);
        DefineProperty("foldAtWS2", &OptionsSimple::foldAtWS2);
        DefineProperty("foldAtWS3", &OptionsSimple::foldAtWS3);
        DefineProperty("foldAtWS4", &OptionsSimple::foldAtWS4);
        DefineProperty("foldAtWS5", &OptionsSimple::foldAtWS5);
        DefineProperty("foldAtWS6", &OptionsSimple::foldAtWS6);
        DefineProperty("foldAtWS7", &OptionsSimple::foldAtWS7);
        DefineProperty("foldAtWS8", &OptionsSimple::foldAtWS8);
        DefineProperty("foldAtWS9", &OptionsSimple::foldAtWS9);

        DefineWordListSets(simpleWordLists);
    }
};

static inline bool IsAWordChar(const int ch)
{
    return (ch < 0x80) && (isalnum(ch) || ch == '.' || ch == '_' || ch == '$');
}

static inline bool IsAnOperator(StyleContext* sc, const std::vector<std::string>& operators)
{
    if (IsAlphaNumeric(sc->ch))
        return false;
    for (const auto& op : operators)
    {
        if (sc->Match(op.c_str()))
            return true;
    }
    return false;
}

class WordListAbridged : public WordList
{
public:
    WordListAbridged()
    {
        caseSensitive = false;
    };
    ~WordListAbridged()
    {
        Clear();
    };
    bool caseSensitive;
    bool Contains(const char* s)
    {
        return InList(s);
    };
};

} // namespace

class LexerSimple : public DefaultLexer
{
    WordListAbridged keywords1;
    WordListAbridged keywords2;
    WordListAbridged keywords3;
    WordListAbridged keywords4;
    WordListAbridged keywords5;
    WordListAbridged keywords6;
    WordListAbridged keywords7;
    WordListAbridged keywords8;
    WordListAbridged keywords9;
    OptionsSimple    options;
    OptionSetSimple  osSimple;
    std::set<int>    wordchars;

public:
    LexerSimple()
        : DefaultLexer("simple", SCLEX_AUTOMATIC + 100)
    {
    }

    virtual ~LexerSimple()
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

    Sci_Position SCI_METHOD WordListSet(int n, const char* wl) override;

    void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess) override;

    void SCI_METHOD Fold(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess) override;

    void* SCI_METHOD PrivateCall(int, void*) override
    {
        return NULL;
    }

    static ILexer5* LexerFactorySimple()
    {
        return new LexerSimple();
    }
};

Sci_Position SCI_METHOD LexerSimple::PropertySet(const char* key, const char* val)
{
    if (osSimple.PropertySet(&options, key, val))
    {
        if (strcmp(key, "operators") == 0)
        {
            options.initOperators();
        }
        return 0;
    }
    return -1;
}

Sci_Position SCI_METHOD LexerSimple::WordListSet(int n, const char* wl)
{
    WordListAbridged* WordListAbridgedN = 0;
    switch (n)
    {
        case 0:
            WordListAbridgedN                = &keywords1;
            WordListAbridgedN->caseSensitive = options.ws1CaseSensitive;
            break;
        case 1:
            WordListAbridgedN                = &keywords2;
            WordListAbridgedN->caseSensitive = options.ws2CaseSensitive;
            break;
        case 2:
            WordListAbridgedN                = &keywords3;
            WordListAbridgedN->caseSensitive = options.ws3CaseSensitive;
            break;
        case 3:
            WordListAbridgedN                = &keywords4;
            WordListAbridgedN->caseSensitive = options.ws4CaseSensitive;
            break;
        case 4:
            WordListAbridgedN                = &keywords5;
            WordListAbridgedN->caseSensitive = options.ws5CaseSensitive;
            break;
        case 5:
            WordListAbridgedN                = &keywords6;
            WordListAbridgedN->caseSensitive = options.ws6CaseSensitive;
            break;
        case 6:
            WordListAbridgedN                = &keywords7;
            WordListAbridgedN->caseSensitive = options.ws7CaseSensitive;
            break;
        case 7:
            WordListAbridgedN                = &keywords8;
            WordListAbridgedN->caseSensitive = options.ws8CaseSensitive;
            break;
        case 8:
            WordListAbridgedN                = &keywords9;
            WordListAbridgedN->caseSensitive = options.ws9CaseSensitive;
            break;
    }
    Sci_Position firstModification = -1;
    if (WordListAbridgedN)
    {
        WordListAbridged wlNew;
        wlNew.Set(wl);
        if (*WordListAbridgedN != wlNew)
        {
            WordListAbridgedN->Set(wl);
            firstModification = 0;
        }
        wordchars.insert('_');
        for (int i = 0; i < WordListAbridgedN->Length(); ++i)
        {
            auto word = WordListAbridgedN->WordAt(i);

            if (!IsAlphaNumeric(word[0]))
                wordchars.insert(word[0]);
        }
    }
    return firstModification;
}

void SCI_METHOD LexerSimple::Lex(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess)
{
    int  visibleChars = 0;
    bool numberIsHex  = false;

    LexAccessor  styler(pAccess);
    StyleContext sc(startPos, length, initStyle, styler);

    for (; sc.More(); sc.Forward())
    {
        // Determine if the current state should terminate.
        switch (sc.state)
        {
            case SimpleStyles::Comment:
                if (sc.Match(options.inlineCommentEnd.c_str()))
                {
                    sc.Forward(options.inlineCommentEnd.size());
                    sc.SetState(SimpleStyles::Default);
                }
                break;
            case SimpleStyles::CommentLine:
                if (sc.ch == '\r' || sc.ch == '\n')
                {
                    sc.SetState(SimpleStyles::Default);
                }
                break;
            case SimpleStyles::Operator:
                if (IsASpaceOrTab(sc.ch) || IsAWordChar(sc.ch) || sc.ch == '\r' || sc.ch == '\n' || IsAnOperator(&sc, options.operatorsvec))
                    sc.SetState(SimpleStyles::Default);
                break;
            case SimpleStyles::Number:
                if (IsASpaceOrTab(sc.ch) || sc.ch == '\r' || sc.ch == '\n' || IsAnOperator(&sc, options.operatorsvec))
                {
                    sc.SetState(SimpleStyles::Default);
                }
                else if ((numberIsHex && !(MakeLowerCase(sc.ch) == 'x' || MakeLowerCase(sc.ch) == 'e' ||
                                           IsADigit(sc.ch, 16) || sc.ch == '.' || sc.ch == '-' || sc.ch == '+')) ||
                         (!numberIsHex && !(MakeLowerCase(sc.ch) == 'e' || IsADigit(sc.ch) || sc.ch == '.' || sc.ch == '-' || sc.ch == '+')))
                {
                    // check '-' for possible -10e-5. Add '+' as well.
                    numberIsHex = false;
                    sc.ChangeState(SimpleStyles::Identifier);
                    sc.SetState(SimpleStyles::Default);
                }
                break;
            case SimpleStyles::Identifier:
                if (!IsAWordChar(sc.ch))
                {
                    char s[1000];
                    char sl[1000];
                    sc.GetCurrentLowered(sl, _countof(sl));
                    sc.GetCurrent(s, _countof(s));

                    auto doKeyWords = [&](WordListAbridged& keywords, int style) {
                        if (keywords.Length())
                        {
                            if (keywords.caseSensitive)
                            {
                                if (keywords.Contains(s))
                                    sc.ChangeState(style);
                            }
                            else
                            {
                                if (keywords.Contains(sl))
                                {
                                    sc.ChangeState(style);
                                }
                            }
                        } };

                    doKeyWords(keywords1, SimpleStyles::Word1);
                    doKeyWords(keywords2, SimpleStyles::Word2);
                    doKeyWords(keywords3, SimpleStyles::Word3);
                    doKeyWords(keywords4, SimpleStyles::Word4);
                    doKeyWords(keywords5, SimpleStyles::Word5);
                    doKeyWords(keywords6, SimpleStyles::Word6);
                    doKeyWords(keywords7, SimpleStyles::Word7);
                    doKeyWords(keywords8, SimpleStyles::Word8);
                    doKeyWords(keywords9, SimpleStyles::Word9);

                    sc.SetState(SimpleStyles::Default);
                }
                break;
            case SimpleStyles::String:
                if (options.stringchars.find(sc.ch) != std::string::npos)
                {
                    sc.ForwardSetState(SimpleStyles::Default);
                }
                else if ((sc.atLineEnd) && (sc.chNext != options.eol))
                {
                    sc.ChangeState(SimpleStyles::String);
                    sc.ForwardSetState(SimpleStyles::Default);
                    visibleChars = 0;
                }
                break;
        }

        // Determine if a new state should be entered.
        if (sc.state == SimpleStyles::Default)
        {
            if (options.stylenumbers &&
                ((IsADigit(sc.ch) || (sc.ch == '.' && IsADigit(sc.chNext)) || ((sc.ch == '-' || sc.ch == '+') && (IsADigit(sc.chNext) || sc.chNext == '.')) || (MakeLowerCase(sc.ch) == 'e' && (IsADigit(sc.chNext) || sc.chNext == '+' || sc.chNext == '-')))))
            {
                if ((sc.ch == '0' && MakeLowerCase(sc.chNext) == 'x') ||
                    ((sc.ch == '-' || sc.ch == '+') && sc.chNext == '0' && MakeLowerCase(sc.GetRelativeCharacter(2)) == 'x'))
                {
                    numberIsHex = true;
                }
                sc.SetState(SimpleStyles::Number);
            }
            else if (iswordstart(sc.ch) || (std::find(wordchars.cbegin(), wordchars.cend(), sc.ch) != wordchars.cend()))
            {
                sc.SetState(SimpleStyles::Identifier);
            }
            else if (sc.Match(options.inlineCommentStart.c_str()))
            {
                sc.SetState(SimpleStyles::Comment);
            }
            else if (sc.Match(options.linecomment.c_str()))
            {
                sc.SetState(SimpleStyles::CommentLine);
            }
            else if (options.stringchars.find(sc.ch) != std::string::npos)
            {
                sc.SetState(SimpleStyles::String);
            }
            else if (IsAnOperator(&sc, options.operatorsvec))
            {
                sc.SetState(SimpleStyles::Operator);
            }
        }

        if (sc.atLineEnd)
        {
            // Reset states to begining of colourise so no surprises
            // if different sets of lines lexed.
            visibleChars = 0;
            numberIsHex  = false;
        }
        if (!IsASpace(sc.ch))
        {
            ++visibleChars;
        }
    }
    sc.Complete();
}

void SCI_METHOD LexerSimple::Fold(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess)
{
    if (!options.fold)
        return;

    LexAccessor   styler(pAccess);
    Sci_PositionU endPos       = startPos + length;
    Sci_Position  lineCurrent  = styler.GetLine(startPos);
    int           visibleChars = 0;

    // Backtrack to previous line in case need to fix its fold status
    if (startPos > 0)
    {
        if (lineCurrent > 0)
        {
            --lineCurrent;
            startPos = styler.LineStart(lineCurrent);
        }
    }

    int levelPrev = SC_FOLDLEVELBASE;
    if (lineCurrent > 0)
        levelPrev = styler.LevelAt(lineCurrent - 1) >> 16;
    int  levelCurrent  = levelPrev;
    char chNext        = styler[startPos];
    int  style         = initStyle;
    int  styleNext     = styler.StyleAt(startPos);
    bool inLineComment = false;
    bool inWS1Fold     = false;
    bool inWS2Fold     = false;
    bool inWS3Fold     = false;
    bool inWS4Fold     = false;
    bool inWS5Fold     = false;
    bool inWS6Fold     = false;
    bool inWS7Fold     = false;
    bool inWS8Fold     = false;
    bool inWS9Fold     = false;

    for (Sci_PositionU i = startPos; i < endPos; ++i)
    {
        char ch        = chNext;
        chNext         = styler.SafeGetCharAt(i + 1);
        style          = styleNext;
        styleNext      = styler.StyleAt(i + 1);
        int  stylePrev = (i) ? styler.StyleAt(i - 1) : SimpleStyles::Default;
        bool atEOL     = (ch == '\r' && chNext != '\n') || (ch == '\n');

        // fold comments
        if (options.foldComments && (style == SimpleStyles::Comment) || (style == SimpleStyles::CommentLine))
        {
            if (style != stylePrev)
            {
                if (!inLineComment)
                    ++levelCurrent;
                if (style == SimpleStyles::CommentLine)
                    inLineComment = true;
            }
            else if (style != styleNext)
            {
                int skipline = 2;
                if (styler.SafeGetCharAt(i + 2) == '\n')
                    ++skipline;

                if ((style != SimpleStyles::CommentLine) || (styleNext != SimpleStyles::Default) ||
                    (styler.StyleAt(i + skipline) != SimpleStyles::CommentLine))
                {
                    --levelCurrent;
                    inLineComment = false;
                }
            }
        }

        auto foldAtWS = [&](bool fold, int wsStyle, bool& inFold) {
            if (fold)
            {
                if (style == wsStyle)
                {
                    if (style != stylePrev)
                    {
                        ++levelCurrent;
                        inFold = true;
                    }
                }
                else if (inFold && styleNext == wsStyle)
                {
                    --levelCurrent;
                    inFold = false;
                }
            }
        };

        foldAtWS(options.foldAtWS1, SimpleStyles::Word1, inWS1Fold);
        foldAtWS(options.foldAtWS2, SimpleStyles::Word2, inWS2Fold);
        foldAtWS(options.foldAtWS3, SimpleStyles::Word3, inWS3Fold);
        foldAtWS(options.foldAtWS4, SimpleStyles::Word4, inWS4Fold);
        foldAtWS(options.foldAtWS5, SimpleStyles::Word5, inWS5Fold);
        foldAtWS(options.foldAtWS6, SimpleStyles::Word6, inWS6Fold);
        foldAtWS(options.foldAtWS7, SimpleStyles::Word7, inWS7Fold);
        foldAtWS(options.foldAtWS8, SimpleStyles::Word8, inWS8Fold);
        foldAtWS(options.foldAtWS9, SimpleStyles::Word9, inWS9Fold);
        if (atEOL || i == (endPos - 1))
        {
            int lev = levelPrev;
            lev |= levelCurrent << 16;
            if (visibleChars == 0)
                lev |= SC_FOLDLEVELWHITEFLAG;
            if ((levelCurrent > levelPrev) && (visibleChars > 0))
                lev |= SC_FOLDLEVELHEADERFLAG;
            if (lev != styler.LevelAt(lineCurrent))
            {
                styler.SetLevel(lineCurrent, lev);
            }
            ++lineCurrent;
            levelPrev    = levelCurrent;
            visibleChars = 0;
        }
        if (!isspacechar(ch))
            ++visibleChars;
    }
    int flagsNext = styler.LevelAt(lineCurrent) & ~SC_FOLDLEVELNUMBERMASK;
    styler.SetLevel(lineCurrent, levelPrev | flagsNext);
}

LexerModule lmSimple(SCLEX_AUTOMATIC + 100, LexerSimple::LexerFactorySimple, "simple", simpleWordLists);
