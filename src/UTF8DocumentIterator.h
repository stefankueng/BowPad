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

class UTF8DocumentIterator : public std::iterator<std::bidirectional_iterator_tag, wchar_t>
{
public:
    UTF8DocumentIterator()
        : m_doc(0)
        , m_pos(0)
        , m_end(0)
        , m_characterIndex(0)
        , m_utf8Length(0)
        , m_utf16Length(0)
    {
    }

    UTF8DocumentIterator(Scintilla::Document* doc, int pos, int end)
        : m_doc(doc)
        , m_pos(pos)
        , m_end(end)
        , m_characterIndex(0)
    {
        if (m_pos > m_end)
        {
            m_pos = m_end;
        }
        readCharacter();
    }

    UTF8DocumentIterator(const UTF8DocumentIterator& copy)
        : m_doc(copy.m_doc)
        , m_pos(copy.m_pos)
        , m_end(copy.m_end)
        , m_characterIndex(copy.m_characterIndex)
        , m_utf8Length(copy.m_utf8Length)
        , m_utf16Length(copy.m_utf16Length)
    {
        m_character[0] = copy.m_character[0];
        m_character[1] = copy.m_character[1];

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

    wchar_t operator * () const
    {
        return m_character[m_characterIndex];
    }

    UTF8DocumentIterator& operator = (int other)
    {
        m_pos = other;
        return *this;
    }

    UTF8DocumentIterator& operator ++ ()
    {
        if (2 == m_utf16Length && 0 == m_characterIndex)
        {
            m_characterIndex = 1;
        }
        else
        {
            m_pos += m_utf8Length;

            if (m_pos > m_end)
            {
                m_pos = m_end;
            }
            m_characterIndex = 0;
            readCharacter();
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
        if (m_utf16Length == 2 && m_characterIndex == 1)
        {
            m_characterIndex = 0;
        }
        else
        {
            --m_pos;
            // Skip past the UTF-8 extension bytes
            while (0x80 == (m_doc->CharAt(m_pos) & 0xC0) && m_pos > 0)
                --m_pos;

            readCharacter();
            if (m_utf16Length == 2)
            {
                m_characterIndex = 1;
            }
        }
        return *this;
    }

    int pos() const
    {
        return m_pos;
    }

private:
    void readCharacter();


    bool ended() const
    {
        return m_pos >= m_end;
    }

    int m_pos;
    wchar_t m_character[2];
    int m_characterIndex;
    int m_end;
    int m_utf8Length;
    int m_utf16Length;
    Scintilla::Document* m_doc;
    static const unsigned char m_firstByteMask[];
};

