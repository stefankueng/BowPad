// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2020-2021 - Stefan Kueng
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
#include "Document.h"
#include "StringUtils.h"
#include <CommandHandler.h>

std::wstring getEolFormatDescription(EOLFormat ft)
{
    std::wstring sFt;
    switch (ft)
    {
        case EOLFormat::Win_Format:
            sFt = L"Windows (CRLF)";
            break;
        case EOLFormat::Mac_Format:
            sFt = L"Mac (CR)";
            break;
        case EOLFormat::Unix_Format:
            sFt = L"Unix (LF)";
            break;
        case EOLFormat::Unknown_Format:
            break;
    }
    return sFt;
}

std::wstring CDocument::GetEncodingString() const
{
    std::wstring sEnc;
    switch (m_encoding)
    {
        case CP_UTF8:
            sEnc = L"UTF-8";
            break;
        case 1200:
            sEnc = L"UTF-16 LE";
            break;
        case 1201:
            sEnc = L"UTF-16 BE";
            break;
        case 12001:
            sEnc = L"UTF-32 BE";
            break;
        case 12000:
            sEnc = L"UTF-32 LE";
            break;
        case 0:
            sEnc = L"ANSI";
            break;
        case -1:
            sEnc = L"Binary";
            break;
        default:
            if (static_cast<UINT>(m_encoding) == GetACP())
                sEnc = L"ANSI";
            else
                sEnc = CStringUtils::Format(L"codepage: %d", m_encoding);
            break;
    }
    if (m_bHasBOM)
        sEnc += L", BOM";

    if ((m_encodingSaving != -1) &&
        ((m_encoding != m_encodingSaving) || (m_bHasBOM != m_bHasBOMSaving)))
    {
        sEnc += L" --> ";
        switch (m_encodingSaving)
        {
            case CP_UTF8:
                sEnc += L"UTF-8";
                break;
            case 1200:
                sEnc += L"UTF-16 LE";
                break;
            case 1201:
                sEnc += L"UTF-16 BE";
                break;
            case 12001:
                sEnc += L"UTF-32 BE";
                break;
            case 12000:
                sEnc += L"UTF-32 LE";
                break;
            case 0:
                sEnc += L"ANSI";
                break;
            default:
                if (static_cast<UINT>(m_encoding) == GetACP())
                    sEnc += L"ANSI";
                else
                    sEnc += CStringUtils::Format(L"codepage: %d", m_encoding);
                break;
        }
        if (m_bHasBOMSaving)
            sEnc += L", BOM";
    }

    return sEnc;
}

void CDocument::SetLanguage(const std::string& lang)
{
    bool bDoEvents = !m_language.empty() && (m_language != lang);
    m_language     = lang;
    if (bDoEvents)
        CCommandHandler::Instance().OnLangChanged();
}

EOLFormat toEolFormat(Scintilla::EndOfLine eolMode)
{
    switch (eolMode)
    {
        case Scintilla::EndOfLine::CrLf:
            return EOLFormat::Win_Format;
        case Scintilla::EndOfLine::Lf:
            return EOLFormat::Unix_Format;
        case Scintilla::EndOfLine::Cr:
            return EOLFormat::Mac_Format;
        default:
            break;
    }
    return EOLFormat::Unknown_Format;
}

Scintilla::EndOfLine toEolMode(EOLFormat eolFormat)
{
    switch (eolFormat)
    {
        case EOLFormat::Win_Format:
            return Scintilla::EndOfLine::CrLf;
        case EOLFormat::Unix_Format:
            return Scintilla::EndOfLine::Lf;
        case EOLFormat::Mac_Format:
            return Scintilla::EndOfLine::Cr;
        case EOLFormat::Unknown_Format:
            break;
    }
    return Scintilla::EndOfLine::Lf;
}