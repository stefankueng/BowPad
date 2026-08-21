// This file is part of BowPad.
//
// Copyright (C) 2026 - Stefan Kueng
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
enum CSVStyles
{
    Delimiter = 0,
    Col0,
    Col1,
    Col2,
    Col3,
    Col4,
    Col5,
    Col6,
    Col7,
    Col8,
    Col9
};
}


class LexerCSV : public DefaultLexer
{
public:
    LexerCSV()
        : DefaultLexer("csv", SCLEX_AUTOMATIC + 104)
    {
    }

    ~LexerCSV() override
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

    const char* SCI_METHOD  PropertyGet(const char* /*key*/) override
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

    void SCI_METHOD  Lex(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess) override;

    void SCI_METHOD  Fold(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess) override;

    void* SCI_METHOD PrivateCall(int, void*) override
    {
        return nullptr;
    }

    static ILexer5* LexerFactorySimple()
    {
        return new LexerCSV();
    }
};


void SCI_METHOD LexerCSV::Lex(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument* pAccess)
{
    size_t       lineSize    = 1000;
    auto         line        = std::make_unique<char[]>(lineSize);

    LexAccessor  styler(pAccess);
    StyleContext sc(startPos, length, initStyle, styler);
    char         delimiter = '\0';
    CSVStyles    csvState     = CSVStyles::Col0;
    for (; sc.More(); sc.Forward())
    {
        if (sc.atLineStart)
        {
            csvState     = CSVStyles::Col0;
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

            // count how many ";", "," or "\t" are there to determine which is the delimiter in this csv file.
            if (delimiter == '\0')
            {
                auto semicolonCount = std::count(sLine.begin(), sLine.end(), ';');
                auto commaCount     = std::count(sLine.begin(), sLine.end(), ',');
                auto tabCount       = std::count(sLine.begin(), sLine.end(), '\t');
                if (semicolonCount > commaCount && semicolonCount > tabCount)
                    delimiter = ';';
                else if (commaCount > semicolonCount && commaCount > tabCount)
                    delimiter = ',';
                else if (tabCount > semicolonCount && tabCount > commaCount)
                    delimiter = '\t';
            }
            sc.SetState(CSVStyles::Col0);
        }
        // Determine if the current state should terminate.
        if (delimiter != '\0' && sc.ch == delimiter)
        {
            sc.SetState(CSVStyles::Delimiter);
            csvState = static_cast<CSVStyles>((static_cast<int>(csvState) + 1) % 10);
        }
        else if (sc.state == CSVStyles::Delimiter)
        {
            sc.SetState(csvState);
        }

        if (sc.atLineEnd)
        {
            // Reset states to begining of colourise so no surprises
            // if different sets of lines lexed.
            csvState = CSVStyles::Col0;
        }
    }
    sc.Complete();
}

void SCI_METHOD LexerCSV::Fold(Sci_PositionU /*startPos*/, Sci_Position /*length*/, int /*initStyle*/, IDocument* /*pAccess*/)
{
    // no folding : log files are usually big, and this simply is too slow
}

LexerModule lmCSV(SCLEX_AUTOMATIC + 104, LexerCSV::LexerFactorySimple, "bp_csv", nullptr);
