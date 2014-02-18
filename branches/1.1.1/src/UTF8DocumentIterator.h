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

#include <stdlib.h>
#include <iterator>
#include <vector>
#include "scintilla.h"
#define PLATFORM_ASSERT(c) ((void)0)
#include "../ext/scintilla/src/SplitVector.h"
#include "../ext/scintilla/src/Partitioning.h"
#include "../ext/scintilla/src/RunStyles.h"
#include "../ext/scintilla/src/CellBuffer.h"
#include "../ext/scintilla/src/CharClassify.h"
#include "../ext/scintilla/src/Decoration.h"
#include "../ext/scintilla/src/CaseFolder.h"
#include "ILexer.h"
#include "../ext/scintilla/src/RESearch.h"
#include "../ext/scintilla/src/Document.h"

class UTF8DocumentIterator : public std::iterator<std::bidirectional_iterator_tag, char>
{
public:
    UTF8DocumentIterator()
        : m_doc(0)
        , m_pos(0)
        , m_end(0)
    {
    }

    UTF8DocumentIterator(Scintilla::Document* doc, int pos, int end)
        : m_doc(doc)
        , m_pos(pos)
        , m_end(end)
    {
        if (m_pos > m_end)
        {
            m_pos = m_end;
        }
    }

    UTF8DocumentIterator(const UTF8DocumentIterator& copy)
        : m_doc(copy.m_doc)
        , m_pos(copy.m_pos)
        , m_end(copy.m_end)
    {
        if (m_pos > m_end)
        {
            m_pos = m_end;
        }
    }

    bool operator == (const UTF8DocumentIterator& other) const
    {
        return (ended() == other.ended()) && (m_doc == other.m_doc) && (m_pos == other.m_pos);
    }

    bool operator != (const UTF8DocumentIterator& other) const
    {
        return !(*this == other);
    }

    char operator * () const
    {
        return m_doc->CharAt(m_pos);
    }

    UTF8DocumentIterator& operator = (int other)
    {
        m_pos = other;
        return *this;
    }

    UTF8DocumentIterator& operator ++ ()
    {
        m_pos++;

        if (m_pos > m_end)
        {
            m_pos = m_end;
        }
        return *this;
    }

    UTF8DocumentIterator operator++(int)
    {
        UTF8DocumentIterator temp = *this;
        operator++();
        return temp;
    }

    UTF8DocumentIterator& operator -- ()
    {
        --m_pos;
        return *this;
    }

    int pos() const
    {
        return m_pos;
    }

private:

    bool ended() const
    {
        return m_pos >= m_end;
    }

    int m_pos;
    int m_end;
    Scintilla::Document* m_doc;
};

