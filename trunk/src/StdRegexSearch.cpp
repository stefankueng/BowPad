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
#include "stdafx.h"
#include <iterator>
#include <regex>
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
#include "../ext/scintilla/src/UniConversion.h"
#include "UTF8DocumentIterator.h"

#ifdef SCI_NAMESPACE
using namespace Scintilla;
#endif

class StdRegexSearch : public RegexSearchBase
{
public:
    StdRegexSearch() : _substituted(NULL)
    {
    }

    virtual ~StdRegexSearch()
    {
        delete[] _substituted;
        _substituted = NULL;
    }

    virtual long FindText(Document* doc, int startPosition, int endPosition, const char *regex,
                          bool caseSensitive, bool word, bool wordStart, int sciSearchFlags, int *lengthRet);

    virtual const char *SubstituteByPosition(Document* doc, const char *text, int *length);

private:
    class SearchParameters;

    class Match
    {
    public:
        Match()
            : _document(NULL)
            , _position(-1)
            , _endPosition(-1)
            , _endPositionForContinuationCheck(-1)
        {
        }

        ~Match()
        {
            setDocument(NULL);
        }
        Match(Document* document, int position = -1, int endPosition = -1)
            : _document(NULL)
        {
            set(document, position, endPosition);
        }
        Match& operator=(Match& m)
        {
            set(m._document, m.position(), m.endPosition());
            return *this;
        }
        Match& operator=(int /*nullptr*/)
        {
            _position = -1;
            return *this;
        }

        void set(Document* document = NULL, int position = -1, int endPosition = -1)
        {
            setDocument(document);
            _position = position;
            _endPositionForContinuationCheck = _endPosition = endPosition;
        }

        bool isEmpty()
        {
            return _position == _endPosition;
        }
        int position()
        {
            return _position;
        }
        int endPosition()
        {
            return _endPosition;
        }
        int length()
        {
            return _endPosition - _position;
        }
        int found()
        {
            return _position >= 0;
        }

    private:
        void setDocument(Document* newDocument)
        {
            if (newDocument != _document)
            {
                _document = newDocument;
            }
        }

        Document* _document;
        int _position, _endPosition;
        int _endPositionForContinuationCheck;
    };

    class CharTPtr
    {
    public:
        CharTPtr(const char* ptr)
            : _charPtr(ptr)
        {
        }
        ~CharTPtr()
        {
        }
        operator const char*()
        {
            return _charPtr;
        }
    private:
        const char* _charPtr;
    };

    template <class CharT, class CharacterIterator>
    class EncodingDependent
    {
    public:
        EncodingDependent() : _lastCompileFlags(-1)
        {
        }
        void compileRegex(const char *regex, const int compileFlags);
        Match FindText(SearchParameters& search);
        char *SubstituteByPosition(const char *text, int *length);
    private:
        Match FindTextForward(SearchParameters& search);
        Match FindTextBackward(SearchParameters& search);

    public:
        typedef CharT Char;
        typedef std::basic_regex<CharT> Regex;
        typedef std::match_results<CharacterIterator> MatchResults;

        MatchResults _match;
    private:
        Regex _regex;
        std::string _lastRegexString;
        int _lastCompileFlags;
    };

    class SearchParameters
    {
    public:
        int nextCharacter(int position);
        bool isLineStart(int position);
        bool isLineEnd(int position);

        Document* _document;
        const char *_regexString;
        int _compileFlags;
        int _startPosition;
        int _endPosition;
        std::regex_constants::match_flag_type regexFlags;
        int _direction;
    };

    static char    *stringToCharPtr(const std::string& str);

    EncodingDependent<char, UTF8DocumentIterator> _utf8;

    char *_substituted;

    Match _lastMatch;
    int _lastDirection;
};

#ifdef SCI_NAMESPACE
namespace Scintilla
{
#endif

RegexSearchBase *CreateRegexSearch(CharClassify* /* charClassTable */)
{
    return new StdRegexSearch();
}

#ifdef SCI_NAMESPACE
}
#endif

/**
 * Find text in document, supporting both forward and backward
 * searches (just pass startPosition > endPosition to do a backward search).
 */

long StdRegexSearch::FindText(Document* doc, int startPosition, int endPosition, const char *regexString,
                        bool caseSensitive, bool /*word*/, bool /*wordStart*/, int /*sciSearchFlags*/, int *lengthRet)
{
    try
    {
        SearchParameters search;

        search._document = doc;

        if (startPosition > endPosition
            || startPosition == endPosition && _lastDirection < 0)  // If we search in an empty region, suppose the direction is the same as last search (this is only important to verify if there can be an empty match in that empty region).
        {
            search._startPosition = endPosition;
            search._endPosition = startPosition;
            search._direction = -1;
        }
        else
        {
            search._startPosition = startPosition;
            search._endPosition = endPosition;
            search._direction = 1;
        }
        _lastDirection = search._direction;

        // Range endpoints should not be inside DBCS characters, but just in case, move them.
        search._startPosition = doc->MovePositionOutsideChar(search._startPosition, 1, false);
        search._endPosition = doc->MovePositionOutsideChar(search._endPosition, 1, false);

        search._compileFlags =
            std::regex_constants::ECMAScript
            | (caseSensitive ? 0 : std::regex_constants::icase);
        search._regexString = regexString;

        const bool starts_at_line_start = search.isLineStart(search._startPosition);
        const bool ends_at_line_end     = search.isLineEnd(search._endPosition);
        search.regexFlags = std::regex_constants::match_not_null | std::regex_constants::format_first_only |
              (starts_at_line_start ? std::regex_constants::match_default : std::regex_constants::match_not_bol)
            | (ends_at_line_end     ? std::regex_constants::match_default : std::regex_constants::match_not_eol);

        Match match = _utf8.FindText(search);

        if (match.found())
        {
            *lengthRet = match.length();
            _lastMatch = match;
            return match.position();
        }
        else
        {
            _lastMatch = NULL;
            return -1;
        }
    }
    catch(std::regex_error& /*ex*/)
    {
        // -1 is normally used for not found, -2 is used here for invalid regex
        return -2;
    }
}

