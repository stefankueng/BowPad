#pragma once
#include "../ext/scintilla/include/ScintillaCall.h"
#include "../ext/scintilla/include/ScintillaTypes.h"

class SciTextReader
{
public:
    explicit SciTextReader(Scintilla::ScintillaCall& scCall) noexcept
        : startPos(EXTREME_POSITION)
        , endPos(0)
        , sc(scCall)
        , lenDoc(-1)
    {
        buf[0] = 0;
    }

    // Safe version of operator[], returning a defined value for invalid position.
    char SafeGetCharAt(Scintilla::Position position, char chDefault = ' ')
    {
        if (position < startPos || position >= endPos)
        {
            Fill(position);
            if (position < startPos || position >= endPos)
            {
                // Position is outside range of document
                return chDefault;
            }
        }
        return buf[position - startPos];
    }

protected:
    void Fill(Scintilla::Position position)
    {
        if (lenDoc == -1)
            lenDoc = sc.Length();
        startPos = position - SLOP_SIZE;
        if (startPos + BUFFER_SIZE > lenDoc)
            startPos = lenDoc - BUFFER_SIZE;
        if (startPos < 0)
            startPos = 0;
        endPos = startPos + BUFFER_SIZE;
        if (endPos > lenDoc)
            endPos = lenDoc;
        sc.SetTarget(Scintilla::Span(startPos, endPos));
        sc.TargetText(buf);
    }

protected:
    static constexpr Scintilla::Position EXTREME_POSITION = INTPTR_MAX;
    // bufferSize is a trade off between time taken to copy the characters
    // and retrieval overhead.
    static constexpr Scintilla::Position BUFFER_SIZE      = 4000;
    // slopSize positions the buffer before the desired position
    // in case there is some backtracking.
    static constexpr Scintilla::Position SLOP_SIZE        = BUFFER_SIZE / 8;
    char                                 buf[BUFFER_SIZE + 1];
    Scintilla::Position                  startPos;
    Scintilla::Position                  endPos;
    Scintilla::ScintillaCall&            sc;
    Scintilla::Position                  lenDoc;
};
