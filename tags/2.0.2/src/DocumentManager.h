// This file is part of BowPad.
//
// Copyright (C) 2013-2014 - Stefan Kueng
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

#include "Document.h"

enum DocModifiedState
{
    DM_Unmodified,
    DM_Modified,
    DM_Removed,
    DM_Unknown
};

class CDocumentManager
{
public:
    CDocumentManager(void);
    ~CDocumentManager(void);

    void                        AddDocumentAtEnd(const CDocument& doc, int id);
    void                        RemoveDocument(int id);
    void                        SetDocument(int id, const CDocument& doc);
    int                         GetCount() const { return (int)m_documents.size(); }
    int                         GetIdForPath(const std::wstring& path) const;
    bool                        HasDocumentID(int id) const;
    CDocument                   GetDocumentFromID(int id) const;
    Document                    GetScintillaDocument(int id) const
    {
        return GetDocumentFromID(id).m_document;
    }
    COLORREF                    GetColorForDocument(int id);

    CDocument                   LoadFile(HWND hWnd, const std::wstring& path, int encoding, bool createIfMissing);
    bool                        SaveFile(HWND hWnd, const CDocument& doc, bool & bTabMoved);
    bool                        UpdateFileTime(CDocument& doc, bool bIncludeReadonly);
    DocModifiedState            HasFileChanged(int id) const;
private:
    FormatType                  GetEOLFormatForm(const char *data, DWORD len) const;
    bool                        SaveDoc(HWND hWnd, const std::wstring& path, const CDocument& doc);

private:
    std::map<int,CDocument>     m_documents;
    std::map<std::wstring, int> m_foldercolorindexes;
    int                         m_lastfoldercolorindex;
    CScintillaWnd               m_scratchScintilla;
};
