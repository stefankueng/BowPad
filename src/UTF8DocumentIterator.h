// This file is part of BowPad.
//
// Copyright (C) 2013, 2016-2018, 2021 - Stefan Kueng
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

#include <iterator>
#define PLATFORM_ASSERT(c) ((void)0)
#include "../ext/scintilla/src/Document.h"
#include "../ext/scintilla/src/uniconversion.h"
#include "../ext/scintilla/src/Position.h"

class UTF8DocumentIterator
{
    // These 3 fields determine the iterator position and are used for comparisons
    const Scintilla::Document *doc;
    Sci::Position position;
    size_t characterIndex;
    // Remaining fields are derived from the determining fields so are excluded in comparisons
    unsigned int lenBytes;
    size_t lenCharacters;
    wchar_t buffered[2];
public:
    // ReSharper disable CppInconsistentNaming
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = wchar_t;
    using difference_type = ptrdiff_t;
    using pointer = wchar_t*;
    using reference = wchar_t&;
    // ReSharper restore CppInconsistentNaming

    UTF8DocumentIterator(const Scintilla::Document *doc = nullptr, Sci::Position position = 0) :
        doc(doc), position(position), characterIndex(0), lenBytes(0), lenCharacters(0)
    {
        buffered[0] = 0;
        buffered[1] = 0;
        if (doc)
        {
            ReadCharacter();
        }
    }
    UTF8DocumentIterator(const UTF8DocumentIterator &other)
    {
        doc = other.doc;
        position = other.position;
        characterIndex = other.characterIndex;
        lenBytes = other.lenBytes;
        lenCharacters = other.lenCharacters;
        buffered[0] = other.buffered[0];
        buffered[1] = other.buffered[1];
    }
    UTF8DocumentIterator &operator=(const UTF8DocumentIterator &other)
    {
        if (this != &other)
        {
            doc = other.doc;
            position = other.position;
            characterIndex = other.characterIndex;
            lenBytes = other.lenBytes;
            lenCharacters = other.lenCharacters;
            buffered[0] = other.buffered[0];
            buffered[1] = other.buffered[1];
        }
        return *this;
    }
    wchar_t operator*() const
    {
        return buffered[characterIndex];
    }
    UTF8DocumentIterator &operator++()
    {
        if ((characterIndex + 1) < (lenCharacters))
        {
            characterIndex++;
        }
        else
        {
            position += lenBytes;
            ReadCharacter();
            characterIndex = 0;
        }
        return *this;
    }
    UTF8DocumentIterator operator++(int)
    {
        UTF8DocumentIterator retVal(*this);
        if ((characterIndex + 1) < (lenCharacters))
        {
            characterIndex++;
        }
        else
        {
            position += lenBytes;
            ReadCharacter();
            characterIndex = 0;
        }
        return retVal;
    }
    UTF8DocumentIterator &operator--()
    {
        if (characterIndex)
        {
            characterIndex--;
        }
        else
        {
            position = doc->NextPosition(position, -1);
            ReadCharacter();
            characterIndex = lenCharacters - 1;
        }
        return *this;
    }
    bool operator==(const UTF8DocumentIterator &other) const
    {
        // Only test the determining fields, not the character widths and values derived from this
        return doc == other.doc &&
            position == other.position &&
            characterIndex == other.characterIndex;
    }
    bool operator!=(const UTF8DocumentIterator &other) const
    {
        // Only test the determining fields, not the character widths and values derived from this
        return doc != other.doc ||
            position != other.position ||
            characterIndex != other.characterIndex;
    }
    Sci::Position Pos() const
    {
        return position;
    }
    Sci::Position PosRoundUp() const
    {
        if (characterIndex)
            return position + lenBytes;	// Force to end of character
        else
            return position;
    }

private:
    void ReadCharacter()
    {
        Scintilla::Document::CharacterExtracted charExtracted = doc->ExtractCharacter(position);
        lenBytes = charExtracted.widthBytes;
        if (charExtracted.character == Scintilla::unicodeReplacementChar)
        {
            lenCharacters = 1;
            buffered[0] = static_cast<wchar_t>(charExtracted.character);
        }
        else
        {
            lenCharacters = Scintilla::UTF16FromUTF32Character(charExtracted.character, buffered);
        }
    }
};


