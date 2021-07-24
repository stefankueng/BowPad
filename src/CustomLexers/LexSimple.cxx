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
#include <set>
#include <vector>
#include <regex>

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

using namespace Scintilla;
using namespace Lexilla;

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
    Word9,
    MarkedWord1,
    MarkedWord2
};

struct OptionsSimple
{
    std::string stringChars;
    bool        styleNumbers = false;
    std::string operators;

    std::vector<std::string> lineComments;
    std::string              lineComment;
    std::string              inlineCommentStart;
    std::string              inlineCommentEnd;

    std::string markedWords1;
    std::string markedWords2;

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

    std::vector<std::string> operatorsVec;

    bool fold         = false;
    bool foldComments = false;
    bool foldAtWs1    = false;
    bool foldAtWs2    = false;
    bool foldAtWs3    = false;
    bool foldAtWs4    = false;
    bool foldAtWs5    = false;
    bool foldAtWs6    = false;
    bool foldAtWs7    = false;
    bool foldAtWs8    = false;
    bool foldAtWs9    = false;

    void initOperators()
    {
        stringtok(operatorsVec, operators, true, " \t\n");
    }
    void initLineComments()
    {
        stringtok(lineComments, lineComment, true, " \t\n");
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
    nullptr,
};

struct OptionSetSimple : public OptionSet<OptionsSimple>
{
    OptionSetSimple()
    {
        DefineProperty("stringchars", &OptionsSimple::stringChars);
        DefineProperty("stylenumbers", &OptionsSimple::styleNumbers);
        DefineProperty("operators", &OptionsSimple::operators);

        DefineProperty("linecomment", &OptionsSimple::lineComment);
        DefineProperty("inlineCommentStart", &OptionsSimple::inlineCommentStart);
        DefineProperty("inlineCommentEnd", &OptionsSimple::inlineCommentEnd);
        DefineProperty("eol", &OptionsSimple::eol);

        DefineProperty("markedWords1", &OptionsSimple::markedWords1);
        DefineProperty("markedWords2", &OptionsSimple::markedWords2);

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
        DefineProperty("foldAtWS1", &OptionsSimple::foldAtWs1);
        DefineProperty("foldAtWS2", &OptionsSimple::foldAtWs2);
        DefineProperty("foldAtWS3", &OptionsSimple::foldAtWs3);
        DefineProperty("foldAtWS4", &OptionsSimple::foldAtWs4);
        DefineProperty("foldAtWS5", &OptionsSimple::foldAtWs5);
        DefineProperty("foldAtWS6", &OptionsSimple::foldAtWs6);
        DefineProperty("foldAtWS7", &OptionsSimple::foldAtWs7);
        DefineProperty("foldAtWS8", &OptionsSimple::foldAtWs8);
        DefineProperty("foldAtWS9", &OptionsSimple::foldAtWs9);

        DefineWordListSets(simpleWordLists);
    }
};

bool IsAWordChar(const int ch)
{
    return (ch < 0x80) && (isalnum(ch) || ch == '.' || ch == '_' || ch == '$');
}

bool IsAnOperator(StyleContext* sc, const std::vector<std::string>& operators)
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
    bool Contains(const char* s) const
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
    std::set<int>    wordChars;

public:
    LexerSimple()
        : DefaultLexer("simple", SCLEX_AUTOMATIC + 100)
        , m_pSciMsg(nullptr)
        , m_pSciWndData(0)
        , m_annotations(nullptr)
    {
    }

    ~LexerSimple() override
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

    void* SCI_METHOD PrivateCall(int code, void* param) override
    {
        switch (code)
        {
            case 100:
                m_pSciMsg     = reinterpret_cast<SciFnDirect>(SendMessage(static_cast<HWND>(param), SCI_GETDIRECTFUNCTION, 0, 0));
                m_pSciWndData = static_cast<sptr_t>(SendMessage(static_cast<HWND>(param), SCI_GETDIRECTPOINTER, 0, 0));
                break;
            case 101:
                m_annotations = static_cast<std::map<std::string, std::string>*>(param);
                break;
            default:
                break;
        }
        return nullptr;
    }