template <class CharT, class CharacterIterator>
StdRegexSearch::Match StdRegexSearch::EncodingDependent<CharT, CharacterIterator>::FindText(SearchParameters& search)
{
    compileRegex(search._regexString, search._compileFlags);
    return (search._direction > 0)
        ? FindTextForward(search)
        : FindTextBackward(search);
}

template <class CharT, class CharacterIterator>
StdRegexSearch::Match StdRegexSearch::EncodingDependent<CharT, CharacterIterator>::FindTextForward(SearchParameters& search)
{
    CharacterIterator endIterator(search._document, search._endPosition, search._endPosition);
    int next_search_from_position = search._startPosition;
    bool found = false;
    search.regexFlags = search.isLineStart(next_search_from_position)
        ? search.regexFlags & ~std::regex_constants::match_not_bol
        : search.regexFlags |  std::regex_constants::match_not_bol;
    const bool end_reached = next_search_from_position > search._endPosition;
    found = !end_reached && std::regex_search(CharacterIterator(search._document, next_search_from_position, search._endPosition), endIterator, _match, _regex, search.regexFlags);
    if (found)
    {
        const int  position = _match[0].first.pos();
        next_search_from_position = search.nextCharacter(position);
    }

    if (found)
        return Match(search._document, _match[0].first.pos(), _match[0].second.pos());
    else
        return Match();
}

template <class CharT, class CharacterIterator>
StdRegexSearch::Match StdRegexSearch::EncodingDependent<CharT, CharacterIterator>::FindTextBackward(SearchParameters& search)
{
    // Change backward search into series of forward search. It is slow: search all backward becomes O(n^2) instead of O(n) (if search forward is O(n)).
    //NOTE: Maybe we should cache results. Maybe we could reverse regex to do a real backward search, for simple regex.
    search._direction = 1;

    MatchResults bestMatch;
    int bestPosition = -1;
    int bestEnd = -1;
    for (;;)
    {
        Match matchRange = FindText(search);
        if (!matchRange.found())
            break;
        int position = matchRange.position();
        int endPosition = matchRange.endPosition();
        if (endPosition > bestEnd && (endPosition < search._endPosition || position != endPosition)) // We are searching for the longest match which has the fathest end (but may not accept empty match at end position).
        {
            bestMatch = _match;
            bestPosition = position;
            bestEnd = endPosition;
        }
        search._startPosition = search.nextCharacter(position);
    }
    if (bestPosition >= 0)
        return Match(search._document, bestPosition, bestEnd);
    else
        return Match();
}

template <class CharT, class CharacterIterator>
void StdRegexSearch::EncodingDependent<CharT, CharacterIterator>::compileRegex(const char *regex, const int compileFlags)
{
    if (_lastCompileFlags != compileFlags || _lastRegexString != regex)
    {
        _regex = Regex(CharTPtr(regex), static_cast<std::regex_constants::syntax_option_type>(compileFlags));
        _lastRegexString = regex;
        _lastCompileFlags = compileFlags;
    }
}

int StdRegexSearch::SearchParameters::nextCharacter(int position)
{
    if (_document->CharAt(position) == '\r' && _document->CharAt(position+1) == '\n')
        return position + 2;
    else
        return position + 1;
}

bool StdRegexSearch::SearchParameters::isLineStart(int position)
{
    return (position == 0)
        || _document->CharAt(position-1) == '\n'
        || _document->CharAt(position-1) == '\r' && _document->CharAt(position) != '\n';
}

bool StdRegexSearch::SearchParameters::isLineEnd(int position)
{
    return (position == _document->Length())
        || _document->CharAt(position) == '\r'
        || _document->CharAt(position) == '\n' && (position == 0 || _document->CharAt(position-1) != '\n');
}

const char *StdRegexSearch::SubstituteByPosition(Document* /*doc*/, const char *text, int *length)
{
    delete[] _substituted;
    _substituted = _utf8.SubstituteByPosition(text, length);
    return _substituted;
}

template <class CharT, class CharacterIterator>
char *StdRegexSearch::EncodingDependent<CharT, CharacterIterator>::SubstituteByPosition(const char *text, int *length)
{
    char *substituted = stringToCharPtr(_match.format((const CharT*)CharTPtr(text), std::regex_constants::format_default));
    *length = (int)strlen(substituted);
    return substituted;
}


char *StdRegexSearch::stringToCharPtr(const std::string& str)
{
    char *charPtr = new char[str.length() + 1];
    strcpy_s(charPtr, str.length() + 1, str.c_str());
    return charPtr;
}
