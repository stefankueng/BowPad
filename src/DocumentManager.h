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
#include "ScintillaWnd.h"

#include <vector>
#include "Document.h"


class CDocumentManager
{
public:
    CDocumentManager(void);
    ~CDocumentManager(void);

    void                    AddDocumentAtEnd(CDocument doc);
    void                    RemoveDocument(int index);
    void                    SetDocument(int index, CDocument doc) { m_documents[index] = doc; }
    int                     GetCount() const { return (int)m_documents.size(); }
    int                     GetIndexForPath(const std::wstring& path) const;
    CDocument               GetDocument(int index) const { return m_documents.at(index); }
    Document                GetScintillaDocument(int index) const { return m_documents.at(index).m_document; }
    void                    ExchangeDocs(int src, int dst);

    CDocument               LoadFile(HWND hWnd, const std::wstring& path, int encoding);
    bool                    SaveFile(HWND hWnd, const CDocument& doc);
private:
    FormatType              GetEOLFormatForm(const char *data) const;

private:
    std::vector<CDocument>  m_documents;
    CScintillaWnd           m_scratchScintilla;
};

