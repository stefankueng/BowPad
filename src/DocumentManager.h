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

