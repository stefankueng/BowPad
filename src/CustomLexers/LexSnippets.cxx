// This file is part of BowPad.
//
// Copyright (C) 2021 - Stefan Kueng
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
#include <stdarg.h>
#include <cassert>
#include <ctype.h>
#include <vector>

#include "StringUtils.h"

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "../scintilla/lexlib/LexAccessor.h"
#include "../scintilla/lexlib/StyleContext.h"
#include "../scintilla/lexlib/CharacterSet.h"
#include "../scintilla/lexlib/LexerModule.h"
#include "../scintilla/lexlib/DefaultLexer.h"

using namespace Scintilla;

namespace
{
// Use an unnamed namespace to protect the functions and classes from name conflicts
enum SnippetsStyles
{
    Default = 0,
    CaretStart,
    CaretEnd,
    Mark,
    Mark0,
};

} // namespace


class LexerSnippets : public DefaultLexer
{

public:
    LexerSnippets()
        : DefaultLexer("Snippets", SCLEX_AUTOMATIC + 102)
    {
    }

    virtual ~LexerSnippets()
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

    Sci_Position SCI_METHOD PropertySet(const char* key, const char* val) override;

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
        return nullptr;
    }

    static ILexer5* LexerFactorySimple()
    {
        return new LexerSnippets();
    }
};

Sci_Position SCI_METHOD LexerSnippets::PropertySet(const char* /*key*/, const char* /*val*/)
{
    return -1;
}

void SCI_METHOD LexerSnippets::Lex(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess)
{
    LexAccessor  styler(pAccess);
    StyleContext sc(startPos, length, initStyle, styler);

    for (; sc.More(); sc.Forward())
    {
        // Determine if the current state should terminate.
        switch (sc.state)
        {
            case SnippetsStyles::Default:
                if (sc.ch == '^' && sc.chPrev != '\\' && IsADigit(sc.chNext))
                {
                    sc.SetState(SnippetsStyles::CaretStart);
                }
                break;
            case SnippetsStyles::CaretStart:
                if (sc.ch == '0')
                    sc.SetState(SnippetsStyles::Mark0);
                else
                    sc.SetState(SnippetsStyles::Mark);
                break;
            case SnippetsStyles::Mark:
            case SnippetsStyles::Mark0:
                if (sc.ch == '^' && sc.chPrev != '\\')
                    sc.SetState(SnippetsStyles::CaretEnd);
                break;
            case SnippetsStyles::CaretEnd:
                if (sc.ch == '^' && sc.chPrev != '\\' && IsADigit(sc.chNext))
                {
                    sc.SetState(SnippetsStyles::CaretStart);
                }
                else
                sc.SetState(SnippetsStyles::Default);
                break;
        }
    }
    sc.Complete();
}

void SCI_METHOD LexerSnippets::Fold(Sci_PositionU /*startPos*/, Sci_Position /*length*/, int /*initStyle*/, IDocument* /*pAccess*/)
{
    // no folding
}

LexerModule lmSnippets(SCLEX_AUTOMATIC + 102, LexerSnippets::LexerFactorySimple, "Snippets", nullptr);
