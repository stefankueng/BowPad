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
#pragma once
#include "ScintillaWnd.h"

#include "Document.h"

enum class DocModifiedState
{
    Unmodified,
    Modified,
    Removed,
    Unknown
};

class DocID
{
public:
    explicit DocID(int i)
        : id(i)
    {
    }
    DocID()
        : id(-1)
    {
    }
    bool IsValid() const { return id >= 0; }
    int  GetValue() const { return id; }

    bool operator==(const DocID& other) const
    {
        return (this->id == other.id);
    }
    bool operator!=(const DocID& other) const
    {
        return (this->id != other.id);
    }
    bool operator<(const DocID& rhs) const
    {
        return id < rhs.id;
    }

private:
    int id;
    friend struct std::hash<DocID>;
};

namespace std
{
template <>
struct hash<DocID>
{
    std::size_t operator()(const DocID& cmp) const noexcept
    {
        using std::hash;
        using std::size_t;

        return hash<int>()(cmp.id); // int hash has also been implemented
    }
};
} // namespace std

constexpr int ReadBlockSize  = 128 * 1024; //128 kB
constexpr int WriteBlockSize = 128 * 1024; //128 kB

class CDocumentManager
{
public:
    CDocumentManager();
    ~CDocumentManager();

    void             AddDocumentAtEnd(const CDocument& doc, DocID id);
    void             RemoveDocument(DocID id);
    int              GetCount() const { return static_cast<int>(m_documents.size()); }
    DocID            GetIdForPath(const std::wstring& path) const;
    bool             HasDocumentID(DocID id) const;
    const CDocument& GetDocumentFromID(DocID id) const;
    CDocument&       GetModDocumentFromID(DocID id);

    CDocument        LoadFile(HWND hWnd, const std::wstring& path, int encoding, bool createIfMissing);
    bool             SaveFile(HWND hWnd, CDocument& doc, bool& bTabMoved) const;
    bool             SaveFile(HWND hWnd, CDocument& doc, const std::wstring& path) const;
    static bool      UpdateFileTime(CDocument& doc, bool bIncludeReadonly);
    DocModifiedState HasFileChanged(DocID id) const;

private:
    bool SaveDoc(HWND hWnd, const std::wstring& path, const CDocument& doc) const;

private:
    std::map<DocID, CDocument> m_documents;
    CScintillaWnd              m_scratchScintilla;

    char                       m_data[ReadBlockSize + 8];
    const int                  m_wideBufSize = ReadBlockSize * 2;
    std::unique_ptr<wchar_t[]> m_wideBuf;
    const int                  m_charBufSize = ReadBlockSize * 4;
    std::unique_ptr<char[]>    m_charBuf;
};
