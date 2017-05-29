// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017 - Stefan Kueng
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
#include <iterator>
#include <regex>
#include <codecvt>
#include <memory>
#include "scintilla.h"
#define PLATFORM_ASSERT(c) ((void)0)
#include "../ext/scintilla/src/Position.h"
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


class StdRegexSearch : public RegexSearchBase
{
public:
    StdRegexSearch()
        : _lastDirection(0)
    {
    }

    virtual ~StdRegexSearch()
    {
    }

    virtual long FindText(Document* doc, int startPosition, int endPosition, const char *regex,
                          bool caseSensitive, bool word, bool wordStart, int sciSearchFlags, int *lengthRet) override;

    virtual const char *SubstituteByPosition(Document* doc, const char *text, int *length) override;

private:
    class SearchParameters;

    class Match
    {
    public:
        Match()
            : _document(nullptr)
            , _position(-1)
            , _endPosition(-1)
            , _endPositionForContinuationCheck(-1)
        {
        }

        ~Match()
        {
            setDocument(nullptr);
        }
        Match(Document* document, int position = -1, int endPosition = -1)
            : _document(nullptr)
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

        void set(Document* document = nullptr, int position = -1, int endPosition = -1)
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

    template <class CharT, class CharacterIterator>
    class EncodingDependent
    {
    public:
        EncodingDependent() : _lastCompileFlags(-1)
        {
        }
        void compileRegex(const char *regex, const int compileFlags);
        Match FindText(SearchParameters& search);
        std::string SubstituteByPosition(const char *text, int *length);
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

    EncodingDependent<wchar_t, UTF8DocumentIterator> _utf8;

    std::string _substituted;

    Match _lastMatch;
    int _lastDirection;
};


RegexSearchBase *CreateRegexSearch(CharClassify* /* charClassTable */)
{
    return new StdRegexSearch();
}


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

        search.regexFlags = std::regex_constants::format_first_only;

        Match match = _utf8.FindText(search);

        if (match.found())
        {
            *lengthRet = match.length();
            _lastMatch = match;
            return match.position();
        }
        else
        {
            _lastMatch = 0;
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
    CharacterIterator endIterator(search._document, search._endPosition);
    int next_search_from_position = search._startPosition;
    bool found;

    const bool end_reached = next_search_from_position > search._endPosition;
    found = !end_reached && std::regex_search(CharacterIterator(search._document, next_search_from_position), endIterator, _match, _regex, search.regexFlags);

    if (found)
        return Match(search._document, _match[0].first.Pos(), _match[0].second.Pos());
    else
        return Match();
}

template <class CharT, class CharacterIterator>
StdRegexSearch::Match StdRegexSearch::EncodingDependent<CharT, CharacterIterator>::FindTextBackward(SearchParameters& search)
{
    // Change backward search into series of forward search. It is slow: search all backward becomes O(n^2) instead of O(n) (if search forward is O(n)).
    // NOTE: Maybe we should cache results. Maybe we could reverse regex to do a real backward search, for simple regex.
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
        if (endPosition > bestEnd && (endPosition < search._endPosition || position != endPosition)) // We are searching for the longest match which has the farthest end (but may not accept empty match at end position).
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
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> convert;

        _regex = Regex(convert.from_bytes(regex).c_str(), static_cast<std::regex_constants::syntax_option_type>(compileFlags));
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
    _substituted = _utf8.SubstituteByPosition(text, length);
    return _substituted.c_str();
}

template <class CharT, class CharacterIterator>
std::string StdRegexSearch::EncodingDependent<CharT, CharacterIterator>::SubstituteByPosition(const char *text, int *length)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> convert;
    auto s = convert.to_bytes(_match.format(convert.from_bytes(text), std::regex_constants::format_default));
    *length = (int)s.size();
    return s;
}