    bool checkLineComments(Lexilla::StyleContext* sc)
    {
        for (const auto& cs : options.lineComments)
        {
            if (sc->Match(cs.c_str()))
                return true;
        }
        return false;
    }
    sptr_t Call(unsigned int iMessage, uptr_t wParam = 0, sptr_t lParam = 0) const
    {
        return m_pSciMsg(m_pSciWndData, iMessage, wParam, lParam);
    }

    static ILexer5* LexerFactorySimple()
    {
        return new LexerSimple();
    }

private:
    SciFnDirect                         m_pSciMsg;
    sptr_t                              m_pSciWndData;
    std::map<std::string, std::string>* m_annotations;
};

Sci_Position SCI_METHOD LexerSimple::PropertySet(const char* key, const char* val)
{
    if (osSimple.PropertySet(&options, key, val))
    {
        if (strcmp(key, "operators") == 0)
        {
            options.initOperators();
        }
        if (strcmp(key, "linecomment") == 0)
        {
            options.initLineComments();
        }
        return 0;
    }
    return -1;
}

Sci_Position SCI_METHOD LexerSimple::WordListSet(int n, const char* wl)
{
    WordListAbridged* wordListAbridgedN = nullptr;
    switch (n)
    {
        case 0:
            wordListAbridgedN                = &keywords1;
            wordListAbridgedN->caseSensitive = options.ws1CaseSensitive;
            break;
        case 1:
            wordListAbridgedN                = &keywords2;
            wordListAbridgedN->caseSensitive = options.ws2CaseSensitive;
            break;
        case 2:
            wordListAbridgedN                = &keywords3;
            wordListAbridgedN->caseSensitive = options.ws3CaseSensitive;
            break;
        case 3:
            wordListAbridgedN                = &keywords4;
            wordListAbridgedN->caseSensitive = options.ws4CaseSensitive;
            break;
        case 4:
            wordListAbridgedN                = &keywords5;
            wordListAbridgedN->caseSensitive = options.ws5CaseSensitive;
            break;
        case 5:
            wordListAbridgedN                = &keywords6;
            wordListAbridgedN->caseSensitive = options.ws6CaseSensitive;
            break;
        case 6:
            wordListAbridgedN                = &keywords7;
            wordListAbridgedN->caseSensitive = options.ws7CaseSensitive;
            break;
        case 7:
            wordListAbridgedN                = &keywords8;
            wordListAbridgedN->caseSensitive = options.ws8CaseSensitive;
            break;
        case 8:
            wordListAbridgedN                = &keywords9;
            wordListAbridgedN->caseSensitive = options.ws9CaseSensitive;
            break;
        default:
            break;
    }
    Sci_Position firstModification = -1;
    if (wordListAbridgedN)
    {
        WordListAbridged wlNew;
        wlNew.Set(wl);
        if (*wordListAbridgedN != wlNew)
        {
            wordListAbridgedN->Set(wl);
            firstModification = 0;
        }
        wordChars.insert('_');
        for (int i = 0; i < wordListAbridgedN->Length(); ++i)
        {
            auto word = wordListAbridgedN->WordAt(i);

            if (!IsAlphaNumeric(word[0]))
                wordChars.insert(word[0]);
        }
    }
    return firstModification;
}

void SCI_METHOD LexerSimple::Lex(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess)
{
    int    visibleChars = 0;
    bool   numberIsHex  = false;
    size_t lineSize     = 1000;
    auto   line         = std::make_unique<char[]>(lineSize);

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
                if (IsASpaceOrTab(sc.ch) || IsAWordChar(sc.ch) || sc.ch == '\r' || sc.ch == '\n' || IsAnOperator(&sc, options.operatorsVec))
                    sc.SetState(SimpleStyles::Default);
                break;
            case SimpleStyles::Number:
                if (IsASpaceOrTab(sc.ch) || sc.ch == '\r' || sc.ch == '\n' || sc.ch == ',' || sc.ch == ';' || IsAnOperator(&sc, options.operatorsVec))
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

                    // first check for markedWords
                    if (!options.markedWords1.empty() && (strncmp(options.markedWords1.c_str(), s, options.markedWords1.size()) == 0))
                    {
                        sc.ChangeState(SimpleStyles::MarkedWord1);
                    }
                    else if (!options.markedWords2.empty() && (strncmp(options.markedWords2.c_str(), s, options.markedWords2.size()) == 0))
                    {
                        sc.ChangeState(SimpleStyles::MarkedWord2);
                    }

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
                if (options.stringChars.find(sc.ch) != std::string::npos)
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
            case SimpleStyles::MarkedWord1:
            case SimpleStyles::MarkedWord2:
                if (IsASpaceOrTab(sc.ch) || sc.ch == '\r' || sc.ch == '\n' || IsAnOperator(&sc, options.operatorsVec))
                    sc.SetState(SimpleStyles::Default);
                break;
            default:
                break;
        }

        // Determine if a new state should be entered.
        if (sc.state == SimpleStyles::Default)
        {
            if (options.styleNumbers && !IsAlphaNumeric(sc.chPrev) &&
                (IsADigit(sc.ch) ||
                 (sc.ch == '.' && IsADigit(sc.chNext)) ||
                 ((sc.ch == '-' || sc.ch == '+') && (IsADigit(sc.chNext) || sc.chNext == '.')) ||
                 (MakeLowerCase(sc.ch) == 'e' && (IsADigit(sc.chNext) || sc.chNext == '+' || sc.chNext == '-'))))
            {
                if ((sc.ch == '0' && MakeLowerCase(sc.chNext) == 'x') ||
                    ((sc.ch == '-' || sc.ch == '+') && sc.chNext == '0' && MakeLowerCase(sc.GetRelativeCharacter(2)) == 'x'))
                {
                    numberIsHex = true;
                }
                sc.SetState(SimpleStyles::Number);
            }
            else if (sc.Match(options.inlineCommentStart.c_str()))
            {
                sc.SetState(SimpleStyles::Comment);
            }
            else if (checkLineComments(&sc))
            {
                sc.SetState(SimpleStyles::CommentLine);
            }
            else if (iswordstart(sc.ch) || (std::find(wordChars.cbegin(), wordChars.cend(), sc.ch) != wordChars.cend()))
            {
                sc.SetState(SimpleStyles::Identifier);
            }
            else if (options.stringChars.find(sc.ch) != std::string::npos)
            {
                sc.SetState(SimpleStyles::String);
            }
            else if (IsAnOperator(&sc, options.operatorsVec))
            {
                sc.SetState(SimpleStyles::Operator);
            }
            else if (sc.Match(options.markedWords1.c_str()))
            {
                sc.SetState(SimpleStyles::MarkedWord1);
            }
            else if (sc.Match(options.markedWords2.c_str()))
            {
                sc.SetState(SimpleStyles::MarkedWord2);
            }
        }

        if (sc.atLineEnd)
        {
            // Reset states to beginning of colourise so no surprises
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
    bool inWs1Fold     = false;
    bool inWs2Fold     = false;
    bool inWs3Fold     = false;
    bool inWs4Fold     = false;
    bool inWs5Fold     = false;
    bool inWs6Fold     = false;
    bool inWs7Fold     = false;
    bool inWs8Fold     = false;
    bool inWs9Fold     = false;

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

        auto foldAtWs = [&](bool fold, int wsStyle, bool& inFold) {
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

        foldAtWs(options.foldAtWs1, SimpleStyles::Word1, inWs1Fold);
        foldAtWs(options.foldAtWs2, SimpleStyles::Word2, inWs2Fold);
        foldAtWs(options.foldAtWs3, SimpleStyles::Word3, inWs3Fold);
        foldAtWs(options.foldAtWs4, SimpleStyles::Word4, inWs4Fold);
        foldAtWs(options.foldAtWs5, SimpleStyles::Word5, inWs5Fold);
        foldAtWs(options.foldAtWs6, SimpleStyles::Word6, inWs6Fold);
        foldAtWs(options.foldAtWs7, SimpleStyles::Word7, inWs7Fold);
        foldAtWs(options.foldAtWs8, SimpleStyles::Word8, inWs8Fold);
        foldAtWs(options.foldAtWs9, SimpleStyles::Word9, inWs9Fold);
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

LexerModule lmSimple(SCLEX_AUTOMATIC + 100, LexerSimple::LexerFactorySimple, "bp_simple", simpleWordLists);
